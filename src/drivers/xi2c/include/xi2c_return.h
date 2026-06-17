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

// @file xi2c_return.h
// @brief xI2C driver return codes.
//

#ifndef XI2C_RETURN_H
#define XI2C_RETURN_H

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
        xRETURN_xERR_xI2C_NULL_POINTER = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x001U),
        xRETURN_xERR_xI2C_INVALID_ARG = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x002U),
        xRETURN_xERR_xI2C_INVALID_STATE = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x003U),
        xRETURN_xERR_xI2C_NOT_INITIALIZED = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x004U),
        xRETURN_xERR_xI2C_NOT_STARTED = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x005U),
        xRETURN_xERR_xI2C_BUSY = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x006U),
        xRETURN_xERR_xI2C_TIMEOUT = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x007U),
        xRETURN_xERR_xI2C_NACK = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x008U),
        xRETURN_xERR_xI2C_ARBITRATION_LOST = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x009U),
        xRETURN_xERR_xI2C_BUS_ERROR = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x00AU),
        xRETURN_xERR_xI2C_UNSUPPORTED = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x00BU),
        xRETURN_xERR_xI2C_HARDWARE = xRETURN_MAKE(xRETURN_xI2C_MODULE, xRETURN_SEVERITY_ERROR, 0x00CU),
    } xRETURN_xI2C_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XI2C_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
