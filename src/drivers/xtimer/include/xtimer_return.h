// Copyright 2026 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xtimer_return.h
// @brief xTIMER driver return codes.
//

#ifndef XTIMER_RETURN_H
#define XTIMER_RETURN_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xreturn.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef enum
    {
        xRETURN_xERR_xTIMER_NULL_POINTER = xRETURN_MAKE(xRETURN_xTIMER_MODULE, xRETURN_SEVERITY_ERROR, 0x001U),
        xRETURN_xERR_xTIMER_INVALID_ARG = xRETURN_MAKE(xRETURN_xTIMER_MODULE, xRETURN_SEVERITY_ERROR, 0x002U),
        xRETURN_xERR_xTIMER_INVALID_STATE = xRETURN_MAKE(xRETURN_xTIMER_MODULE, xRETURN_SEVERITY_ERROR, 0x003U),
        xRETURN_xERR_xTIMER_NOT_INITIALIZED = xRETURN_MAKE(xRETURN_xTIMER_MODULE, xRETURN_SEVERITY_ERROR, 0x004U),
        xRETURN_xERR_xTIMER_UNSUPPORTED = xRETURN_MAKE(xRETURN_xTIMER_MODULE, xRETURN_SEVERITY_ERROR, 0x005U),
        xRETURN_xERR_xTIMER_HARDWARE = xRETURN_MAKE(xRETURN_xTIMER_MODULE, xRETURN_SEVERITY_ERROR, 0x006U),
    } xRETURN_xTIMER_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XTIMER_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
