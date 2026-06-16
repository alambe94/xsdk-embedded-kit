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

// @file xusbh_core.c
// @brief xUSB Host Stack lifecycle API implementation.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusb_descriptor.h"
#include "xusbh_class.h"
#include "xusbh_core.h"
#include "xusbh_descriptor.h"
#include "xusbh_enum.h"
#include "xusbh_trace.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////
typedef struct xUSBH_Topology_Parse_Context_t
{
    uint8_t device_index;
    xUSBH_Interface_Context_t *current_interface;
    uint8_t current_expected_endpoint_count;
    bool is_skipping_alternate;
    uint8_t active_interface_count;
} xUSBH_Topology_Parse_Context_t;

// VARIABLES //////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static bool init_config_is_valid(const xUSBH_Init_Config_t *config);
static bool context_is_initialized(const xUSBH_Context_t *host_ctx);
static bool context_is_started(const xUSBH_Context_t *host_ctx);
static void hcd_event_callback(void *host_ctx, const xUSBH_HCD_Event_t *event);
static void hcd_port_event_handle(xUSBH_Context_t *host_ctx, const xUSBH_HCD_Event_t *event);
static xRETURN_t host_hcd_start(xUSBH_Context_t *host_ctx, const xUSBH_Start_Config_t *config);
static xRETURN_t host_hcd_stop(xUSBH_Context_t *host_ctx);
static xRETURN_t context_ready_for_runtime_slots(const xUSBH_Context_t *host_ctx);
static bool port_is_valid(const xUSBH_Context_t *host_ctx, uint8_t port);
static void root_port_clear(xUSBH_Context_t *host_ctx, uint8_t port);
static xRETURN_t root_port_connected_set(xUSBH_Context_t *host_ctx, uint8_t port);
static xRETURN_t root_port_reset_complete_set(xUSBH_Context_t *host_ctx, uint8_t port);
static xRETURN_t root_port_suspended_set(xUSBH_Context_t *host_ctx, uint8_t port);
static xRETURN_t root_port_resumed_set(xUSBH_Context_t *host_ctx, uint8_t port);
static xRETURN_t root_port_error_set(xUSBH_Context_t *host_ctx, uint8_t port);
static void root_port_device_binding_clear(xUSBH_Context_t *host_ctx, uint8_t device_index);
static xRETURN_t root_port_process_connected(xUSBH_Context_t *host_ctx, uint8_t port);
static xRETURN_t root_port_process_powered(xUSBH_Context_t *host_ctx, uint8_t port);
static xRETURN_t root_port_process_resetting(xUSBH_Context_t *host_ctx, uint8_t port);
static xRETURN_t root_port_process_enabled(xUSBH_Context_t *host_ctx, uint8_t port);
static xRETURN_t root_port_process(xUSBH_Context_t *host_ctx, uint8_t port);
static xRETURN_t root_ports_process(xUSBH_Context_t *host_ctx);
static bool device_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Device_Context_t *device_ctx, uint8_t *index);
static bool interface_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Interface_Context_t *interface_ctx, uint8_t *index);
static bool endpoint_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Endpoint_Context_t *endpoint_ctx, uint8_t *index);
static bool transfer_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer, uint8_t *index);
static void release_interfaces_for_device(xUSBH_Context_t *host_ctx, uint8_t device_index);
static void release_endpoints_for_interface(xUSBH_Context_t *host_ctx, uint8_t interface_index);
static void release_transfers_for_device(xUSBH_Context_t *host_ctx, uint8_t device_address);
static xRETURN_t transfer_cancel_if_submitted(xUSBH_Context_t *host_ctx, xUSBH_Transfer_t *transfer);
static bool endpoint_address_is_valid(uint8_t endpoint_address);
static bool endpoint_address_is_in(uint8_t endpoint_address);
static bool endpoint_exists_for_device(const xUSBH_Context_t *host_ctx, uint8_t device_index, uint8_t endpoint_address);
static xRETURN_t topology_current_interface_finish(xUSBH_Topology_Parse_Context_t *parser);
static xRETURN_t topology_interface_store(xUSBH_Context_t *host_ctx,
                                          xUSBH_Device_Context_t *device_ctx,
                                          const xUSBH_Descriptor_Header_t *header,
                                          xUSBH_Topology_Parse_Context_t *parser);
static xRETURN_t
topology_endpoint_store(xUSBH_Context_t *host_ctx, const xUSBH_Descriptor_Header_t *header, xUSBH_Topology_Parse_Context_t *parser);
static xRETURN_t topology_descriptor_store(xUSBH_Context_t *host_ctx,
                                           xUSBH_Device_Context_t *device_ctx,
                                           const xUSBH_Descriptor_Header_t *header,
                                           xUSBH_Topology_Parse_Context_t *parser);

// MODULE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
static bool init_config_is_valid(const xUSBH_Init_Config_t *config)
{
    return (config->root_port_count > 0U) && ((uint32_t)config->root_port_count <= xUSBH_MAX_ROOT_PORTS);
}

