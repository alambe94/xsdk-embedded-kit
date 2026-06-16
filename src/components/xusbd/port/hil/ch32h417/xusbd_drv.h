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

// @file xusbd_drv.h
// @brief CH32H417 USBSS (USB 3.0) Device Controller Driver Header

#ifndef XUSBD_DRV_H
#define XUSBD_DRV_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusb_defs.h"
#include "xusbd_dcd.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

#define xUSBD_MAX_PORTS               1U
#define xUSBD_DEVICE_MAX_INSTANCE     1U
#define xUSBD_DEVICE_MAX_IN_ENDPOINT  7U
#define xUSBD_DEVICE_MAX_OUT_ENDPOINT 7U

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        uint8_t *Data;
        uint8_t *Current_Data;
        uint8_t EP_Type;
        uint8_t Transfers_Per_Microframe;
        uint8_t Send_ZLP;
        uint16_t MPS;
        uint32_t Remaining_XFER_Length;
        uint32_t Actual_XFER_Length;
    } xUSBD_CH32H417_EP_Handle_t;

    typedef struct
    {
        uint8_t port;
        void *device_ctx;
        USB_Speed_t speed;
        bool is_hardware_initialized;
        bool is_connected;
        xUSBD_DCD_Event_Callback_t event_callback;

        // Endpoint state (0 is Control, 1-7 are standard endpoints)
        xUSBD_CH32H417_EP_Handle_t in_ep_handles[xUSBD_DEVICE_MAX_IN_ENDPOINT + 1];
        xUSBD_CH32H417_EP_Handle_t out_ep_handles[xUSBD_DEVICE_MAX_OUT_ENDPOINT + 1];

    } xUSBD_CH32H417_DCD_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    extern xUSBD_DCD_Ops_t xUSBD_CH32H417_DCD_Ops;
    extern xUSBD_CH32H417_DCD_Context_t xUSBD_CH32H417_DCD_Context;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#define XUSBD_CH32H417_IRQ_ATTR __attribute__((interrupt("machine")))

    void USBSS_IRQHandler(void) XUSBD_CH32H417_IRQ_ATTR;
    void USBSS_LINK_IRQHandler(void) XUSBD_CH32H417_IRQ_ATTR;

#ifdef __cplusplus
}
#endif

#endif // XUSBD_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////
