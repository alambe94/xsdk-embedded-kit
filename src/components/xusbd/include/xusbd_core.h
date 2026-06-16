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

// @file xusbd_core.h
// @brief xUSB Device Stack core lifecycle functions and state query interfaces.

#ifndef XUSBD_CORE_H
#define XUSBD_CORE_H

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
    typedef struct
    {
        USB_Speed_t speed;
        const uint8_t *vendor_string;
        const uint8_t *product_string;
        const uint8_t *serial_number_string;
        uint16_t vendor_id;
        uint16_t product_id;
    } xUSBD_Init_Config_t;

    typedef struct
    {
        uint8_t port;
        xUSBD_DCD_Ops_t *dcd_ops;
        void *dcd_ctx;
    } xUSBD_Start_Config_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    // ---------------------------------------------------------
    // Public APIs
    // ---------------------------------------------------------
    void xUSBD_DCD_Event_Callback(void *device_ctx, USB_DCD_Event_t event, uint8_t ep_addr, uint8_t *data, uint32_t length);

    xRETURN_t xUSBD_Init(xUSBD_Device_Context_t *device_ctx, const xUSBD_Init_Config_t *config);
    xRETURN_t xUSBD_Start(xUSBD_Device_Context_t *device_ctx, const xUSBD_Start_Config_t *config);
    xRETURN_t xUSBD_Stop(xUSBD_Device_Context_t *device_ctx);

    // Attach or detach an initialized caller-owned xTRACE context.
    // Passing NULL for trace_ctx detaches tracing from the device context.
#if xTRACE_ENABLE
    xRETURN_t xUSBD_Trace_Init(xUSBD_Device_Context_t *device_ctx, struct xTRACE_Context_t *trace_ctx);
#else
struct xTRACE_Context_t;
static inline xRETURN_t xUSBD_Trace_Init(xUSBD_Device_Context_t *device_ctx, struct xTRACE_Context_t *trace_ctx)
{
    (void)device_ctx;
    (void)trace_ctx;
    return xRETURN_OK;
}
#endif

    // DEVICE STATE ACCESSORS //////////////////////////////////////////////////////
    xRETURN_t xUSBD_Get_Lifecycle_State(const xUSBD_Device_Context_t *device_ctx, xUSBD_Lifecycle_State_t *state);
    xRETURN_t xUSBD_Is_Started(const xUSBD_Device_Context_t *device_ctx, bool *is_started);
    xRETURN_t xUSBD_Is_Configured(const xUSBD_Device_Context_t *device_ctx, bool *is_configured);
    xRETURN_t xUSBD_Get_Address(const xUSBD_Device_Context_t *device_ctx, uint8_t *address);
    xRETURN_t xUSBD_Get_Configuration_Value(const xUSBD_Device_Context_t *device_ctx, uint8_t *configuration_value);
    xRETURN_t xUSBD_Get_Link_State(const xUSBD_Device_Context_t *device_ctx, USB_DCD_Link_State_t *link_state);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_CORE_H
// EOF /////////////////////////////////////////////////////////////////////////////
