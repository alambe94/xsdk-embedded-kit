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

// @file xreturn.h
// @brief SDK-wide return type, severity classification, and module ID registry.
//

#ifndef XRETURN_H
#define XRETURN_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    typedef uint32_t xRETURN_t;

    // MACROS //////////////////////////////////////////////////////////////////////
    // Return code layout:
    //   bits [31:16]  module ID  - which SDK module produced this return value
    //   bits [15:14]  severity   - 0=OK, 1=error, 2=warning, 3=message
    //   bits [13:0]   code       - per-module, per-severity specific code
    //
    // 0 is always success regardless of module.

#define xRETURN_SEVERITY_OK      0x0U
#define xRETURN_SEVERITY_ERROR   0x1U
#define xRETURN_SEVERITY_WARNING 0x2U
#define xRETURN_SEVERITY_MESSAGE 0x3U

#define xRETURN_MAKE(module, severity, code)                                                                                               \
    (((uint32_t)(module) << 16) | (((uint32_t)(severity) & 0x3U) << 14) | ((uint32_t)(code) & 0x3FFFU))

#define xRETURN_GET_MODULE(ret)   (((uint32_t)(ret) >> 16) & 0xFFFFU)
#define xRETURN_GET_SEVERITY(ret) (((uint32_t)(ret) >> 14) & 0x3U)
#define xRETURN_GET_CODE(ret)     ((uint32_t)(ret) & 0x3FFFU)

#define xRETURN_IS_OK(ret)      ((ret) == xRETURN_OK)
#define xRETURN_IS_ERROR(ret)   (xRETURN_GET_SEVERITY(ret) == xRETURN_SEVERITY_ERROR)
#define xRETURN_IS_WARNING(ret) (xRETURN_GET_SEVERITY(ret) == xRETURN_SEVERITY_WARNING)
#define xRETURN_IS_MESSAGE(ret) (xRETURN_GET_SEVERITY(ret) == xRETURN_SEVERITY_MESSAGE)

// Module ID registry - assign a unique ID for each SDK module.
#define xRETURN_xUSBD_MODULE   0x0001U
#define xRETURN_xFS_MODULE     0x0002U
#define xRETURN_xTRACE_MODULE  0x0003U
#define xRETURN_xRTOS_MODULE   0x0004U
#define xRETURN_xFAULT_MODULE  0x0005U
#define xRETURN_xSHELL_MODULE  0x0006U
#define xRETURN_xUSBH_MODULE   0x0007U
#define xRETURN_xNET_MODULE    0x0008U
#define xRETURN_xBRIDGE_MODULE 0x0009U
#define xRETURN_xLOGIC_MODULE  0x000AU
#define xRETURN_xUSBIP_MODULE  0x000BU
#define xRETURN_xSPI_MODULE    0x000CU
#define xRETURN_xBOOT_MODULE   0x000DU
#define xRETURN_xUART_MODULE   0x000EU
#define xRETURN_xI2C_MODULE    0x000FU
#define xRETURN_xTIMER_MODULE  0x0010U

#define xRETURN_OK ((xRETURN_t)0)

// Generic SDK-level error codes (module ID 0)
#define xRETURN_xERROR        xRETURN_MAKE(0U, xRETURN_SEVERITY_ERROR, 1U)
#define xRETURN_xERR_NULL_PTR xRETURN_MAKE(0U, xRETURN_SEVERITY_ERROR, 2U)
#define xRETURN_xERR_TIMEOUT  xRETURN_MAKE(0U, xRETURN_SEVERITY_ERROR, 3U)

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
