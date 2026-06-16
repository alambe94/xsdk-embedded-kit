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

// @file xusbd_drv.c
// @brief Implements the fake USB Device Controller Driver port.

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xassert.h"
#include "xusbd_drv.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////
xUSBD_Fake_DCD_Context_t xUSBD_Fake_DCD_Context;

// EXTERN VARIABLES ////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
static xRETURN_t fake_dcd_init(void *dcd_ctx, USB_Speed_t speed, void *device_ctx);
static xRETURN_t fake_dcd_deinit(void *dcd_ctx);
static xRETURN_t fake_dcd_set_event_callback(void *dcd_ctx, xUSBD_DCD_Event_Callback_t callback);
static xRETURN_t fake_dcd_connect(void *dcd_ctx);
static xRETURN_t fake_dcd_disconnect(void *dcd_ctx);
static xRETURN_t fake_dcd_enable_interrupts(void *dcd_ctx);
static xRETURN_t fake_dcd_disable_interrupts(void *dcd_ctx);
static xRETURN_t fake_dcd_set_address(void *dcd_ctx, uint8_t address);
static xRETURN_t fake_dcd_set_remote_wakeup(void *dcd_ctx, bool enable);
static xRETURN_t fake_dcd_set_test_mode(void *dcd_ctx, uint8_t mode);
static uint32_t fake_dcd_get_frame_number(void *dcd_ctx);
static USB_Speed_t fake_dcd_get_speed(void *dcd_ctx);
static xRETURN_t fake_dcd_ep_init(void *dcd_ctx, uint8_t ep_addr, uint8_t ep_type, uint16_t mps);
static xRETURN_t fake_dcd_ep_deinit(void *dcd_ctx, uint8_t ep_addr);
static xRETURN_t fake_dcd_ep_receive(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t fake_dcd_ep_send(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required);
static xRETURN_t fake_dcd_ep_transfer_queue(void *dcd_ctx, const xUSBD_DCD_Transfer_t *transfer);
static xRETURN_t fake_dcd_ep_stall(void *dcd_ctx, uint8_t ep_addr);
static xRETURN_t fake_dcd_ep_clear_stall(void *dcd_ctx, uint8_t ep_addr);
static bool fake_dcd_ep_is_stalled(void *dcd_ctx, uint8_t ep_addr);
static bool is_endpoint_valid(uint8_t ep_addr);
static uint32_t endpoint_slot(uint8_t ep_addr);
static xRETURN_t emit_event(xUSBD_Fake_DCD_Context_t *fake_ctx, USB_DCD_Event_t event, uint8_t ep_addr, uint8_t *data, uint32_t length);

// INLINE FUNCTIONS ////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////
static xRETURN_t fake_dcd_init(void *dcd_ctx, USB_Speed_t speed, void *device_ctx)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");
    xASSERT(device_ctx != NULL, "device_ctx is NULL");

    if ((dcd_ctx == NULL) || (device_ctx == NULL))
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->init_count++;
    if (fake_ctx->init_return == xRETURN_OK)
    {
        fake_ctx->device_ctx = device_ctx;
        fake_ctx->speed = speed;
        fake_ctx->is_initialized = true;
    }

    return fake_ctx->init_return;
}

static xRETURN_t fake_dcd_deinit(void *dcd_ctx)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->deinit_count++;
    if (fake_ctx->deinit_return == xRETURN_OK)
    {
        fake_ctx->is_initialized = false;
        fake_ctx->is_connected = false;
        fake_ctx->are_interrupts_enabled = false;
    }

    return fake_ctx->deinit_return;
}

static xRETURN_t fake_dcd_set_event_callback(void *dcd_ctx, xUSBD_DCD_Event_Callback_t callback)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");
    xASSERT(callback != NULL, "callback is NULL");

    if ((dcd_ctx == NULL) || (callback == NULL))
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->set_event_callback_count++;
    if (fake_ctx->set_event_callback_return == xRETURN_OK)
    {
        fake_ctx->event_callback = callback;
    }

    return fake_ctx->set_event_callback_return;
}

