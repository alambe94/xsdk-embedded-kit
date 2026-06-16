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

// @file xusbh_config.h
// @brief Global configuration macros for the xUSB Host Stack.

#ifndef XUSBH_CONFIG_H
#define XUSBH_CONFIG_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES

#ifdef __cplusplus
extern "C"
{
#endif

// MACROS //////////////////////////////////////////////////////////////////////

// Module version
#define xUSBH_VERSION_MAJOR  0U
#define xUSBH_VERSION_MINOR  1U
#define xUSBH_VERSION_PATCH  0U
#define xUSBH_VERSION_STRING "0.1.0"

#ifndef xUSBH_MAX_ROOT_PORTS
#define xUSBH_MAX_ROOT_PORTS 1U
#endif

#ifndef xUSBH_MAX_DEVICES
#define xUSBH_MAX_DEVICES 1U
#endif

#ifndef xUSBH_MAX_INTERFACES
#define xUSBH_MAX_INTERFACES 4U
#endif

#ifndef xUSBH_MAX_ENDPOINTS
#define xUSBH_MAX_ENDPOINTS 8U
#endif

#ifndef xUSBH_MAX_TRANSFERS
#define xUSBH_MAX_TRANSFERS 4U
#endif

#ifndef xUSBH_MAX_CLASS_DRIVERS
#define xUSBH_MAX_CLASS_DRIVERS 4U
#endif

#ifndef xUSBH_HID_MAX_INSTANCES
#define xUSBH_HID_MAX_INSTANCES 2U
#endif

#ifndef xUSBH_MSC_MAX_INSTANCES
#define xUSBH_MSC_MAX_INSTANCES 1U
#endif

#ifndef xUSBH_CONTROL_BUFFER_SIZE
#define xUSBH_CONTROL_BUFFER_SIZE 256U
#endif

#ifndef xUSBH_MAX_CONFIG_DESCRIPTOR_SIZE
#define xUSBH_MAX_CONFIG_DESCRIPTOR_SIZE 256U
#endif

#ifndef xUSBH_CONNECT_DEBOUNCE_SAMPLES
#define xUSBH_CONNECT_DEBOUNCE_SAMPLES 1U
#endif

#ifndef xUSBH_CONTROL_TRANSFER_TIMEOUT_TICKS
#define xUSBH_CONTROL_TRANSFER_TIMEOUT_TICKS 100U
#endif

#ifndef xUSBH_ADDRESS_SETTLE_TICKS
#define xUSBH_ADDRESS_SETTLE_TICKS 1U
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // XUSBH_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
