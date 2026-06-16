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

// @file xbridge_return.h
// @brief xBRIDGE module return codes.
//

#ifndef XBRIDGE_RETURN_H
#define XBRIDGE_RETURN_H

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
        // Errors
        xRETURN_xERR_xBRIDGE_NULL_POINTER = xRETURN_MAKE(xRETURN_xBRIDGE_MODULE, xRETURN_SEVERITY_ERROR, 0x001),
        xRETURN_xERR_xBRIDGE_INVALID_ARGUMENT = xRETURN_MAKE(xRETURN_xBRIDGE_MODULE, xRETURN_SEVERITY_ERROR, 0x002),
        xRETURN_xERR_xBRIDGE_INVALID_STATE = xRETURN_MAKE(xRETURN_xBRIDGE_MODULE, xRETURN_SEVERITY_ERROR, 0x003),
        xRETURN_xERR_xBRIDGE_PERIPHERAL_FAIL = xRETURN_MAKE(xRETURN_xBRIDGE_MODULE, xRETURN_SEVERITY_ERROR, 0x004),
        xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE = xRETURN_MAKE(xRETURN_xBRIDGE_MODULE, xRETURN_SEVERITY_ERROR, 0x005),
        xRETURN_xERR_xBRIDGE_QUEUE_FULL = xRETURN_MAKE(xRETURN_xBRIDGE_MODULE, xRETURN_SEVERITY_ERROR, 0x006),
        xRETURN_xERR_xBRIDGE_TIMEOUT = xRETURN_MAKE(xRETURN_xBRIDGE_MODULE, xRETURN_SEVERITY_ERROR, 0x007),
        xRETURN_xERR_xBRIDGE_UNKNOWN_CMD = xRETURN_MAKE(xRETURN_xBRIDGE_MODULE, xRETURN_SEVERITY_ERROR, 0x008),
        xRETURN_xERR_xBRIDGE_PARSE_ERROR = xRETURN_MAKE(xRETURN_xBRIDGE_MODULE, xRETURN_SEVERITY_ERROR, 0x009),

        // Warnings
        xRETURN_xWRN_xBRIDGE_QUEUE_NEAR_FULL = xRETURN_MAKE(xRETURN_xBRIDGE_MODULE, xRETURN_SEVERITY_WARNING, 0x001),

        // Messages
        xRETURN_xMSG_xBRIDGE_CHANNEL_ACTIVE = xRETURN_MAKE(xRETURN_xBRIDGE_MODULE, xRETURN_SEVERITY_MESSAGE, 0x001),
    } xRETURN_xBRIDGE_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
