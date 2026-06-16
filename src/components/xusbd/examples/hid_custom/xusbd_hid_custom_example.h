// Copyright 2022 alambe94
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

// @file xusbd_hid_custom_example.h
// @brief Application-level hooks and configuration for Custom USB HID loopback.

#ifndef XUSBD_HID_CUSTOM_EXAMPLE_H
#define XUSBD_HID_CUSTOM_EXAMPLE_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_hid.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct
    {
        volatile uint8_t custom_tx_complete;
        volatile uint8_t reset_complete;
        uint8_t loopback_buffer[64];
        volatile uint32_t loopback_len;
        volatile uint8_t loopback_pending;
    } xUSBD_HID_Custom_App_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    void xUSBD_HID_Custom_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_HID_Custom_App_Context_t *app_context);
    void xUSBD_HID_Custom_App_Process(xUSBD_Class_Context_t *class_ctx);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_HID_CUSTOM_EXAMPLE_H
// EOF /////////////////////////////////////////////////////////////////////////////
