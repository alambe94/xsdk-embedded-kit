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

// @file xusbh_class.c
// @brief USB host class-driver registration and binding implementation.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbh_class.h"
#include "xusbh_trace.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////
typedef struct xUSBH_Class_Match_Result_t
{
    const xUSBH_Class_Registration_t *registration;
    uint8_t match_count;
} xUSBH_Class_Match_Result_t;

// VARIABLES //////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static bool context_is_initialized(const xUSBH_Context_t *host_ctx);
static bool context_is_started(const xUSBH_Context_t *host_ctx);
static bool class_driver_is_valid(const xUSBH_Class_Driver_t *driver);
static bool device_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Device_Context_t *device_ctx, uint8_t *index);
static xRETURN_t
class_match_interface(const xUSBH_Context_t *host_ctx, const xUSBH_Interface_Context_t *interface_ctx, xUSBH_Class_Match_Result_t *result);
static xRETURN_t class_bind_interface(xUSBH_Context_t *host_ctx, xUSBH_Interface_Context_t *interface_ctx);
static xRETURN_t
class_transfer_endpoint_get(xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer, xUSBH_Endpoint_Context_t **endpoint_ctx);
static bool class_transfer_device_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer, uint8_t *device_index);
static xRETURN_t class_transfer_control_dispatch(xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer);

// MODULE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
static bool context_is_initialized(const xUSBH_Context_t *host_ctx)
{
    return host_ctx->is_initialized;
}

static bool context_is_started(const xUSBH_Context_t *host_ctx)
{
    return host_ctx->is_started;
}

static bool class_driver_is_valid(const xUSBH_Class_Driver_t *driver)
{
    return (driver != NULL) && (driver->match != NULL) && (driver->start != NULL) && (driver->stop != NULL);
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

static xRETURN_t
class_match_interface(const xUSBH_Context_t *host_ctx, const xUSBH_Interface_Context_t *interface_ctx, xUSBH_Class_Match_Result_t *result)
{
    uint8_t i;

    result->registration = NULL;
    result->match_count = 0U;

    for (i = 0U; i < xUSBH_MAX_CLASS_DRIVERS; i++)
    {
        if (host_ctx->class_registrations[i].is_registered == true)
        {
            bool is_match = false;
            xRETURN_t status = host_ctx->class_registrations[i].driver->match(interface_ctx, &is_match);
            if (status != xRETURN_OK)
            {
                return status;
            }

            if (is_match == true)
            {
                result->registration = &host_ctx->class_registrations[i];
                result->match_count++;
            }
        }
    }

    return xRETURN_OK;
}

static xRETURN_t class_bind_interface(xUSBH_Context_t *host_ctx, xUSBH_Interface_Context_t *interface_ctx)
{
    xUSBH_Class_Match_Result_t match = {0};

    xRETURN_t status = class_match_interface(host_ctx, interface_ctx, &match);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (match.match_count > 1U)
    {
        return xRETURN_xERR_xUSBH_AMBIGUOUS_CLASS_MATCH;
    }

    if (match.match_count == 0U)
    {
        return xRETURN_OK;
    }

    status = match.registration->driver->start(interface_ctx, match.registration->class_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    interface_ctx->class_driver = match.registration->driver;
    interface_ctx->class_ctx = match.registration->class_ctx;
    xUSBH_TRACE_E1(host_ctx, xUSBH_TRACE_CODE_CLASS_BIND, interface_ctx->interface_number);

    return xRETURN_OK;
}

static xRETURN_t
class_transfer_endpoint_get(xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer, xUSBH_Endpoint_Context_t **endpoint_ctx)
{
    uint8_t i;
    uint8_t device_index = xUSBH_MAX_DEVICES;

    *endpoint_ctx = NULL;

    for (i = 0U; i < xUSBH_MAX_DEVICES; i++)
    {
        if ((host_ctx->devices[i].is_allocated == true) && (host_ctx->devices[i].address == transfer->device_address))
        {
            device_index = i;
            break;
        }
    }

    if (device_index >= xUSBH_MAX_DEVICES)
    {
        return xRETURN_OK;
    }

    for (i = 0U; i < xUSBH_MAX_ENDPOINTS; i++)
    {
        if ((host_ctx->endpoints[i].is_allocated == true) && (host_ctx->endpoints[i].device_index == device_index) &&
            (host_ctx->endpoints[i].endpoint_address == transfer->endpoint_address))
        {
            *endpoint_ctx = &host_ctx->endpoints[i];
            return xRETURN_OK;
        }
    }

    return xRETURN_OK;
}

static bool class_transfer_device_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer, uint8_t *device_index)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_DEVICES; i++)
    {
        if ((host_ctx->devices[i].is_allocated == true) && (host_ctx->devices[i].address == transfer->device_address))
        {
            *device_index = i;
            return true;
        }
    }

    return false;
}

