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

// @file xshell_return.h
// @brief xSHELL module return codes.
//

#ifndef XSHELL_RETURN_H
#define XSHELL_RETURN_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xreturn.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef enum
    {
        // Success.
        xRETURN_xSHELL_OK = 0,

        // A required pointer argument was NULL.
        xRETURN_xERR_xSHELL_NULL_POINTER = xRETURN_MAKE(xRETURN_xSHELL_MODULE, xRETURN_SEVERITY_ERROR, 0x001),

        // An argument value was invalid for the requested operation.
        xRETURN_xERR_xSHELL_INVALID_ARG,

        // The context is not in a valid state for the requested operation.
        xRETURN_xERR_xSHELL_INVALID_STATE,

        // A buffer or registry is full and cannot accept more data.
        xRETURN_xERR_xSHELL_BUFFER_FULL,

        // The requested command path was not found in the registry.
        xRETURN_xERR_xSHELL_NOT_FOUND,

        // A registered command callback returned a non-OK status.
        xRETURN_xERR_xSHELL_CALLBACK_FAILED,

        // The input line contained no command tokens (empty or whitespace only).
        xRETURN_xMSG_xSHELL_NO_COMMAND = xRETURN_MAKE(xRETURN_xSHELL_MODULE, xRETURN_SEVERITY_MESSAGE, 0x001),
    } xRETURN_xSHELL_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSHELL_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