static xRETURN_t fake_dcd_connect(void *dcd_ctx)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->connect_count++;
    if (fake_ctx->connect_return == xRETURN_OK)
    {
        fake_ctx->is_connected = true;
    }

    return fake_ctx->connect_return;
}

static xRETURN_t fake_dcd_disconnect(void *dcd_ctx)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->disconnect_count++;
    if (fake_ctx->disconnect_return == xRETURN_OK)
    {
        fake_ctx->is_connected = false;
    }

    return fake_ctx->disconnect_return;
}

static xRETURN_t fake_dcd_enable_interrupts(void *dcd_ctx)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->enable_interrupts_count++;
    if (fake_ctx->enable_interrupts_return == xRETURN_OK)
    {
        fake_ctx->are_interrupts_enabled = true;
    }

    return fake_ctx->enable_interrupts_return;
}

static xRETURN_t fake_dcd_disable_interrupts(void *dcd_ctx)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->disable_interrupts_count++;
    if (fake_ctx->disable_interrupts_return == xRETURN_OK)
    {
        fake_ctx->are_interrupts_enabled = false;
    }

    return fake_ctx->disable_interrupts_return;
}

static xRETURN_t fake_dcd_set_address(void *dcd_ctx, uint8_t address)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->set_address_count++;
    if (fake_ctx->set_address_return == xRETURN_OK)
    {
        fake_ctx->address = address;
    }

    return fake_ctx->set_address_return;
}

static xRETURN_t fake_dcd_set_remote_wakeup(void *dcd_ctx, bool enable)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->set_remote_wakeup_count++;
    if (fake_ctx->set_remote_wakeup_return == xRETURN_OK)
    {
        fake_ctx->has_remote_wakeup = enable;
    }

    return fake_ctx->set_remote_wakeup_return;
}

static xRETURN_t fake_dcd_set_test_mode(void *dcd_ctx, uint8_t mode)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->set_test_mode_count++;
    if (fake_ctx->set_test_mode_return == xRETURN_OK)
    {
        fake_ctx->test_mode = mode;
    }

    return fake_ctx->set_test_mode_return;
}

static uint32_t fake_dcd_get_frame_number(void *dcd_ctx)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return 0U;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->get_frame_number_count++;

    return fake_ctx->frame_number;
}

static USB_Speed_t fake_dcd_get_speed(void *dcd_ctx)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return USB_SPEED_FULL;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->get_speed_count++;

    return fake_ctx->speed;
}

static xRETURN_t fake_dcd_ep_init(void *dcd_ctx, uint8_t ep_addr, uint8_t ep_type, uint16_t mps)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }
    if (is_endpoint_valid(ep_addr) == false)
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->ep_init_count++;
    fake_ctx->last_ep_addr = ep_addr;
    if (fake_ctx->ep_init_return == xRETURN_OK)
    {
        uint32_t slot = endpoint_slot(ep_addr);
        fake_ctx->endpoints[slot].ep_type = ep_type;
        fake_ctx->endpoints[slot].mps = mps;
        fake_ctx->endpoints[slot].is_initialized = true;
    }

    return fake_ctx->ep_init_return;
}

static xRETURN_t fake_dcd_ep_deinit(void *dcd_ctx, uint8_t ep_addr)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }
    if (is_endpoint_valid(ep_addr) == false)
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->ep_deinit_count++;
    fake_ctx->last_ep_addr = ep_addr;
    if (fake_ctx->ep_deinit_return == xRETURN_OK)
    {
        (void)memset(&fake_ctx->endpoints[endpoint_slot(ep_addr)], 0, sizeof(fake_ctx->endpoints[0]));
    }

    return fake_ctx->ep_deinit_return;
}

static xRETURN_t fake_dcd_ep_receive(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }
    if ((data == NULL) && (length > 0U))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }
    if (is_endpoint_valid(ep_addr) == false)
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->ep_receive_count++;
    fake_ctx->last_ep_addr = ep_addr;
    if (fake_ctx->ep_receive_return == xRETURN_OK)
    {
        uint32_t slot = endpoint_slot(ep_addr);
        fake_ctx->endpoints[slot].receive_buffer = data;
        fake_ctx->endpoints[slot].receive_length = length;
        fake_ctx->endpoints[slot].is_receive_armed = true;
    }

    return fake_ctx->ep_receive_return;
}

