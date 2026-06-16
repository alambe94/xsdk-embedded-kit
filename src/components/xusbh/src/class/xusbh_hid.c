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

// @file xusbh_hid.c
// @brief USB host HID boot keyboard and boot mouse class driver implementation.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbh_hid.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t hid_match(const xUSBH_Interface_Context_t *interface_ctx, bool *is_match);
static xRETURN_t hid_start(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx);
static xRETURN_t hid_stop(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx);
static xRETURN_t hid_transfer_complete(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx, const xUSBH_Transfer_t *transfer);
static bool interface_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Interface_Context_t *interface_ctx, uint8_t *index);
static xUSBH_HID_Instance_t *hid_instance_allocate(xUSBH_HID_Context_t *hid_ctx);
static xUSBH_HID_Instance_t *hid_instance_find(xUSBH_HID_Context_t *hid_ctx, const xUSBH_Interface_Context_t *interface_ctx);
static xUSBH_Endpoint_Context_t *hid_interrupt_in_endpoint_find(xUSBH_Context_t *host_ctx, const xUSBH_Interface_Context_t *interface_ctx);
static xUSBH_HID_Report_Type_t hid_report_type_get(const xUSBH_Interface_Context_t *interface_ctx);
static uint8_t hid_report_length_get(xUSBH_HID_Report_Type_t report_type);
static xRETURN_t hid_transfer_submit(xUSBH_HID_Context_t *hid_ctx, xUSBH_HID_Instance_t *instance);
static void hid_keyboard_report_emit(xUSBH_HID_Context_t *hid_ctx, const uint8_t *data);
static void hid_mouse_report_emit(xUSBH_HID_Context_t *hid_ctx, const uint8_t *data, uint32_t length);

