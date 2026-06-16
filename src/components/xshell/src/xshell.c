// Copyright 2022 alambe94

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xshell.c
// @brief xSHELL - polling shell session, built-in commands, and transport I/O.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xcli.h"
#include "xcmd.h"
#include "xassert.h"
#include "xshell.h"
#include "xshell_config.h"
#include "xshell_return.h"

#include "xshell_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static void copy_prompt(char *dst, const char *src, size_t max_len);
static xRETURN_t write_prompt(xSHELL_Context_t *shell_ctx);
static xRETURN_t dispatch_line(xSHELL_Context_t *shell_ctx);
static xRETURN_t process_byte(xSHELL_Context_t *shell_ctx, uint8_t byte);

#ifndef xSHELL_CONFIG_DISABLE_BUILTINS
static xRETURN_t builtin_help(xCMD_Request_t *request);
static xRETURN_t builtin_echo(xCMD_Request_t *request);
static xRETURN_t list_all_commands(xSHELL_Context_t *shell_ctx);
static xRETURN_t show_command_help(xSHELL_Context_t *shell_ctx, const char *path);
#endif

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static void copy_prompt(char *dst, const char *src, size_t max_len)
{
    size_t i = 0U;
    while ((i < (max_len - 1U)) && (src[i] != '\0'))
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static xRETURN_t write_prompt(xSHELL_Context_t *shell_ctx)
{
    if (shell_ctx->prompt[0] == '\0')
    {
        return xRETURN_OK;
    }
    size_t len = 0U;
    while ((len < xSHELL_MAX_PROMPT_LENGTH) && (shell_ctx->prompt[len] != '\0'))
    {
        len++;
    }
    size_t written = 0U;
    return shell_ctx->transport->write(shell_ctx->transport_ctx, (const uint8_t *)shell_ctx->prompt, len, &written);
}

static xRETURN_t dispatch_line(xSHELL_Context_t *shell_ctx)
{
    static const uint8_t newline[] = {'\r', '\n'};
    size_t written = 0U;

    (void)shell_ctx->transport->write(shell_ctx->transport_ctx, newline, 2U, &written);

    if (shell_ctx->is_overflow)
    {
        shell_ctx->is_overflow = false;
        shell_ctx->line_length = 0U;
        xSHELL_LOG(xRETURN_xERR_xSHELL_BUFFER_FULL, "line buffer overflow discarded");
        return write_prompt(shell_ctx);
    }

    shell_ctx->line_buffer[shell_ctx->line_length] = '\0';
    shell_ctx->line_length = 0U;

    xRETURN_t ret = xCLI_Execute_Line(shell_ctx->cmd_ctx, shell_ctx->line_buffer, shell_ctx);
    if (xRETURN_IS_ERROR(ret))
    {
        xSHELL_LOG(ret, "command dispatch error");
    }

    return write_prompt(shell_ctx);
}

static xRETURN_t process_byte(xSHELL_Context_t *shell_ctx, uint8_t byte)
{
    // Absorb the LF that follows a CR in CRLF sequences.
    if (shell_ctx->skip_next_lf && ((char)byte == '\n'))
    {
        shell_ctx->skip_next_lf = false;
        return xRETURN_OK;
    }
    shell_ctx->skip_next_lf = false;

    if (((char)byte == '\r') || ((char)byte == '\n'))
    {
        if ((char)byte == '\r')
        {
            shell_ctx->skip_next_lf = true;
        }
        return dispatch_line(shell_ctx);
    }

    // Backspace: DEL (0x7F) or BS (0x08).
    if (((char)byte == '\b') || (byte == 0x7FU))
    {
        if (shell_ctx->line_length > 0U)
        {
            shell_ctx->line_length--;
            // Recover overflow state when the buffer drains below the limit.
            if (shell_ctx->line_length < (xSHELL_MAX_LINE_LENGTH - 1U))
            {
                shell_ctx->is_overflow = false;
            }
        }
        return xRETURN_OK;
    }

    if (shell_ctx->line_length >= (xSHELL_MAX_LINE_LENGTH - 1U))
    {
        shell_ctx->is_overflow = true;
        return xRETURN_xERR_xSHELL_BUFFER_FULL;
    }

    shell_ctx->line_buffer[shell_ctx->line_length] = (char)byte;
    shell_ctx->line_length++;
    return xRETURN_OK;
}

#ifndef xSHELL_CONFIG_DISABLE_BUILTINS

static xRETURN_t list_all_commands(xSHELL_Context_t *shell_ctx)
{
    const xCMD_Context_t *cmd_ctx = shell_ctx->cmd_ctx;

    for (size_t i = 0U; i < cmd_ctx->count; i++)
    {
        xRETURN_t ret = xSHELL_Write_String(shell_ctx, cmd_ctx->commands[i].path);
        if (ret != xRETURN_OK)
        {
            return ret;
        }
        if (cmd_ctx->commands[i].summary != NULL)
        {
            ret = xSHELL_Write_String(shell_ctx, " - ");
            if (ret != xRETURN_OK)
            {
                return ret;
            }
            ret = xSHELL_Write_String(shell_ctx, cmd_ctx->commands[i].summary);
            if (ret != xRETURN_OK)
            {
                return ret;
            }
        }
        ret = xSHELL_Write_String(shell_ctx, "\r\n");
        if (ret != xRETURN_OK)
        {
            return ret;
        }
    }
    return xRETURN_OK;
}

static xRETURN_t show_command_help(xSHELL_Context_t *shell_ctx, const char *path)
{
    const xCMD_Context_t *cmd_ctx = shell_ctx->cmd_ctx;

    for (size_t i = 0U; i < cmd_ctx->count; i++)
    {
        // Manual string compare; xcmd.c::paths_equal is private.
        const char *a = cmd_ctx->commands[i].path;
        const char *b = path;
        bool match = true;
        while ((*a != '\0') && (*b != '\0'))
        {
            if (*a != *b)
            {
                match = false;
                break;
            }
            a++;
            b++;
        }
        if (match && (*a == '\0') && (*b == '\0'))
        {
            const char *usage = cmd_ctx->commands[i].usage;
            if (usage != NULL)
            {
                xRETURN_t ret = xSHELL_Write_String(shell_ctx, usage);
                if (ret != xRETURN_OK)
                {
                    return ret;
                }
                return xSHELL_Write_String(shell_ctx, "\r\n");
            }
            return xSHELL_Write_String(shell_ctx, "No usage available.\r\n");
        }
    }
    return xSHELL_Write_String(shell_ctx, "Command not found.\r\n");
}

static xRETURN_t builtin_help(xCMD_Request_t *request)
{
    xSHELL_Context_t *shell_ctx = (xSHELL_Context_t *)request->session_ctx;

    if (shell_ctx == NULL)
    {
        return xRETURN_xERR_xSHELL_INVALID_ARG;
    }
    if (request->argc > 1U)
    {
        return show_command_help(shell_ctx, request->argv[1]);
    }
    return list_all_commands(shell_ctx);
}

static xRETURN_t builtin_echo(xCMD_Request_t *request)
{
    xSHELL_Context_t *shell_ctx = (xSHELL_Context_t *)request->session_ctx;

    if (shell_ctx == NULL)
    {
        return xRETURN_xERR_xSHELL_INVALID_ARG;
    }
    for (size_t i = 1U; i < request->argc; i++)
    {
        if (i > 1U)
        {
            xRETURN_t ret = xSHELL_Write_String(shell_ctx, " ");
            if (ret != xRETURN_OK)
            {
                return ret;
            }
        }
        xRETURN_t ret = xSHELL_Write_String(shell_ctx, request->argv[i]);
        if (ret != xRETURN_OK)
        {
            return ret;
        }
    }
    return xSHELL_Write_String(shell_ctx, "\r\n");
}

#endif // xSHELL_CONFIG_DISABLE_BUILTINS

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xSHELL_Init(xSHELL_Context_t *shell_ctx, const xSHELL_Config_t *config)
{
    xASSERT(shell_ctx != NULL, "shell_ctx is NULL");
    xASSERT(config != NULL, "config is NULL");

    if ((shell_ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }
    if ((config->cmd_ctx == NULL) || (config->transport == NULL))
    {
        return xRETURN_xERR_xSHELL_INVALID_ARG;
    }
    if ((config->transport->read == NULL) || (config->transport->write == NULL))
    {
        return xRETURN_xERR_xSHELL_INVALID_ARG;
    }

    shell_ctx->cmd_ctx = config->cmd_ctx;
    shell_ctx->transport = config->transport;
    shell_ctx->transport_ctx = config->transport_ctx;
    shell_ctx->line_length = 0U;
    shell_ctx->state = xSHELL_STATE_READY;
    shell_ctx->is_overflow = false;
    shell_ctx->skip_next_lf = false;

    if (config->prompt != NULL)
    {
        copy_prompt(shell_ctx->prompt, config->prompt, xSHELL_MAX_PROMPT_LENGTH);
    }
    else
    {
        shell_ctx->prompt[0] = '\0';
    }

    return xRETURN_OK;
}

xRETURN_t xSHELL_Start(xSHELL_Context_t *shell_ctx)
{
    xASSERT(shell_ctx != NULL, "shell_ctx is NULL");

    if (shell_ctx == NULL)
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }
    if (shell_ctx->state != xSHELL_STATE_READY)
    {
        return xRETURN_xERR_xSHELL_INVALID_STATE;
    }

    shell_ctx->state = xSHELL_STATE_RUNNING;
    return write_prompt(shell_ctx);
}

xRETURN_t xSHELL_Process(xSHELL_Context_t *shell_ctx)
{
    xASSERT(shell_ctx != NULL, "shell_ctx is NULL");

    if (shell_ctx == NULL)
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }
    if (shell_ctx->state != xSHELL_STATE_RUNNING)
    {
        return xRETURN_xERR_xSHELL_INVALID_STATE;
    }

    uint8_t read_buf[xSHELL_CONFIG_READ_CHUNK_SIZE];
    size_t bytes_read = 0U;

    xRETURN_t ret = shell_ctx->transport->read(shell_ctx->transport_ctx, read_buf, xSHELL_CONFIG_READ_CHUNK_SIZE, &bytes_read);
    if (xRETURN_IS_ERROR(ret))
    {
        return ret;
    }

    for (size_t i = 0U; i < bytes_read; i++)
    {
        xRETURN_t byte_ret = process_byte(shell_ctx, read_buf[i]);
        if (xRETURN_IS_ERROR(byte_ret))
        {
            xSHELL_LOG(byte_ret, "process_byte error");
        }
    }

    return xRETURN_OK;
}

