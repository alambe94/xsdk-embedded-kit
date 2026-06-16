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

// @file xusbh_core.h
// @brief Public lifecycle API for the xUSB Host Stack core.

#ifndef XUSBH_CORE_H
#define XUSBH_CORE_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbh_config.h"
#include "xusbh_hcd.h"
#include "xusbh_return.h"
#include "xtrace_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xUSBH_Class_Driver_t xUSBH_Class_Driver_t;
    struct xTRACE_Context_t;

    typedef enum xUSBH_Lifecycle_State_t
    {
        xUSBH_LIFECYCLE_CREATED = 0,
        xUSBH_LIFECYCLE_INITIALIZED,
        xUSBH_LIFECYCLE_STARTED,
        xUSBH_LIFECYCLE_STOPPED,
    } xUSBH_Lifecycle_State_t;

    typedef enum xUSBH_Root_Port_State_t
    {
        xUSBH_ROOT_PORT_DISCONNECTED = 0,
        xUSBH_ROOT_PORT_CONNECTED,
        xUSBH_ROOT_PORT_POWERED,
        xUSBH_ROOT_PORT_RESETTING,
        xUSBH_ROOT_PORT_ENABLED,
        xUSBH_ROOT_PORT_ENUMERATING,
        xUSBH_ROOT_PORT_CONFIGURED,
        xUSBH_ROOT_PORT_SUSPENDED,
        xUSBH_ROOT_PORT_ERROR,
    } xUSBH_Root_Port_State_t;

    typedef enum xUSBH_Device_State_t
    {
        xUSBH_DEVICE_STATE_DETACHED = 0,
        xUSBH_DEVICE_STATE_ATTACHED,
        xUSBH_DEVICE_STATE_ADDRESSED,
        xUSBH_DEVICE_STATE_CONFIGURED,
    } xUSBH_Device_State_t;

    typedef enum xUSBH_Enumeration_State_t
    {
        xUSBH_ENUMERATION_IDLE = 0,
        xUSBH_ENUMERATION_GET_DEVICE_HEADER_SUBMIT,
        xUSBH_ENUMERATION_GET_DEVICE_HEADER_WAIT,
        xUSBH_ENUMERATION_SET_ADDRESS_SUBMIT,
        xUSBH_ENUMERATION_SET_ADDRESS_WAIT,
        xUSBH_ENUMERATION_ADDRESS_SETTLE,
        xUSBH_ENUMERATION_GET_DEVICE_FULL_SUBMIT,
        xUSBH_ENUMERATION_GET_DEVICE_FULL_WAIT,
        xUSBH_ENUMERATION_GET_CONFIG_HEADER_SUBMIT,
        xUSBH_ENUMERATION_GET_CONFIG_HEADER_WAIT,
        xUSBH_ENUMERATION_GET_CONFIG_FULL_SUBMIT,
        xUSBH_ENUMERATION_GET_CONFIG_FULL_WAIT,
        xUSBH_ENUMERATION_SET_CONFIGURATION_SUBMIT,
        xUSBH_ENUMERATION_SET_CONFIGURATION_WAIT,
        xUSBH_ENUMERATION_COMPLETE,
        xUSBH_ENUMERATION_ERROR,
    } xUSBH_Enumeration_State_t;

    typedef struct xUSBH_Init_Config_t
    {
        uint8_t root_port_count;
    } xUSBH_Init_Config_t;

    typedef struct xUSBH_Start_Config_t
    {
        const xUSBH_HCD_Ops_t *hcd_ops;
        void *hcd_ctx;
    } xUSBH_Start_Config_t;

    typedef struct xUSBH_Class_Registration_t
    {
        bool is_registered;
        const xUSBH_Class_Driver_t *driver;
        void *class_ctx;
    } xUSBH_Class_Registration_t;

    typedef struct xUSBH_Device_Context_t
    {
        bool is_allocated;
        xUSBH_Device_State_t state;
        uint8_t port;
        uint8_t address;
        USB_Speed_t speed;
        uint16_t ep0_max_packet_size;
        uint16_t vendor_id;
        uint16_t product_id;
        uint8_t device_class;
        uint8_t device_subclass;
        uint8_t device_protocol;
        uint8_t active_configuration_value;
        bool is_configured;
    } xUSBH_Device_Context_t;

    typedef struct xUSBH_Interface_Context_t
    {
        bool is_allocated;
        uint8_t device_index;
        uint8_t interface_number;
        uint8_t alternate_setting;
        uint8_t class_code;
        uint8_t subclass;
        uint8_t protocol;
        uint8_t endpoint_count;
        const xUSBH_Class_Driver_t *class_driver;
        void *class_ctx;
    } xUSBH_Interface_Context_t;

    typedef struct xUSBH_Endpoint_Context_t
    {
        bool is_allocated;
        uint8_t device_index;
        uint8_t interface_index;
        uint8_t endpoint_address;
        uint8_t endpoint_type;
        bool is_in;
        uint16_t max_packet_size;
        uint8_t interval;
    } xUSBH_Endpoint_Context_t;

    typedef struct xUSBH_Root_Port_Context_t
    {
        xUSBH_Root_Port_State_t state;
        bool is_connected;
        bool is_reset_complete;
        bool has_device;
        uint8_t debounce_remaining;
        uint8_t device_index;
        USB_Speed_t speed;
    } xUSBH_Root_Port_Context_t;

    typedef struct xUSBH_Enumeration_Context_t
    {
        xUSBH_Enumeration_State_t state;
        bool is_active;
        bool has_transfer;
        uint8_t port;
        uint8_t device_index;
        uint8_t transfer_index;
        uint8_t assigned_address;
        uint8_t address_settle_remaining;
        uint16_t timeout_remaining;
        uint16_t config_total_length;
        uint8_t configuration_value;
    } xUSBH_Enumeration_Context_t;

    typedef struct xUSBH_Context_t
    {
        xUSBH_Lifecycle_State_t lifecycle_state;
        const xUSBH_HCD_Ops_t *hcd_ops;
        void *hcd_ctx;
        uint8_t root_port_count;
        bool is_initialized;
        bool is_started;
        xUSBH_Root_Port_Context_t root_ports[xUSBH_MAX_ROOT_PORTS];
        xUSBH_Enumeration_Context_t enumeration;
        uint8_t control_buffer[xUSBH_CONTROL_BUFFER_SIZE];
        xUSBH_Device_Context_t devices[xUSBH_MAX_DEVICES];
        xUSBH_Interface_Context_t interfaces[xUSBH_MAX_INTERFACES];
        xUSBH_Endpoint_Context_t endpoints[xUSBH_MAX_ENDPOINTS];
        xUSBH_Transfer_t transfers[xUSBH_MAX_TRANSFERS];
        xUSBH_Class_Registration_t class_registrations[xUSBH_MAX_CLASS_DRIVERS];
#if xTRACE_ENABLE
        struct xTRACE_Context_t *trace_ctx; // Optional; NULL until xUSBH_Trace_Init is called.
#endif
    } xUSBH_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    // Lifecycle:
    // 1. Caller owns a zeroable xUSBH_Context_t for the whole host lifetime.
    // 2. xUSBH_Init() clears runtime state and creates static host slots.
    // 3. Optional class drivers are registered before xUSBH_Start().
    // 4. xUSBH_Start() binds the caller-owned HCD ops/context and enables HCD
    //    interrupts.
    // 5. xUSBH_Process() advances root-port debounce and enumeration from task
    //    context while HCD callbacks report asynchronous port/transfer events.
    // 6. xUSBH_Stop() tears the HCD down and leaves allocated host slots intact
    //    for explicit cleanup or reinitialization.
    xRETURN_t xUSBH_Init(xUSBH_Context_t *host_ctx, const xUSBH_Init_Config_t *config);
    xRETURN_t xUSBH_Start(xUSBH_Context_t *host_ctx, const xUSBH_Start_Config_t *config);
    xRETURN_t xUSBH_Process(xUSBH_Context_t *host_ctx);
    xRETURN_t xUSBH_Stop(xUSBH_Context_t *host_ctx);

    // Attach or detach an initialized caller-owned xTRACE context.
    // Passing NULL for trace_ctx detaches tracing from the host context.
