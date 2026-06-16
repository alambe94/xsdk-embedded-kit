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

// @file xbridge_can.c
// @brief xBRIDGE CAN channel - SLCAN over CDC ACM bridge implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xbridge_can.h"
#include "xassert.h"

#include "xbridge_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// Standard SLCAN bitrate table (S0-S8)
#define SLCAN_BITRATE_COUNT (9U)

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

static const uint32_t slcan_bitrate_table[SLCAN_BITRATE_COUNT] = {
    10000U,   // S0
    20000U,   // S1
    50000U,   // S2
    100000U,  // S3
    125000U,  // S4
    250000U,  // S5
    500000U,  // S6
    800000U,  // S7
    1000000U, // S8
};

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t dispatch_slcan_line(xBRIDGE_CAN_Context_t *ctx);
static xRETURN_t send_ack(xBRIDGE_CAN_Context_t *ctx, const uint8_t *resp, uint32_t len);
static uint8_t hex_nibble(uint8_t c);
static uint32_t parse_hex(const uint8_t *buf, uint32_t nibbles);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static uint8_t hex_nibble(uint8_t c)
{
    if ((c >= (uint8_t)'0') && (c <= (uint8_t)'9'))
    {
        return c - (uint8_t)'0';
    }

    if ((c >= (uint8_t)'A') && (c <= (uint8_t)'F'))
    {
        return (uint8_t)(c - (uint8_t)'A' + 10U);
    }

    if ((c >= (uint8_t)'a') && (c <= (uint8_t)'f'))
    {
        return (uint8_t)(c - (uint8_t)'a' + 10U);
    }

    return 0U;
}

static uint32_t parse_hex(const uint8_t *buf, uint32_t nibbles)
{
    uint32_t val = 0U;

    for (uint32_t i = 0U; i < nibbles; i++)
    {
        val = (val << 4U) | hex_nibble(buf[i]);
    }

    return val;
}

static xRETURN_t send_ack(xBRIDGE_CAN_Context_t *ctx, const uint8_t *resp, uint32_t len)
{
    return ctx->usb_ops->send(ctx->usb_ctx, resp, len);
}

