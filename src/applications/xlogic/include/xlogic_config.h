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

// @file xlogic_config.h
// @brief xLOGIC compile-time configuration - buffer sizes, transport enables, log levels.

#ifndef XLOGIC_CONFIG_H
#define XLOGIC_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////

    // Log level (0=silent, 1=status hex, 2=verbose).
#ifndef xLOGIC_CONFIG_LOG_LEVEL
#define xLOGIC_CONFIG_LOG_LEVEL 0U
#endif

    // Transport enables (1=included, 0=excluded at compile time).
    // Both may be 1 simultaneously for a composite USB device.
#ifndef xLOGIC_CONFIG_TRANSPORT_WINUSB_ENABLE
#define xLOGIC_CONFIG_TRANSPORT_WINUSB_ENABLE 1U // USB 3 SS streaming (primary)
#endif

#ifndef xLOGIC_CONFIG_TRANSPORT_CDC_ENABLE
#define xLOGIC_CONFIG_TRANSPORT_CDC_ENABLE 1U // SUMP/PulseView compatibility
#endif

    // Streaming ping-pong half-buffer sizes (bytes).
    // Each half must be drained within 3 ms at 333 MB/s; 512 KB gives 1.5 ms margin.
#ifndef xLOGIC_CONFIG_PING_BUF_BYTES
#define xLOGIC_CONFIG_PING_BUF_BYTES (512U * 1024U)
#endif

#ifndef xLOGIC_CONFIG_PONG_BUF_BYTES
#define xLOGIC_CONFIG_PONG_BUF_BYTES (512U * 1024U)
#endif

    // SUMP metadata device name and firmware version strings.
#define xLOGIC_CONFIG_SUMP_DEVICE_NAME "xLOGIC AM243x"
#define xLOGIC_CONFIG_SUMP_FW_VERSION  "PRU 1.0"

    // Maximum size of the built metadata response buffer (bytes).
#define xLOGIC_CONFIG_METADATA_BUF_BYTES 64U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XLOGIC_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
