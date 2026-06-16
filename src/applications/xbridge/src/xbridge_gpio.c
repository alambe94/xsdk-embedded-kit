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

// @file xbridge_gpio.c
// @brief xBRIDGE GPIO channel - WINUSB binary frame pin direction, read, write, and pull control.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xbridge_gpio.h"
#include "xbridge_core.h"
#include "xassert.h"

#include "xbridge_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t handle_set_direction(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_write(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_read(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_set_pull(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_toggle(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t send_status_response(xBRIDGE_GPIO_Context_t *ctx, uint8_t status, uint32_t seq);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t send_status_response(xBRIDGE_GPIO_Context_t *ctx, uint8_t status, uint32_t seq)
{
    uint32_t resp_len = xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_GPIO, status, seq, NULL, 0U);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

// Payload: port(4) + pin(4) + direction(1)
static xRETURN_t handle_set_direction(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 9U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t port = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);
    uint32_t pin = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8U) | ((uint32_t)payload[6] << 16U) | ((uint32_t)payload[7] << 24U);
    uint8_t direction = payload[8];

    xRETURN_t ret = ctx->gpio_ops->set_direction(ctx->gpio_ctx, port, pin, direction);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// Payload: port(4) + pin(4) + value(1)  OR  port(4) + mask(4) + port_value(4) for write_port
static xRETURN_t handle_write(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 9U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t port = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);
    uint32_t pin = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8U) | ((uint32_t)payload[6] << 16U) | ((uint32_t)payload[7] << 24U);
    uint8_t value = payload[8];

    xRETURN_t ret = ctx->gpio_ops->write_pin(ctx->gpio_ctx, port, pin, value);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// Payload: port(4) + pin(4)  - returns value(1) in response data
static xRETURN_t handle_read(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 8U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t port = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);
    uint32_t pin = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8U) | ((uint32_t)payload[6] << 16U) | ((uint32_t)payload[7] << 24U);
    uint8_t value = 0U;

    xRETURN_t ret = ctx->gpio_ops->read_pin(ctx->gpio_ctx, port, pin, &value);

    if (ret != xRETURN_OK)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t resp_len =
        xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_GPIO, xBRIDGE_STATUS_OK, seq, &value, 1U);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

// Payload: port(4) + pin(4) + pull(1)
static xRETURN_t handle_set_pull(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 9U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t port = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);
    uint32_t pin = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8U) | ((uint32_t)payload[6] << 16U) | ((uint32_t)payload[7] << 24U);
    uint8_t pull = payload[8];

    xRETURN_t ret = ctx->gpio_ops->set_pull(ctx->gpio_ctx, port, pin, pull);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// Payload: port(4) + pin(4)
static xRETURN_t handle_toggle(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 8U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t port = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);
    uint32_t pin = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8U) | ((uint32_t)payload[6] << 16U) | ((uint32_t)payload[7] << 24U);

    xRETURN_t ret = ctx->gpio_ops->toggle_pin(ctx->gpio_ctx, port, pin);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBRIDGE_GPIO_Init(xBRIDGE_GPIO_Context_t *ctx,
                            const xBRIDGE_USB_Ops_t *usb_ops,
                            void *usb_ctx,
                            const xBRIDGE_GPIO_Peripheral_Ops_t *gpio_ops,
                            void *gpio_ctx)
{
    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (usb_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (gpio_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    (void)memset(ctx, 0, sizeof(xBRIDGE_GPIO_Context_t));

    ctx->usb_ops = usb_ops;
    ctx->usb_ctx = usb_ctx;
    ctx->gpio_ops = gpio_ops;
    ctx->gpio_ctx = gpio_ctx;
    ctx->state = xBRIDGE_STATE_IDLE;

    return xRETURN_OK;
}

xRETURN_t xBRIDGE_GPIO_On_USB_Receive(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *data, uint32_t length)
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
    case xBRIDGE_GPIO_CMD_SET_DIRECTION:
        return handle_set_direction(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_GPIO_CMD_WRITE:
        return handle_write(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_GPIO_CMD_READ:
        return handle_read(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_GPIO_CMD_SET_PULL:
        return handle_set_pull(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_GPIO_CMD_TOGGLE:
        return handle_toggle(ctx, payload, cmd.length, cmd.seq);

    default:
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, cmd.seq);
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
