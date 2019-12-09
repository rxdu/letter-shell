/**
 * @file shell.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief letter shell
 * @version 2.0.0
 * @date 2018-12-29
 * 
 * @Copyright (c) 2018 Letter
 * 
 */

#include "shell.h"
#include "string.h"
#include "stdio.h"
#include "stdarg.h"

#if SHELL_AUTO_PRASE == 1
#include "shell_ext.h"
#endif

/**
 * @brief shell提示信息文本索引
 */
enum
{
    TEXT_INFO,
    TEXT_PWD_HINT,
    TEXT_PWD_RIGHT,
    TEXT_PWD_ERROR,
    TEXT_FUN_LIST,
    TEXT_VAR_LIST,
    TEXT_CMD_NONE,
    TEXT_CMD_TOO_LONG,
    TEXT_READ_NOT_DEF,
};

/**
 * @brief shell提示信息文本
 */
static const char *shellText[] =
    {
        [TEXT_INFO] = "\r\n\r\n"
                      "+=========================================================+\r\n"
                      "|                (C) COPYRIGHT 2019 Letter                |\r\n"
                      "|                   Letter shell v" SHELL_VERSION "                   |\r\n"
                      "|               Build: "__DATE__
                      " "__TIME__
                      "               |\r\n"
                      "+=========================================================+\r\n",
        [TEXT_PWD_HINT] = "\r\nPlease input password:",
        [TEXT_PWD_RIGHT] = "\r\npassword confirm success.\r\n",
        [TEXT_PWD_ERROR] = "\r\npassword confirm failed.\r\n",
        [TEXT_FUN_LIST] = "\r\nCOMMAND LIST:\r\n\r\n",
        [TEXT_VAR_LIST] = "\r\nVARIABLE LIST:\r\n\r\n",
        [TEXT_CMD_NONE] = "Command not found\r\n",
        [TEXT_CMD_TOO_LONG] = "\r\nWarnig: Command is too long\r\n",
        [TEXT_READ_NOT_DEF] = "error: shell.read must be defined\r\n",
};

static SHELL_TypeDef *shellList[SHELL_MAX_NUMBER] = {NULL}; /**< shell列表 */

static void shellAdd(SHELL_TypeDef *shell);
static void shellDisplayItem(SHELL_TypeDef *shell, unsigned short index);

static void shellEnter(SHELL_TypeDef *shell);
static void shellTab(SHELL_TypeDef *shell);
static void shellBackspace(SHELL_TypeDef *shell);
static void shellAnsiStart(SHELL_TypeDef *shell);

/**
 * @brief 默认按键响应映射函数表
 * 
 */
const SHELL_KeyFunctionDef shellDefaultKeyFunctionList[] =
    {
        {SHELL_KEY_LF, shellEnter},
        {SHELL_KEY_CR, shellEnter},
        {SHELL_KEY_TAB, shellTab},
        {SHELL_KEY_BACKSPACE, shellBackspace},
        {SHELL_KEY_DELETE, shellBackspace},
        {SHELL_KEY_ESC, shellAnsiStart},
};

/**
 * @brief shell初始化
 * 
 * @param shell shell对象
 */
void shellInit(SHELL_TypeDef *shell)
{
    shell->length = 0;
    shell->cursor = 0;
    shell->historyCount = 0;
    shell->historyFlag = 0;
    shell->historyOffset = 0;
    shell->status.inputMode = SHELL_IN_NORMAL;
    shell->status.tabFlag = 0;
    shell->command = SHELL_DEFAULT_COMMAND;
    shell->isActive = 0;
    shellAdd(shell);

    shellDisplay(shell, shellText[TEXT_INFO]);
    shellDisplay(shell, shell->command);

    shell->commandBase = (SHELL_CommandTypeDef *)shellDefaultCommandList;
    shell->commandNumber = sizeof(shellDefaultCommandList) / sizeof(SHELL_CommandTypeDef);
}

/**
 * @brief shell设置按键响应
 * 
 * @param shell shell对象
 * @param base 按键响应表基址
 * @param size 按键响应数量
 */
void shellSetKeyFuncList(SHELL_TypeDef *shell, SHELL_KeyFunctionDef *base, unsigned short size)
{
    shell->keyFuncBase = (int)base;
    shell->keyFuncNumber = size;
}