static xRETURN_t class_transfer_control_dispatch(xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer)
{
    uint8_t device_index = 0U;
    uint8_t i;

    if ((transfer->has_setup == false) || (class_transfer_device_index_get(host_ctx, transfer, &device_index) == false))
    {
        return xRETURN_OK;
    }

    for (i = 0U; i < xUSBH_MAX_INTERFACES; i++)
    {
        xUSBH_Interface_Context_t *interface_ctx = &host_ctx->interfaces[i];
        if ((interface_ctx->is_allocated == true) && (interface_ctx->device_index == device_index) &&
            (interface_ctx->class_driver != NULL) && (interface_ctx->class_driver->transfer_complete != NULL))
        {
            xRETURN_t status = interface_ctx->class_driver->transfer_complete(interface_ctx, interface_ctx->class_ctx, transfer);
            if (status != xRETURN_OK)
            {
                return status;
            }
        }
    }

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
xRETURN_t xUSBH_Register_Class(xUSBH_Context_t *host_ctx, const xUSBH_Class_Register_Config_t *config)
{
    uint8_t i;

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
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    if (class_driver_is_valid(config->driver) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    for (i = 0U; i < xUSBH_MAX_CLASS_DRIVERS; i++)
    {
        if (host_ctx->class_registrations[i].is_registered == true)
        {
            if (host_ctx->class_registrations[i].driver == config->driver)
            {
                return xRETURN_xERR_xUSBH_INVALID_STATE;
            }
        }
        else
        {
            host_ctx->class_registrations[i].is_registered = true;
            host_ctx->class_registrations[i].driver = config->driver;
            host_ctx->class_registrations[i].class_ctx = config->class_ctx;
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED;
}

xRETURN_t xUSBH_Class_Bind_Device(xUSBH_Context_t *host_ctx, xUSBH_Device_Context_t *device_ctx)
{
    uint8_t device_index = 0U;
    uint8_t i;

    if ((host_ctx == NULL) || (device_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (context_is_initialized(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    if ((device_index_get(host_ctx, device_ctx, &device_index) == false) || (device_ctx->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    for (i = 0U; i < xUSBH_MAX_INTERFACES; i++)
    {
        if ((host_ctx->interfaces[i].is_allocated == true) && (host_ctx->interfaces[i].device_index == device_index))
        {
            xRETURN_t status = class_bind_interface(host_ctx, &host_ctx->interfaces[i]);
            if (status != xRETURN_OK)
            {
                (void)xUSBH_Class_Unbind_Device(host_ctx, device_ctx);
                return status;
            }
        }
    }

    return xRETURN_OK;
}

xRETURN_t xUSBH_Class_Unbind_Device(xUSBH_Context_t *host_ctx, xUSBH_Device_Context_t *device_ctx)
{
    uint8_t device_index = 0U;
    uint8_t i;

    if ((host_ctx == NULL) || (device_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (context_is_initialized(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    if ((device_index_get(host_ctx, device_ctx, &device_index) == false) || (device_ctx->is_allocated == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    for (i = 0U; i < xUSBH_MAX_INTERFACES; i++)
    {
        if ((host_ctx->interfaces[i].is_allocated == true) && (host_ctx->interfaces[i].device_index == device_index) &&
            (host_ctx->interfaces[i].class_driver != NULL))
        {
            xRETURN_t status = host_ctx->interfaces[i].class_driver->stop(&host_ctx->interfaces[i], host_ctx->interfaces[i].class_ctx);
            host_ctx->interfaces[i].class_driver = NULL;
            host_ctx->interfaces[i].class_ctx = NULL;
            if (status != xRETURN_OK)
            {
                return status;
            }
        }
    }

    return xRETURN_OK;
}

xRETURN_t xUSBH_Class_Transfer_Complete(xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer)
{
    xUSBH_Endpoint_Context_t *endpoint_ctx = NULL;

    if ((host_ctx == NULL) || (transfer == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (context_is_initialized(host_ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    xRETURN_t status = class_transfer_endpoint_get(host_ctx, transfer, &endpoint_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }
    if (endpoint_ctx == NULL)
    {
        return class_transfer_control_dispatch(host_ctx, transfer);
    }
    if (endpoint_ctx->interface_index >= xUSBH_MAX_INTERFACES)
    {
        return xRETURN_OK;
    }

    xUSBH_Interface_Context_t *interface_ctx = &host_ctx->interfaces[endpoint_ctx->interface_index];
    if ((interface_ctx->is_allocated == true) && (interface_ctx->class_driver != NULL) &&
        (interface_ctx->class_driver->transfer_complete != NULL))
    {
        return interface_ctx->class_driver->transfer_complete(interface_ctx, interface_ctx->class_ctx, transfer);
    }

    return xRETURN_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
