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

// @file xcmd.c
// @brief xCMD - command table registration and deterministic linear dispatch.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xcmd.h"
#include "xassert.h"
#include "xshell_return.h"

#include "xshell_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static bool paths_equal(const char *a, const char *b);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// Returns true if a and b are identical null-terminated strings.
static bool paths_equal(const char *a, const char *b)
{
    while ((*a != '\0') && (*b != '\0'))
    {
        if (*a != *b)
        {
            return false;
        }
        a++;
        b++;
    }
    return (*a == '\0') && (*b == '\0');
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xCMD_Init(xCMD_Context_t *cmd_ctx, xCMD_Command_t *command_table, size_t capacity)
{
    xASSERT(cmd_ctx != NULL, "cmd_ctx is NULL");
    xASSERT(command_table != NULL, "command_table is NULL");
    xASSERT(capacity > 0U, "capacity is zero");

    if ((cmd_ctx == NULL) || (command_table == NULL))
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }
    if (capacity == 0U)
    {
        return xRETURN_xERR_xSHELL_INVALID_ARG;
    }

    cmd_ctx->commands = command_table;
    cmd_ctx->capacity = capacity;
    cmd_ctx->count = 0U;

    return xRETURN_OK;
}

xRETURN_t xCMD_Register(xCMD_Context_t *cmd_ctx, const xCMD_Command_t *command)
{
    xASSERT(cmd_ctx != NULL, "cmd_ctx is NULL");
    xASSERT(command != NULL, "command is NULL");

    if ((cmd_ctx == NULL) || (command == NULL))
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }
    if ((command->path == NULL) || (command->callback == NULL))
    {
        xSHELL_LOG(xRETURN_xERR_xSHELL_INVALID_ARG, "command path or callback is NULL");
        return xRETURN_xERR_xSHELL_INVALID_ARG;
    }
    if (cmd_ctx->count >= cmd_ctx->capacity)
    {
        xSHELL_LOG(xRETURN_xERR_xSHELL_BUFFER_FULL, "command registry is full");
        return xRETURN_xERR_xSHELL_BUFFER_FULL;
    }

    for (size_t i = 0U; i < cmd_ctx->count; i++)
    {
        if (paths_equal(cmd_ctx->commands[i].path, command->path))
        {
            xSHELL_LOG(xRETURN_xERR_xSHELL_INVALID_ARG, "duplicate command path");
            return xRETURN_xERR_xSHELL_INVALID_ARG;
        }
    }

    cmd_ctx->commands[cmd_ctx->count] = *command;
    cmd_ctx->count++;

    return xRETURN_OK;
}

xRETURN_t xCMD_Dispatch(xCMD_Context_t *cmd_ctx, xCMD_Request_t *request)
{
    xASSERT(cmd_ctx != NULL, "cmd_ctx is NULL");
    xASSERT(request != NULL, "request is NULL");

    if ((cmd_ctx == NULL) || (request == NULL))
    {
        return xRETURN_xERR_xSHELL_NULL_POINTER;
    }
    if (request->path == NULL)
    {
        return xRETURN_xERR_xSHELL_INVALID_ARG;
    }

    for (size_t i = 0U; i < cmd_ctx->count; i++)
    {
        if (paths_equal(cmd_ctx->commands[i].path, request->path))
        {
            request->user_ctx = cmd_ctx->commands[i].command_ctx;
            return cmd_ctx->commands[i].callback(request);
        }
    }

    xSHELL_LOG(xRETURN_xERR_xSHELL_NOT_FOUND, "command not found");
    return xRETURN_xERR_xSHELL_NOT_FOUND;
}

// EOF /////////////////////////////////////////////////////////////////////////////
