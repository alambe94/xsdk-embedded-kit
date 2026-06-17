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

// @file xsdk_port_ch32h417.h
// @brief CH32H417 SoC port integration and initialization.
//

#ifndef XSDK_PORT_CH32H417_H
#define XSDK_PORT_CH32H417_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

// MODULE INCLUDES
#include "xgpio_drv.h"
#include "xrcc_drv.h"
#include "xpfic_drv.h"
#include "xsystick_drv.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    void xSDK_Port_Init(void);
    void xSDK_Port_USART1_Pinmux_Init(void);
    void xSDK_Port_I2C1_Pinmux_Init(void);
    void xSDK_Port_SPI1_Pinmux_Init(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSDK_PORT_CH32H417_H
// EOF /////////////////////////////////////////////////////////////////////////////
