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

// @file xusbh_drv.c
// @brief Implements the fake USB Host Controller Driver port.

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xassert.h"
#include "xusbh_drv.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////
xUSBH_Fake_HCD_Context_t xUSBH_Fake_HCD_Context;

// EXTERN VARIABLES ////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
static xRETURN_t fake_hcd_init(void *hcd_ctx, void *host_ctx, xUSBH_HCD_Event_Callback_t callback);
static xRETURN_t fake_hcd_deinit(void *hcd_ctx);
static xRETURN_t fake_hcd_start(void *hcd_ctx);
static xRETURN_t fake_hcd_stop(void *hcd_ctx);
static xRETURN_t fake_hcd_enable_interrupts(void *hcd_ctx);
static xRETURN_t fake_hcd_disable_interrupts(void *hcd_ctx);
static xRETURN_t fake_hcd_port_power(void *hcd_ctx, uint8_t port, bool enable);
static xRETURN_t fake_hcd_port_reset(void *hcd_ctx, uint8_t port);
static xRETURN_t fake_hcd_get_port_status(void *hcd_ctx, uint8_t port, xUSBH_HCD_Port_Status_t *status);
static xRETURN_t fake_hcd_submit_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer);
static xRETURN_t fake_hcd_cancel_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer);
static uint32_t fake_hcd_get_frame_number(void *hcd_ctx);
static bool transfer_queue_remove(xUSBH_Fake_HCD_Context_t *fake_ctx, xUSBH_Transfer_t *transfer);

// INLINE FUNCTIONS ////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////
static xRETURN_t fake_hcd_init(void *hcd_ctx, void *host_ctx, xUSBH_HCD_Event_Callback_t callback)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");
    xASSERT(host_ctx != NULL, "host_ctx is NULL");
    xASSERT(callback != NULL, "callback is NULL");

    if ((hcd_ctx == NULL) || (host_ctx == NULL) || (callback == NULL))
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->init_count++;
    if (fake_ctx->init_return == xRETURN_OK)
    {
        fake_ctx->host_ctx = host_ctx;
        fake_ctx->event_callback = callback;
        fake_ctx->is_initialized = true;
    }

    return fake_ctx->init_return;
}

static xRETURN_t fake_hcd_deinit(void *hcd_ctx)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");

    if (hcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->deinit_count++;
    if (fake_ctx->deinit_return == xRETURN_OK)
    {
        fake_ctx->is_initialized = false;
        fake_ctx->is_started = false;
        fake_ctx->are_interrupts_enabled = false;
    }

    return fake_ctx->deinit_return;
}

static xRETURN_t fake_hcd_start(void *hcd_ctx)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");

    if (hcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->start_count++;
    if (fake_ctx->start_return == xRETURN_OK)
    {
        fake_ctx->is_started = true;
    }

    return fake_ctx->start_return;
}

static xRETURN_t fake_hcd_stop(void *hcd_ctx)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");

    if (hcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->stop_count++;
    if (fake_ctx->stop_return == xRETURN_OK)
    {
        fake_ctx->is_started = false;
    }

    return fake_ctx->stop_return;
}

static xRETURN_t fake_hcd_enable_interrupts(void *hcd_ctx)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");

    if (hcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->enable_interrupts_count++;
    if (fake_ctx->enable_interrupts_return == xRETURN_OK)
    {
        fake_ctx->are_interrupts_enabled = true;
    }

    return fake_ctx->enable_interrupts_return;
}

static xRETURN_t fake_hcd_disable_interrupts(void *hcd_ctx)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");

    if (hcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->disable_interrupts_count++;
    if (fake_ctx->disable_interrupts_return == xRETURN_OK)
    {
        fake_ctx->are_interrupts_enabled = false;
    }

    return fake_ctx->disable_interrupts_return;
}

static xRETURN_t fake_hcd_port_power(void *hcd_ctx, uint8_t port, bool enable)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");

    if (hcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }
    if (port >= xUSBH_FAKE_HCD_MAX_PORTS)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->port_power_count++;
    fake_ctx->last_port = port;
    fake_ctx->last_port_power_enable = enable;
    if (fake_ctx->port_power_return == xRETURN_OK)
    {
        fake_ctx->is_port_powered = enable;
    }

    return fake_ctx->port_power_return;
}