static xRETURN_t fake_dcd_ep_send(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }
    if ((data == NULL) && (length > 0U))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }
    if (is_endpoint_valid(ep_addr) == false)
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }
    if (length > xUSBD_FAKE_DCD_PACKET_SIZE)
    {
        return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->ep_send_count++;
    fake_ctx->last_ep_addr = ep_addr;
    if (fake_ctx->ep_send_return != xRETURN_OK)
    {
        return fake_ctx->ep_send_return;
    }
    if (fake_ctx->tx_count >= xUSBD_FAKE_DCD_TX_QUEUE_DEPTH)
    {
        return xRETURN_xERR_xUSBD_RESOURCE_EXHAUSTED;
    }

    xUSBD_Fake_DCD_Packet_t *packet = &fake_ctx->tx_queue[fake_ctx->tx_write_idx];
    packet->ep_addr = ep_addr;
    packet->length = length;
    packet->is_zlp_required = is_zlp_required;
    if (length > 0U)
    {
        (void)memcpy(packet->data, data, length);
    }
    fake_ctx->tx_write_idx = (fake_ctx->tx_write_idx + 1U) % xUSBD_FAKE_DCD_TX_QUEUE_DEPTH;
    fake_ctx->tx_count++;

    return xRETURN_OK;
}

static xRETURN_t fake_dcd_ep_transfer_queue(void *dcd_ctx, const xUSBD_DCD_Transfer_t *transfer)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");
    xASSERT(transfer != NULL, "transfer is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }
    if (transfer == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }
    if (is_endpoint_valid(transfer->ep_addr) == false)
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->ep_transfer_queue_count++;
    fake_ctx->last_ep_addr = transfer->ep_addr;
    if (fake_ctx->ep_transfer_queue_return != xRETURN_OK)
    {
        return fake_ctx->ep_transfer_queue_return;
    }
    if (fake_ctx->transfer_count >= xUSBD_FAKE_DCD_TRANSFER_QUEUE_DEPTH)
    {
        return xRETURN_xERR_xUSBD_RESOURCE_EXHAUSTED;
    }

    fake_ctx->transfer_queue[fake_ctx->transfer_write_idx] = transfer;
    fake_ctx->transfer_write_idx = (fake_ctx->transfer_write_idx + 1U) % xUSBD_FAKE_DCD_TRANSFER_QUEUE_DEPTH;
    fake_ctx->transfer_count++;

    return xRETURN_OK;
}

static xRETURN_t fake_dcd_ep_stall(void *dcd_ctx, uint8_t ep_addr)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }
    if (is_endpoint_valid(ep_addr) == false)
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->ep_stall_count++;
    fake_ctx->last_ep_addr = ep_addr;
    if (fake_ctx->ep_stall_return == xRETURN_OK)
    {
        fake_ctx->endpoints[endpoint_slot(ep_addr)].is_stalled = true;
    }

    return fake_ctx->ep_stall_return;
}

static xRETURN_t fake_dcd_ep_clear_stall(void *dcd_ctx, uint8_t ep_addr)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if (dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }
    if (is_endpoint_valid(ep_addr) == false)
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->ep_clear_stall_count++;
    fake_ctx->last_ep_addr = ep_addr;
    if (fake_ctx->ep_clear_stall_return == xRETURN_OK)
    {
        fake_ctx->endpoints[endpoint_slot(ep_addr)].is_stalled = false;
    }

    return fake_ctx->ep_clear_stall_return;
}

static bool fake_dcd_ep_is_stalled(void *dcd_ctx, uint8_t ep_addr)
{
    xASSERT(dcd_ctx != NULL, "dcd_ctx is NULL");

    if ((dcd_ctx == NULL) || (is_endpoint_valid(ep_addr) == false))
    {
        return false;
    }

    xUSBD_Fake_DCD_Context_t *fake_ctx = (xUSBD_Fake_DCD_Context_t *)dcd_ctx;
    fake_ctx->ep_is_stalled_count++;
    fake_ctx->last_ep_addr = ep_addr;

    return fake_ctx->endpoints[endpoint_slot(ep_addr)].is_stalled;
}