#if xTRACE_ENABLE
    xRETURN_t xUSBH_Trace_Init(xUSBH_Context_t *host_ctx, struct xTRACE_Context_t *trace_ctx);
#else
static inline xRETURN_t xUSBH_Trace_Init(xUSBH_Context_t *host_ctx, struct xTRACE_Context_t *trace_ctx)
{
    (void)host_ctx;
    (void)trace_ctx;
    return xRETURN_OK;
}
#endif

    xRETURN_t xUSBH_Get_Lifecycle_State(const xUSBH_Context_t *host_ctx, xUSBH_Lifecycle_State_t *state);
    xRETURN_t xUSBH_Is_Started(const xUSBH_Context_t *host_ctx, bool *is_started);
    xRETURN_t xUSBH_Root_Port_Get_State(const xUSBH_Context_t *host_ctx, uint8_t port, xUSBH_Root_Port_State_t *state);
    xRETURN_t xUSBH_Device_Allocate(xUSBH_Context_t *host_ctx, uint8_t port, xUSBH_Device_Context_t **device_ctx);
    xRETURN_t xUSBH_Device_Release(xUSBH_Context_t *host_ctx, xUSBH_Device_Context_t *device_ctx);
    xRETURN_t
    xUSBH_Interface_Allocate(xUSBH_Context_t *host_ctx, xUSBH_Device_Context_t *device_ctx, xUSBH_Interface_Context_t **interface_ctx);
    xRETURN_t xUSBH_Interface_Release(xUSBH_Context_t *host_ctx, xUSBH_Interface_Context_t *interface_ctx);
    xRETURN_t
    xUSBH_Endpoint_Allocate(xUSBH_Context_t *host_ctx, xUSBH_Interface_Context_t *interface_ctx, xUSBH_Endpoint_Context_t **endpoint_ctx);
    xRETURN_t xUSBH_Endpoint_Release(xUSBH_Context_t *host_ctx, xUSBH_Endpoint_Context_t *endpoint_ctx);
    xRETURN_t xUSBH_Endpoint_Find_By_Address(xUSBH_Context_t *host_ctx,
                                             const xUSBH_Device_Context_t *device_ctx,
                                             uint8_t endpoint_address,
                                             xUSBH_Endpoint_Context_t **endpoint_ctx);
    xRETURN_t xUSBH_Device_Build_Topology(xUSBH_Context_t *host_ctx,
                                          xUSBH_Device_Context_t *device_ctx,
                                          const uint8_t *buffer,
                                          uint32_t buffer_length);
    xRETURN_t xUSBH_Transfer_Allocate(xUSBH_Context_t *host_ctx, xUSBH_Transfer_t **transfer);
    xRETURN_t xUSBH_Transfer_Release(xUSBH_Context_t *host_ctx, xUSBH_Transfer_t *transfer);
    xRETURN_t xUSBH_Transfer_Submit(xUSBH_Context_t *host_ctx, xUSBH_Transfer_t *transfer);
    xRETURN_t xUSBH_Transfer_Cancel(xUSBH_Context_t *host_ctx, xUSBH_Transfer_t *transfer);
    xRETURN_t xUSBH_Port_Disconnect_Cleanup(xUSBH_Context_t *host_ctx, uint8_t port);

#ifdef __cplusplus
}
#endif

#endif // XUSBH_CORE_H
// EOF /////////////////////////////////////////////////////////////////////////////