static bool context_is_initialized(const xUSBH_Context_t *host_ctx)
{
    return host_ctx->is_initialized;
}

static bool context_is_started(const xUSBH_Context_t *host_ctx)
{
    return host_ctx->is_started;
}

static void hcd_event_callback(void *host_ctx, const xUSBH_HCD_Event_t *event)
{
    if ((host_ctx == NULL) || (event == NULL))
    {
        return;
    }

    xUSBH_Context_t *ctx = (xUSBH_Context_t *)host_ctx;

    if ((event->type == xUSBH_HCD_EVENT_TYPE_TRANSFER) && (event->transfer != NULL))
    {
        uint8_t transfer_index = 0U;
        if ((transfer_index_get(ctx, event->transfer, &transfer_index) == true) && (event->transfer->is_allocated == true))
        {
            (void)transfer_index;
            event->transfer->last_event = event->transfer_event;
            event->transfer->is_submitted = false;
            xUSBH_TRACE_E3(ctx, xUSBH_TRACE_CODE_TRANSFER_COMPLETE, event->transfer->endpoint_address, event->transfer_event,
                           event->transfer->actual_length);
            (void)xUSBH_Class_Transfer_Complete(ctx, event->transfer);
        }
    }
    else if (event->type == xUSBH_HCD_EVENT_TYPE_PORT)
    {
        hcd_port_event_handle(ctx, event);
    }
}

static void hcd_port_event_handle(xUSBH_Context_t *host_ctx, const xUSBH_HCD_Event_t *event)
{
    if ((host_ctx == NULL) || (event == NULL) || (port_is_valid(host_ctx, event->port) == false))
    {
        return;
    }

    switch (event->port_event)
    {
    case xUSBH_HCD_PORT_EVENT_CONNECTED:
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_PORT_CONNECT, event->port);
        (void)root_port_connected_set(host_ctx, event->port);
        break;

    case xUSBH_HCD_PORT_EVENT_DISCONNECTED:
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_PORT_DISCONNECT, event->port);
        (void)xUSBH_Port_Disconnect_Cleanup(host_ctx, event->port);
        break;

    case xUSBH_HCD_PORT_EVENT_RESET_COMPLETE:
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_PORT_RESET, event->port);
        (void)root_port_reset_complete_set(host_ctx, event->port);
        break;

    case xUSBH_HCD_PORT_EVENT_SUSPENDED:
        (void)root_port_suspended_set(host_ctx, event->port);
        break;

    case xUSBH_HCD_PORT_EVENT_RESUMED:
        (void)root_port_resumed_set(host_ctx, event->port);
        break;

    case xUSBH_HCD_PORT_EVENT_OVERCURRENT:
        (void)root_port_error_set(host_ctx, event->port);
        break;

    default:
        break;
    }
}

static xRETURN_t host_hcd_start(xUSBH_Context_t *host_ctx, const xUSBH_Start_Config_t *config)
{
    if (config->hcd_ops == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }

    if (xUSBH_HCD_Ops_Are_Complete(config->hcd_ops) == false)
    {
        return xRETURN_xERR_xUSBH_HCD_INCOMPLETE_OPS;
    }

    xRETURN_t status = xUSBH_HCD_Init(config->hcd_ops, config->hcd_ctx, host_ctx, hcd_event_callback);
    if (status != xRETURN_OK)
    {
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_HCD_ERROR, status);
        return status;
    }

    status = xUSBH_HCD_Start(config->hcd_ops, config->hcd_ctx);
    if (status != xRETURN_OK)
    {
        (void)xUSBH_HCD_Deinit(config->hcd_ops, config->hcd_ctx);
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_HCD_ERROR, status);
        return status;
    }

    status = xUSBH_HCD_Enable_Interrupts(config->hcd_ops, config->hcd_ctx);
    if (status != xRETURN_OK)
    {
        (void)xUSBH_HCD_Stop(config->hcd_ops, config->hcd_ctx);
        (void)xUSBH_HCD_Deinit(config->hcd_ops, config->hcd_ctx);
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_HCD_ERROR, status);
        return status;
    }

    host_ctx->hcd_ops = config->hcd_ops;
    host_ctx->hcd_ctx = config->hcd_ctx;

    return xRETURN_OK;
}

