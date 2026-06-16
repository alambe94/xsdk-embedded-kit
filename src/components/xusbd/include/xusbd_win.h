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

// @file xusbd_win.h
// @brief xUSB Windows USB (WinUSB) driver interface.

#ifndef XUSBD_WIN_H
#define XUSBD_WIN_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusb_win_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

// MACROS //////////////////////////////////////////////////////////////////////
// xUSBD_ENABLE_MOS2 and xUSBD_WINUSB_VENDOR_CODE are defined in xusbd_config.h.
#define xUSBD_WIN_DESC_BASE_SIZE (USB_INTERFACE_DESC_LEN + (2U * USB_ENDPOINT_DESC_LEN))
#define xUSBD_WIN_DESC_SIZE(speed)                                                                                                         \
    (xUSBD_WIN_DESC_BASE_SIZE + (((speed) == USB_SPEED_SUPER) ? (2U * USB_SS_ENDPOINT_COMPANION_DESC_LEN) : 0U))

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xUSBD_WIN_Context_t
    {
        xUSBD_Class_Context_t class_ctx;
        uint8_t interface;
        uint8_t in_ep;
        uint8_t out_ep;
    } xUSBD_WIN_Context_t;

    typedef struct xUSBD_WIN_Callbacks_t
    {
        xRETURN_t (*on_bus_event)(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
        xRETURN_t (*on_data_received)(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
        xRETURN_t (*on_transmit_complete)(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
    } xUSBD_WIN_Callbacks_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xUSBD_Class_Driver_t *xUSBD_WIN_Class(void);
    xRETURN_t xUSBD_WIN_Prepare_To_Receive(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);
    xRETURN_t xUSBD_WIN_Transmit(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);

    xRETURN_t xUSBD_WIN_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, xUSBD_WIN_Callbacks_t *callbacks);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_WIN_H
// EOF /////////////////////////////////////////////////////////////////////////////
