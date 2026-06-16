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

// @file xbridge_i2c.c
// @brief xBRIDGE I2C channel - WINUSB binary frame I2C controller bridge implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xbridge_i2c.h"
#include "xbridge_core.h"
#include "xassert.h"

#include "xbridge_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t handle_write(xBRIDGE_I2C_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_read(xBRIDGE_I2C_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_write_read(xBRIDGE_I2C_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_scan(xBRIDGE_I2C_Context_t *ctx, uint32_t seq);
static xRETURN_t handle_set_speed(xBRIDGE_I2C_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t send_status_response(xBRIDGE_I2C_Context_t *ctx, uint8_t status, uint32_t seq);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t send_status_response(xBRIDGE_I2C_Context_t *ctx, uint8_t status, uint32_t seq)
{
    uint32_t resp_len = xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_I2C, status, seq, NULL, 0U);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

static xRETURN_t handle_write(xBRIDGE_I2C_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 4U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint16_t addr = payload[0];
    uint8_t flags = payload[1];
    uint16_t data_len = (uint16_t)((uint16_t)payload[2] | ((uint16_t)payload[3] << 8U));
    bool no_stop = ((flags & xBRIDGE_I2C_FLAG_NO_STOP) != 0U);

    if ((uint32_t)(4U + data_len) > payload_len)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    xRETURN_t ret = ctx->i2c_ops->write(ctx->i2c_ctx, addr, payload + 4U, data_len, no_stop);

    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

static xRETURN_t handle_read(xBRIDGE_I2C_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 4U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint16_t addr = payload[0];
    uint16_t data_len = (uint16_t)((uint16_t)payload[2] | ((uint16_t)payload[3] << 8U));

    if (data_len > xBRIDGE_MAX_PAYLOAD_BYTES)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    xRETURN_t ret = ctx->i2c_ops->read(ctx->i2c_ctx, addr, ctx->resp_buf + sizeof(xBRIDGE_Frame_Resp_t), data_len);

    if (ret != xRETURN_OK)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t resp_len = xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_I2C, xBRIDGE_STATUS_OK, seq,
                                                    ctx->resp_buf + sizeof(xBRIDGE_Frame_Resp_t), data_len);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

static xRETURN_t handle_write_read(xBRIDGE_I2C_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 6U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint16_t addr = payload[0];
    uint16_t wlen = (uint16_t)((uint16_t)payload[2] | ((uint16_t)payload[3] << 8U));
    uint16_t rlen = (uint16_t)((uint16_t)payload[4] | ((uint16_t)payload[5] << 8U));

    if (((uint32_t)(6U + wlen) > payload_len) || (rlen > xBRIDGE_MAX_PAYLOAD_BYTES))
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint8_t *rdata = ctx->resp_buf + sizeof(xBRIDGE_Frame_Resp_t);
    xRETURN_t ret = ctx->i2c_ops->write_read(ctx->i2c_ctx, addr, payload + 6U, wlen, rdata, rlen);

    if (ret != xRETURN_OK)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t resp_len =
        xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_I2C, xBRIDGE_STATUS_OK, seq, rdata, rlen);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

static xRETURN_t handle_scan(xBRIDGE_I2C_Context_t *ctx, uint32_t seq)
{
    uint8_t found[128U];
    uint32_t count = 0U;

    for (uint32_t addr = 1U; addr <= 127U; addr++)
    {
        uint8_t dummy = 0U;
        xRETURN_t result = ctx->i2c_ops->read(ctx->i2c_ctx, (uint16_t)addr, &dummy, 1U);

        if (result == xRETURN_OK)
        {
            found[count] = (uint8_t)addr;
            count++;
        }
    }

    uint32_t resp_len =
        xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_I2C, xBRIDGE_STATUS_OK, seq, found, count);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

static xRETURN_t handle_set_speed(xBRIDGE_I2C_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 4U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t hz = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);

    xRETURN_t ret = ctx->i2c_ops->set_speed(ctx->i2c_ctx, hz);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBRIDGE_I2C_Init(xBRIDGE_I2C_Context_t *ctx,
                           const xBRIDGE_USB_Ops_t *usb_ops,
                           void *usb_ctx,
                           const xBRIDGE_I2C_Peripheral_Ops_t *i2c_ops,
                           void *i2c_ctx)
{
    xASSERT(ctx != NULL, "ctx is NULL");
    xASSERT(usb_ops != NULL, "usb_ops is NULL");
    xASSERT(i2c_ops != NULL, "i2c_ops is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (usb_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (i2c_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    (void)memset(ctx, 0, sizeof(xBRIDGE_I2C_Context_t));

    ctx->usb_ops = usb_ops;
    ctx->usb_ctx = usb_ctx;
    ctx->i2c_ops = i2c_ops;
    ctx->i2c_ctx = i2c_ctx;
    ctx->state = xBRIDGE_STATE_IDLE;

    return xRETURN_OK;
}

xRETURN_t xBRIDGE_I2C_On_USB_Receive(xBRIDGE_I2C_Context_t *ctx, const uint8_t *data, uint32_t length)
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
    case xBRIDGE_I2C_CMD_WRITE:
        return handle_write(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_I2C_CMD_READ:
        return handle_read(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_I2C_CMD_WRITE_READ:
        return handle_write_read(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_I2C_CMD_SCAN:
        return handle_scan(ctx, cmd.seq);

    case xBRIDGE_I2C_CMD_SET_SPEED:
        return handle_set_speed(ctx, payload, cmd.length, cmd.seq);

    default:
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, cmd.seq);
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
