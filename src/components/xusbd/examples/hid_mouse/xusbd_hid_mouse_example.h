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

// @file xusbd_hid_mouse_example.h
// @brief Application-level hooks and configuration for USB HID Mouse example.

#ifndef XUSBD_HID_MOUSE_EXAMPLE_H
#define XUSBD_HID_MOUSE_EXAMPLE_H

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
    typedef enum
    {
        xUSBD_HID_MOUSE_APP_STATE_INIT,
        xUSBD_HID_MOUSE_APP_STATE_SEND_DATA,
    } xUSBD_HID_Mouse_App_State_t;

    typedef struct
    {
        volatile uint8_t mouse_tx_complete;
        volatile uint8_t reset_complete;
        xUSBD_HID_Mouse_App_State_t state;
        uint32_t init_delay;
        uint32_t mouse_send_delay;
        int8_t x_pos;
        int8_t y_pos;
    } xUSBD_HID_Mouse_App_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    void xUSBD_HID_Mouse_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_HID_Mouse_App_Context_t *app_context);
    void xUSBD_HID_Mouse_App_Process(xUSBD_Class_Context_t *class_ctx);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_HID_MOUSE_EXAMPLE_H
// EOF /////////////////////////////////////////////////////////////////////////////
