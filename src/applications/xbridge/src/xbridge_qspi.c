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

// @file xbridge_qspi.c
// @brief xBRIDGE QSPI channel - WINUSB binary frame Quad-SPI bridge implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xbridge_qspi.h"
#include "xbridge_core.h"
#include "xassert.h"

#include "xbridge_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t handle_transfer(xBRIDGE_QSPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_set_speed(xBRIDGE_QSPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t send_status_response(xBRIDGE_QSPI_Context_t *ctx, uint8_t status, uint32_t seq);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t send_status_response(xBRIDGE_QSPI_Context_t *ctx, uint8_t status, uint32_t seq)
{
    uint32_t resp_len = xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_QSPI, status, seq, NULL, 0U);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

static xRETURN_t handle_transfer(xBRIDGE_QSPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    // Minimum payload: mode(1) + dummy_cycles(1) + cmd(4) + addr(4) + len(2) = 12 bytes
    if (payload_len < 12U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint8_t mode = payload[0];
    uint8_t dummy_cycles = payload[1];
    uint32_t qspi_cmd = (uint32_t)payload[2] | ((uint32_t)payload[3] << 8U) | ((uint32_t)payload[4] << 16U) | ((uint32_t)payload[5] << 24U);
    uint32_t addr = (uint32_t)payload[6] | ((uint32_t)payload[7] << 8U) | ((uint32_t)payload[8] << 16U) | ((uint32_t)payload[9] << 24U);
    uint16_t data_len = (uint16_t)((uint16_t)payload[10] | ((uint16_t)payload[11] << 8U));

    if (data_len > xBRIDGE_MAX_PAYLOAD_BYTES)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    const uint8_t *tx_data = (data_len > 0U) ? (payload + 12U) : NULL;
    uint8_t *rx_data = ctx->resp_buf + sizeof(xBRIDGE_Frame_Resp_t);

    xRETURN_t ret = ctx->qspi_ops->transfer(ctx->qspi_ctx, mode, dummy_cycles, qspi_cmd, addr, tx_data, rx_data, data_len);

    if (ret != xRETURN_OK)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t resp_len =
        xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_QSPI, xBRIDGE_STATUS_OK, seq, rx_data, data_len);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

static xRETURN_t handle_set_speed(xBRIDGE_QSPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 4U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t hz = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);

    xRETURN_t ret = ctx->qspi_ops->set_speed(ctx->qspi_ctx, hz);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBRIDGE_QSPI_Init(xBRIDGE_QSPI_Context_t *ctx,
                            const xBRIDGE_USB_Ops_t *usb_ops,
                            void *usb_ctx,
                            const xBRIDGE_QSPI_Peripheral_Ops_t *qspi_ops,
                            void *qspi_ctx)
{
    xASSERT(ctx != NULL, "ctx is NULL");
    xASSERT(usb_ops != NULL, "usb_ops is NULL");
    xASSERT(qspi_ops != NULL, "qspi_ops is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (usb_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (qspi_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    (void)memset(ctx, 0, sizeof(xBRIDGE_QSPI_Context_t));

    ctx->usb_ops = usb_ops;
    ctx->usb_ctx = usb_ctx;
    ctx->qspi_ops = qspi_ops;
    ctx->qspi_ctx = qspi_ctx;
    ctx->state = xBRIDGE_STATE_IDLE;

    return xRETURN_OK;
}

xRETURN_t xBRIDGE_QSPI_On_USB_Receive(xBRIDGE_QSPI_Context_t *ctx, const uint8_t *data, uint32_t length)
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
    case xBRIDGE_QSPI_CMD_TRANSFER:
        return handle_transfer(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_QSPI_CMD_SET_SPEED:
        return handle_set_speed(ctx, payload, cmd.length, cmd.seq);

    default:
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, cmd.seq);
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
