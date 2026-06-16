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

// @file xbridge_config.h
// @brief xBRIDGE compile-time configuration - channel enable flags, protocol selection, and log levels.
//

#ifndef XBRIDGE_CONFIG_H
#define XBRIDGE_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////

    // Channel enable - set 0 to exclude a channel from the build
#ifndef xBRIDGE_CONFIG_UART_ENABLE
#define xBRIDGE_CONFIG_UART_ENABLE 1U
#endif

#ifndef xBRIDGE_CONFIG_I2C_ENABLE
#define xBRIDGE_CONFIG_I2C_ENABLE 1U
#endif

#ifndef xBRIDGE_CONFIG_SPI_ENABLE
#define xBRIDGE_CONFIG_SPI_ENABLE 1U
#endif

#ifndef xBRIDGE_CONFIG_CAN_ENABLE
#define xBRIDGE_CONFIG_CAN_ENABLE 1U
#endif

#ifndef xBRIDGE_CONFIG_QSPI_ENABLE
#define xBRIDGE_CONFIG_QSPI_ENABLE 1U
#endif

#ifndef xBRIDGE_CONFIG_DAP_ENABLE
#define xBRIDGE_CONFIG_DAP_ENABLE 1U
#endif

#ifndef xBRIDGE_CONFIG_GPIO_ENABLE
#define xBRIDGE_CONFIG_GPIO_ENABLE 1U
#endif

#ifndef xBRIDGE_CONFIG_PWM_ENABLE
#define xBRIDGE_CONFIG_PWM_ENABLE 1U
#endif

#ifndef xBRIDGE_CONFIG_ADC_ENABLE
#define xBRIDGE_CONFIG_ADC_ENABLE 1U
#endif

    // CAN protocol selection
#ifndef xBRIDGE_CONFIG_CAN_SLCAN
#define xBRIDGE_CONFIG_CAN_SLCAN 1U // SLCAN over CDC (Phase 6)
#endif

#ifndef xBRIDGE_CONFIG_CAN_GSUSB
#define xBRIDGE_CONFIG_CAN_GSUSB 0U // gs_usb binary (Phase 10)
#endif

    // CMSIS-DAP version selection
#ifndef xBRIDGE_CONFIG_DAP_V1_HID
#define xBRIDGE_CONFIG_DAP_V1_HID 1U // v1 over HID (64-byte reports)
#endif

#ifndef xBRIDGE_CONFIG_DAP_V2_WINUSB
#define xBRIDGE_CONFIG_DAP_V2_WINUSB 1U // v2 over WINUSB bulk (512-byte packets)
#endif

    // Log level (0=silent, 1=status, 2=verbose).
#ifndef xBRIDGE_CONFIG_LOG_LEVEL
#define xBRIDGE_CONFIG_LOG_LEVEL 0U
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
