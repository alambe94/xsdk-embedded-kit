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

// @file xusbd_drv.h
// @brief AM243x USB Device Controller Driver

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

    // TYPES ///////////////////////////////////////////////////////////////////////

#define xUSBD_MAX_PORTS               1U
#define xUSBD_DEVICE_MAX_INSTANCE     1U
#define xUSBD_DEVICE_MAX_IN_ENDPOINT  15U // HW instances might have different number EPs
#define xUSBD_DEVICE_MAX_OUT_ENDPOINT 15U // HW instances might have different number EPs

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
    } xUSBD_AM243x_EP_Handle_t;

    typedef struct
    {
        uint8_t port;
        void *device_ctx;
        USB_Speed_t speed;
        bool is_hardware_initialized;
        xUSBD_DCD_Event_Callback_t event_callback;

        // Endpoint state
        xUSBD_AM243x_EP_Handle_t in_ep_handles[xUSBD_DEVICE_MAX_IN_ENDPOINT + 1];
        xUSBD_AM243x_EP_Handle_t out_ep_handles[xUSBD_DEVICE_MAX_OUT_ENDPOINT + 1];

    } xUSBD_AM243x_DCD_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // Single-instance constraint: the AM243x port driver supports exactly one USB
    // device instance (USB0). Both the Ops table and the context are module-level
    // singletons. This is an architectural limit of the Cadence CUSBD callback
    // model, which does not thread a user context pointer through its completion
    // callbacks, making it impossible to demultiplex events across multiple
    // instances without a global lookup. Multi-instance use requires a redesign
    // of the platform port layer.
    extern xUSBD_DCD_Ops_t xUSBD_AM243x_DCD_Ops;
    extern xUSBD_AM243x_DCD_Context_t xUSBD_AM243x_DCD_Context;

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    void xUSBD_AM243x_DCD_IRQ_Handler(uint8_t port);

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // XUSBD_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////