static bool is_endpoint_valid(uint8_t ep_addr)
{
    return (ep_addr & 0x70U) == 0U;
}

static uint32_t endpoint_slot(uint8_t ep_addr)
{
    uint32_t slot = (uint32_t)(ep_addr & USB_ENDP_ADDR_MASK);
    if ((ep_addr & USB_ENDP_DIR_MASK) != 0U)
    {
        slot += 16U;
    }

    return slot;
}

static xRETURN_t emit_event(xUSBD_Fake_DCD_Context_t *fake_ctx, USB_DCD_Event_t event, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    if ((fake_ctx->event_callback == NULL) || (fake_ctx->device_ctx == NULL) || (fake_ctx->are_interrupts_enabled == false))
    {
        return xRETURN_xERR_xUSBD_NOT_INITIALIZED;
    }

    fake_ctx->event_callback(fake_ctx->device_ctx, event, ep_addr, data, length);

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////
xUSBD_DCD_Ops_t xUSBD_Fake_DCD_Ops = {
    .init = fake_dcd_init,
    .deinit = fake_dcd_deinit,
    .set_event_callback = fake_dcd_set_event_callback,
    .connect = fake_dcd_connect,
    .disconnect = fake_dcd_disconnect,
    .enable_interrupts = fake_dcd_enable_interrupts,
    .disable_interrupts = fake_dcd_disable_interrupts,
    .set_address = fake_dcd_set_address,
    .set_remote_wakeup = fake_dcd_set_remote_wakeup,
    .set_test_mode = fake_dcd_set_test_mode,
    .get_frame_number = fake_dcd_get_frame_number,
    .get_speed = fake_dcd_get_speed,
    .ep_init = fake_dcd_ep_init,
    .ep_deinit = fake_dcd_ep_deinit,
    .ep_receive = fake_dcd_ep_receive,
    .ep_send = fake_dcd_ep_send,
    .ep_transfer_queue = fake_dcd_ep_transfer_queue,
    .ep_stall = fake_dcd_ep_stall,
    .ep_clear_stall = fake_dcd_ep_clear_stall,
    .ep_is_stalled = fake_dcd_ep_is_stalled,
};

xRETURN_t xUSBD_Fake_DCD_Init(xUSBD_Fake_DCD_Context_t *fake_ctx)
{
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");

    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    (void)memset(fake_ctx, 0, sizeof(*fake_ctx));
    fake_ctx->speed = USB_SPEED_HIGH;
    fake_ctx->frame_number = xUSBD_FAKE_DCD_DEFAULT_FRAME_NUMBER;
    fake_ctx->init_return = xRETURN_OK;
    fake_ctx->deinit_return = xRETURN_OK;
    fake_ctx->set_event_callback_return = xRETURN_OK;
    fake_ctx->connect_return = xRETURN_OK;
    fake_ctx->disconnect_return = xRETURN_OK;
    fake_ctx->enable_interrupts_return = xRETURN_OK;
    fake_ctx->disable_interrupts_return = xRETURN_OK;
    fake_ctx->set_address_return = xRETURN_OK;
    fake_ctx->set_remote_wakeup_return = xRETURN_OK;
    fake_ctx->set_test_mode_return = xRETURN_OK;
    fake_ctx->ep_init_return = xRETURN_OK;
    fake_ctx->ep_deinit_return = xRETURN_OK;
    fake_ctx->ep_receive_return = xRETURN_OK;
    fake_ctx->ep_send_return = xRETURN_OK;
    fake_ctx->ep_transfer_queue_return = xRETURN_OK;
    fake_ctx->ep_stall_return = xRETURN_OK;
    fake_ctx->ep_clear_stall_return = xRETURN_OK;

    return xRETURN_OK;
}

xRETURN_t
xUSBD_Fake_DCD_Fire_Event(xUSBD_Fake_DCD_Context_t *fake_ctx, USB_DCD_Event_t event, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");

    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }
    if (is_endpoint_valid(ep_addr) == false)
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }
    if ((data == NULL) && (length > 0U))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    return emit_event(fake_ctx, event, ep_addr, data, length);
}