static xRETURN_t dispatch_slcan_line(xBRIDGE_CAN_Context_t *ctx)
{
    if (ctx->slcan_len == 0U)
    {
        return xRETURN_OK;
    }

    const uint8_t *line = ctx->slcan_line;
    uint8_t cmd = line[0];

    // Bell character = NACK for unknown commands
    const uint8_t bell[1U] = {0x07U};
    const uint8_t ok_cr[2U] = {(uint8_t)'z', (uint8_t)'\r'};
    const uint8_t ok_z_cr[2U] = {(uint8_t)'Z', (uint8_t)'\r'};

    switch (cmd)
    {
    case 'O': // Open channel
    {
        if (!ctx->is_open)
        {
            xRETURN_t ret = ctx->can_ops->open(ctx->can_ctx);
            if (ret == xRETURN_OK)
            {
                ctx->is_open = true;
            }
        }

        static const uint8_t cr[1U] = {(uint8_t)'\r'};
        return send_ack(ctx, cr, 1U);
    }

    case 'C': // Close channel
    {
        if (ctx->is_open)
        {
            (void)ctx->can_ops->close(ctx->can_ctx);
            ctx->is_open = false;
        }

        static const uint8_t cr[1U] = {(uint8_t)'\r'};
        return send_ack(ctx, cr, 1U);
    }

    case 'S': // Set standard bitrate S<n>
    {
        if (ctx->slcan_len >= 2U)
        {
            uint32_t idx = (uint32_t)(line[1] - (uint8_t)'0');

            if (idx < SLCAN_BITRATE_COUNT)
            {
                (void)ctx->can_ops->set_bitrate(ctx->can_ctx, slcan_bitrate_table[idx]);
            }
        }

        static const uint8_t cr[1U] = {(uint8_t)'\r'};
        return send_ack(ctx, cr, 1U);
    }

    case 't': // Transmit standard (11-bit ID)
    {
        if (!ctx->is_open || (ctx->slcan_len < 5U))
        {
            return send_ack(ctx, bell, 1U);
        }

        uint32_t id = parse_hex(line + 1U, 3U);
        uint32_t dlc = hex_nibble(line[4]);

        if ((dlc > 8U) || (ctx->slcan_len < (uint32_t)(5U + dlc * 2U)))
        {
            return send_ack(ctx, bell, 1U);
        }

        uint8_t data[8U];

        for (uint32_t i = 0U; i < dlc; i++)
        {
            data[i] = (uint8_t)parse_hex(line + 5U + (size_t)i * 2U, 2U);
        }

        xRETURN_t ret = ctx->can_ops->transmit(ctx->can_ctx, id, false, false, (uint8_t)dlc, data);

        if (ret != xRETURN_OK)
        {
            return send_ack(ctx, bell, 1U);
        }

        return send_ack(ctx, ok_cr, 2U);
    }

    case 'T': // Transmit extended (29-bit ID)
    {
        if (!ctx->is_open || (ctx->slcan_len < 10U))
        {
            return send_ack(ctx, bell, 1U);
        }

        uint32_t id = parse_hex(line + 1U, 8U);
        uint32_t dlc = hex_nibble(line[9]);

        if ((dlc > 8U) || (ctx->slcan_len < (uint32_t)(10U + dlc * 2U)))
        {
            return send_ack(ctx, bell, 1U);
        }

        uint8_t data[8U];

        for (uint32_t i = 0U; i < dlc; i++)
        {
            data[i] = (uint8_t)parse_hex(line + 10U + (size_t)i * 2U, 2U);
        }

        xRETURN_t ret = ctx->can_ops->transmit(ctx->can_ctx, id, true, false, (uint8_t)dlc, data);

        if (ret != xRETURN_OK)
        {
            return send_ack(ctx, bell, 1U);
        }

        return send_ack(ctx, ok_z_cr, 2U);
    }

    case 'Z': // Set timestamp mode Z<n>
    {
        if (ctx->slcan_len >= 2U)
        {
            ctx->is_timestamp_enabled = (line[1] != (uint8_t)'0');
        }

        static const uint8_t cr[1U] = {(uint8_t)'\r'};
        return send_ack(ctx, cr, 1U);
    }

    default:
        return send_ack(ctx, bell, 1U);
    }
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBRIDGE_CAN_Init(xBRIDGE_CAN_Context_t *ctx,
                           const xBRIDGE_USB_Ops_t *usb_ops,
                           void *usb_ctx,
                           const xBRIDGE_CAN_Peripheral_Ops_t *can_ops,
                           void *can_ctx)
{
    xASSERT(ctx != NULL, "ctx is NULL");
    xASSERT(usb_ops != NULL, "usb_ops is NULL");
    xASSERT(can_ops != NULL, "can_ops is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (usb_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (can_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    (void)memset(ctx, 0, sizeof(xBRIDGE_CAN_Context_t));

    ctx->usb_ops = usb_ops;
    ctx->usb_ctx = usb_ctx;
    ctx->can_ops = can_ops;
    ctx->can_ctx = can_ctx;
    ctx->state = xBRIDGE_STATE_IDLE;

    return xRETURN_OK;
}

xRETURN_t xBRIDGE_CAN_On_USB_Receive(xBRIDGE_CAN_Context_t *ctx, const uint8_t *data, uint32_t length)
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

    xRETURN_t ret = xRETURN_OK;

    for (uint32_t i = 0U; i < length; i++)
    {
        uint8_t byte = data[i];

        if (byte == (uint8_t)'\r')
        {
            ret = dispatch_slcan_line(ctx);
            ctx->slcan_len = 0U;
        }
        else
        {
            if (ctx->slcan_len < xBRIDGE_CAN_SLCAN_LINE_MAX)
            {
                ctx->slcan_line[ctx->slcan_len] = byte;
                ctx->slcan_len++;
            }
        }

        if (ret != xRETURN_OK)
        {
            return ret;
        }
    }

    return xRETURN_OK;
}

xRETURN_t xBRIDGE_CAN_Poll(xBRIDGE_CAN_Context_t *ctx)
{
    xASSERT(ctx != NULL, "ctx is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (!ctx->is_open)
    {
        return xRETURN_OK;
    }

    if ((ctx->can_ops == NULL) || (ctx->can_ops->rx_available == NULL))
    {
        return xRETURN_xERR_xBRIDGE_INVALID_STATE;
    }

    while (ctx->can_ops->rx_available(ctx->can_ctx))
    {
        uint32_t id = 0U;
        bool is_extended = false;
        bool is_rtr = false;
        uint8_t dlc = 0U;
        uint8_t data[8U];

        xRETURN_t ret = ctx->can_ops->receive(ctx->can_ctx, &id, &is_extended, &is_rtr, &dlc, data);

        if (ret != xRETURN_OK)
        {
            return ret;
        }

        // Build SLCAN receive string: t<ID3><DLC><DATA>\r or T<ID8><DLC><DATA>\r
        uint8_t slcan_out[30U];
        uint32_t out_len = 0U;

        if (is_extended)
        {
            slcan_out[out_len++] = (uint8_t)'T';
            static const uint8_t hex_chars[16U] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

            slcan_out[out_len++] = hex_chars[(id >> 28U) & 0xFU];
            slcan_out[out_len++] = hex_chars[(id >> 24U) & 0xFU];
            slcan_out[out_len++] = hex_chars[(id >> 20U) & 0xFU];
            slcan_out[out_len++] = hex_chars[(id >> 16U) & 0xFU];
            slcan_out[out_len++] = hex_chars[(id >> 12U) & 0xFU];
            slcan_out[out_len++] = hex_chars[(id >> 8U) & 0xFU];
            slcan_out[out_len++] = hex_chars[(id >> 4U) & 0xFU];
            slcan_out[out_len++] = hex_chars[(id >> 0U) & 0xFU];
        }
        else
        {
            slcan_out[out_len++] = (uint8_t)'t';
            static const uint8_t hex_chars[16U] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

            slcan_out[out_len++] = hex_chars[(id >> 8U) & 0xFU];
            slcan_out[out_len++] = hex_chars[(id >> 4U) & 0xFU];
            slcan_out[out_len++] = hex_chars[(id >> 0U) & 0xFU];
        }

        slcan_out[out_len++] = (uint8_t)('0' + dlc);

        for (uint32_t d = 0U; d < (uint32_t)dlc; d++)
        {
            static const uint8_t hex_chars[16U] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

            slcan_out[out_len++] = hex_chars[(data[d] >> 4U) & 0xFU];
            slcan_out[out_len++] = hex_chars[(data[d] >> 0U) & 0xFU];
        }

        slcan_out[out_len++] = (uint8_t)'\r';

        ret = ctx->usb_ops->send(ctx->usb_ctx, slcan_out, out_len);

        if (ret != xRETURN_OK)
        {
            return ret;
        }
    }

    return xRETURN_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
