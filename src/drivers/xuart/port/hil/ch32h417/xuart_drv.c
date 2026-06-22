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

// @file xuart_drv.c
// @brief CH32H417 USART hardware port implementation for the xUART driver core.
//        Uses direct register access via ch32h417.h — no peripheral HAL library.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// MODULE INCLUDES
#include "xassert.h"
#include "xuart_drv.h"
#include "xuart_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// Approximate CPU cycles consumed per polling iteration (1 MMIO read + compare + branch).
#define XUART_CH32_CYCLES_PER_POLL_LOOP 8U

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xUART_CH32H417_Context_t *as_port_context(void *driver_ctx);
static void                      enable_clock(const xUART_CH32H417_Context_t *ctx);
static void                      reset_periph(const xUART_CH32H417_Context_t *ctx);
static void                      fire_event(xUART_CH32H417_Context_t *ctx, xUART_Event_t event, uint32_t bytes, xRETURN_t error);

static xRETURN_t ch32_init(void *driver_ctx, const xUART_Config_t *config);
static xRETURN_t ch32_deinit(void *driver_ctx);
static xRETURN_t ch32_start(void *driver_ctx);
static xRETURN_t ch32_stop(void *driver_ctx);
static xRETURN_t ch32_set_event_callback(void *driver_ctx, xUART_Driver_Event_Callback_t callback, void *callback_ctx);
static xRETURN_t ch32_transmit(void *driver_ctx, const uint8_t *buffer, uint32_t length, uint32_t timeout_ms);
static xRETURN_t ch32_receive(void *driver_ctx, uint8_t *buffer, uint32_t length, uint32_t timeout_ms);
static xRETURN_t ch32_transmit_async(void *driver_ctx, const uint8_t *buffer, uint32_t length);
static xRETURN_t ch32_receive_async(void *driver_ctx, uint8_t *buffer, uint32_t length);
static xRETURN_t ch32_abort_tx(void *driver_ctx);
static xRETURN_t ch32_abort_rx(void *driver_ctx);

const xUART_Driver_Ops_t xUART_CH32H417_Driver_Ops = {
    .init               = ch32_init,
    .deinit             = ch32_deinit,
    .start              = ch32_start,
    .stop               = ch32_stop,
    .set_event_callback = ch32_set_event_callback,
    .transmit           = ch32_transmit,
    .receive            = ch32_receive,
    .transmit_async     = ch32_transmit_async,
    .receive_async      = ch32_receive_async,
    .abort_tx           = ch32_abort_tx,
    .abort_rx           = ch32_abort_rx,
};

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xUART_CH32H417_Context_t *as_port_context(void *driver_ctx)
{
    return (xUART_CH32H417_Context_t *)driver_ctx;
}

static void enable_clock(const xUART_CH32H417_Context_t *ctx)
{
    if (ctx->usart == USART1)
    {
        RCC->HB2PCENR |= RCC_USART1EN;
    }
    else if (ctx->usart == USART2)
    {
        RCC->HB1PCENR |= RCC_USART2EN;
    }
    else if (ctx->usart == USART3)
    {
        RCC->HB1PCENR |= RCC_USART3EN;
    }
}

static void reset_periph(const xUART_CH32H417_Context_t *ctx)
{
    if (ctx->usart == USART1)
    {
        RCC->HB2PRSTR |= RCC_USART1RST;
        RCC->HB2PRSTR &= ~RCC_USART1RST;
    }
    else if (ctx->usart == USART2)
    {
        RCC->HB1PRSTR |= RCC_USART2RST;
        RCC->HB1PRSTR &= ~RCC_USART2RST;
    }
    else if (ctx->usart == USART3)
    {
        RCC->HB1PRSTR |= RCC_USART3RST;
        RCC->HB1PRSTR &= ~RCC_USART3RST;
    }
}

