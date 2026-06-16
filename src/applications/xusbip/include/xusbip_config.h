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

// @file xusbip_config.h
// @brief xUSBIP compile-time configuration limits, timeouts, buffer sizes, and log levels.
//

#ifndef XUSBIP_CONFIG_H
#define XUSBIP_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////

    // Module version
#define xUSBIP_VERSION_MAJOR  0U
#define xUSBIP_VERSION_MINOR  1U
#define xUSBIP_VERSION_PATCH  0U
#define xUSBIP_VERSION_STRING "0.1.0"

    // Maximum number of devices the server can export simultaneously
#define xUSBIP_CONFIG_MAX_EXPORTED_DEVICES 4U

    // Maximum number of pending (in-flight) URBs tracked per device
#define xUSBIP_CONFIG_MAX_PENDING_URBS 16U

    // Maximum transfer buffer size for a single URB (bytes)
#define xUSBIP_CONFIG_MAX_TRANSFER_SIZE (64U * 1024U)

    // TCP receive buffer size (bytes) - must fit one full URB header (48 bytes)
#define xUSBIP_CONFIG_RX_BUFFER_SIZE 512U

    // TCP transmit buffer size (bytes)
#define xUSBIP_CONFIG_TX_BUFFER_SIZE 512U

    // Per-URB timeout in milliseconds (0 = no timeout)
#define xUSBIP_CONFIG_URB_TIMEOUT_MS 5000U

    // Log level (0=silent, 1=status, 2=verbose).
#define xUSBIP_CONFIG_LOG_LEVEL 0U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUSBIP_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
