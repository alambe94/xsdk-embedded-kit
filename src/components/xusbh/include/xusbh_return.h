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

// @file xusbh_return.h
// @brief Status and error code definitions for the xUSB Host Stack.

#ifndef XUSBH_RETURN_H
#define XUSBH_RETURN_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xreturn.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum xRETURN_xUSBH_t
    {
        xRETURN_xUSBH_OK = 0,

        // Host Controller Driver errors [0x001 - 0x0FF]
        xRETURN_xERR_xUSBH_HCD_NULL_POINTER = xRETURN_MAKE(xRETURN_xUSBH_MODULE, xRETURN_SEVERITY_ERROR, 0x001U),
        xRETURN_xERR_xUSBH_HCD_INCOMPLETE_OPS,

        // Generic USBH errors [0x100 - 0x1FF]
        xRETURN_xERR_xUSBH_NULL_POINTER = xRETURN_MAKE(xRETURN_xUSBH_MODULE, xRETURN_SEVERITY_ERROR, 0x100U),
        xRETURN_xERR_xUSBH_INVALID_CONFIGURATION,
        xRETURN_xERR_xUSBH_NOT_INITIALIZED,
        xRETURN_xERR_xUSBH_ALREADY_STARTED,
        xRETURN_xERR_xUSBH_NOT_STARTED,
        xRETURN_xERR_xUSBH_INVALID_ARGUMENT,
        xRETURN_xERR_xUSBH_INVALID_STATE,
        xRETURN_xERR_xUSBH_INVALID_OBJECT,
        xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED,
        xRETURN_xERR_xUSBH_TIMEOUT,
        xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR,
        xRETURN_xERR_xUSBH_AMBIGUOUS_CLASS_MATCH,
        xRETURN_xERR_xUSBH_UNSUPPORTED_OPERATION,
    } xRETURN_xUSBH_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // XUSBH_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