static void fire_event(xUART_CH32H417_Context_t *ctx, xUART_Event_t event, uint32_t bytes, xRETURN_t error)
{
    if (ctx->event_callback != NULL)
    {
        xUART_Event_Info_t info;
        info.bytes_transferred = bytes;
        info.error_code        = error;
        ctx->event_callback(ctx->event_callback_ctx, event, &info);
    }
}

static xRETURN_t ch32_init(void *driver_ctx, const xUART_Config_t *config)
{
    xUART_CH32H417_Context_t *ctx;
    uint16_t                   ctlr1;
    uint16_t                   ctlr2;
    uint16_t                   ctlr3;

    if ((driver_ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    if (ctx->usart == NULL)
    {
        return xRETURN_xERR_xUART_INVALID_ARG;
    }

    enable_clock(ctx);

    // ---- CTLR1: word length, parity, RE+TE ----------------------------------
    ctlr1 = (uint16_t)(USART_CTLR1_RE | USART_CTLR1_TE);

    switch (config->data_bits)
    {
    case xUART_DATA_BITS_5: ctlr1 |= USART_CTLR1_M_EXT5; break;
    case xUART_DATA_BITS_6: ctlr1 |= USART_CTLR1_M_EXT6; break;
    case xUART_DATA_BITS_7: ctlr1 |= USART_CTLR1_M_EXT7; break;
    case xUART_DATA_BITS_8: /* M=0, M_EXT=0 — no bits to set */ break;
    case xUART_DATA_BITS_9: ctlr1 |= USART_CTLR1_M;       break;
    default:                return xRETURN_xERR_xUART_INVALID_ARG;
    }

    switch (config->parity)
    {
    case xUART_PARITY_NONE: /* PCE=0 */ break;
    case xUART_PARITY_EVEN: ctlr1 |= USART_CTLR1_PCE;                     break;
    case xUART_PARITY_ODD:  ctlr1 |= (uint16_t)(USART_CTLR1_PCE | USART_CTLR1_PS); break;
    default:                return xRETURN_xERR_xUART_INVALID_ARG;
    }

    // ---- CTLR2: stop bits ---------------------------------------------------
    ctlr2 = 0U;

    switch (config->stop_bits)
    {
    case xUART_STOP_BITS_1:   /* STOP=0b00 */ break;
    case xUART_STOP_BITS_0_5: ctlr2 |= USART_CTLR2_STOP_0;                          break;
    case xUART_STOP_BITS_2:   ctlr2 |= USART_CTLR2_STOP_1;                          break;
    case xUART_STOP_BITS_1_5: ctlr2 |= (uint16_t)(USART_CTLR2_STOP_0 | USART_CTLR2_STOP_1); break;
    default:                  return xRETURN_xERR_xUART_INVALID_ARG;
    }

    // ---- CTLR3: flow control ------------------------------------------------
    ctlr3 = 0U;

    switch (config->flow_control)
    {
    case xUART_FLOW_CONTROL_NONE:    /* RTSE=0, CTSE=0 */ break;
    case xUART_FLOW_CONTROL_RTS_CTS: ctlr3 |= (uint16_t)(USART_CTLR3_RTSE | USART_CTLR3_CTSE); break;
    default:                         return xRETURN_xERR_xUART_INVALID_ARG;
    }

    ctx->usart->CTLR1 = ctlr1;
    ctx->usart->CTLR2 = ctlr2;
    ctx->usart->CTLR3 = ctlr3;

    // BRR = pclk / baud (16x oversampling): mantissa in bits[15:4], fraction in bits[3:0].
    ctx->usart->BRR = (uint16_t)((ctx->pclk_hz + (config->baud_rate >> 1U)) / config->baud_rate);

    ctx->is_initialized = true;

    return xRETURN_OK;
}

static xRETURN_t ch32_deinit(void *driver_ctx)
{
    xUART_CH32H417_Context_t *ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    ctx->usart->CTLR1 = 0U;
    ctx->usart->CTLR2 = 0U;
    ctx->usart->CTLR3 = 0U;

    reset_periph(ctx);

    ctx->is_initialized = false;
    ctx->is_started     = false;
    ctx->is_tx_busy     = false;
    ctx->is_rx_busy     = false;
    ctx->tx_buffer      = NULL;
    ctx->rx_buffer      = NULL;

    return xRETURN_OK;
}

static xRETURN_t ch32_start(void *driver_ctx)
{
    xUART_CH32H417_Context_t *ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    // Enable RXNE, PE interrupts in CTLR1; error interrupt in CTLR3.
    ctx->usart->CTLR1 |= (uint16_t)(USART_CTLR1_RXNEIE | USART_CTLR1_PEIE);
    ctx->usart->CTLR3 |= (uint16_t)USART_CTLR3_EIE;

    ctx->usart->CTLR1 |= (uint16_t)USART_CTLR1_UE;

    ctx->is_started = true;

    return xRETURN_OK;
}

static xRETURN_t ch32_stop(void *driver_ctx)
{
    xUART_CH32H417_Context_t *ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    ctx->usart->CTLR1 &= ~(uint16_t)(USART_CTLR1_UE | USART_CTLR1_RXNEIE | USART_CTLR1_TXEIE |
                                      USART_CTLR1_TCIE | USART_CTLR1_PEIE);
    ctx->usart->CTLR3 &= ~(uint16_t)USART_CTLR3_EIE;

    ctx->is_started = false;

    return xRETURN_OK;
}

static xRETURN_t ch32_set_event_callback(void *driver_ctx, xUART_Driver_Event_Callback_t callback, void *callback_ctx)
{
    xUART_CH32H417_Context_t *ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);
    ctx->event_callback     = callback;
    ctx->event_callback_ctx = callback_ctx;

    return xRETURN_OK;
}

static xRETURN_t ch32_transmit(void *driver_ctx, const uint8_t *buffer, uint32_t length, uint32_t timeout_ms)
{
    xUART_CH32H417_Context_t *ctx;
    uint32_t                  i;
    uint32_t                  deadline;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    uint32_t loops_per_ms = ctx->pclk_hz / (XUART_CH32_CYCLES_PER_POLL_LOOP * 1000U);
    if (loops_per_ms == 0U) { loops_per_ms = 1U; }
    deadline = (timeout_ms == 0U) ? (loops_per_ms * 1000U)
                                  : (uint32_t)((uint64_t)timeout_ms * loops_per_ms);

    for (i = 0U; i < length; i++)
    {
        uint32_t guard = 0U;
        while ((ctx->usart->STATR & USART_STATR_TXE) == 0U)
        {
            if (++guard >= deadline)
            {
                return xRETURN_xERR_xUART_TIMEOUT;
            }
        }
        ctx->usart->DATAR = (uint16_t)buffer[i];
    }

    // Wait for TC so the last byte is fully shifted out before returning.
    {
        uint32_t guard = 0U;
        while ((ctx->usart->STATR & USART_STATR_TC) == 0U)
        {
            if (++guard >= deadline)
            {
                return xRETURN_xERR_xUART_TIMEOUT;
            }
        }
    }

    return xRETURN_OK;
}

static xRETURN_t ch32_receive(void *driver_ctx, uint8_t *buffer, uint32_t length, uint32_t timeout_ms)
{
    xUART_CH32H417_Context_t *ctx;
    uint32_t                  i;
    uint32_t                  deadline;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    uint32_t loops_per_ms = ctx->pclk_hz / (XUART_CH32_CYCLES_PER_POLL_LOOP * 1000U);
    if (loops_per_ms == 0U) { loops_per_ms = 1U; }
    deadline = (timeout_ms == 0U) ? (loops_per_ms * 1000U)
                                  : (uint32_t)((uint64_t)timeout_ms * loops_per_ms);

    for (i = 0U; i < length; i++)
    {
        uint32_t guard = 0U;
        while ((ctx->usart->STATR & USART_STATR_RXNE) == 0U)
        {
            if (++guard >= deadline)
            {
                return xRETURN_xERR_xUART_TIMEOUT;
            }
        }
        buffer[i] = (uint8_t)(ctx->usart->DATAR & 0xFFU);
    }

    return xRETURN_OK;
}

static xRETURN_t ch32_transmit_async(void *driver_ctx, const uint8_t *buffer, uint32_t length)
{
    xUART_CH32H417_Context_t *ctx;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    ctx->tx_buffer  = buffer;
    ctx->tx_length  = length;
    ctx->tx_index   = 0U;
    ctx->is_tx_busy = true;

    // Write the first byte to kick off interrupt-driven TX; TXE fires immediately
    // when the shift register is empty, so writing to DATAR here seeds the pipeline.
    ctx->usart->DATAR = (uint16_t)ctx->tx_buffer[ctx->tx_index++];
    ctx->usart->CTLR1 |= (uint16_t)USART_CTLR1_TXEIE;

    return xRETURN_OK;
}

static xRETURN_t ch32_receive_async(void *driver_ctx, uint8_t *buffer, uint32_t length)
{
    xUART_CH32H417_Context_t *ctx;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    ctx->rx_buffer  = buffer;
    ctx->rx_length  = length;
    ctx->rx_index   = 0U;
    ctx->is_rx_busy = true;

    // RXNE interrupt is already enabled in ch32_start; data will flow on arrival.

    return xRETURN_OK;
}

static xRETURN_t ch32_abort_tx(void *driver_ctx)
{
    xUART_CH32H417_Context_t *ctx;
    uint32_t                  bytes_done;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    ctx->usart->CTLR1 &= ~(uint16_t)(USART_CTLR1_TXEIE | USART_CTLR1_TCIE);

    bytes_done      = ctx->tx_index;
    ctx->tx_buffer  = NULL;
    ctx->tx_length  = 0U;
    ctx->tx_index   = 0U;
    ctx->is_tx_busy = false;

    fire_event(ctx, xUART_EVENT_TX_ABORTED, bytes_done, xRETURN_xERR_xUART_ABORTED);

    return xRETURN_OK;
}

static xRETURN_t ch32_abort_rx(void *driver_ctx)
{
    xUART_CH32H417_Context_t *ctx;
    uint32_t                  bytes_done;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    // RXNE interrupt is left enabled — disable async buffering by clearing the buffer.
    bytes_done      = ctx->rx_index;
    ctx->rx_buffer  = NULL;
    ctx->rx_length  = 0U;
    ctx->rx_index   = 0U;
    ctx->is_rx_busy = false;

    fire_event(ctx, xUART_EVENT_RX_ABORTED, bytes_done, xRETURN_xERR_xUART_ABORTED);

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xUART_CH32H417_IRQ_Handler(xUART_CH32H417_Context_t *ctx)
{
    uint16_t statr;
    uint16_t ctlr1;
    uint16_t ctlr3;

    if (ctx == NULL)
    {
        return;
    }

    statr = ctx->usart->STATR;
    ctlr1 = ctx->usart->CTLR1;
    ctlr3 = ctx->usart->CTLR3;

    // ---- Parity error -------------------------------------------------------
    // PE requires PEIE+PE. Read DATAR to clear both PE and RXNE simultaneously.
    if (((ctlr1 & USART_CTLR1_PEIE) != 0U) && ((statr & USART_STATR_PE) != 0U))
    {
        (void)ctx->usart->DATAR; // must always read to clear the flag
        if (ctx->is_rx_busy)
        {
            uint32_t bytes_done = ctx->rx_index;
            ctx->rx_buffer      = NULL;
            ctx->rx_length      = 0U;
            ctx->rx_index       = 0U;
            ctx->is_rx_busy     = false;
            fire_event(ctx, xUART_EVENT_RX_PARITY, bytes_done, xRETURN_xERR_xUART_PARITY);
        }
        return;
    }

    // ---- Framing error ------------------------------------------------------
    // FE requires EIE+FE. Read DATAR to clear FE (it is sticky until DATAR is read).
    if (((ctlr3 & USART_CTLR3_EIE) != 0U) && ((statr & USART_STATR_FE) != 0U))
    {
        (void)ctx->usart->DATAR; // must always read to clear the flag
        if (ctx->is_rx_busy)
        {
            uint32_t bytes_done = ctx->rx_index;
            ctx->rx_buffer      = NULL;
            ctx->rx_length      = 0U;
            ctx->rx_index       = 0U;
            ctx->is_rx_busy     = false;
            fire_event(ctx, xUART_EVENT_RX_FRAMING, bytes_done, xRETURN_xERR_xUART_FRAMING);
        }
        return;
    }

    // ---- Overrun error ------------------------------------------------------
    // ORE_RX: set when RXNEIE is active and an overrun occurs. Clear by reading DATAR.
    if (((ctlr1 & USART_CTLR1_RXNEIE) != 0U) && ((statr & USART_STATR_ORE) != 0U))
    {
        (void)ctx->usart->DATAR; // must always read to clear the flag
        if (ctx->is_rx_busy)
        {
            uint32_t bytes_done = ctx->rx_index;
            ctx->rx_buffer      = NULL;
            ctx->rx_length      = 0U;
            ctx->rx_index       = 0U;
            ctx->is_rx_busy     = false;
            fire_event(ctx, xUART_EVENT_RX_OVERRUN, bytes_done, xRETURN_xERR_xUART_OVERRUN);
        }
        return;
    }

    // ---- RXNE: byte received ------------------------------------------------
    if (((ctlr1 & USART_CTLR1_RXNEIE) != 0U) && ((statr & USART_STATR_RXNE) != 0U))
    {
        uint8_t byte = (uint8_t)(ctx->usart->DATAR & 0xFFU);

        if (ctx->is_rx_busy && (ctx->rx_buffer != NULL) && (ctx->rx_index < ctx->rx_length))
        {
            ctx->rx_buffer[ctx->rx_index++] = byte;

            if (ctx->rx_index >= ctx->rx_length)
            {
                uint32_t bytes_done = ctx->rx_index;
                ctx->rx_buffer      = NULL;
                ctx->rx_length      = 0U;
                ctx->rx_index       = 0U;
                ctx->is_rx_busy     = false;

                fire_event(ctx, xUART_EVENT_RX_COMPLETE, bytes_done, xRETURN_OK);
            }
        }
    }

    // ---- TXE: TX data register empty, load next byte ------------------------
    if (((ctlr1 & USART_CTLR1_TXEIE) != 0U) && ((statr & USART_STATR_TXE) != 0U))
    {
        if (ctx->is_tx_busy && (ctx->tx_buffer != NULL) && (ctx->tx_index < ctx->tx_length))
        {
            ctx->usart->DATAR = (uint16_t)ctx->tx_buffer[ctx->tx_index++];

            if (ctx->tx_index >= ctx->tx_length)
            {
                // All bytes queued; wait for TC before signalling completion.
                ctx->usart->CTLR1 &= ~(uint16_t)USART_CTLR1_TXEIE;
                ctx->usart->CTLR1 |=  (uint16_t)USART_CTLR1_TCIE;
            }
        }
        else
        {
            ctx->usart->CTLR1 &= ~(uint16_t)USART_CTLR1_TXEIE;
        }
    }

    // ---- TC: last byte fully shifted out ------------------------------------
    if (((ctlr1 & USART_CTLR1_TCIE) != 0U) && ((statr & USART_STATR_TC) != 0U))
    {
        ctx->usart->CTLR1 &= ~(uint16_t)USART_CTLR1_TCIE;
        // Clear TC by writing 0 to the TC bit in STATR.
        ctx->usart->STATR &= ~(uint16_t)USART_STATR_TC;

        if (ctx->is_tx_busy)
        {
            uint32_t bytes_done = ctx->tx_index;
            ctx->tx_buffer      = NULL;
            ctx->tx_length      = 0U;
            ctx->tx_index       = 0U;
            ctx->is_tx_busy     = false;

            fire_event(ctx, xUART_EVENT_TX_COMPLETE, bytes_done, xRETURN_OK);
        }
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
