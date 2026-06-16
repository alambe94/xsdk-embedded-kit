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

// @file xuart.c
// @brief Portable xUART controller core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xassert.h"
#include "xuart.h"
#include "xuart_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static bool ops_are_valid(const xUART_Start_Config_t *start_config);
static void core_event_sink(void *callback_ctx, xUART_Event_t event, const xUART_Event_Info_t *event_info);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static bool ops_are_valid(const xUART_Start_Config_t *start_config)
{
    if ((start_config->drv_ops == NULL) || (start_config->drv_ctx == NULL))
    {
        return false;
    }

    if ((start_config->drv_ops->init == NULL) || (start_config->drv_ops->deinit == NULL) || (start_config->drv_ops->start == NULL) ||
        (start_config->drv_ops->stop == NULL) || (start_config->drv_ops->transmit == NULL) || (start_config->drv_ops->receive == NULL))
    {
        return false;
    }

    return true;
}

static void core_event_sink(void *callback_ctx, xUART_Event_t event, const xUART_Event_Info_t *event_info)
{
    xUART_Context_t *uart_ctx = (xUART_Context_t *)callback_ctx;

    if (uart_ctx == NULL)
    {
        return;
    }

    xUART_LOG(xRETURN_OK, "event_sink: event=%u", (unsigned)event);

    switch (event)
    {
    case xUART_EVENT_TX_COMPLETE:
        uart_ctx->is_tx_busy = false;
        uart_ctx->last_tx_error = (event_info != NULL) ? event_info->error_code : xRETURN_OK;
        xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_TX_COMPLETE, uart_ctx->port, (event_info != NULL) ? event_info->bytes_transferred : 0U);
        break;

    case xUART_EVENT_TX_ABORTED:
        uart_ctx->is_tx_busy = false;
        uart_ctx->last_tx_error = (event_info != NULL) ? event_info->error_code : xRETURN_OK;
        xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_TX_ABORTED, uart_ctx->port, (event_info != NULL) ? event_info->bytes_transferred : 0U);
        break;

    case xUART_EVENT_TX_TIMEOUT:
        uart_ctx->is_tx_busy = false;
        uart_ctx->last_tx_error = (event_info != NULL) ? event_info->error_code : xRETURN_OK;
        xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_TX_TIMEOUT, uart_ctx->port, (event_info != NULL) ? event_info->bytes_transferred : 0U);
        break;

    case xUART_EVENT_RX_COMPLETE:
        uart_ctx->is_rx_busy = false;
        uart_ctx->last_rx_error = (event_info != NULL) ? event_info->error_code : xRETURN_OK;
        xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_RX_COMPLETE, uart_ctx->port, (event_info != NULL) ? event_info->bytes_transferred : 0U);
        break;

    case xUART_EVENT_RX_ABORTED:
        uart_ctx->is_rx_busy = false;
        uart_ctx->last_rx_error = (event_info != NULL) ? event_info->error_code : xRETURN_OK;
        xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_RX_ABORTED, uart_ctx->port, (event_info != NULL) ? event_info->bytes_transferred : 0U);
        break;

    case xUART_EVENT_RX_TIMEOUT:
        uart_ctx->is_rx_busy = false;
        uart_ctx->last_rx_error = (event_info != NULL) ? event_info->error_code : xRETURN_OK;
        xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_RX_TIMEOUT, uart_ctx->port, (event_info != NULL) ? event_info->bytes_transferred : 0U);
        break;

    case xUART_EVENT_RX_OVERRUN:
        uart_ctx->is_rx_busy = false;
        uart_ctx->last_rx_error = (event_info != NULL) ? event_info->error_code : xRETURN_OK;
        xUART_TRACE_E1(uart_ctx, xUART_TRACE_CODE_RX_OVERRUN, uart_ctx->port);
        break;

    case xUART_EVENT_RX_FRAMING:
        uart_ctx->is_rx_busy = false;
        uart_ctx->last_rx_error = (event_info != NULL) ? event_info->error_code : xRETURN_OK;
        xUART_TRACE_E1(uart_ctx, xUART_TRACE_CODE_RX_FRAMING, uart_ctx->port);
        break;

    case xUART_EVENT_RX_PARITY:
        uart_ctx->is_rx_busy = false;
        uart_ctx->last_rx_error = (event_info != NULL) ? event_info->error_code : xRETURN_OK;
        xUART_TRACE_E1(uart_ctx, xUART_TRACE_CODE_RX_PARITY, uart_ctx->port);
        break;

    default:
        break;
    }

    if (uart_ctx->config.callbacks.on_event != NULL)
    {
        uart_ctx->config.callbacks.on_event(uart_ctx, event, event_info);
    }
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xUART_Init(xUART_Context_t *uart_ctx, const xUART_Config_t *config)
{
    if ((uart_ctx == NULL) || (config == NULL))
    {
        xUART_LOG(xRETURN_xERR_xUART_NULL_POINTER, "Init: null pointer");
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    (void)memset(uart_ctx, 0, sizeof(*uart_ctx));
    uart_ctx->config = *config;
    uart_ctx->is_initialized = true;

    xUART_TRACE_E1(uart_ctx, xUART_TRACE_CODE_INIT, config->baud_rate);
    xUART_LOG(xRETURN_OK, "Init: ok baud=%u", (unsigned)config->baud_rate);

    return xRETURN_OK;
}

xRETURN_t xUART_Deinit(xUART_Context_t *uart_ctx)
{
    xRETURN_t status;

    if (uart_ctx == NULL)
    {
        xUART_LOG(xRETURN_xERR_xUART_NULL_POINTER, "Deinit: null context");
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    if (!uart_ctx->is_initialized)
    {
        xUART_LOG(xRETURN_xERR_xUART_NOT_INITIALIZED, "Deinit: not initialized");
        return xRETURN_xERR_xUART_NOT_INITIALIZED;
    }

    if (uart_ctx->is_tx_busy)
    {
        xUART_LOG(xRETURN_xERR_xUART_TX_BUSY, "Deinit: TX busy");
        return xRETURN_xERR_xUART_TX_BUSY;
    }

    if (uart_ctx->is_rx_busy)
    {
        xUART_LOG(xRETURN_xERR_xUART_RX_BUSY, "Deinit: RX busy");
        return xRETURN_xERR_xUART_RX_BUSY;
    }

    if (uart_ctx->is_started && (uart_ctx->ops != NULL))
    {
        status = uart_ctx->ops->deinit(uart_ctx->driver_ctx);
        if (status != xRETURN_OK)
        {
            xUART_LOG(status, "Deinit: port deinit failed");
            return status;
        }
    }

    xUART_TRACE_E1(uart_ctx, xUART_TRACE_CODE_DEINIT, (uint32_t)uart_ctx->port);
    (void)memset(uart_ctx, 0, sizeof(*uart_ctx));

    xUART_LOG(xRETURN_OK, "Deinit: ok");

    return xRETURN_OK;
}

xRETURN_t xUART_Start(xUART_Context_t *uart_ctx, const xUART_Start_Config_t *start_config)
{
    xRETURN_t status;

    if ((uart_ctx == NULL) || (start_config == NULL))
    {
        xUART_LOG(xRETURN_xERR_xUART_NULL_POINTER, "Start: null pointer");
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    if (!uart_ctx->is_initialized)
    {
        xUART_LOG(xRETURN_xERR_xUART_NOT_INITIALIZED, "Start: not initialized");
        return xRETURN_xERR_xUART_NOT_INITIALIZED;
    }

    if (uart_ctx->is_started)
    {
        xUART_LOG(xRETURN_xERR_xUART_INVALID_STATE, "Start: already started");
        return xRETURN_xERR_xUART_INVALID_STATE;
    }

    if (!ops_are_valid(start_config))
    {
        xUART_LOG(xRETURN_xERR_xUART_INVALID_ARG, "Start: invalid ops");
        return xRETURN_xERR_xUART_INVALID_ARG;
    }

    if (start_config->drv_ops->set_event_callback != NULL)
    {
        status = start_config->drv_ops->set_event_callback(start_config->drv_ctx, core_event_sink, uart_ctx);
        if (status != xRETURN_OK)
        {
            xUART_LOG(status, "Start: set_event_callback failed");
            return status;
        }
    }

    status = start_config->drv_ops->init(start_config->drv_ctx, &uart_ctx->config);
    if (status != xRETURN_OK)
    {
        xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_ERROR, start_config->port, status);
        xUART_LOG(status, "Start: port init failed");
        return status;
    }

    status = start_config->drv_ops->start(start_config->drv_ctx);
    if (status != xRETURN_OK)
    {
        xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_ERROR, start_config->port, status);
        xUART_LOG(status, "Start: port start failed");
        (void)start_config->drv_ops->deinit(start_config->drv_ctx);
        return status;
    }

    uart_ctx->ops = start_config->drv_ops;
    uart_ctx->driver_ctx = start_config->drv_ctx;
    uart_ctx->port = start_config->port;
    uart_ctx->is_started = true;

    xUART_TRACE_E1(uart_ctx, xUART_TRACE_CODE_START, start_config->port);
    xUART_LOG(xRETURN_OK, "Start: ok port=%u", (unsigned)start_config->port);

    return xRETURN_OK;
}

xRETURN_t xUART_Stop(xUART_Context_t *uart_ctx)
{
    xRETURN_t status;

    if (uart_ctx == NULL)
    {
        xUART_LOG(xRETURN_xERR_xUART_NULL_POINTER, "Stop: null context");
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    if (!uart_ctx->is_started)
    {
        xUART_LOG(xRETURN_xERR_xUART_NOT_STARTED, "Stop: not started");
        return xRETURN_xERR_xUART_NOT_STARTED;
    }

    if (uart_ctx->is_tx_busy)
    {
        xUART_LOG(xRETURN_xERR_xUART_TX_BUSY, "Stop: TX busy");
        return xRETURN_xERR_xUART_TX_BUSY;
    }

    if (uart_ctx->is_rx_busy)
    {
        xUART_LOG(xRETURN_xERR_xUART_RX_BUSY, "Stop: RX busy");
        return xRETURN_xERR_xUART_RX_BUSY;
    }

    status = uart_ctx->ops->stop(uart_ctx->driver_ctx);
    if (status != xRETURN_OK)
    {
        xUART_LOG(status, "Stop: port stop failed");
        return status;
    }

    xUART_TRACE_E1(uart_ctx, xUART_TRACE_CODE_STOP, uart_ctx->port);
    uart_ctx->ops = NULL;
    uart_ctx->driver_ctx = NULL;
    uart_ctx->is_started = false;

    xUART_LOG(xRETURN_OK, "Stop: ok");

    return xRETURN_OK;
}

xRETURN_t xUART_Transmit(xUART_Context_t *uart_ctx, const uint8_t *buffer, uint32_t length, uint32_t timeout_ms)
{
    xRETURN_t status;

    if ((uart_ctx == NULL) || (buffer == NULL))
    {
        xUART_LOG(xRETURN_xERR_xUART_NULL_POINTER, "Transmit: null pointer");
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    if (length == 0U)
    {
        xUART_LOG(xRETURN_xERR_xUART_INVALID_ARG, "Transmit: zero length");
        return xRETURN_xERR_xUART_INVALID_ARG;
    }

    if (!uart_ctx->is_started)
    {
        xUART_LOG(xRETURN_xERR_xUART_NOT_STARTED, "Transmit: not started");
        return xRETURN_xERR_xUART_NOT_STARTED;
    }

    if (uart_ctx->is_tx_busy)
    {
        xUART_LOG(xRETURN_xERR_xUART_TX_BUSY, "Transmit: TX busy");
        return xRETURN_xERR_xUART_TX_BUSY;
    }

    uart_ctx->is_tx_busy = true;
    xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_TX_START, uart_ctx->port, length);
    xUART_LOG(xRETURN_OK, "Transmit: start len=%u", (unsigned)length);

    status = uart_ctx->ops->transmit(uart_ctx->driver_ctx, buffer, length, timeout_ms);

    uart_ctx->is_tx_busy = false;
    uart_ctx->last_tx_error = status;
    xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_TX_DONE, uart_ctx->port, status);

    if (status != xRETURN_OK)
    {
        xUART_LOG(status, "Transmit: port error");
    }

    return status;
}

xRETURN_t xUART_Receive(xUART_Context_t *uart_ctx, uint8_t *buffer, uint32_t length, uint32_t timeout_ms)
{
    xRETURN_t status;

    if ((uart_ctx == NULL) || (buffer == NULL))
    {
        xUART_LOG(xRETURN_xERR_xUART_NULL_POINTER, "Receive: null pointer");
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    if (length == 0U)
    {
        xUART_LOG(xRETURN_xERR_xUART_INVALID_ARG, "Receive: zero length");
        return xRETURN_xERR_xUART_INVALID_ARG;
    }

    if (!uart_ctx->is_started)
    {
        xUART_LOG(xRETURN_xERR_xUART_NOT_STARTED, "Receive: not started");
        return xRETURN_xERR_xUART_NOT_STARTED;
    }

    if (uart_ctx->is_rx_busy)
    {
        xUART_LOG(xRETURN_xERR_xUART_RX_BUSY, "Receive: RX busy");
        return xRETURN_xERR_xUART_RX_BUSY;
    }

    uart_ctx->is_rx_busy = true;
    xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_RX_START, uart_ctx->port, length);
    xUART_LOG(xRETURN_OK, "Receive: start len=%u", (unsigned)length);

    status = uart_ctx->ops->receive(uart_ctx->driver_ctx, buffer, length, timeout_ms);

    uart_ctx->is_rx_busy = false;
    uart_ctx->last_rx_error = status;
    xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_RX_DONE, uart_ctx->port, status);

    if (status != xRETURN_OK)
    {
        xUART_LOG(status, "Receive: port error");
    }

    return status;
}

