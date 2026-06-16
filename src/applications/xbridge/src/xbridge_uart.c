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

// @file xbridge_uart.c
// @brief xBRIDGE UART channel - transparent CDC ACM to hardware UART bridge implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xbridge_uart.h"
#include "xassert.h"

#include "xbridge_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static uint32_t ring_available(uint32_t head, uint32_t tail, uint32_t capacity);
static xRETURN_t ring_enqueue(uint8_t *ring, uint32_t *head, uint32_t *tail, uint32_t capacity, const uint8_t *data, uint32_t length);
static xRETURN_t
ring_dequeue(uint8_t *ring, uint32_t *head, uint32_t *tail, uint32_t capacity, uint8_t *data, uint32_t max_len, uint32_t *read_len);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static uint32_t ring_available(uint32_t head, uint32_t tail, uint32_t capacity)
{
    return (tail >= head) ? (tail - head) : (capacity - head + tail);
}

static xRETURN_t ring_enqueue(uint8_t *ring, uint32_t *head, uint32_t *tail, uint32_t capacity, const uint8_t *data, uint32_t length)
{
    uint32_t used = ring_available(*head, *tail, capacity);
    uint32_t free_space = capacity - 1U - used;

    if (length > free_space)
    {
        return xRETURN_xERR_xBRIDGE_QUEUE_FULL;
    }

    for (uint32_t i = 0U; i < length; i++)
    {
        ring[*tail] = data[i];
        *tail = (*tail + 1U) % capacity;
    }

    return xRETURN_OK;
}

static xRETURN_t
ring_dequeue(uint8_t *ring, uint32_t *head, uint32_t *tail, uint32_t capacity, uint8_t *data, uint32_t max_len, uint32_t *read_len)
{
    uint32_t avail = ring_available(*head, *tail, capacity);
    uint32_t count = (avail < max_len) ? avail : max_len;

    for (uint32_t i = 0U; i < count; i++)
    {
        data[i] = ring[*head];
        *head = (*head + 1U) % capacity;
    }

    *read_len = count;

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBRIDGE_UART_Init(xBRIDGE_UART_Context_t *ctx,
                            const xBRIDGE_USB_Ops_t *usb_ops,
                            void *usb_ctx,
                            const xBRIDGE_UART_Peripheral_Ops_t *uart_ops,
                            void *uart_ctx)
{
    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (usb_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (uart_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    (void)memset(ctx, 0, sizeof(xBRIDGE_UART_Context_t));

    ctx->usb_ops = usb_ops;
    ctx->usb_ctx = usb_ctx;
    ctx->uart_ops = uart_ops;
    ctx->uart_ctx = uart_ctx;
    ctx->state = xBRIDGE_STATE_IDLE;

    return xRETURN_OK;
}

xRETURN_t xBRIDGE_UART_On_USB_Receive(xBRIDGE_UART_Context_t *ctx, const uint8_t *data, uint32_t length)
{
    xASSERT(ctx != NULL, "ctx is NULL");
    xASSERT(data != NULL, "data is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (data == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (!ctx->is_dtr_active)
    {
        return xRETURN_OK;
    }

    return ring_enqueue(ctx->usb_to_uart_buf, &ctx->usb_to_uart_head, &ctx->usb_to_uart_tail, xBRIDGE_UART_QUEUE_BYTES, data, length);
}

xRETURN_t xBRIDGE_UART_On_Line_Coding(xBRIDGE_UART_Context_t *ctx, uint32_t baud_rate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits)
{
    xASSERT(ctx != NULL, "ctx is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if ((ctx->uart_ops == NULL) || (ctx->uart_ops->set_line_coding == NULL))
    {
        return xRETURN_xERR_xBRIDGE_INVALID_STATE;
    }

    return ctx->uart_ops->set_line_coding(ctx->uart_ctx, baud_rate, stop_bits, parity, data_bits);
}

xRETURN_t xBRIDGE_UART_On_Control_Line_State(xBRIDGE_UART_Context_t *ctx, bool dtr, bool rts)
{
    xASSERT(ctx != NULL, "ctx is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    ctx->is_dtr_active = dtr;
    (void)rts;

    return xRETURN_OK;
}

xRETURN_t xBRIDGE_UART_Poll(xBRIDGE_UART_Context_t *ctx)
{
    xASSERT(ctx != NULL, "ctx is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if ((ctx->uart_ops == NULL) || (ctx->uart_ops->write == NULL))
    {
        return xRETURN_xERR_xBRIDGE_INVALID_STATE;
    }

    if ((ctx->usb_ops == NULL) || (ctx->usb_ops->send == NULL))
    {
        return xRETURN_xERR_xBRIDGE_INVALID_STATE;
    }

    // Drain USB->UART ring
    uint8_t tx_buf[64U];
    uint32_t tx_len = 0U;
    xRETURN_t ret = ring_dequeue(ctx->usb_to_uart_buf, &ctx->usb_to_uart_head, &ctx->usb_to_uart_tail, xBRIDGE_UART_QUEUE_BYTES, tx_buf,
                                 sizeof(tx_buf), &tx_len);

    if (ret != xRETURN_OK)
    {
        return ret;
    }

    if (tx_len > 0U)
    {
        ret = ctx->uart_ops->write(ctx->uart_ctx, tx_buf, tx_len);
        if (ret != xRETURN_OK)
        {
            return ret;
        }
    }

    // Drain UART->USB ring
    if ((ctx->uart_ops->is_rx_ready != NULL) && ctx->uart_ops->is_rx_ready(ctx->uart_ctx))
    {
        uint8_t rx_buf[64U];
        uint32_t rx_len = 0U;

        ret = ctx->uart_ops->read(ctx->uart_ctx, rx_buf, sizeof(rx_buf), &rx_len);
        if (ret != xRETURN_OK)
        {
            return ret;
        }

        if (rx_len > 0U)
        {
            ret = ring_enqueue(ctx->uart_to_usb_buf, &ctx->uart_to_usb_head, &ctx->uart_to_usb_tail, xBRIDGE_UART_QUEUE_BYTES, rx_buf,
                               rx_len);
            if (ret != xRETURN_OK)
            {
                return ret;
            }
        }
    }

    // Flush UART->USB ring to USB
    uint8_t usb_tx[64U];
    uint32_t usb_tx_len = 0U;

    ret = ring_dequeue(ctx->uart_to_usb_buf, &ctx->uart_to_usb_head, &ctx->uart_to_usb_tail, xBRIDGE_UART_QUEUE_BYTES, usb_tx,
                       sizeof(usb_tx), &usb_tx_len);

    if (ret != xRETURN_OK)
    {
        return ret;
    }

    if (usb_tx_len > 0U)
    {
        ret = ctx->usb_ops->send(ctx->usb_ctx, usb_tx, usb_tx_len);
    }

    return ret;
}

// EOF /////////////////////////////////////////////////////////////////////////////