static xRETURN_t fake_hcd_port_reset(void *hcd_ctx, uint8_t port)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");

    if (hcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }
    if (port >= xUSBH_FAKE_HCD_MAX_PORTS)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->port_reset_count++;
    fake_ctx->last_port = port;
    if (fake_ctx->port_reset_return == xRETURN_OK)
    {
        fake_ctx->port_status.is_enabled = true;
    }

    return fake_ctx->port_reset_return;
}

static xRETURN_t fake_hcd_get_port_status(void *hcd_ctx, uint8_t port, xUSBH_HCD_Port_Status_t *status)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");
    xASSERT(status != NULL, "status is NULL");

    if (hcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }
    if (status == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (port >= xUSBH_FAKE_HCD_MAX_PORTS)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->get_port_status_count++;
    fake_ctx->last_port = port;
    *status = fake_ctx->port_status;

    return fake_ctx->get_port_status_return;
}

static xRETURN_t fake_hcd_submit_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");
    xASSERT(transfer != NULL, "transfer is NULL");

    if (hcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }
    if (transfer == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->submit_transfer_count++;
    fake_ctx->last_transfer = transfer;
    if (fake_ctx->submit_transfer_return != xRETURN_OK)
    {
        return fake_ctx->submit_transfer_return;
    }
    if (fake_ctx->transfer_count >= xUSBH_FAKE_HCD_TRANSFER_QUEUE_DEPTH)
    {
        return xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED;
    }

    fake_ctx->transfer_queue[fake_ctx->transfer_write_idx] = transfer;
    fake_ctx->transfer_write_idx = (fake_ctx->transfer_write_idx + 1U) % xUSBH_FAKE_HCD_TRANSFER_QUEUE_DEPTH;
    fake_ctx->transfer_count++;

    return xRETURN_OK;
}

static xRETURN_t fake_hcd_cancel_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");
    xASSERT(transfer != NULL, "transfer is NULL");

    if (hcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }
    if (transfer == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->cancel_transfer_count++;
    fake_ctx->last_transfer = transfer;
    if (fake_ctx->cancel_transfer_return != xRETURN_OK)
    {
        return fake_ctx->cancel_transfer_return;
    }

    (void)transfer_queue_remove(fake_ctx, transfer);

    return xRETURN_OK;
}

static uint32_t fake_hcd_get_frame_number(void *hcd_ctx)
{
    xASSERT(hcd_ctx != NULL, "hcd_ctx is NULL");

    if (hcd_ctx == NULL)
    {
        return 0U;
    }

    xUSBH_Fake_HCD_Context_t *fake_ctx = (xUSBH_Fake_HCD_Context_t *)hcd_ctx;
    fake_ctx->get_frame_number_count++;

    return fake_ctx->frame_number;
}

static bool transfer_queue_remove(xUSBH_Fake_HCD_Context_t *fake_ctx, xUSBH_Transfer_t *transfer)
{
    bool is_found = false;

    for (uint32_t i = 0U; i < fake_ctx->transfer_count; i++)
    {
        uint32_t idx = (fake_ctx->transfer_read_idx + i) % xUSBH_FAKE_HCD_TRANSFER_QUEUE_DEPTH;
        if (fake_ctx->transfer_queue[idx] == transfer)
        {
            is_found = true;
        }
        if (is_found && ((i + 1U) < fake_ctx->transfer_count))
        {
            uint32_t next_idx = (fake_ctx->transfer_read_idx + i + 1U) % xUSBH_FAKE_HCD_TRANSFER_QUEUE_DEPTH;
            fake_ctx->transfer_queue[idx] = fake_ctx->transfer_queue[next_idx];
        }
    }

    if (is_found)
    {
        fake_ctx->transfer_write_idx =
            (fake_ctx->transfer_write_idx + xUSBH_FAKE_HCD_TRANSFER_QUEUE_DEPTH - 1U) % xUSBH_FAKE_HCD_TRANSFER_QUEUE_DEPTH;
        fake_ctx->transfer_queue[fake_ctx->transfer_write_idx] = NULL;
        fake_ctx->transfer_count--;
    }

    return is_found;
}