xRETURN_t xUART_Transmit_Async(xUART_Context_t *uart_ctx, const uint8_t *buffer, uint32_t length)
{
    xRETURN_t status;

    if ((uart_ctx == NULL) || (buffer == NULL))
    {
        xUART_LOG(xRETURN_xERR_xUART_NULL_POINTER, "Transmit_Async: null pointer");
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    if (length == 0U)
    {
        xUART_LOG(xRETURN_xERR_xUART_INVALID_ARG, "Transmit_Async: zero length");
        return xRETURN_xERR_xUART_INVALID_ARG;
    }

    if (!uart_ctx->is_started)
    {
        xUART_LOG(xRETURN_xERR_xUART_NOT_STARTED, "Transmit_Async: not started");
        return xRETURN_xERR_xUART_NOT_STARTED;
    }

    if (uart_ctx->is_tx_busy)
    {
        xUART_LOG(xRETURN_xERR_xUART_TX_BUSY, "Transmit_Async: TX busy");
        return xRETURN_xERR_xUART_TX_BUSY;
    }

    if (uart_ctx->ops->transmit_async == NULL)
    {
        xUART_LOG(xRETURN_xERR_xUART_UNSUPPORTED, "Transmit_Async: not supported by port");
        return xRETURN_xERR_xUART_UNSUPPORTED;
    }

    uart_ctx->is_tx_busy = true;
    xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_TX_START, uart_ctx->port, length);
    xUART_LOG(xRETURN_OK, "Transmit_Async: start len=%u", (unsigned)length);

    status = uart_ctx->ops->transmit_async(uart_ctx->driver_ctx, buffer, length);
    if (status != xRETURN_OK)
    {
        uart_ctx->is_tx_busy = false;
        xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_ERROR, uart_ctx->port, status);
        xUART_LOG(status, "Transmit_Async: port error");
    }

    return status;
}

