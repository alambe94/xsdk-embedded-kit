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

// @file xuart_fake.c
// @brief Host-test fake port for the xUART driver core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xuart_fake.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xUART_Fake_Context_t *as_fake_context(void *driver_ctx);

static xRETURN_t fake_init(void *driver_ctx, const xUART_Config_t *config);
static xRETURN_t fake_deinit(void *driver_ctx);
static xRETURN_t fake_start(void *driver_ctx);
static xRETURN_t fake_stop(void *driver_ctx);
static xRETURN_t fake_set_event_callback(void *driver_ctx, xUART_Driver_Event_Callback_t callback, void *callback_ctx);
static xRETURN_t fake_transmit(void *driver_ctx, const uint8_t *buffer, uint32_t length, uint32_t timeout_ms);
static xRETURN_t fake_receive(void *driver_ctx, uint8_t *buffer, uint32_t length, uint32_t timeout_ms);
static xRETURN_t fake_transmit_async(void *driver_ctx, const uint8_t *buffer, uint32_t length);
static xRETURN_t fake_receive_async(void *driver_ctx, uint8_t *buffer, uint32_t length);
static xRETURN_t fake_abort_tx(void *driver_ctx);
static xRETURN_t fake_abort_rx(void *driver_ctx);

const xUART_Driver_Ops_t xUART_Fake_Driver_Ops = {
    .init               = fake_init,
    .deinit             = fake_deinit,
    .start              = fake_start,
    .stop               = fake_stop,
    .set_event_callback = fake_set_event_callback,
    .transmit           = fake_transmit,
    .receive            = fake_receive,
    .transmit_async     = fake_transmit_async,
    .receive_async      = fake_receive_async,
    .abort_tx           = fake_abort_tx,
    .abort_rx           = fake_abort_rx,
};

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xUART_Fake_Context_t *as_fake_context(void *driver_ctx)
{
    return (xUART_Fake_Context_t *)driver_ctx;
}

