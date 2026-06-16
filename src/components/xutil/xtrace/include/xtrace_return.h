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

// @file xtrace_return.h
// @brief xTrace module return codes.
//

#ifndef XTRACE_RETURN_H
#define XTRACE_RETURN_H

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
        // A required pointer argument was NULL.
        xRETURN_xERR_xTRACE_NULL_POINTER = xRETURN_MAKE(xRETURN_xTRACE_MODULE, xRETURN_SEVERITY_ERROR, 0x001U),

        // An argument value was out of range or invalid for the requested operation.
        xRETURN_xERR_xTRACE_INVALID_ARG,

        // The context is in a state that does not permit the requested operation.
        xRETURN_xERR_xTRACE_INVALID_STATE,

        // xTRACE_Init has not been called or did not succeed.
        xRETURN_xERR_xTRACE_NOT_INITIALIZED,

        // The caller-provided buffer is too small to hold the minimum required records.
        xRETURN_xERR_xTRACE_BUFFER_TOO_SMALL,

        // The transport write function returned a failure.
        xRETURN_xERR_xTRACE_TRANSPORT,

        // The requested operation or configuration is not supported in this build.
        xRETURN_xERR_xTRACE_UNSUPPORTED,
    } xRETURN_xTRACE_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XTRACE_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