xRETURN_t xUART_Receive_Async(xUART_Context_t *uart_ctx, uint8_t *buffer, uint32_t length)
{
    xRETURN_t status;

    if ((uart_ctx == NULL) || (buffer == NULL))
    {
        xUART_LOG(xRETURN_xERR_xUART_NULL_POINTER, "Receive_Async: null pointer");
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    if (length == 0U)
    {
        xUART_LOG(xRETURN_xERR_xUART_INVALID_ARG, "Receive_Async: zero length");
        return xRETURN_xERR_xUART_INVALID_ARG;
    }

    if (!uart_ctx->is_started)
    {
        xUART_LOG(xRETURN_xERR_xUART_NOT_STARTED, "Receive_Async: not started");
        return xRETURN_xERR_xUART_NOT_STARTED;
    }

    if (uart_ctx->is_rx_busy)
    {
        xUART_LOG(xRETURN_xERR_xUART_RX_BUSY, "Receive_Async: RX busy");
        return xRETURN_xERR_xUART_RX_BUSY;
    }

    if (uart_ctx->ops->receive_async == NULL)
    {
        xUART_LOG(xRETURN_xERR_xUART_UNSUPPORTED, "Receive_Async: not supported by port");
        return xRETURN_xERR_xUART_UNSUPPORTED;
    }

    uart_ctx->is_rx_busy = true;
    xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_RX_START, uart_ctx->port, length);
    xUART_LOG(xRETURN_OK, "Receive_Async: start len=%u", (unsigned)length);

    status = uart_ctx->ops->receive_async(uart_ctx->driver_ctx, buffer, length);
    if (status != xRETURN_OK)
    {
        uart_ctx->is_rx_busy = false;
        xUART_TRACE_E2(uart_ctx, xUART_TRACE_CODE_ERROR, uart_ctx->port, status);
        xUART_LOG(status, "Receive_Async: port error");
    }

    return status;
}

