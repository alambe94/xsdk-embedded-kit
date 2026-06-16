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

// @file xbridge_adc.c
// @brief xBRIDGE ADC channel - WINUSB binary frame ADC single/multi-channel sampling bridge.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xbridge_adc.h"
#include "xbridge_core.h"
#include "xassert.h"

#include "xbridge_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t handle_read_single(xBRIDGE_ADC_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_read_multi(xBRIDGE_ADC_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_set_resolution(xBRIDGE_ADC_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_set_reference(xBRIDGE_ADC_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_set_sample_rate(xBRIDGE_ADC_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t send_status_response(xBRIDGE_ADC_Context_t *ctx, uint8_t status, uint32_t seq);
static uint32_t popcount32(uint32_t v);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t send_status_response(xBRIDGE_ADC_Context_t *ctx, uint8_t status, uint32_t seq)
{
    uint32_t resp_len = xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_ADC, status, seq, NULL, 0U);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

static uint32_t popcount32(uint32_t v)
{
    uint32_t count = 0U;

    while (v != 0U)
    {
        count += v & 1U;
        v >>= 1U;
    }

    return count;
}

// Payload: channel(4)  - returns result(4) in response data
static xRETURN_t handle_read_single(xBRIDGE_ADC_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 4U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t channel = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);
    uint32_t result = 0U;

    xRETURN_t ret = ctx->adc_ops->read_single(ctx->adc_ctx, channel, &result);

    if (ret != xRETURN_OK)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint8_t result_bytes[4U];
    result_bytes[0] = (uint8_t)(result & 0xFFU);
    result_bytes[1] = (uint8_t)((result >> 8U) & 0xFFU);
    result_bytes[2] = (uint8_t)((result >> 16U) & 0xFFU);
    result_bytes[3] = (uint8_t)((result >> 24U) & 0xFFU);

    uint32_t resp_len = xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_ADC, xBRIDGE_STATUS_OK, seq,
                                                    result_bytes, sizeof(result_bytes));
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

// Payload: channel_mask(4)  - returns count * 4 bytes of results in ascending channel order
static xRETURN_t handle_read_multi(xBRIDGE_ADC_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 4U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t channel_mask =
        (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);
    uint32_t count = popcount32(channel_mask);

    if ((count == 0U) || (count > xBRIDGE_ADC_MAX_CHANNELS))
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t results[xBRIDGE_ADC_MAX_CHANNELS];
    (void)memset(results, 0, sizeof(results));

    xRETURN_t ret = ctx->adc_ops->read_multi(ctx->adc_ctx, channel_mask, results, count);

    if (ret != xRETURN_OK)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    // Serialise uint32_t array to little-endian byte stream
    uint8_t *out = ctx->resp_buf + sizeof(xBRIDGE_Frame_Resp_t);
    uint32_t out_len = count * 4U;

    for (uint32_t i = 0U; i < count; i++)
    {
        out[i * 4U + 0U] = (uint8_t)(results[i] & 0xFFU);
        out[i * 4U + 1U] = (uint8_t)((results[i] >> 8U) & 0xFFU);
        out[i * 4U + 2U] = (uint8_t)((results[i] >> 16U) & 0xFFU);
        out[i * 4U + 3U] = (uint8_t)((results[i] >> 24U) & 0xFFU);
    }

    uint32_t resp_len =
        xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_ADC, xBRIDGE_STATUS_OK, seq, out, out_len);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

// Payload: bits(1)  - 8, 10, 12, or 16
static xRETURN_t handle_set_resolution(xBRIDGE_ADC_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 1U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint8_t bits = payload[0];

    if ((bits != 8U) && (bits != 10U) && (bits != 12U) && (bits != 16U))
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    xRETURN_t ret = ctx->adc_ops->set_resolution(ctx->adc_ctx, bits);

    if (ret == xRETURN_OK)
    {
        ctx->resolution_bits = bits;
    }

    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// Payload: reference(1)  - xBRIDGE_ADC_REF_*
static xRETURN_t handle_set_reference(xBRIDGE_ADC_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 1U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    xRETURN_t ret = ctx->adc_ops->set_reference(ctx->adc_ctx, payload[0]);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// Payload: samples_per_sec(4)
static xRETURN_t handle_set_sample_rate(xBRIDGE_ADC_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 4U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t sps = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);

    xRETURN_t ret = ctx->adc_ops->set_sample_rate(ctx->adc_ctx, sps);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBRIDGE_ADC_Init(xBRIDGE_ADC_Context_t *ctx,
                           const xBRIDGE_USB_Ops_t *usb_ops,
                           void *usb_ctx,
                           const xBRIDGE_ADC_Peripheral_Ops_t *adc_ops,
                           void *adc_ctx)
{
    xASSERT(ctx != NULL, "ctx is NULL");
    xASSERT(usb_ops != NULL, "usb_ops is NULL");
    xASSERT(adc_ops != NULL, "adc_ops is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (usb_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (adc_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    (void)memset(ctx, 0, sizeof(xBRIDGE_ADC_Context_t));

    ctx->usb_ops = usb_ops;
    ctx->usb_ctx = usb_ctx;
    ctx->adc_ops = adc_ops;
    ctx->adc_ctx = adc_ctx;
    ctx->state = xBRIDGE_STATE_IDLE;
    ctx->resolution_bits = 12U; // default to 12-bit

    return xRETURN_OK;
}

xRETURN_t xBRIDGE_ADC_On_USB_Receive(xBRIDGE_ADC_Context_t *ctx, const uint8_t *data, uint32_t length)
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

    xBRIDGE_Frame_Cmd_t cmd;
    const uint8_t *payload = NULL;
    xRETURN_t ret = xBRIDGE_Core_Parse_Frame(data, length, &cmd, &payload);

    if (ret != xRETURN_OK)
    {
        return ret;
    }

    switch (cmd.cmd)
    {
    case xBRIDGE_ADC_CMD_READ_SINGLE:
        return handle_read_single(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_ADC_CMD_READ_MULTI:
        return handle_read_multi(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_ADC_CMD_SET_RESOLUTION:
        return handle_set_resolution(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_ADC_CMD_SET_REFERENCE:
        return handle_set_reference(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_ADC_CMD_SET_SAMPLE_RATE:
        return handle_set_sample_rate(ctx, payload, cmd.length, cmd.seq);

    default:
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, cmd.seq);
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
