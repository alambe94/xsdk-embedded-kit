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

// @file xshell.h
// @brief xSHELL - polling shell session, prompt, line buffer, and transport I/O.
//
// xSHELL drives a command-line session over a caller-provided transport. The
// caller owns all storage. xSHELL_Process() is polling-friendly and may be
// called from a superloop or an application-managed task.
//
// Layering: xSHELL calls xCLI and xCMD. xCMD callbacks must not call xSHELL
// transport functions directly; use xSHELL_Write or xSHELL_Write_String with
// the session_ctx pointer passed through xCMD_Request_t.
//

#ifndef XSHELL_H
#define XSHELL_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xcmd.h"
#include "xcli.h"
#include "xshell_config.h"
#include "xshell_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Caller-supplied byte-stream transport interface.
    // All function pointers are required except flush, which may be NULL.
    typedef struct xSHELL_Transport_t
    {
        // Read up to buffer_length bytes into buffer. Sets *bytes_read on success.
        xRETURN_t (*read)(void *transport_ctx, uint8_t *buffer, size_t buffer_length, size_t *bytes_read);

        // Write length bytes from buffer. Sets *bytes_written on success.
        xRETURN_t (*write)(void *transport_ctx, const uint8_t *buffer, size_t length, size_t *bytes_written);

        // Flush any buffered output. May be NULL if the transport has no output buffer.
        xRETURN_t (*flush)(void *transport_ctx);
    } xSHELL_Transport_t;

    // Session lifecycle states.
    typedef enum
    {
        xSHELL_STATE_UNINIT = 0,  // Context has not been initialised.
        xSHELL_STATE_READY = 1,   // Initialised; waiting for xSHELL_Start.
        xSHELL_STATE_RUNNING = 2, // Active; xSHELL_Process may be called.
        xSHELL_STATE_STOPPED = 3, // Stopped; must be re-initialised to restart.
    } xSHELL_State_t;

    // Shell session initialisation parameters.
    typedef struct xSHELL_Config_t
    {
        xCMD_Context_t *cmd_ctx;             // Initialised command registry; required.
        const xSHELL_Transport_t *transport; // Transport interface; required.
        void *transport_ctx;                 // Passed to every transport call.
        const char *prompt;                  // Prompt string; NULL disables prompt output.
    } xSHELL_Config_t;

    // Caller-owned shell session context.
    // Do not access fields directly; use the xSHELL_* API.
    typedef struct xSHELL_Context_t
    {
        xCMD_Context_t *cmd_ctx;
        const xSHELL_Transport_t *transport;
        void *transport_ctx;
        char prompt[xSHELL_MAX_PROMPT_LENGTH];
        char line_buffer[xSHELL_MAX_LINE_LENGTH];
        char *argv[xSHELL_MAX_ARGS];
        size_t line_length;
        xSHELL_State_t state;
        bool is_overflow;
        bool skip_next_lf;
    } xSHELL_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialise shell_ctx from config. Transitions state from UNINIT to READY.
    // Returns xRETURN_xERR_xSHELL_NULL_POINTER if any required pointer is NULL.
    // Returns xRETURN_xERR_xSHELL_INVALID_ARG if required transport callbacks are missing.
    xRETURN_t xSHELL_Init(xSHELL_Context_t *shell_ctx, const xSHELL_Config_t *config);

    // Start the session. Transitions READY -> RUNNING and emits the prompt.
    // Returns xRETURN_xERR_xSHELL_INVALID_STATE if not in READY state.
    xRETURN_t xSHELL_Start(xSHELL_Context_t *shell_ctx);

    // Poll the transport for input and process any complete lines.
    // Must be called only in RUNNING state.
    // Returns xRETURN_xERR_xSHELL_INVALID_STATE if not in RUNNING state.
    xRETURN_t xSHELL_Process(xSHELL_Context_t *shell_ctx);

    // Stop the session. Transitions RUNNING -> STOPPED.
    // Returns xRETURN_xERR_xSHELL_INVALID_STATE if not in RUNNING state.
    xRETURN_t xSHELL_Stop(xSHELL_Context_t *shell_ctx);

    // Write raw bytes to the transport. Safe to call from command callbacks via session_ctx.
    // Returns xRETURN_xERR_xSHELL_NULL_POINTER if shell_ctx or data is NULL.
    xRETURN_t xSHELL_Write(xSHELL_Context_t *shell_ctx, const uint8_t *data, size_t length);

    // Write a null-terminated string to the transport.
    // Returns xRETURN_xERR_xSHELL_NULL_POINTER if shell_ctx or str is NULL.
    xRETURN_t xSHELL_Write_String(xSHELL_Context_t *shell_ctx, const char *str);

    // Register the built-in help and echo commands into shell_ctx->cmd_ctx.
    // Compiled out when xSHELL_CONFIG_DISABLE_BUILTINS is defined.
#ifndef xSHELL_CONFIG_DISABLE_BUILTINS
    xRETURN_t xSHELL_Register_Builtins(xSHELL_Context_t *shell_ctx);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSHELL_H
// EOF /////////////////////////////////////////////////////////////////////////////