static xRETURN_t host_hcd_stop(xUSBH_Context_t *host_ctx)
{
    xRETURN_t status = xUSBH_HCD_Disable_Interrupts(host_ctx->hcd_ops, host_ctx->hcd_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xUSBH_HCD_Stop(host_ctx->hcd_ops, host_ctx->hcd_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xUSBH_HCD_Deinit(host_ctx->hcd_ops, host_ctx->hcd_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    host_ctx->hcd_ops = NULL;
    host_ctx->hcd_ctx = NULL;

    return xRETURN_OK;
}

static xRETURN_t context_ready_for_runtime_slots(const xUSBH_Context_t *host_ctx)
{
    if (host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (context_is_initialized(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    return xRETURN_OK;
}

static bool port_is_valid(const xUSBH_Context_t *host_ctx, uint8_t port)
{
    return port < host_ctx->root_port_count;
}

static void root_port_clear(xUSBH_Context_t *host_ctx, uint8_t port)
{
    (void)memset(&host_ctx->root_ports[port], 0, sizeof(host_ctx->root_ports[port]));
    host_ctx->root_ports[port].state = xUSBH_ROOT_PORT_DISCONNECTED;
}

static xRETURN_t root_port_connected_set(xUSBH_Context_t *host_ctx, uint8_t port)
{
    if (port_is_valid(host_ctx, port) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    xUSBH_Root_Port_Context_t *root_port = &host_ctx->root_ports[port];
    if ((root_port->state != xUSBH_ROOT_PORT_DISCONNECTED) && (root_port->state != xUSBH_ROOT_PORT_ERROR))
    {
        return xRETURN_OK;
    }

    root_port_clear(host_ctx, port);
    root_port->state = xUSBH_ROOT_PORT_CONNECTED;
    root_port->is_connected = true;
    root_port->debounce_remaining = xUSBH_CONNECT_DEBOUNCE_SAMPLES;

    return xRETURN_OK;
}

static xRETURN_t root_port_reset_complete_set(xUSBH_Context_t *host_ctx, uint8_t port)
{
    if (port_is_valid(host_ctx, port) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    host_ctx->root_ports[port].is_reset_complete = true;

    return xRETURN_OK;
}

static xRETURN_t root_port_suspended_set(xUSBH_Context_t *host_ctx, uint8_t port)
{
    if (port_is_valid(host_ctx, port) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    host_ctx->root_ports[port].state = xUSBH_ROOT_PORT_SUSPENDED;

    return xRETURN_OK;
}

static xRETURN_t root_port_resumed_set(xUSBH_Context_t *host_ctx, uint8_t port)
{
    if (port_is_valid(host_ctx, port) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    if (host_ctx->root_ports[port].state == xUSBH_ROOT_PORT_SUSPENDED)
    {
        host_ctx->root_ports[port].state = xUSBH_ROOT_PORT_ENABLED;
    }

    return xRETURN_OK;
}

static xRETURN_t root_port_error_set(xUSBH_Context_t *host_ctx, uint8_t port)
{
    if (port_is_valid(host_ctx, port) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    host_ctx->root_ports[port].state = xUSBH_ROOT_PORT_ERROR;

    return xRETURN_OK;
}

static void root_port_device_binding_clear(xUSBH_Context_t *host_ctx, uint8_t device_index)
{
    uint8_t i;

    for (i = 0U; i < host_ctx->root_port_count; i++)
    {
        if ((host_ctx->root_ports[i].has_device == true) && (host_ctx->root_ports[i].device_index == device_index))
        {
            host_ctx->root_ports[i].has_device = false;
            host_ctx->root_ports[i].device_index = 0U;
        }
    }
}

static xRETURN_t root_port_process_connected(xUSBH_Context_t *host_ctx, uint8_t port)
{
    xUSBH_HCD_Port_Status_t status = {0};
    xUSBH_Root_Port_Context_t *root_port = &host_ctx->root_ports[port];
    xRETURN_t result = xUSBH_HCD_Get_Port_Status(host_ctx->hcd_ops, host_ctx->hcd_ctx, port, &status);
    if (result != xRETURN_OK)
    {
        root_port->state = xUSBH_ROOT_PORT_ERROR;
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_HCD_ERROR, result);
        return result;
    }

    if ((status.is_connected == false) || (status.is_overcurrent == true))
    {
        if (status.is_overcurrent == true)
        {
            root_port->state = xUSBH_ROOT_PORT_ERROR;
            return xRETURN_OK;
        }

        return xUSBH_Port_Disconnect_Cleanup(host_ctx, port);
    }

    if (root_port->debounce_remaining > 0U)
    {
        root_port->debounce_remaining--;
        return xRETURN_OK;
    }

    result = xUSBH_HCD_Port_Power(host_ctx->hcd_ops, host_ctx->hcd_ctx, port, true);
    if (result != xRETURN_OK)
    {
        root_port->state = xUSBH_ROOT_PORT_ERROR;
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_HCD_ERROR, result);
        return result;
    }

    root_port->state = xUSBH_ROOT_PORT_POWERED;

    return xRETURN_OK;
}

static xRETURN_t root_port_process_powered(xUSBH_Context_t *host_ctx, uint8_t port)
{
    xRETURN_t result = xUSBH_HCD_Port_Reset(host_ctx->hcd_ops, host_ctx->hcd_ctx, port);
    if (result != xRETURN_OK)
    {
        host_ctx->root_ports[port].state = xUSBH_ROOT_PORT_ERROR;
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_HCD_ERROR, result);
        return result;
    }

    host_ctx->root_ports[port].is_reset_complete = false;
    host_ctx->root_ports[port].state = xUSBH_ROOT_PORT_RESETTING;

    return xRETURN_OK;
}

static xRETURN_t root_port_process_resetting(xUSBH_Context_t *host_ctx, uint8_t port)
{
    xUSBH_Root_Port_Context_t *root_port = &host_ctx->root_ports[port];
    xUSBH_HCD_Port_Status_t status = {0};

    if (root_port->is_reset_complete == false)
    {
        return xRETURN_OK;
    }

    xRETURN_t result = xUSBH_HCD_Get_Port_Status(host_ctx->hcd_ops, host_ctx->hcd_ctx, port, &status);
    if (result != xRETURN_OK)
    {
        root_port->state = xUSBH_ROOT_PORT_ERROR;
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_HCD_ERROR, result);
        return result;
    }

    if (status.is_connected == false)
    {
        return xUSBH_Port_Disconnect_Cleanup(host_ctx, port);
    }

    if ((status.is_enabled == false) || (status.is_overcurrent == true))
    {
        root_port->state = xUSBH_ROOT_PORT_ERROR;
        return xRETURN_OK;
    }

    root_port->speed = status.speed;
    root_port->state = xUSBH_ROOT_PORT_ENABLED;

    return xRETURN_OK;
}

static xRETURN_t root_port_process_enabled(xUSBH_Context_t *host_ctx, uint8_t port)
{
    xUSBH_Root_Port_Context_t *root_port = &host_ctx->root_ports[port];
    xRETURN_t result = xUSBH_Enumeration_Start(host_ctx, port);
    if (result == xRETURN_OK)
    {
        root_port->state = xUSBH_ROOT_PORT_ENUMERATING;
    }
    else
    {
        root_port->state = xUSBH_ROOT_PORT_ERROR;
    }

    return result;
}

static xRETURN_t root_port_process(xUSBH_Context_t *host_ctx, uint8_t port)
{
    switch (host_ctx->root_ports[port].state)
    {
    case xUSBH_ROOT_PORT_CONNECTED:
        return root_port_process_connected(host_ctx, port);

    case xUSBH_ROOT_PORT_POWERED:
        return root_port_process_powered(host_ctx, port);

    case xUSBH_ROOT_PORT_RESETTING:
        return root_port_process_resetting(host_ctx, port);

    case xUSBH_ROOT_PORT_ENABLED:
        return root_port_process_enabled(host_ctx, port);

    case xUSBH_ROOT_PORT_DISCONNECTED:
    case xUSBH_ROOT_PORT_ENUMERATING:
        return xUSBH_Enumeration_Process(host_ctx, port);

    case xUSBH_ROOT_PORT_CONFIGURED:
    case xUSBH_ROOT_PORT_SUSPENDED:
    case xUSBH_ROOT_PORT_ERROR:
    default:
        return xRETURN_OK;
    }
}

static xRETURN_t root_ports_process(xUSBH_Context_t *host_ctx)
{
    uint8_t i;

    for (i = 0U; i < host_ctx->root_port_count; i++)
    {
        xRETURN_t result = root_port_process(host_ctx, i);
        if (result != xRETURN_OK)
        {
            return result;
        }
    }

    return xRETURN_OK;
}

static bool device_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Device_Context_t *device_ctx, uint8_t *index)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_DEVICES; i++)
    {
        if (&host_ctx->devices[i] == device_ctx)
        {
            *index = i;
            return true;
        }
    }

    return false;
}

static bool interface_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Interface_Context_t *interface_ctx, uint8_t *index)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_INTERFACES; i++)
    {
        if (&host_ctx->interfaces[i] == interface_ctx)
        {
            *index = i;
            return true;
        }
    }

    return false;
}

static bool endpoint_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Endpoint_Context_t *endpoint_ctx, uint8_t *index)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_ENDPOINTS; i++)
    {
        if (&host_ctx->endpoints[i] == endpoint_ctx)
        {
            *index = i;
            return true;
        }
    }

    return false;
}