// PUBLIC FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////
const xUSBH_HCD_Ops_t xUSBH_Fake_HCD_Ops = {
    .init = fake_hcd_init,
    .deinit = fake_hcd_deinit,
    .start = fake_hcd_start,
    .stop = fake_hcd_stop,
    .enable_interrupts = fake_hcd_enable_interrupts,
    .disable_interrupts = fake_hcd_disable_interrupts,
    .port_power = fake_hcd_port_power,
    .port_reset = fake_hcd_port_reset,
    .get_port_status = fake_hcd_get_port_status,
    .submit_transfer = fake_hcd_submit_transfer,
    .cancel_transfer = fake_hcd_cancel_transfer,
    .get_frame_number = fake_hcd_get_frame_number,
};

xRETURN_t xUSBH_Fake_HCD_Init(xUSBH_Fake_HCD_Context_t *fake_ctx)
{
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");

    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }

    (void)memset(fake_ctx, 0, sizeof(*fake_ctx));
    fake_ctx->init_return = xRETURN_OK;
    fake_ctx->deinit_return = xRETURN_OK;
    fake_ctx->start_return = xRETURN_OK;
    fake_ctx->stop_return = xRETURN_OK;
    fake_ctx->enable_interrupts_return = xRETURN_OK;
    fake_ctx->disable_interrupts_return = xRETURN_OK;
    fake_ctx->port_power_return = xRETURN_OK;
    fake_ctx->port_reset_return = xRETURN_OK;
    fake_ctx->get_port_status_return = xRETURN_OK;
    fake_ctx->submit_transfer_return = xRETURN_OK;
    fake_ctx->cancel_transfer_return = xRETURN_OK;
    fake_ctx->port_status.speed = USB_SPEED_HIGH;
    fake_ctx->frame_number = xUSBH_FAKE_HCD_DEFAULT_FRAME_NUMBER;

    return xRETURN_OK;
}

xRETURN_t xUSBH_Fake_HCD_Fire_Port_Event(xUSBH_Fake_HCD_Context_t *fake_ctx, uint8_t port, xUSBH_HCD_Port_Event_t port_event)
{
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");

    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }
    if (port >= xUSBH_FAKE_HCD_MAX_PORTS)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }
    if ((fake_ctx->event_callback == NULL) || (fake_ctx->host_ctx == NULL) || (fake_ctx->are_interrupts_enabled == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    xUSBH_HCD_Event_t event = {
        .type = xUSBH_HCD_EVENT_TYPE_PORT,
        .port = port,
        .port_event = port_event,
    };
    fake_ctx->event_callback(fake_ctx->host_ctx, &event);

    return xRETURN_OK;
}

xRETURN_t xUSBH_Fake_HCD_Complete_Transfer(xUSBH_Fake_HCD_Context_t *fake_ctx,
                                           xUSBH_Transfer_t *transfer,
                                           xUSBH_HCD_Transfer_Event_t transfer_event,
                                           uint32_t actual_length)
{
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");
    xASSERT(transfer != NULL, "transfer is NULL");

    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }
    if (transfer == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if ((fake_ctx->event_callback == NULL) || (fake_ctx->host_ctx == NULL) || (fake_ctx->are_interrupts_enabled == false))
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }
    if (transfer_queue_remove(fake_ctx, transfer) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    transfer->actual_length = actual_length;
    transfer->last_event = transfer_event;

    xUSBH_HCD_Event_t event = {
        .type = xUSBH_HCD_EVENT_TYPE_TRANSFER,
        .transfer_event = transfer_event,
        .transfer = transfer,
    };
    fake_ctx->event_callback(fake_ctx->host_ctx, &event);

    return xRETURN_OK;
}

xRETURN_t xUSBH_Fake_HCD_Submitted_Pop(xUSBH_Fake_HCD_Context_t *fake_ctx, xUSBH_Transfer_t **transfer)
{
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");
    xASSERT(transfer != NULL, "transfer is NULL");

    if ((fake_ctx == NULL) || (transfer == NULL))
    {
        return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
    }
    if (fake_ctx->transfer_count == 0U)
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    *transfer = fake_ctx->transfer_queue[fake_ctx->transfer_read_idx];
    fake_ctx->transfer_queue[fake_ctx->transfer_read_idx] = NULL;
    fake_ctx->transfer_read_idx = (fake_ctx->transfer_read_idx + 1U) % xUSBH_FAKE_HCD_TRANSFER_QUEUE_DEPTH;
    fake_ctx->transfer_count--;

    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