static xRETURN_t fake_init(void *driver_ctx, const xUART_Config_t *config)
{
    xUART_Fake_Context_t *fake_ctx;

    if ((driver_ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->init_count++;

    if (fake_ctx->next_init_status != xRETURN_OK)
    {
        return fake_ctx->next_init_status;
    }

    fake_ctx->is_initialized = true;

    return xRETURN_OK;
}

static xRETURN_t fake_deinit(void *driver_ctx)
{
    xUART_Fake_Context_t *fake_ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->deinit_count++;

    if (fake_ctx->next_deinit_status != xRETURN_OK)
    {
        return fake_ctx->next_deinit_status;
    }

    fake_ctx->is_initialized = false;
    fake_ctx->is_started     = false;
    fake_ctx->is_tx_busy     = false;
    fake_ctx->is_rx_busy     = false;

    return xRETURN_OK;
}

static xRETURN_t fake_start(void *driver_ctx)
{
    xUART_Fake_Context_t *fake_ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->start_count++;

    if (fake_ctx->next_start_status != xRETURN_OK)
    {
        return fake_ctx->next_start_status;
    }

    fake_ctx->is_started = true;

    return xRETURN_OK;
}

static xRETURN_t fake_stop(void *driver_ctx)
{
    xUART_Fake_Context_t *fake_ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->stop_count++;

    if (fake_ctx->next_stop_status != xRETURN_OK)
    {
        return fake_ctx->next_stop_status;
    }

    fake_ctx->is_started = false;

    return xRETURN_OK;
}

static xRETURN_t fake_set_event_callback(void *driver_ctx, xUART_Driver_Event_Callback_t callback, void *callback_ctx)
{
    xUART_Fake_Context_t *fake_ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->event_callback     = callback;
    fake_ctx->event_callback_ctx = callback_ctx;

    return xRETURN_OK;
}

static xRETURN_t fake_transmit(void *driver_ctx, const uint8_t *buffer, uint32_t length, uint32_t timeout_ms)
{
    xUART_Fake_Context_t *fake_ctx;

    (void)timeout_ms;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->transmit_count++;
    fake_ctx->last_tx_buffer = buffer;
    fake_ctx->last_tx_length = length;

    if (fake_ctx->next_transmit_status != xRETURN_OK)
    {
        return fake_ctx->next_transmit_status;
    }

    return xRETURN_OK;
}

static xRETURN_t fake_receive(void *driver_ctx, uint8_t *buffer, uint32_t length, uint32_t timeout_ms)
{
    xUART_Fake_Context_t *fake_ctx;
    uint32_t              i;

    (void)timeout_ms;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->receive_count++;
    fake_ctx->last_rx_buffer = buffer;
    fake_ctx->last_rx_length = length;

    if (fake_ctx->next_receive_status != xRETURN_OK)
    {
        return fake_ctx->next_receive_status;
    }

    for (i = 0U; i < length; i++)
    {
        buffer[i] = xUART_FAKE_RX_FILL_BYTE;
    }

    return xRETURN_OK;
}

static xRETURN_t fake_transmit_async(void *driver_ctx, const uint8_t *buffer, uint32_t length)
{
    xUART_Fake_Context_t *fake_ctx;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->transmit_async_count++;
    fake_ctx->last_tx_buffer = buffer;
    fake_ctx->last_tx_length = length;
    fake_ctx->is_tx_busy     = true;

    if (fake_ctx->next_transmit_async_status != xRETURN_OK)
    {
        fake_ctx->is_tx_busy = false;
        return fake_ctx->next_transmit_async_status;
    }

    return xRETURN_OK;
}

static xRETURN_t fake_receive_async(void *driver_ctx, uint8_t *buffer, uint32_t length)
{
    xUART_Fake_Context_t *fake_ctx;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->receive_async_count++;
    fake_ctx->last_rx_buffer = buffer;
    fake_ctx->last_rx_length = length;
    fake_ctx->is_rx_busy     = true;

    if (fake_ctx->next_receive_async_status != xRETURN_OK)
    {
        fake_ctx->is_rx_busy = false;
        return fake_ctx->next_receive_async_status;
    }

    return xRETURN_OK;
}

static xRETURN_t fake_abort_tx(void *driver_ctx)
{
    xUART_Fake_Context_t *fake_ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->abort_tx_count++;

    if (fake_ctx->next_abort_tx_status != xRETURN_OK)
    {
        return fake_ctx->next_abort_tx_status;
    }

    if ((fake_ctx->is_tx_busy) && (fake_ctx->event_callback != NULL))
    {
        xUART_Event_Info_t info = {0U, xRETURN_xERR_xUART_ABORTED};
        fake_ctx->is_tx_busy = false;
        fake_ctx->event_callback(fake_ctx->event_callback_ctx, xUART_EVENT_TX_ABORTED, &info);
    }

    return xRETURN_OK;
}

static xRETURN_t fake_abort_rx(void *driver_ctx)
{
    xUART_Fake_Context_t *fake_ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->abort_rx_count++;

    if (fake_ctx->next_abort_rx_status != xRETURN_OK)
    {
        return fake_ctx->next_abort_rx_status;
    }

    if ((fake_ctx->is_rx_busy) && (fake_ctx->event_callback != NULL))
    {
        xUART_Event_Info_t info = {0U, xRETURN_xERR_xUART_ABORTED};
        fake_ctx->is_rx_busy = false;
        fake_ctx->event_callback(fake_ctx->event_callback_ctx, xUART_EVENT_RX_ABORTED, &info);
    }

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xUART_Fake_Context_Init(xUART_Fake_Context_t *fake_ctx)
{
    if (fake_ctx != NULL)
    {
        (void)memset(fake_ctx, 0, sizeof(*fake_ctx));
    }
}

void xUART_Fake_Fire_Event(xUART_Fake_Context_t *fake_ctx, xUART_Event_t event, const xUART_Event_Info_t *event_info)
{
    if ((fake_ctx != NULL) && (fake_ctx->event_callback != NULL))
    {
        fake_ctx->event_callback(fake_ctx->event_callback_ctx, event, event_info);
    }
}

void xUART_Fake_Fire_Tx_Complete(xUART_Fake_Context_t *fake_ctx, uint32_t bytes_transferred)
{
    if (fake_ctx != NULL)
    {
        fake_ctx->is_tx_busy = false;
        xUART_Event_Info_t info = {bytes_transferred, xRETURN_OK};
        xUART_Fake_Fire_Event(fake_ctx, xUART_EVENT_TX_COMPLETE, &info);
    }
}

void xUART_Fake_Fire_Rx_Complete(xUART_Fake_Context_t *fake_ctx, uint32_t bytes_transferred)
{
    if (fake_ctx != NULL)
    {
        fake_ctx->is_rx_busy = false;
        xUART_Event_Info_t info = {bytes_transferred, xRETURN_OK};
        xUART_Fake_Fire_Event(fake_ctx, xUART_EVENT_RX_COMPLETE, &info);
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
