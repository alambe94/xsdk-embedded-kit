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

// @file xcli.c
// @brief xCLI - text line parser and xCMD dispatch adapter.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xcli.h"
#include "xcmd.h"
#include "xassert.h"
#include "xshell_config.h"
#include "xshell_return.h"

#include "xshell_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static void skip_whitespace(char **p);
static xRETURN_t consume_quoted_token(char **p, char **argv, size_t cap, size_t *cnt, char close_char);
static void consume_unquoted_chars(char **p);
static xRETURN_t consume_unquoted_token(char **p, char **argv, size_t cap, size_t *cnt);
static xRETURN_t consume_token(char **p, char **argv, size_t cap, size_t *cnt);
static xRETURN_t parse_tokens(char *line, char **argv, size_t cap, size_t *cnt);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static void skip_whitespace(char **p)
{
    while ((**p == ' ') || (**p == '\t'))
    {
        *p = *p + 1;
    }
}

// Consume characters up to the matching close_char. Null-terminates on close.
// *p must point to the first character after the opening quote on entry.
static xRETURN_t consume_quoted_token(char **p, char **argv, size_t cap, size_t *cnt, char close_char)
{
    if (*cnt >= cap)
    {
        return xRETURN_xERR_xSHELL_BUFFER_FULL;
    }
    argv[*cnt] = *p;
    *cnt = *cnt + 1U;

    while ((**p != '\0') && (**p != close_char))
    {
        *p = *p + 1;
    }
    if (**p == '\0')
    {
        xSHELL_LOG(xRETURN_xERR_xSHELL_INVALID_ARG, "unterminated quote");
        return xRETURN_xERR_xSHELL_INVALID_ARG;
    }
    **p = '\0';
    *p = *p + 1;
    return xRETURN_OK;
}

// Advance *p past non-whitespace characters, null-terminating at the first whitespace.
static void consume_unquoted_chars(char **p)
{
    while ((**p != '\0') && (**p != ' ') && (**p != '\t'))
    {
        *p = *p + 1;
    }
    if (**p != '\0')
    {
        **p = '\0';
        *p = *p + 1;
    }
}

static xRETURN_t consume_unquoted_token(char **p, char **argv, size_t cap, size_t *cnt)
{
    if (*cnt >= cap)
    {
        return xRETURN_xERR_xSHELL_BUFFER_FULL;
    }
    argv[*cnt] = *p;
    *cnt = *cnt + 1U;
    consume_unquoted_chars(p);
    return xRETURN_OK;
}

// Dispatch to the appropriate token consumer based on the current character.
static xRETURN_t consume_token(char **p, char **argv, size_t cap, size_t *cnt)
{
    char c = **p;
    if (c == '"')
    {
        *p = *p + 1;
        return consume_quoted_token(p, argv, cap, cnt, '"');
    }
    if (c == '\'')
    {
        *p = *p + 1;
        return consume_quoted_token(p, argv, cap, cnt, '\'');
    }
    return consume_unquoted_token(p, argv, cap, cnt);
}

// Walk line, skip whitespace, and call consume_token for each token found.
static xRETURN_t parse_tokens(char *line, char **argv, size_t cap, size_t *cnt)
{
    char *p = line;

    while (*p != '\0')
    {
        skip_whitespace(&p);
        if (*p == '\0')
        {
            break;
        }
        xRETURN_t ret = consume_token(&p, argv, cap, cnt);
        if (ret != xRETURN_OK)
        {
            return ret;
        }
    }
    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xCLI_Parse_Line(char *line, char **argv, size_t argv_capacity, size_t *argc)
{
    xASSERT(line != NULL, "line is NULL");
    xASSERT(argv != NULL, "argv is NULL");
    xASSERT(argc != NULL, "argc is NULL");

    if ((line == NULL) || (argv == NULL) || (argc == NULL))
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }
    if (argv_capacity == 0U)
    {
        return xRETURN_xERR_xSHELL_INVALID_ARG;
    }

    *argc = 0U;

    xRETURN_t ret = parse_tokens(line, argv, argv_capacity, argc);
    if (ret != xRETURN_OK)
    {
        *argc = 0U;
        return ret;
    }
    if (*argc == 0U)
    {
        return xRETURN_xMSG_xSHELL_NO_COMMAND;
    }
    return xRETURN_OK;
}

xRETURN_t xCLI_Execute_Line(xCMD_Context_t *cmd_ctx, char *line, void *session_ctx)
{
    xASSERT(cmd_ctx != NULL, "cmd_ctx is NULL");
    xASSERT(line != NULL, "line is NULL");

    if ((cmd_ctx == NULL) || (line == NULL))
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }

    char *argv[xSHELL_MAX_ARGS];
    size_t argc = 0U;

    xRETURN_t ret = xCLI_Parse_Line(line, argv, xSHELL_MAX_ARGS, &argc);
    if (ret != xRETURN_OK)
    {
        return ret;
    }

    xCMD_Request_t request;
    request.path = argv[0];
    request.argc = argc;
    request.argv = (const char *const *)argv;
    request.source = xCMD_SOURCE_CLI;
    request.session_ctx = session_ctx;
    request.user_ctx = NULL;

    return xCMD_Dispatch(cmd_ctx, &request);
}

// EOF /////////////////////////////////////////////////////////////////////////////
