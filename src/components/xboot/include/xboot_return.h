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

// @file xboot_return.h
// @brief Module-specific return codes and severity levels for xBOOT.
//

#ifndef XBOOT_RETURN_H
#define XBOOT_RETURN_H

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
#define xRETURN_xBOOT_OK                     xRETURN_OK
#define xRETURN_xERR_xBOOT_NULL_POINTER      xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 1U)
#define xRETURN_xERR_xBOOT_INVALID_ARGUMENT  xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 2U)
#define xRETURN_xERR_xBOOT_INVALID_STATE     xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 3U)
#define xRETURN_xERR_xBOOT_INVALID_IMAGE     xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 4U)
#define xRETURN_xERR_xBOOT_INVALID_PARTITION xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 5U)
#define xRETURN_xERR_xBOOT_IMAGE_TOO_LARGE   xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 6U)
#define xRETURN_xERR_xBOOT_CRC_MISMATCH      xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 7U)
#define xRETURN_xERR_xBOOT_SIGNATURE_INVALID xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 8U)
#define xRETURN_xERR_xBOOT_VERSION_ROLLBACK  xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 9U)
#define xRETURN_xERR_xBOOT_STORAGE_READ      xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 10U)
#define xRETURN_xERR_xBOOT_STORAGE_WRITE     xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 11U)
#define xRETURN_xERR_xBOOT_STORAGE_ERASE     xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 12U)
#define xRETURN_xERR_xBOOT_NO_BOOTABLE_IMAGE xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 13U)
#define xRETURN_xERR_xBOOT_UNSUPPORTED       xRETURN_MAKE(xRETURN_xBOOT_MODULE, xRETURN_SEVERITY_ERROR, 14U)

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