/**
 * @brief shell字符串复制
 * 
 * @param dest 目标字符串
 * @param src 源字符串
 * @return unsigned short 字符串长度
 */
static unsigned short shellStringCopy(char *dest, char *src)
{
    unsigned short count = 0;
    while (*(src + count))
    {
        *(dest + count) = *(src + count);
        count++;
    }
    *(dest + count) = 0;
    return count;
}

/**
 * @brief shell字符串比较
 * 
 * @param dest 目标字符串
 * @param src 源字符串
 * @return unsigned short 匹配长度
 */
static unsigned short shellStringCompare(char *dest, char *src)
{
    unsigned short match = 0;
    unsigned short i = 0;

    while (*(dest + i) && *(src + i))
    {
        if (*(dest + i) != *(src + i))
        {
            break;
        }
        match++;
        i++;
    }
    return match;
}

/**
 * @brief shell删除
 * 
 * @param shell shell对象
 * @param length 删除的长度
 */
static void shellDelete(SHELL_TypeDef *shell, unsigned short length)
{
    while (length--)
    {
        shellDisplay(shell, "\b \b");
    }
}

/**
 * @brief shell清除输入
 * 
 * @param shell shell对象
 */
static void shellClearLine(SHELL_TypeDef *shell)
{
    for (short i = shell->length - shell->cursor; i > 0; i--)
    {
        shellDisplayByte(shell, ' ');
    }
    shellDelete(shell, shell->length);
}

/**
 * @brief shell历史记录添加
 * 
 * @param shell shell对象
 */
static void shellHistoryAdd(SHELL_TypeDef *shell)
{
    shell->historyOffset = 0;
    if (strcmp(shell->history[shell->historyFlag - 1], shell->buffer) == 0)
    {
        return;
    }
    if (shellStringCopy(shell->history[shell->historyFlag], shell->buffer) != 0)
    {
        shell->historyFlag++;
    }
    if (++shell->historyCount > SHELL_HISTORY_MAX_NUMBER)
    {
        shell->historyCount = SHELL_HISTORY_MAX_NUMBER;
    }
    if (shell->historyFlag >= SHELL_HISTORY_MAX_NUMBER)
    {
        shell->historyFlag = 0;
    }
}

/**
 * @brief shell历史记录查找
 * 
 * @param shell shell对象
 * @param dir 查找方向
 */
static void shellHistory(SHELL_TypeDef *shell, unsigned char dir)
{
    if (dir == 0)
    {
        if (shell->historyOffset-- <= -((shell->historyCount > shell->historyFlag)
                                            ? shell->historyCount
                                            : shell->historyFlag))
        {
            shell->historyOffset = -((shell->historyCount > shell->historyFlag)
                                         ? shell->historyCount
                                         : shell->historyFlag);
        }
    }
    else if (dir == 1)
    {
        if (++shell->historyOffset > 0)
        {
            shell->historyOffset = 0;
            return;
        }
    }
    else
    {
        return;
    }
    shellClearLine(shell);
    if (shell->historyOffset == 0)
    {
        shell->cursor = shell->length = 0;
    }
    else
    {
        if ((shell->length = shellStringCopy(shell->buffer,
                                             shell->history[(shell->historyFlag + SHELL_HISTORY_MAX_NUMBER + shell->historyOffset) % SHELL_HISTORY_MAX_NUMBER])) == 0)
        {
            return;
        }
        shell->cursor = shell->length;
        shellDisplay(shell, shell->buffer);
    }
}

/**
 * @brief shell回车输入处理
 * 
 * @param shell shell对象
 */
