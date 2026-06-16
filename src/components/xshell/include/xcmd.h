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

// @file xcmd.h
// @brief xCMD - parser-independent command table, lookup, and dispatch.
//
// xCMD provides a deterministic command registry and dispatcher. It operates
// on pre-parsed argc/argv and is completely text-format agnostic. The caller
// owns all storage: the command table, contexts, and request descriptors.
//

#ifndef XCMD_H
#define XCMD_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stddef.h>
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xshell_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Identifies where a command request originated.
    typedef enum
    {
        xCMD_SOURCE_DIRECT = 0, // Dispatched directly by application code.
        xCMD_SOURCE_CLI,        // Originated from xCLI text parsing.
        xCMD_SOURCE_TEST,       // Injected by a host test.
    } xCMD_Source_t;

    // Describes one command dispatch request.
    // Populated by the caller before calling xCMD_Dispatch.
    typedef struct xCMD_Request_t
    {
        const char *path;        // Command path string to look up.
        size_t argc;             // Number of arguments in argv.
        const char *const *argv; // Argument vector; argv[0] is the command name.
        xCMD_Source_t source;    // Origin of the request.
        void *session_ctx;       // Opaque session context (e.g. xSHELL_Context_t *).
        void *user_ctx;          // Populated by the dispatcher from xCMD_Command_t.command_ctx.
    } xCMD_Request_t;

    // Signature for a command callback.
    typedef xRETURN_t (*xCMD_Callback_t)(xCMD_Request_t *request);

    // Describes a single registered command.
    // All pointer fields must remain valid for the lifetime of the registry.
    typedef struct xCMD_Command_t
    {
        const char *path;         // Command path string; must be non-NULL.
        const char *summary;      // One-line description shown by help; may be NULL.
        const char *usage;        // Usage string shown by help <cmd>; may be NULL.
        xCMD_Callback_t callback; // Dispatch target; must be non-NULL.
        void *command_ctx;        // Passed to the callback via request->user_ctx.
    } xCMD_Command_t;

    // Caller-owned command registry context.
    // Initialise with xCMD_Init before use.
    typedef struct xCMD_Context_t
    {
        xCMD_Command_t *commands; // Pointer to the caller-owned command table array.
        size_t capacity;          // Total number of slots in the commands array.
        size_t count;             // Number of registered commands.
    } xCMD_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialise cmd_ctx with a caller-owned command_table of capacity slots.
    // Returns xRETURN_xERR_xSHELL_NULL_POINTER if any pointer is NULL.
    // Returns xRETURN_xERR_xSHELL_INVALID_ARG if capacity is zero.
    xRETURN_t xCMD_Init(xCMD_Context_t *cmd_ctx, xCMD_Command_t *command_table, size_t capacity);

    // Register a command into cmd_ctx.
    // Returns xRETURN_xERR_xSHELL_INVALID_ARG if command->path or callback is NULL,
    //   or if an identical path is already registered.
    // Returns xRETURN_xERR_xSHELL_BUFFER_FULL if the registry is at capacity.
    xRETURN_t xCMD_Register(xCMD_Context_t *cmd_ctx, const xCMD_Command_t *command);

    // Dispatch request to the matching registered command.
    // request->user_ctx is populated from the command's command_ctx before the callback.
    // Returns xRETURN_xERR_xSHELL_NOT_FOUND if no command matches request->path.
    // Returns the callback's own return value on invocation.
    xRETURN_t xCMD_Dispatch(xCMD_Context_t *cmd_ctx, xCMD_Request_t *request);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XCMD_H
// EOF /////////////////////////////////////////////////////////////////////////////
