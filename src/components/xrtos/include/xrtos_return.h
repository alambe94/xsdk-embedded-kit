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

// @file xrtos_return.h
// @brief xRTOS-specific return code definitions.
//
// All xRTOS return codes use module ID xRETURN_xRTOS_MODULE (0x0004U),
// which is registered in xreturn.h. No xRTOS source file shall introduce
// return constants outside this header.
//

#ifndef XRTOS_RETURN_H
#define XRTOS_RETURN_H

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

    // xRETURN_xRTOS_MODULE = 0x0004U (registered in xreturn.h)

#define xRETURN_xRTOS_OK                    xRETURN_OK
#define xRETURN_xERR_xRTOS_NULL_POINTER     xRETURN_MAKE(xRETURN_xRTOS_MODULE, xRETURN_SEVERITY_ERROR, 1U)
#define xRETURN_xERR_xRTOS_INVALID_ARGUMENT xRETURN_MAKE(xRETURN_xRTOS_MODULE, xRETURN_SEVERITY_ERROR, 2U)
#define xRETURN_xERR_xRTOS_INVALID_STATE    xRETURN_MAKE(xRETURN_xRTOS_MODULE, xRETURN_SEVERITY_ERROR, 3U)
#define xRETURN_xERR_xRTOS_TASK_LIMIT       xRETURN_MAKE(xRETURN_xRTOS_MODULE, xRETURN_SEVERITY_ERROR, 4U)
#define xRETURN_xERR_xRTOS_PRIORITY_IN_USE  xRETURN_MAKE(xRETURN_xRTOS_MODULE, xRETURN_SEVERITY_ERROR, 5U)
#define xRETURN_xERR_xRTOS_TIMEOUT          xRETURN_MAKE(xRETURN_xRTOS_MODULE, xRETURN_SEVERITY_ERROR, 6U)
#define xRETURN_xERR_xRTOS_WOULD_BLOCK      xRETURN_MAKE(xRETURN_xRTOS_MODULE, xRETURN_SEVERITY_ERROR, 7U)
#define xRETURN_xERR_xRTOS_NO_TASKS_READY   xRETURN_MAKE(xRETURN_xRTOS_MODULE, xRETURN_SEVERITY_ERROR, 8U)
#define xRETURN_xERR_xRTOS_RESOURCE_FULL    xRETURN_MAKE(xRETURN_xRTOS_MODULE, xRETURN_SEVERITY_ERROR, 9U)

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
