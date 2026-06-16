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

// @file xfault_return.h
// @brief xFAULT module return codes.
//

#ifndef XFAULT_RETURN_H
#define XFAULT_RETURN_H

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
        xRETURN_xERR_xFAULT_NULL_POINTER = xRETURN_MAKE(xRETURN_xFAULT_MODULE, xRETURN_SEVERITY_ERROR, 0x001U),

        // An argument value was out of range or invalid for the requested operation.
        xRETURN_xERR_xFAULT_INVALID_ARGUMENT,

        // The context is in a state that does not permit the requested operation.
        xRETURN_xERR_xFAULT_INVALID_STATE,

        // The requested target-specific operation is not available in this build.
        xRETURN_xERR_xFAULT_UNSUPPORTED_TARGET,

        // The configured output sink failed or made no forward progress.
        xRETURN_xERR_xFAULT_OUTPUT_FAILED,

        // A stack frame pointer or stack range is outside the valid bounds.
        xRETURN_xERR_xFAULT_STACK_OUT_OF_RANGE,
    } xRETURN_xFAULT_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XFAULT_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