static bool transfer_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer, uint8_t *index)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_TRANSFERS; i++)
    {
        if (&host_ctx->transfers[i] == transfer)
        {
            *index = i;
            return true;
        }
    }

    return false;
}

static void release_endpoints_for_interface(xUSBH_Context_t *host_ctx, uint8_t interface_index)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_ENDPOINTS; i++)
    {
        if ((host_ctx->endpoints[i].is_allocated == true) && (host_ctx->endpoints[i].interface_index == interface_index))
        {
            (void)memset(&host_ctx->endpoints[i], 0, sizeof(host_ctx->endpoints[i]));
        }
    }
}

static void release_interfaces_for_device(xUSBH_Context_t *host_ctx, uint8_t device_index)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_INTERFACES; i++)
    {
        if ((host_ctx->interfaces[i].is_allocated == true) && (host_ctx->interfaces[i].device_index == device_index))
        {
            release_endpoints_for_interface(host_ctx, i);
            (void)memset(&host_ctx->interfaces[i], 0, sizeof(host_ctx->interfaces[i]));
        }
    }
}

static void release_transfers_for_device(xUSBH_Context_t *host_ctx, uint8_t device_address)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_TRANSFERS; i++)
    {
        if ((host_ctx->transfers[i].is_allocated == true) && (host_ctx->transfers[i].device_address == device_address))
        {
            (void)transfer_cancel_if_submitted(host_ctx, &host_ctx->transfers[i]);
            (void)memset(&host_ctx->transfers[i], 0, sizeof(host_ctx->transfers[i]));
        }
    }
}