static void shellEnter(SHELL_TypeDef *shell)
{
    unsigned char paramCount = 0;
    unsigned char quotes = 0;
    unsigned char record = 1;
    SHELL_CommandTypeDef *base;
    unsigned char runFlag = 0;
    int returnValue;
    (void)returnValue;

    if (shell->length == 0)
    {
        shellDisplay(shell, shell->command);
        return;
    }

    *(shell->buffer + shell->length++) = 0;
    shellHistoryAdd(shell);

    for (unsigned short i = 0; i < shell->length; i++)
    {
        if ((quotes != 0 ||
             (*(shell->buffer + i) != ' ' &&
              *(shell->buffer + i) != '\t' &&
              *(shell->buffer + i) != ',')) &&
            *(shell->buffer + i) != 0)
        {
            if (*(shell->buffer + i) == '\"')
            {
                quotes = quotes ? 0 : 1;
#if SHELL_AUTO_PRASE == 0
                *(shell->buffer + i) = 0;
                continue;
#endif
            }
            if (record == 1)
            {
                shell->param[paramCount++] = shell->buffer + i;
                record = 0;
            }
            if (*(shell->buffer + i) == '\\' &&
                *(shell->buffer + i) != 0)
            {
                i++;
            }
        }
        else
        {
            *(shell->buffer + i) = 0;
            record = 1;
        }
    }
    shell->length = 0;
    shell->cursor = 0;
    if (paramCount == 0)
    {
        shellDisplay(shell, shell->command);
        return;
    }

    shellDisplay(shell, "\r\n");
    base = shell->commandBase;
    if (strcmp((const char *)shell->param[0], "help") == 0)
    {
        shell->isActive = 1;
        shellHelp(paramCount, shell->param);
        shell->isActive = 0;
        shellDisplay(shell, shell->command);
        return;
    }

    for (unsigned char i = 0; i < shell->commandNumber; i++)
    {
        if (strcmp((const char *)shell->param[0], (base + i)->name) == 0)
        {
            runFlag = 1;
            shell->isActive = 1;
#if SHELL_AUTO_PRASE == 0
            returnValue = (base + i)->function(paramCount, shell->param);
#else
            returnValue = shellExtRun((base + i)->function, paramCount, shell->param);
#endif /** SHELL_AUTO_PRASE == 0 */
            shell->isActive = 0;
#if SHELL_DISPLAY_RETURN == 1
            shellDisplayReturn(shell, returnValue);
#endif /** SHELL_DISPLAY_RETURN == 1 */
            break;
        }
    }
    if (runFlag == 0)
    {
        shellDisplay(shell, shellText[TEXT_CMD_NONE]);
    }
    shellDisplay(shell, shell->command);
}

/**
 * @brief shell退格输入处理
 * 
 * @param shell shell对象
 */
static void shellBackspace(SHELL_TypeDef *shell)
{
    if (shell->length == 0)
    {
        return;
    }
    if (shell->cursor == shell->length)
    {
        shell->length--;
        shell->cursor--;
        shell->buffer[shell->length] = 0;
        shellDelete(shell, 1);
    }
    else if (shell->cursor > 0)
    {
        for (short i = 0; i < shell->length - shell->cursor; i++)
        {
            shell->buffer[shell->cursor + i - 1] = shell->buffer[shell->cursor + i];
        }
        shell->length--;
        shell->cursor--;
        shell->buffer[shell->length] = 0;
        shellDisplayByte(shell, '\b');
        for (short i = shell->cursor; i < shell->length; i++)
        {
            shellDisplayByte(shell, shell->buffer[i]);
        }
        shellDisplayByte(shell, ' ');
        for (short i = shell->length - shell->cursor + 1; i > 0; i--)
        {
            shellDisplayByte(shell, '\b');
        }
    }
}

/**
 * @brief shell Tab键输入处理
 * 
 * @param shell shell对象
 */