xRETURN_t xUART_Abort_Tx(xUART_Context_t *uart_ctx)
{
    if (uart_ctx == NULL)
    {
        xUART_LOG(xRETURN_xERR_xUART_NULL_POINTER, "Abort_Tx: null context");
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    if (!uart_ctx->is_started)
    {
        xUART_LOG(xRETURN_xERR_xUART_NOT_STARTED, "Abort_Tx: not started");
        return xRETURN_xERR_xUART_NOT_STARTED;
    }

    if (!uart_ctx->is_tx_busy)
    {
        return xRETURN_OK;
    }

    if (uart_ctx->ops->abort_tx == NULL)
    {
        xUART_LOG(xRETURN_xERR_xUART_UNSUPPORTED, "Abort_Tx: not supported by port");
        return xRETURN_xERR_xUART_UNSUPPORTED;
    }

    xUART_TRACE_E1(uart_ctx, xUART_TRACE_CODE_ABORT_TX, uart_ctx->port);
    xUART_LOG(xRETURN_OK, "Abort_Tx");

    return uart_ctx->ops->abort_tx(uart_ctx->driver_ctx);
}

xRETURN_t xUART_Abort_Rx(xUART_Context_t *uart_ctx)
{
    if (uart_ctx == NULL)
    {
        xUART_LOG(xRETURN_xERR_xUART_NULL_POINTER, "Abort_Rx: null context");
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    if (!uart_ctx->is_started)
    {
        xUART_LOG(xRETURN_xERR_xUART_NOT_STARTED, "Abort_Rx: not started");
        return xRETURN_xERR_xUART_NOT_STARTED;
    }

    if (!uart_ctx->is_rx_busy)
    {
        return xRETURN_OK;
    }

    if (uart_ctx->ops->abort_rx == NULL)
    {
        xUART_LOG(xRETURN_xERR_xUART_UNSUPPORTED, "Abort_Rx: not supported by port");
        return xRETURN_xERR_xUART_UNSUPPORTED;
    }

    xUART_TRACE_E1(uart_ctx, xUART_TRACE_CODE_ABORT_RX, uart_ctx->port);
    xUART_LOG(xRETURN_OK, "Abort_Rx");

    return uart_ctx->ops->abort_rx(uart_ctx->driver_ctx);
}

// EOF /////////////////////////////////////////////////////////////////////////////
