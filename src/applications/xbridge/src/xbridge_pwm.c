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

// @file xbridge_pwm.c
// @brief xBRIDGE PWM channel - WINUSB binary frame PWM frequency, duty-cycle, and enable control.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xbridge_pwm.h"
#include "xbridge_core.h"
#include "xassert.h"

#include "xbridge_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t handle_set_frequency(xBRIDGE_PWM_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_set_duty(xBRIDGE_PWM_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_enable(xBRIDGE_PWM_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_disable(xBRIDGE_PWM_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_set_polarity(xBRIDGE_PWM_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t send_status_response(xBRIDGE_PWM_Context_t *ctx, uint8_t status, uint32_t seq);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t send_status_response(xBRIDGE_PWM_Context_t *ctx, uint8_t status, uint32_t seq)
{
    uint32_t resp_len = xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_PWM, status, seq, NULL, 0U);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

// Payload: channel(4) + frequency_hz(4)
static xRETURN_t handle_set_frequency(xBRIDGE_PWM_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 8U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t channel = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);
    uint32_t hz = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8U) | ((uint32_t)payload[6] << 16U) | ((uint32_t)payload[7] << 24U);

    xRETURN_t ret = ctx->pwm_ops->set_frequency(ctx->pwm_ctx, channel, hz);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// Payload: channel(4) + duty_per_10k(4)  (0-10000 = 0.00-100.00 %)
static xRETURN_t handle_set_duty(xBRIDGE_PWM_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 8U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t channel = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);
    uint32_t duty_per_10k =
        (uint32_t)payload[4] | ((uint32_t)payload[5] << 8U) | ((uint32_t)payload[6] << 16U) | ((uint32_t)payload[7] << 24U);

    if (duty_per_10k > xBRIDGE_PWM_DUTY_MAX)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    xRETURN_t ret = ctx->pwm_ops->set_duty(ctx->pwm_ctx, channel, duty_per_10k);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// Payload: channel(4)
static xRETURN_t handle_enable(xBRIDGE_PWM_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 4U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t channel = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);

    xRETURN_t ret = ctx->pwm_ops->enable(ctx->pwm_ctx, channel);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// Payload: channel(4)
static xRETURN_t handle_disable(xBRIDGE_PWM_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 4U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t channel = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);

    xRETURN_t ret = ctx->pwm_ops->disable(ctx->pwm_ctx, channel);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// Payload: channel(4) + polarity(1)
static xRETURN_t handle_set_polarity(xBRIDGE_PWM_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 5U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t channel = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);
    uint8_t polarity = payload[4];

    xRETURN_t ret = ctx->pwm_ops->set_polarity(ctx->pwm_ctx, channel, polarity);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBRIDGE_PWM_Init(xBRIDGE_PWM_Context_t *ctx,
                           const xBRIDGE_USB_Ops_t *usb_ops,
                           void *usb_ctx,
                           const xBRIDGE_PWM_Peripheral_Ops_t *pwm_ops,
                           void *pwm_ctx)
{
    xASSERT(ctx != NULL, "ctx is NULL");
    xASSERT(usb_ops != NULL, "usb_ops is NULL");
    xASSERT(pwm_ops != NULL, "pwm_ops is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (usb_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (pwm_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    (void)memset(ctx, 0, sizeof(xBRIDGE_PWM_Context_t));

    ctx->usb_ops = usb_ops;
    ctx->usb_ctx = usb_ctx;
    ctx->pwm_ops = pwm_ops;
    ctx->pwm_ctx = pwm_ctx;
    ctx->state = xBRIDGE_STATE_IDLE;

    return xRETURN_OK;
}

xRETURN_t xBRIDGE_PWM_On_USB_Receive(xBRIDGE_PWM_Context_t *ctx, const uint8_t *data, uint32_t length)
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
    case xBRIDGE_PWM_CMD_SET_FREQUENCY:
        return handle_set_frequency(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_PWM_CMD_SET_DUTY:
        return handle_set_duty(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_PWM_CMD_ENABLE:
        return handle_enable(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_PWM_CMD_DISABLE:
        return handle_disable(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_PWM_CMD_SET_POLARITY:
        return handle_set_polarity(ctx, payload, cmd.length, cmd.seq);

    default:
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, cmd.seq);
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