xRETURN_t xUSBD_Fake_DCD_RX(xUSBD_Fake_DCD_Context_t *fake_ctx, uint8_t ep_addr, const uint8_t *data, uint32_t length)
{
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");

    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }
    if ((data == NULL) && (length > 0U))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }
    if (is_endpoint_valid(ep_addr) == false)
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }

    uint32_t slot = endpoint_slot(ep_addr);
    if (fake_ctx->endpoints[slot].is_receive_armed == false)
    {
        return xRETURN_xERR_xUSBD_DCD_NO_BUFFER_AVA;
    }
    if (length > fake_ctx->endpoints[slot].receive_length)
    {
        return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
    }
    if (length > 0U)
    {
        (void)memcpy(fake_ctx->endpoints[slot].receive_buffer, data, length);
    }

    fake_ctx->endpoints[slot].is_receive_armed = false;

    return emit_event(fake_ctx, USB_DCD_DATA_RECEIVED, ep_addr, fake_ctx->endpoints[slot].receive_buffer, length);
}

xRETURN_t xUSBD_Fake_DCD_TX_Pop(xUSBD_Fake_DCD_Context_t *fake_ctx,
                                uint8_t *ep_addr,
                                uint8_t *data,
                                uint32_t data_size,
                                uint32_t *length,
                                bool *is_zlp_required)
{
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");
    xASSERT(ep_addr != NULL, "ep_addr is NULL");
    xASSERT(data != NULL, "data is NULL");
    xASSERT(length != NULL, "length is NULL");
    xASSERT(is_zlp_required != NULL, "is_zlp_required is NULL");

    if ((fake_ctx == NULL) || (ep_addr == NULL) || (data == NULL) || (length == NULL) || (is_zlp_required == NULL))
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }
    if (fake_ctx->tx_count == 0U)
    {
        return xRETURN_xERR_xUSBD_DCD_NO_BUFFER_AVA;
    }

    xUSBD_Fake_DCD_Packet_t *packet = &fake_ctx->tx_queue[fake_ctx->tx_read_idx];
    if (data_size < packet->length)
    {
        *length = packet->length;
        return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
    }

    *ep_addr = packet->ep_addr;
    *length = packet->length;
    *is_zlp_required = packet->is_zlp_required;
    if (packet->length > 0U)
    {
        (void)memcpy(data, packet->data, packet->length);
    }
    (void)memset(packet, 0, sizeof(*packet));
    fake_ctx->tx_read_idx = (fake_ctx->tx_read_idx + 1U) % xUSBD_FAKE_DCD_TX_QUEUE_DEPTH;
    fake_ctx->tx_count--;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Fake_DCD_Transfer_Pop(xUSBD_Fake_DCD_Context_t *fake_ctx, const xUSBD_DCD_Transfer_t **transfer)
{
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");
    xASSERT(transfer != NULL, "transfer is NULL");

    if ((fake_ctx == NULL) || (transfer == NULL))
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }
    if (fake_ctx->transfer_count == 0U)
    {
        return xRETURN_xERR_xUSBD_DCD_NO_BUFFER_AVA;
    }

    *transfer = fake_ctx->transfer_queue[fake_ctx->transfer_read_idx];
    fake_ctx->transfer_queue[fake_ctx->transfer_read_idx] = NULL;
    fake_ctx->transfer_read_idx = (fake_ctx->transfer_read_idx + 1U) % xUSBD_FAKE_DCD_TRANSFER_QUEUE_DEPTH;
    fake_ctx->transfer_count--;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Fake_DCD_Complete_Transfer(xUSBD_Fake_DCD_Context_t *fake_ctx, xRETURN_t status, uint32_t actual_length)
{
    const xUSBD_DCD_Transfer_t *transfer = NULL;
    xRETURN_t ret = xUSBD_Fake_DCD_Transfer_Pop(fake_ctx, &transfer);
    if (ret != xRETURN_OK)
    {
        return ret;
    }

    if (transfer->complete != NULL)
    {
        transfer->complete(transfer->user_ctx, transfer, status, actual_length);
    }

    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
