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

// @file xusbd_cdc.h
// @brief xUSB Device Communications Class (CDC) driver interface.

#ifndef XUSBD_CDC_H
#define XUSBD_CDC_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusb_cdc_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

#define xUSBD_CDC_DESC_BASE_SIZE(subclass_desc_len)                                                                                        \
    (USB_IAD_DESC_LEN + USB_INTERFACE_DESC_LEN + (subclass_desc_len) + USB_ENDPOINT_DESC_LEN + USB_INTERFACE_DESC_LEN +                    \
     (2U * USB_ENDPOINT_DESC_LEN))
#define xUSBD_CDC_DESC_SIZE(speed, subclass_desc_len)                                                                                      \
    (xUSBD_CDC_DESC_BASE_SIZE(subclass_desc_len) + (((speed) == USB_SPEED_SUPER) ? (3U * USB_SS_ENDPOINT_COMPANION_DESC_LEN) : 0U))

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct xUSBD_CDC_Context_t
    {
        xUSBD_Class_Context_t class_ctx;

        // Configuration parameters (filled by application before registration)
        uint8_t subclass;         // CDC subclass: 0x02=ACM, 0x06=ECM, 0x0D=NCM
        uint8_t protocol;         // CDC protocol: 0x01=ACM, 0x00=ECM/NCM
        uint8_t cmd_ep_interval;  // Polling interval for interrupt command endpoint
        uint16_t cmd_ep_mps;      // MPS for interrupt command endpoint
        bool has_notification_ep; // false to skip cmd_ep allocation (ECM optional)

        // Allocated resources (filled by class allocators during init_instance)
        uint8_t cmd_interface;
        uint8_t data_interface;
        uint8_t cmd_ep;
        uint8_t out_ep;
        uint8_t in_ep;

        // Instance runtime state
        uint8_t alt_interface;
    } xUSBD_CDC_Context_t;

    typedef struct xUSBD_CDC_Callbacks_t
    {
        xRETURN_t (*on_bus_event)(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
        xRETURN_t (*on_control_in)(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
        xRETURN_t (*on_control_out)(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length);
        xRETURN_t (*on_data_received)(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
        xRETURN_t (*on_transmit_complete)(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
    } xUSBD_CDC_Callbacks_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // INLINE HELPERS //////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xUSBD_Class_Driver_t *xUSBD_CDC_Class(void);
    xRETURN_t xUSBD_CDC_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, xUSBD_CDC_Callbacks_t *callbacks);
    xRETURN_t xUSBD_CDC_Prepare_To_Receive(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);
    xRETURN_t xUSBD_CDC_Transmit(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);
    xRETURN_t xUSBD_CDC_Send_Notification(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_CDC_H
// EOF /////////////////////////////////////////////////////////////////////////////