static xRETURN_t transfer_cancel_if_submitted(xUSBH_Context_t *host_ctx, xUSBH_Transfer_t *transfer)
{
    if (transfer->is_submitted == false)
    {
        return xRETURN_OK;
    }

    xRETURN_t status = xUSBH_HCD_Cancel_Transfer(host_ctx->hcd_ops, host_ctx->hcd_ctx, transfer);
    if (status != xRETURN_OK)
    {
        return status;
    }

    transfer->is_submitted = false;

    return xRETURN_OK;
}

static bool endpoint_address_is_valid(uint8_t endpoint_address)
{
    return ((endpoint_address & USB_ENDP_ADDR_MASK) != 0U) &&
           ((endpoint_address & (uint8_t)(~(USB_ENDP_DIR_MASK | USB_ENDP_ADDR_MASK))) == 0U);
}

static bool endpoint_address_is_in(uint8_t endpoint_address)
{
    return (endpoint_address & USB_ENDP_DIR_MASK) != 0U;
}

static bool endpoint_exists_for_device(const xUSBH_Context_t *host_ctx, uint8_t device_index, uint8_t endpoint_address)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_ENDPOINTS; i++)
    {
        if ((host_ctx->endpoints[i].is_allocated == true) && (host_ctx->endpoints[i].device_index == device_index) &&
            (host_ctx->endpoints[i].endpoint_address == endpoint_address))
        {
            return true;
        }
    }

    return false;
}

static xRETURN_t topology_current_interface_finish(xUSBH_Topology_Parse_Context_t *parser)
{
    if ((parser->current_interface != NULL) && (parser->current_interface->endpoint_count != parser->current_expected_endpoint_count))
    {
        return xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR;
    }

    parser->current_interface = NULL;
    parser->current_expected_endpoint_count = 0U;
    parser->is_skipping_alternate = false;

    return xRETURN_OK;
}

