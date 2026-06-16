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

// @file xbridge_spi.c
// @brief xBRIDGE SPI channel - WINUSB binary frame full-duplex SPI controller bridge implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xbridge_spi.h"
#include "xbridge_core.h"
#include "xassert.h"

#include "xbridge_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t handle_transfer(xBRIDGE_SPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_cs_assert(xBRIDGE_SPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_cs_deassert(xBRIDGE_SPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_set_mode(xBRIDGE_SPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t handle_set_speed(xBRIDGE_SPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq);
static xRETURN_t send_status_response(xBRIDGE_SPI_Context_t *ctx, uint8_t status, uint32_t seq);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t send_status_response(xBRIDGE_SPI_Context_t *ctx, uint8_t status, uint32_t seq)
{
    uint32_t resp_len = xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_SPI, status, seq, NULL, 0U);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

static xRETURN_t handle_transfer(xBRIDGE_SPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 4U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint8_t cs_idx = payload[0];
    uint8_t flags = payload[1];
    uint16_t data_len = (uint16_t)((uint16_t)payload[2] | ((uint16_t)payload[3] << 8U));
    bool keep_cs = ((flags & xBRIDGE_SPI_FLAG_KEEP_CS) != 0U);

    if (((uint32_t)(4U + data_len) > payload_len) || (data_len > xBRIDGE_MAX_PAYLOAD_BYTES))
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint8_t *miso = ctx->resp_buf + sizeof(xBRIDGE_Frame_Resp_t);
    xRETURN_t ret = ctx->spi_ops->cs_assert(ctx->spi_ctx, cs_idx);

    if (ret != xRETURN_OK)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    ret = ctx->spi_ops->transfer(ctx->spi_ctx, payload + 4U, miso, data_len);

    if (!keep_cs)
    {
        (void)ctx->spi_ops->cs_deassert(ctx->spi_ctx, cs_idx);
    }

    if (ret != xRETURN_OK)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t resp_len =
        xBRIDGE_Core_Build_Response(ctx->resp_buf, sizeof(ctx->resp_buf), xBRIDGE_CHANNEL_SPI, xBRIDGE_STATUS_OK, seq, miso, data_len);
    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

static xRETURN_t handle_cs_assert(xBRIDGE_SPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 1U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    xRETURN_t ret = ctx->spi_ops->cs_assert(ctx->spi_ctx, payload[0]);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

static xRETURN_t handle_cs_deassert(xBRIDGE_SPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 1U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    xRETURN_t ret = ctx->spi_ops->cs_deassert(ctx->spi_ctx, payload[0]);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

static xRETURN_t handle_set_mode(xBRIDGE_SPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 2U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    xRETURN_t ret = ctx->spi_ops->set_mode(ctx->spi_ctx, payload[0], payload[1]);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

static xRETURN_t handle_set_speed(xBRIDGE_SPI_Context_t *ctx, const uint8_t *payload, uint16_t payload_len, uint32_t seq)
{
    if (payload_len < 4U)
    {
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, seq);
    }

    uint32_t hz = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) | ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);

    xRETURN_t ret = ctx->spi_ops->set_speed(ctx->spi_ctx, hz);
    uint8_t status = (ret == xRETURN_OK) ? xBRIDGE_STATUS_OK : xBRIDGE_STATUS_ERROR;
    return send_status_response(ctx, status, seq);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBRIDGE_SPI_Init(xBRIDGE_SPI_Context_t *ctx,
                           const xBRIDGE_USB_Ops_t *usb_ops,
                           void *usb_ctx,
                           const xBRIDGE_SPI_Peripheral_Ops_t *spi_ops,
                           void *spi_ctx)
{
    xASSERT(ctx != NULL, "ctx is NULL");
    xASSERT(usb_ops != NULL, "usb_ops is NULL");
    xASSERT(spi_ops != NULL, "spi_ops is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (usb_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (spi_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    (void)memset(ctx, 0, sizeof(xBRIDGE_SPI_Context_t));

    ctx->usb_ops = usb_ops;
    ctx->usb_ctx = usb_ctx;
    ctx->spi_ops = spi_ops;
    ctx->spi_ctx = spi_ctx;
    ctx->state = xBRIDGE_STATE_IDLE;

    return xRETURN_OK;
}

xRETURN_t xBRIDGE_SPI_On_USB_Receive(xBRIDGE_SPI_Context_t *ctx, const uint8_t *data, uint32_t length)
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
    case xBRIDGE_SPI_CMD_TRANSFER:
        return handle_transfer(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_SPI_CMD_CS_ASSERT:
        return handle_cs_assert(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_SPI_CMD_CS_DEASSERT:
        return handle_cs_deassert(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_SPI_CMD_SET_MODE:
        return handle_set_mode(ctx, payload, cmd.length, cmd.seq);

    case xBRIDGE_SPI_CMD_SET_SPEED:
        return handle_set_speed(ctx, payload, cmd.length, cmd.seq);

    default:
        return send_status_response(ctx, xBRIDGE_STATUS_ERROR, cmd.seq);
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
