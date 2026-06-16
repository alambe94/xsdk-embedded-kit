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

// @file xusbd_std.h
// @brief Internal EP0 interface between the core event dispatcher and the
//        standard USB request processor.  Not part of the public API.

#ifndef XUSBD_STD_H
#define XUSBD_STD_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_class.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // EP0 control-transfer phase - shared between xusbd_std.c (which drives the
    // state machine) and xusbd_core.c (which resets it on bus events).
    typedef enum
    {
        xUSBD_CTRL_PHASE_IDLE = 0,       // ready for next SETUP
        xUSBD_CTRL_PHASE_IN = 1,         // IN data sent; DATA_RECEIVED(0x00) = host status ZLP
        xUSBD_CTRL_PHASE_OUT = 2,        // waiting for OUT data from host
        xUSBD_CTRL_PHASE_OUT_STATUS = 3, // sending status ZLP; DATA_SENT = transfer complete
    } xUSBD_Control_Phase_t;

    // INTERNAL INTERFACES BETWEEN CORE AND STANDARD REQUEST PROCESSOR /////////////
    xRETURN_t xUSBD_EP0_Setup_Process(xUSBD_Device_Context_t *device_ctx);
    xRETURN_t xUSBD_EP0_Data_Received_Process(xUSBD_Device_Context_t *device_ctx, uint32_t length);
    xRETURN_t xUSBD_EP0_Data_Sent_Process(xUSBD_Device_Context_t *device_ctx);
    xRETURN_t xUSBD_EP0_Configure(xUSBD_Device_Context_t *device_ctx, USB_Speed_t speed);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_STD_H
// EOF /////////////////////////////////////////////////////////////////////////////