static xRETURN_t topology_interface_store(xUSBH_Context_t *host_ctx,
                                          xUSBH_Device_Context_t *device_ctx,
                                          const xUSBH_Descriptor_Header_t *header,
                                          xUSBH_Topology_Parse_Context_t *parser)
{
    xUSBH_Interface_Descriptor_t descriptor = {0};
    xUSBH_Interface_Context_t *interface_ctx = NULL;

    xRETURN_t status = topology_current_interface_finish(parser);
    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xUSBH_Interface_Descriptor_Parse(header->data, header->length, &descriptor);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (descriptor.alternate_setting != 0U)
    {
        parser->is_skipping_alternate = true;
        return xRETURN_OK;
    }

    status = xUSBH_Interface_Allocate(host_ctx, device_ctx, &interface_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    interface_ctx->interface_number = descriptor.interface_number;
    interface_ctx->alternate_setting = descriptor.alternate_setting;
    interface_ctx->class_code = descriptor.class_code;
    interface_ctx->subclass = descriptor.subclass;
    interface_ctx->protocol = descriptor.protocol;
    interface_ctx->endpoint_count = 0U;
    parser->current_interface = interface_ctx;
    parser->current_expected_endpoint_count = descriptor.endpoint_count;
    parser->active_interface_count++;

    return xRETURN_OK;
}

static xRETURN_t
topology_endpoint_store(xUSBH_Context_t *host_ctx, const xUSBH_Descriptor_Header_t *header, xUSBH_Topology_Parse_Context_t *parser)
{
    xUSBH_Endpoint_Descriptor_t descriptor = {0};
    xUSBH_Endpoint_Context_t *endpoint_ctx = NULL;

    if (parser->current_interface == NULL)
    {
        if (parser->is_skipping_alternate == true)
        {
            return xRETURN_OK;
        }

        return xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR;
    }

    if (parser->current_interface->endpoint_count >= parser->current_expected_endpoint_count)
    {
        return xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR;
    }

    xRETURN_t status = xUSBH_Endpoint_Descriptor_Parse(header->data, header->length, &descriptor);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if ((endpoint_address_is_valid(descriptor.endpoint_address) == false) || (descriptor.endpoint_type == USB_ENDP_TYPE_CTRL) ||
        (descriptor.max_packet_size == 0U) ||
        (endpoint_exists_for_device(host_ctx, parser->device_index, descriptor.endpoint_address) == true))
    {
        return xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR;
    }

    status = xUSBH_Endpoint_Allocate(host_ctx, parser->current_interface, &endpoint_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    endpoint_ctx->endpoint_address = descriptor.endpoint_address;
    endpoint_ctx->endpoint_type = descriptor.endpoint_type;
    endpoint_ctx->is_in = endpoint_address_is_in(descriptor.endpoint_address);
    endpoint_ctx->max_packet_size = descriptor.max_packet_size;
    endpoint_ctx->interval = descriptor.interval;
    parser->current_interface->endpoint_count++;

    return xRETURN_OK;
}

static xRETURN_t topology_descriptor_store(xUSBH_Context_t *host_ctx,
                                           xUSBH_Device_Context_t *device_ctx,
                                           const xUSBH_Descriptor_Header_t *header,
                                           xUSBH_Topology_Parse_Context_t *parser)
{
    switch (header->type)
    {
    case USB_DESC_TYPE_INTERFACE:
        return topology_interface_store(host_ctx, device_ctx, header, parser);

    case USB_DESC_TYPE_ENDPOINT:
        return topology_endpoint_store(host_ctx, header, parser);

    case USB_DESC_TYPE_CONFIGURATION:
    default:
        return xRETURN_OK;
    }
}

// PUBLIC FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
xRETURN_t xUSBH_Init(xUSBH_Context_t *host_ctx, const xUSBH_Init_Config_t *config)
{
    if ((host_ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (init_config_is_valid(config) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_CONFIGURATION;
    }

    (void)memset(host_ctx, 0, sizeof(xUSBH_Context_t));
    host_ctx->lifecycle_state = xUSBH_LIFECYCLE_INITIALIZED;
    host_ctx->root_port_count = config->root_port_count;
    host_ctx->is_initialized = true;

    return xRETURN_OK;
}

#if xTRACE_ENABLE
xRETURN_t xUSBH_Trace_Init(xUSBH_Context_t *host_ctx, struct xTRACE_Context_t *trace_ctx)
{
    if (host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    host_ctx->trace_ctx = trace_ctx;
    return xRETURN_OK;
}
#endif

xRETURN_t xUSBH_Start(xUSBH_Context_t *host_ctx, const xUSBH_Start_Config_t *config)
{
    if ((host_ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (context_is_initialized(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    if (context_is_started(host_ctx) == true)
    {
        return xRETURN_xERR_xUSBH_ALREADY_STARTED;
    }

    xRETURN_t status = host_hcd_start(host_ctx, config);
    if (status != xRETURN_OK)
    {
        return status;
    }

    host_ctx->is_started = true;
    host_ctx->lifecycle_state = xUSBH_LIFECYCLE_STARTED;

    return xRETURN_OK;
}

xRETURN_t xUSBH_Process(xUSBH_Context_t *host_ctx)
{
    if (host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (context_is_initialized(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    if (context_is_started(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_STARTED;
    }

    xRETURN_t status = root_ports_process(host_ctx);
    if (status != xRETURN_OK)
    {
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_HCD_ERROR, status);
    }

    return status;
}

xRETURN_t xUSBH_Stop(xUSBH_Context_t *host_ctx)
{
    if (host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (context_is_initialized(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    if (context_is_started(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_STARTED;
    }

    xRETURN_t status = host_hcd_stop(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    host_ctx->is_started = false;
    host_ctx->lifecycle_state = xUSBH_LIFECYCLE_STOPPED;

    return xRETURN_OK;
}

xRETURN_t xUSBH_Get_Lifecycle_State(const xUSBH_Context_t *host_ctx, xUSBH_Lifecycle_State_t *state)
{
    if ((host_ctx == NULL) || (state == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    *state = host_ctx->lifecycle_state;

    return xRETURN_OK;
}

xRETURN_t xUSBH_Is_Started(const xUSBH_Context_t *host_ctx, bool *is_started)
{
    if ((host_ctx == NULL) || (is_started == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    *is_started = host_ctx->is_started;

    return xRETURN_OK;
}

xRETURN_t xUSBH_Root_Port_Get_State(const xUSBH_Context_t *host_ctx, uint8_t port, xUSBH_Root_Port_State_t *state)
{
    if ((host_ctx == NULL) || (state == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (context_is_initialized(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    if (port_is_valid(host_ctx, port) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    *state = host_ctx->root_ports[port].state;

    return xRETURN_OK;
}

xRETURN_t xUSBH_Device_Allocate(xUSBH_Context_t *host_ctx, uint8_t port, xUSBH_Device_Context_t **device_ctx)
{
    uint8_t i;
    xRETURN_t status = context_ready_for_runtime_slots(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (device_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (port_is_valid(host_ctx, port) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    for (i = 0U; i < xUSBH_MAX_DEVICES; i++)
    {
        if (host_ctx->devices[i].is_allocated == false)
        {
            (void)memset(&host_ctx->devices[i], 0, sizeof(host_ctx->devices[i]));
            host_ctx->devices[i].is_allocated = true;
            host_ctx->devices[i].state = xUSBH_DEVICE_STATE_ATTACHED;
            host_ctx->devices[i].port = port;
            host_ctx->devices[i].speed = USB_SPEED_FULL;
            *device_ctx = &host_ctx->devices[i];
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED;
}

xRETURN_t xUSBH_Device_Release(xUSBH_Context_t *host_ctx, xUSBH_Device_Context_t *device_ctx)
{
    uint8_t device_index = 0U;
    xRETURN_t status = context_ready_for_runtime_slots(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (device_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((device_index_get(host_ctx, device_ctx, &device_index) == false) || (device_ctx->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    (void)xUSBH_Class_Unbind_Device(host_ctx, device_ctx);
    release_transfers_for_device(host_ctx, device_ctx->address);
    release_interfaces_for_device(host_ctx, device_index);
    root_port_device_binding_clear(host_ctx, device_index);
    (void)memset(device_ctx, 0, sizeof(*device_ctx));

    return xRETURN_OK;
}

xRETURN_t xUSBH_Interface_Allocate(xUSBH_Context_t *host_ctx, xUSBH_Device_Context_t *device_ctx, xUSBH_Interface_Context_t **interface_ctx)
{
    uint8_t device_index = 0U;
    uint8_t i;
    xRETURN_t status = context_ready_for_runtime_slots(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if ((device_ctx == NULL) || (interface_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((device_index_get(host_ctx, device_ctx, &device_index) == false) || (device_ctx->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    for (i = 0U; i < xUSBH_MAX_INTERFACES; i++)
    {
        if (host_ctx->interfaces[i].is_allocated == false)
        {
            (void)memset(&host_ctx->interfaces[i], 0, sizeof(host_ctx->interfaces[i]));
            host_ctx->interfaces[i].is_allocated = true;
            host_ctx->interfaces[i].device_index = device_index;
            *interface_ctx = &host_ctx->interfaces[i];
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED;
}

xRETURN_t xUSBH_Interface_Release(xUSBH_Context_t *host_ctx, xUSBH_Interface_Context_t *interface_ctx)
{
    uint8_t interface_index = 0U;
    xRETURN_t status = context_ready_for_runtime_slots(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (interface_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((interface_index_get(host_ctx, interface_ctx, &interface_index) == false) || (interface_ctx->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    release_endpoints_for_interface(host_ctx, interface_index);
    (void)memset(interface_ctx, 0, sizeof(*interface_ctx));

    return xRETURN_OK;
}

xRETURN_t
xUSBH_Endpoint_Allocate(xUSBH_Context_t *host_ctx, xUSBH_Interface_Context_t *interface_ctx, xUSBH_Endpoint_Context_t **endpoint_ctx)
{
    uint8_t interface_index = 0U;
    uint8_t i;
    xRETURN_t status = context_ready_for_runtime_slots(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if ((interface_ctx == NULL) || (endpoint_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((interface_index_get(host_ctx, interface_ctx, &interface_index) == false) || (interface_ctx->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    for (i = 0U; i < xUSBH_MAX_ENDPOINTS; i++)
    {
        if (host_ctx->endpoints[i].is_allocated == false)
        {
            (void)memset(&host_ctx->endpoints[i], 0, sizeof(host_ctx->endpoints[i]));
            host_ctx->endpoints[i].is_allocated = true;
            host_ctx->endpoints[i].device_index = interface_ctx->device_index;
            host_ctx->endpoints[i].interface_index = interface_index;
            *endpoint_ctx = &host_ctx->endpoints[i];
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED;
}

xRETURN_t xUSBH_Endpoint_Release(xUSBH_Context_t *host_ctx, xUSBH_Endpoint_Context_t *endpoint_ctx)
{
    uint8_t endpoint_index = 0U;
    xRETURN_t status = context_ready_for_runtime_slots(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (endpoint_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((endpoint_index_get(host_ctx, endpoint_ctx, &endpoint_index) == false) || (endpoint_ctx->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    (void)endpoint_index;
    (void)memset(endpoint_ctx, 0, sizeof(*endpoint_ctx));

    return xRETURN_OK;
}

xRETURN_t xUSBH_Endpoint_Find_By_Address(xUSBH_Context_t *host_ctx,
                                         const xUSBH_Device_Context_t *device_ctx,
                                         uint8_t endpoint_address,
                                         xUSBH_Endpoint_Context_t **endpoint_ctx)
{
    uint8_t device_index = 0U;
    uint8_t i;
    xRETURN_t status = context_ready_for_runtime_slots(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if ((device_ctx == NULL) || (endpoint_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (endpoint_address_is_valid(endpoint_address) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    if ((device_index_get(host_ctx, device_ctx, &device_index) == false) || (device_ctx->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    *endpoint_ctx = NULL;
    for (i = 0U; i < xUSBH_MAX_ENDPOINTS; i++)
    {
        if ((host_ctx->endpoints[i].is_allocated == true) && (host_ctx->endpoints[i].device_index == device_index) &&
            (host_ctx->endpoints[i].endpoint_address == endpoint_address))
        {
            *endpoint_ctx = &host_ctx->endpoints[i];
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xUSBH_INVALID_OBJECT;
}

xRETURN_t
xUSBH_Device_Build_Topology(xUSBH_Context_t *host_ctx, xUSBH_Device_Context_t *device_ctx, const uint8_t *buffer, uint32_t buffer_length)
{
    uint8_t device_index = 0U;
    uint16_t total_length = 0U;
    xUSBH_Descriptor_Walker_t walker = {0};
    xUSBH_Descriptor_Header_t header = {0};
    xUSBH_Topology_Parse_Context_t parser = {0};
    bool has_descriptor = false;
    xRETURN_t status = context_ready_for_runtime_slots(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if ((device_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((device_index_get(host_ctx, device_ctx, &device_index) == false) || (device_ctx->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    status = xUSBH_Configuration_Descriptor_Validate(buffer, buffer_length, &total_length);
    if (status != xRETURN_OK)
    {
        return status;
    }

    release_interfaces_for_device(host_ctx, device_index);
    parser.device_index = device_index;
    status = xUSBH_Descriptor_Walker_Init(&walker, buffer, total_length);
    while (status == xRETURN_OK)
    {
        status = xUSBH_Descriptor_Walker_Next(&walker, &header, &has_descriptor);
        if ((status != xRETURN_OK) || (has_descriptor == false))
        {
            break;
        }

        status = topology_descriptor_store(host_ctx, device_ctx, &header, &parser);
    }

    if (status == xRETURN_OK)
    {
        status = topology_current_interface_finish(&parser);
    }

    if ((status == xRETURN_OK) && (parser.active_interface_count == 0U))
    {
        status = xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR;
    }

    if (status != xRETURN_OK)
    {
        release_interfaces_for_device(host_ctx, device_index);
    }

    return status;
}

xRETURN_t xUSBH_Transfer_Allocate(xUSBH_Context_t *host_ctx, xUSBH_Transfer_t **transfer)
{
    uint8_t i;
    xRETURN_t status = context_ready_for_runtime_slots(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (transfer == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    for (i = 0U; i < xUSBH_MAX_TRANSFERS; i++)
    {
        if (host_ctx->transfers[i].is_allocated == false)
        {
            (void)memset(&host_ctx->transfers[i], 0, sizeof(host_ctx->transfers[i]));
            host_ctx->transfers[i].is_allocated = true;
            *transfer = &host_ctx->transfers[i];
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED;
}

xRETURN_t xUSBH_Transfer_Release(xUSBH_Context_t *host_ctx, xUSBH_Transfer_t *transfer)
{
    uint8_t transfer_index = 0U;
    xRETURN_t status = context_ready_for_runtime_slots(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (transfer == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((transfer_index_get(host_ctx, transfer, &transfer_index) == false) || (transfer->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    (void)transfer_index;
    status = transfer_cancel_if_submitted(host_ctx, transfer);
    if (status != xRETURN_OK)
    {
        return status;
    }

    (void)memset(transfer, 0, sizeof(*transfer));

    return xRETURN_OK;
}

xRETURN_t xUSBH_Transfer_Submit(xUSBH_Context_t *host_ctx, xUSBH_Transfer_t *transfer)
{
    uint8_t transfer_index = 0U;

    if (host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (context_is_initialized(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    if (context_is_started(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_STARTED;
    }

    if (transfer == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((transfer_index_get(host_ctx, transfer, &transfer_index) == false) || (transfer->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    if (transfer->is_submitted == true)
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    (void)transfer_index;
    xRETURN_t status = xUSBH_HCD_Submit_Transfer(host_ctx->hcd_ops, host_ctx->hcd_ctx, transfer);
    if (status != xRETURN_OK)
    {
        xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_HCD_ERROR, status);
        return status;
    }

    transfer->is_submitted = true;
    xUSBH_TRACE_E2(host_ctx, xUSBH_TRACE_CODE_TRANSFER_SUBMIT, transfer->endpoint_address, transfer->length);

    return xRETURN_OK;
}

xRETURN_t xUSBH_Transfer_Cancel(xUSBH_Context_t *host_ctx, xUSBH_Transfer_t *transfer)
{
    uint8_t transfer_index = 0U;

    if (host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (context_is_initialized(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    if (context_is_started(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_STARTED;
    }

    if (transfer == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((transfer_index_get(host_ctx, transfer, &transfer_index) == false) || (transfer->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    if (transfer->is_submitted == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    (void)transfer_index;
    return transfer_cancel_if_submitted(host_ctx, transfer);
}

xRETURN_t xUSBH_Port_Disconnect_Cleanup(xUSBH_Context_t *host_ctx, uint8_t port)
{
    uint8_t i;
    xRETURN_t status = context_ready_for_runtime_slots(host_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (port_is_valid(host_ctx, port) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    for (i = 0U; i < xUSBH_MAX_DEVICES; i++)
    {
        if ((host_ctx->devices[i].is_allocated == true) && (host_ctx->devices[i].port == port))
        {
            status = xUSBH_Device_Release(host_ctx, &host_ctx->devices[i]);
            if (status != xRETURN_OK)
            {
                return status;
            }
        }
    }

    xUSBH_Enumeration_Abort(host_ctx, port);
    root_port_clear(host_ctx, port);

    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