// MODULE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
static xRETURN_t hid_match(const xUSBH_Interface_Context_t *interface_ctx, bool *is_match)
{
    if ((interface_ctx == NULL) || (is_match == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    *is_match = (interface_ctx->class_code == USB_CLASS_HID) && (interface_ctx->subclass == xUSBH_HID_BOOT_SUBCLASS) &&
                ((interface_ctx->protocol == xUSBH_HID_PROTOCOL_KEYBOARD) || (interface_ctx->protocol == xUSBH_HID_PROTOCOL_MOUSE));

    return xRETURN_OK;
}

static xRETURN_t hid_start(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx)
{
    xUSBH_HID_Context_t *hid_ctx = (xUSBH_HID_Context_t *)class_ctx;
    xUSBH_HID_Instance_t *instance = NULL;

    if ((interface_ctx == NULL) || (hid_ctx == NULL) || (hid_ctx->host_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xUSBH_Endpoint_Context_t *endpoint_ctx = hid_interrupt_in_endpoint_find(hid_ctx->host_ctx, interface_ctx);
    if (endpoint_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    instance = hid_instance_allocate(hid_ctx);
    if (instance == NULL)
    {
        return xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED;
    }

    instance->report_type = hid_report_type_get(interface_ctx);
    instance->interface_ctx = interface_ctx;
    instance->endpoint_ctx = endpoint_ctx;
    instance->report_length = hid_report_length_get(instance->report_type);

    xRETURN_t status = xUSBH_Transfer_Allocate(hid_ctx->host_ctx, &instance->transfer);
    if (status != xRETURN_OK)
    {
        (void)memset(instance, 0, sizeof(*instance));
        return status;
    }

    status = hid_transfer_submit(hid_ctx, instance);
    if (status != xRETURN_OK)
    {
        (void)xUSBH_Transfer_Release(hid_ctx->host_ctx, instance->transfer);
        (void)memset(instance, 0, sizeof(*instance));
    }

    return status;
}

static xRETURN_t hid_stop(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx)
{
    xUSBH_HID_Context_t *hid_ctx = (xUSBH_HID_Context_t *)class_ctx;

    if ((interface_ctx == NULL) || (hid_ctx == NULL) || (hid_ctx->host_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xUSBH_HID_Instance_t *instance = hid_instance_find(hid_ctx, interface_ctx);
    if (instance == NULL)
    {
        return xRETURN_OK;
    }

    if (instance->transfer != NULL)
    {
        xRETURN_t status = xUSBH_Transfer_Release(hid_ctx->host_ctx, instance->transfer);
        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    (void)memset(instance, 0, sizeof(*instance));

    return xRETURN_OK;
}

static xRETURN_t hid_transfer_complete(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx, const xUSBH_Transfer_t *transfer)
{
    xUSBH_HID_Context_t *hid_ctx = (xUSBH_HID_Context_t *)class_ctx;

    if ((interface_ctx == NULL) || (hid_ctx == NULL) || (transfer == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xUSBH_HID_Instance_t *instance = hid_instance_find(hid_ctx, interface_ctx);
    if ((instance == NULL) || (transfer != instance->transfer))
    {
        return xRETURN_OK;
    }

    if ((transfer->last_event != xUSBH_HCD_TRANSFER_EVENT_COMPLETE) || (transfer->actual_length < instance->report_length))
    {
        return xRETURN_OK;
    }

    if (instance->report_type == xUSBH_HID_REPORT_TYPE_KEYBOARD)
    {
        hid_keyboard_report_emit(hid_ctx, instance->report_buffer);
    }
    else if (instance->report_type == xUSBH_HID_REPORT_TYPE_MOUSE)
    {
        hid_mouse_report_emit(hid_ctx, instance->report_buffer, transfer->actual_length);
    }
    else
    {
        return xRETURN_OK;
    }

    return hid_transfer_submit(hid_ctx, instance);
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

static xUSBH_HID_Instance_t *hid_instance_allocate(xUSBH_HID_Context_t *hid_ctx)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_HID_MAX_INSTANCES; i++)
    {
        if (hid_ctx->instances[i].is_allocated == false)
        {
            (void)memset(&hid_ctx->instances[i], 0, sizeof(hid_ctx->instances[i]));
            hid_ctx->instances[i].is_allocated = true;
            return &hid_ctx->instances[i];
        }
    }

    return NULL;
}

static xUSBH_HID_Instance_t *hid_instance_find(xUSBH_HID_Context_t *hid_ctx, const xUSBH_Interface_Context_t *interface_ctx)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_HID_MAX_INSTANCES; i++)
    {
        if ((hid_ctx->instances[i].is_allocated == true) && (hid_ctx->instances[i].interface_ctx == interface_ctx))
        {
            return &hid_ctx->instances[i];
        }
    }

    return NULL;
}

static xUSBH_Endpoint_Context_t *hid_interrupt_in_endpoint_find(xUSBH_Context_t *host_ctx, const xUSBH_Interface_Context_t *interface_ctx)
{
    uint8_t interface_index = 0U;
    uint8_t i;

    if (interface_index_get(host_ctx, interface_ctx, &interface_index) == false)
    {
        return NULL;
    }

    for (i = 0U; i < xUSBH_MAX_ENDPOINTS; i++)
    {
        if ((host_ctx->endpoints[i].is_allocated == true) && (host_ctx->endpoints[i].interface_index == interface_index) &&
            (host_ctx->endpoints[i].endpoint_type == USB_ENDP_TYPE_INTR) && (host_ctx->endpoints[i].is_in == true))
        {
            return &host_ctx->endpoints[i];
        }
    }

    return NULL;
}

static xUSBH_HID_Report_Type_t hid_report_type_get(const xUSBH_Interface_Context_t *interface_ctx)
{
    if (interface_ctx->protocol == xUSBH_HID_PROTOCOL_KEYBOARD)
    {
        return xUSBH_HID_REPORT_TYPE_KEYBOARD;
    }

    if (interface_ctx->protocol == xUSBH_HID_PROTOCOL_MOUSE)
    {
        return xUSBH_HID_REPORT_TYPE_MOUSE;
    }

    return xUSBH_HID_REPORT_TYPE_NONE;
}

static uint8_t hid_report_length_get(xUSBH_HID_Report_Type_t report_type)
{
    if (report_type == xUSBH_HID_REPORT_TYPE_MOUSE)
    {
        return xUSBH_HID_MOUSE_REPORT_SIZE;
    }

    if (report_type == xUSBH_HID_REPORT_TYPE_KEYBOARD)
    {
        return xUSBH_HID_KEYBOARD_REPORT_SIZE;
    }

    return 0U;
}

static xRETURN_t hid_transfer_submit(xUSBH_HID_Context_t *hid_ctx, xUSBH_HID_Instance_t *instance)
{
    if ((hid_ctx == NULL) || (instance == NULL) || (instance->transfer == NULL) || (instance->endpoint_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    instance->transfer->device_address = hid_ctx->host_ctx->devices[instance->endpoint_ctx->device_index].address;
    instance->transfer->endpoint_address = instance->endpoint_ctx->endpoint_address;
    instance->transfer->endpoint_type = instance->endpoint_ctx->endpoint_type;
    instance->transfer->interval = instance->endpoint_ctx->interval;
    instance->transfer->has_setup = false;
    instance->transfer->last_event = xUSBH_HCD_TRANSFER_EVENT_COMPLETE;
    instance->transfer->data = instance->report_buffer;
    instance->transfer->length = instance->report_length;
    instance->transfer->actual_length = 0U;
    instance->transfer->user_ctx = instance;

    return xUSBH_Transfer_Submit(hid_ctx->host_ctx, instance->transfer);
}

static void hid_keyboard_report_emit(xUSBH_HID_Context_t *hid_ctx, const uint8_t *data)
{
    if (hid_ctx->callbacks.keyboard_report != NULL)
    {
        xUSBH_HID_Keyboard_Report_t report = {
            .modifiers = data[0],
            .reserved = data[1],
        };
        uint8_t i;

        for (i = 0U; i < xUSBH_HID_KEYBOARD_KEY_COUNT; i++)
        {
            report.keys[i] = data[i + 2U];
        }

        hid_ctx->callbacks.keyboard_report(hid_ctx->user_ctx, &report);
    }
}

static void hid_mouse_report_emit(xUSBH_HID_Context_t *hid_ctx, const uint8_t *data, uint32_t length)
{
    if (hid_ctx->callbacks.mouse_report != NULL)
    {
        xUSBH_HID_Mouse_Report_t report = {
            .buttons = data[0],
            .x = (int8_t)data[1],
            .y = (int8_t)data[2],
            .wheel = 0,
        };

        if (length > 3U)
        {
            report.wheel = (int8_t)data[3];
        }

        hid_ctx->callbacks.mouse_report(hid_ctx->user_ctx, &report);
    }
}

// PUBLIC FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
xRETURN_t xUSBH_HID_Init(xUSBH_HID_Context_t *hid_ctx, xUSBH_Context_t *host_ctx, const xUSBH_HID_Callbacks_t *callbacks, void *user_ctx)
{
    if ((hid_ctx == NULL) || (host_ctx == NULL) || (callbacks == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    (void)memset(hid_ctx, 0, sizeof(*hid_ctx));
    hid_ctx->host_ctx = host_ctx;
    hid_ctx->callbacks = *callbacks;
    hid_ctx->user_ctx = user_ctx;

    return xRETURN_OK;
}

const xUSBH_Class_Driver_t *xUSBH_HID_Class(void)
{
    static const xUSBH_Class_Driver_t driver = {
        .match = hid_match,
        .start = hid_start,
        .stop = hid_stop,
        .transfer_complete = hid_transfer_complete,
    };

    return &driver;
}

// EOF /////////////////////////////////////////////////////////////////////////////