static void shellTab(SHELL_TypeDef *shell)
{
    unsigned short maxMatch = SHELL_COMMAND_MAX_LENGTH;
    unsigned short lastMatchIndex = 0;
    unsigned short matchNum = 0;
    unsigned short length;
    SHELL_CommandTypeDef *base = shell->commandBase;

    if (shell->length != 0)
    {
        shell->buffer[shell->length] = 0;
        for (short i = 0; i < shell->commandNumber; i++)
        {
            if (shellStringCompare(shell->buffer,
                                   (char *)(base + i)->name) == shell->length)
            {
                if (matchNum != 0)
                {
                    if (matchNum == 1)
                    {
                        shellDisplay(shell, "\r\n");
                    }
                    shellDisplayItem(shell, lastMatchIndex);
                    length = shellStringCompare((char *)(base + lastMatchIndex)->name,
                                                (char *)(base + i)->name);
                    maxMatch = (maxMatch > length) ? length : maxMatch;
                }
                lastMatchIndex = i;
                matchNum++;
            }
        }

        if (matchNum == 0)
        {
            return;
        }
        if (matchNum == 1)
        {
            shellClearLine(shell);
        }
        if (matchNum != 0)
        {
            shell->length = shellStringCopy(shell->buffer,
                                            (char *)(base + lastMatchIndex)->name);
        }
        if (matchNum > 1)
        {
            shellDisplayItem(shell, lastMatchIndex);
            shellDisplay(shell, shell->command);
            shell->length = maxMatch;
        }
        shell->buffer[shell->length] = 0;
        shell->cursor = shell->length;
        shellDisplay(shell, shell->buffer);
    }
    else
    {
        shell->isActive = 1;
        shellHelp(1, (void *)0);
        shell->isActive = 0;
        shellDisplay(shell, shell->command);
    }

#if SHELL_LONG_HELP == 1
    if (SHELL_GET_TICK())
    {
        if (matchNum == 1 && shell->status.tabFlag == 1 && SHELL_GET_TICK() - shell->activeTime < SHELL_DOUBLE_CLICK_TIME)
        {
            shellClearLine(shell);
            for (short i = shell->length; i >= 0; i--)
            {
                shell->buffer[i + 5] = shell->buffer[i];
            }
            shellStringCopy(shell->buffer, "help");
            shell->buffer[4] = ' ';
            shell->length += 5;
            shell->cursor = shell->length;
            shellDisplay(shell, shell->buffer);
        }
    }
#endif /** SHELL_LONG_HELP == 1 */
}

/**
 * @brief shell正常按键处理
 * 
 * @param shell shell对象
 * @param data 输入的数据
 */
static void shellNormal(SHELL_TypeDef *shell, char data)
{
    if (data == 0)
    {
        return;
    }
    if (shell->length < SHELL_COMMAND_MAX_LENGTH - 1)
    {
        if (shell->length == shell->cursor)
        {
            shell->buffer[shell->length++] = data;
            shell->cursor++;
            shellDisplayByte(shell, data);
        }
        else
        {
            for (short i = shell->length - shell->cursor; i > 0; i--)
            {
                shell->buffer[shell->cursor + i] = shell->buffer[shell->cursor + i - 1];
            }
            shell->buffer[shell->cursor++] = data;
            shell->buffer[++shell->length] = 0;
            for (short i = shell->cursor - 1; i < shell->length; i++)
            {
                shellDisplayByte(shell, shell->buffer[i]);
            }
            for (short i = shell->length - shell->cursor; i > 0; i--)
            {
                shellDisplayByte(shell, '\b');
            }
        }
    }
    else
    {
        shellDisplay(shell, shellText[TEXT_CMD_TOO_LONG]);
        shellDisplay(shell, shell->command);
        shellDisplay(shell, shell->buffer);
        shell->cursor = shell->length;
    }
}

/**
 * @brief shell开始ansi控制序列
 * 
 * @param shell shell对象
 */
static void shellAnsiStart(SHELL_TypeDef *shell)
{
    shell->status.inputMode = SHELL_ANSI_ESC;
}

/**
 * @brief shell ansi控制序列处理
 * 
 * @param shell shell对象
 * @param data 输入的数据
 */
void shellAnsi(SHELL_TypeDef *shell, char data)
{
    switch ((unsigned char)(shell->status.inputMode))
    {
    case SHELL_ANSI_CSI:
        switch (data)
        {
        case 0x41: /** 方向上键 */
            shellHistory(shell, 0);
            break;

        case 0x42: /** 方向下键 */
            shellHistory(shell, 1);
            break;

        case 0x43: /** 方向右键 */
            if (shell->cursor < shell->length)
            {
                shellDisplayByte(shell, shell->buffer[shell->cursor]);
                shell->cursor++;
            }
            break;

        case 0x44: /** 方向左键 */
            if (shell->cursor > 0)
            {
                shellDisplayByte(shell, '\b');
                shell->cursor--;
            }
            break;

        default:
            break;
        }
        shell->status.inputMode = SHELL_IN_NORMAL;
        break;

    case SHELL_ANSI_ESC:
        if (data == 0x5B)
        {
            shell->status.inputMode = SHELL_ANSI_CSI;
        }
        else
        {
            shell->status.inputMode = SHELL_IN_NORMAL;
        }
        break;

    default:
        break;
    }
}

