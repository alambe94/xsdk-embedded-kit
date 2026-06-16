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

// @file xusbh_hid.h
// @brief USB host HID boot keyboard and boot mouse class driver API.

#ifndef XUSBH_HID_H
#define XUSBH_HID_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbh_class.h"

#ifdef __cplusplus
extern "C"
{
#endif

// MACROS //////////////////////////////////////////////////////////////////////
#define xUSBH_HID_BOOT_SUBCLASS        1U
#define xUSBH_HID_PROTOCOL_KEYBOARD    1U
#define xUSBH_HID_PROTOCOL_MOUSE       2U
#define xUSBH_HID_KEYBOARD_REPORT_SIZE 8U
#define xUSBH_HID_MOUSE_REPORT_SIZE    4U
#define xUSBH_HID_REPORT_BUFFER_SIZE   xUSBH_HID_KEYBOARD_REPORT_SIZE
#define xUSBH_HID_KEYBOARD_KEY_COUNT   6U

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum xUSBH_HID_Report_Type_t
    {
        xUSBH_HID_REPORT_TYPE_NONE = 0,
        xUSBH_HID_REPORT_TYPE_KEYBOARD,
        xUSBH_HID_REPORT_TYPE_MOUSE,
    } xUSBH_HID_Report_Type_t;

    typedef struct xUSBH_HID_Keyboard_Report_t
    {
        uint8_t modifiers;
        uint8_t reserved;
        uint8_t keys[xUSBH_HID_KEYBOARD_KEY_COUNT];
    } xUSBH_HID_Keyboard_Report_t;

    typedef struct xUSBH_HID_Mouse_Report_t
    {
        uint8_t buttons;
        int8_t x;
        int8_t y;
        int8_t wheel;
    } xUSBH_HID_Mouse_Report_t;

    typedef struct xUSBH_HID_Callbacks_t
    {
        // Called from the host transfer-complete path when a boot-keyboard
        // interrupt IN report is decoded. report is valid only during callback.
        void (*keyboard_report)(void *user_ctx, const xUSBH_HID_Keyboard_Report_t *report);
        // Called from the host transfer-complete path when a boot-mouse
        // interrupt IN report is decoded. report is valid only during callback.
        void (*mouse_report)(void *user_ctx, const xUSBH_HID_Mouse_Report_t *report);
    } xUSBH_HID_Callbacks_t;

    typedef struct xUSBH_HID_Instance_t
    {
        bool is_allocated;
        xUSBH_HID_Report_Type_t report_type;
        xUSBH_Interface_Context_t *interface_ctx;
        xUSBH_Endpoint_Context_t *endpoint_ctx;
        xUSBH_Transfer_t *transfer;
        uint8_t report_buffer[xUSBH_HID_REPORT_BUFFER_SIZE];
        uint8_t report_length;
    } xUSBH_HID_Instance_t;

    typedef struct xUSBH_HID_Context_t
    {
        xUSBH_Context_t *host_ctx;
        xUSBH_HID_Callbacks_t callbacks;
        void *user_ctx;
        xUSBH_HID_Instance_t instances[xUSBH_HID_MAX_INSTANCES];
    } xUSBH_HID_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    // hid_ctx, host_ctx, callbacks, and user_ctx remain caller-owned. callbacks
    // may be NULL when the caller only wants class binding/probing.
    xRETURN_t
    xUSBH_HID_Init(xUSBH_HID_Context_t *hid_ctx, xUSBH_Context_t *host_ctx, const xUSBH_HID_Callbacks_t *callbacks, void *user_ctx);
    const xUSBH_Class_Driver_t *xUSBH_HID_Class(void);

#ifdef __cplusplus
}
#endif

#endif // XUSBH_HID_H
// EOF /////////////////////////////////////////////////////////////////////////////
