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

// @file xcli.h
// @brief xCLI - text command parser and xCMD dispatch adapter.
//
// xCLI parses a null-terminated C string into an argc/argv token array and
// dispatches the result through xCMD. The input buffer is modified in place;
// token pointers in argv reference substrings of the caller's buffer.
//
// Grammar: command [arg1 [arg2 ...]] ["quoted arg"] ['quoted arg']
// Escape sequences are not supported in v1.
//

#ifndef XCLI_H
#define XCLI_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stddef.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xcmd.h"
#include "xshell_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Parse line in place into argv/argc tokens.
    // line is modified: whitespace and quote delimiters are replaced with '\0'.
    // argv must point to a caller-owned array of at least argv_capacity pointers.
    // Returns xRETURN_xMSG_xSHELL_NO_COMMAND if the line is empty or whitespace only.
    // Returns xRETURN_xERR_xSHELL_INVALID_ARG for an unterminated quote.
    // Returns xRETURN_xERR_xSHELL_BUFFER_FULL if token count exceeds argv_capacity.
    xRETURN_t xCLI_Parse_Line(char *line, char **argv, size_t argv_capacity, size_t *argc);

    // Parse line and dispatch through cmd_ctx.
    // session_ctx is forwarded to xCMD_Request_t.session_ctx for callbacks.
    // Returns xRETURN_xMSG_xSHELL_NO_COMMAND if the line is empty.
    // Returns the result of xCMD_Dispatch on a non-empty line.
    xRETURN_t xCLI_Execute_Line(xCMD_Context_t *cmd_ctx, char *line, void *session_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XCLI_H
// EOF /////////////////////////////////////////////////////////////////////////////