xRETURN_t xSHELL_Stop(xSHELL_Context_t *shell_ctx)
{
    xASSERT(shell_ctx != NULL, "shell_ctx is NULL");

    if (shell_ctx == NULL)
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }
    if (shell_ctx->state != xSHELL_STATE_RUNNING)
    {
        return xRETURN_xERR_xSHELL_INVALID_STATE;
    }

    shell_ctx->state = xSHELL_STATE_STOPPED;
    return xRETURN_OK;
}

xRETURN_t xSHELL_Write(xSHELL_Context_t *shell_ctx, const uint8_t *data, size_t length)
{
    xASSERT(shell_ctx != NULL, "shell_ctx is NULL");
    xASSERT(data != NULL, "data is NULL");

    if ((shell_ctx == NULL) || (data == NULL))
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }

    size_t written = 0U;
    return shell_ctx->transport->write(shell_ctx->transport_ctx, data, length, &written);
}

xRETURN_t xSHELL_Write_String(xSHELL_Context_t *shell_ctx, const char *str)
{
    xASSERT(shell_ctx != NULL, "shell_ctx is NULL");
    xASSERT(str != NULL, "str is NULL");

    if ((shell_ctx == NULL) || (str == NULL))
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }

    size_t len = 0U;
    while (str[len] != '\0')
    {
        len++;
    }
    if (len == 0U)
    {
        return xRETURN_OK;
    }

    size_t written = 0U;
    return shell_ctx->transport->write(shell_ctx->transport_ctx, (const uint8_t *)str, len, &written);
}

#ifndef xSHELL_CONFIG_DISABLE_BUILTINS

xRETURN_t xSHELL_Register_Builtins(xSHELL_Context_t *shell_ctx)
{
    xASSERT(shell_ctx != NULL, "shell_ctx is NULL");

    if (shell_ctx == NULL)
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }

    static const xCMD_Command_t help_cmd = {
        .path = "help",
        .summary = "List commands or show usage for a specific command",
        .usage = "help [command]",
        .callback = builtin_help,
        .command_ctx = NULL,
    };
    static const xCMD_Command_t echo_cmd = {
        .path = "echo",
        .summary = "Echo arguments back to output",
        .usage = "echo [args...]",
        .callback = builtin_echo,
        .command_ctx = NULL,
    };

    xRETURN_t ret = xCMD_Register(shell_ctx->cmd_ctx, &help_cmd);
    if (ret != xRETURN_OK)
    {
        return ret;
    }
    return xCMD_Register(shell_ctx->cmd_ctx, &echo_cmd);
}

#endif // xSHELL_CONFIG_DISABLE_BUILTINS

// EOF /////////////////////////////////////////////////////////////////////////////