/**
 * @brief shell处理
 * 
 * @param shell shell对象
 * @param data 输入数据
 */
void shellHandler(SHELL_TypeDef *shell, char data)
{
    if (shell->status.inputMode == SHELL_IN_NORMAL)
    {
        char keyDefFind = 0;
        SHELL_KeyFunctionDef *base = (SHELL_KeyFunctionDef *)shell->keyFuncBase;

        for (short i = 0; i < shell->keyFuncNumber; i++)
        {
            if (base[i].keyCode == data)
            {
                if (base[i].keyFunction)
                {
                    base[i].keyFunction(shell);
                }
                keyDefFind = 1;
            }
        }

        if (keyDefFind == 0)
        {
            for (short i = 0;
                 i < sizeof(shellDefaultKeyFunctionList) / sizeof(SHELL_KeyFunctionDef);
                 i++)
            {
                if (shellDefaultKeyFunctionList[i].keyCode == data)
                {
                    if (shellDefaultKeyFunctionList[i].keyFunction)
                    {
                        shellDefaultKeyFunctionList[i].keyFunction(shell);
                    }
                    keyDefFind = 1;
                }
            }
        }
        if (keyDefFind == 0)
        {
            shellNormal(shell, data);
        }
    }
    else
    {
        shellAnsi(shell, data);
    }

#if SHELL_LONG_HELP == 1
    shell->status.tabFlag = data == '\t' ? 1 : 0;
#endif
}

/**
 * @brief shell显示一条命令信息
 * 
 * @param shell shell对象
 * @param index 要显示的命令索引
 */
static void shellDisplayItem(SHELL_TypeDef *shell, unsigned short index)
{
    unsigned short spaceLength;
    SHELL_CommandTypeDef *base = shell->commandBase;

    spaceLength = 22 - shellDisplay(shell, (base + index)->name);
    spaceLength = (spaceLength > 0) ? spaceLength : 4;
    do
    {
        shellDisplay(shell, " ");
    } while (--spaceLength);
    shellDisplay(shell, "--");
    shellDisplay(shell, (base + index)->desc);
    shellDisplay(shell, "\r\n");
}

/**
 * @brief shell帮助
 * 
 * @param argc 参数个数
 * @param argv 参数
 */
void shellHelp(int argc, char *argv[])
{
    SHELL_TypeDef *shell = shellGetCurrent();
    if (!shell)
    {
        return;
    }
#if SHELL_LONG_HELP == 1
    if (argc == 1)
    {
#endif /** SHELL_LONG_HELP == 1 */
        shellDisplay(shell, shellText[TEXT_FUN_LIST]);
        for (unsigned short i = 0; i < shell->commandNumber; i++)
        {
            shellDisplayItem(shell, i);
        }
#if SHELL_LONG_HELP == 1
    }
    else if (argc == 2)
    {
        SHELL_CommandTypeDef *base = shell->commandBase;
        for (unsigned char i = 0; i < shell->commandNumber; i++)
        {
            if (strcmp((const char *)argv[1], (base + i)->name) == 0)
            {

                shellDisplay(shell, "command help --");
                shellDisplay(shell, (base + i)->name);
                shellDisplay(shell, ":\r\n");
                shellDisplay(shell, (base + i)->desc);
                shellDisplay(shell, "\r\n");
                if ((base + i)->help)
                {
                    shellDisplay(shell, (base + i)->help);
                    shellDisplay(shell, "\r\n");
                }
                return;
            }
        }
        shellDisplay(shell, shellText[TEXT_CMD_NONE]);
    }
#endif /** SHELL_LONG_HELP == 1 */
}
SHELL_EXPORT_CMD_EX(help, shellHelp, command help, help[command]-- show help info of command);

/**
 * @brief 清空命令行
 * 
 */
void shellClear(void)
{
    SHELL_TypeDef *shell = shellGetCurrent();
    if (!shell)
    {
        return;
    }
    shellDisplay(shell, "\033[2J\033[1H");
}
SHELL_EXPORT_CMD(cls, shellClear, clear command line);
