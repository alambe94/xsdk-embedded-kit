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

// @file xbridge_dap.c
// @brief xBRIDGE CMSIS-DAP channel - ARM debug probe bridge implementation (HID v1 + WINUSB v2).
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xbridge_dap.h"
#include "xassert.h"

#include "xbridge_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static uint32_t handle_dap_info(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len);
static uint32_t handle_dap_host_status(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len);
static uint32_t handle_dap_connect(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len);
static uint32_t handle_dap_disconnect(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len);
static uint32_t handle_dap_swj_clock(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len);
static uint32_t handle_dap_swj_sequence(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len);
static uint32_t handle_dap_reset_target(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len);
static uint32_t handle_dap_delay(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len);
static uint32_t handle_dap_unknown(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static uint32_t handle_dap_info(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len)
{
    (void)req;
    (void)req_len;

    // Minimal DAP_Info response: command ID + capabilities byte
    ctx->resp_buf[0] = xBRIDGE_DAP_ID_INFO;
    ctx->resp_buf[1] = 0x01U; // SWD supported
    return 2U;
}

static uint32_t handle_dap_host_status(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len)
{
    (void)req;
    (void)req_len;

    ctx->resp_buf[0] = xBRIDGE_DAP_ID_HOST_STATUS;
    ctx->resp_buf[1] = 0x00U; // OK
    return 2U;
}

static uint32_t handle_dap_connect(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len)
{
    uint8_t requested = (req_len >= 2U) ? req[1] : (uint8_t)xBRIDGE_DAP_CONNECT_SWD;

    if ((requested == (uint8_t)xBRIDGE_DAP_CONNECT_SWD) || (requested == (uint8_t)xBRIDGE_DAP_CONNECT_JTAG))
    {
        ctx->connect_mode = requested;
    }
    else
    {
        ctx->connect_mode = (uint8_t)xBRIDGE_DAP_CONNECT_NONE;
    }

    ctx->resp_buf[0] = xBRIDGE_DAP_ID_CONNECT;
    ctx->resp_buf[1] = ctx->connect_mode;
    return 2U;
}

static uint32_t handle_dap_disconnect(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len)
{
    (void)req;
    (void)req_len;

    ctx->connect_mode = (uint8_t)xBRIDGE_DAP_CONNECT_NONE;

    ctx->resp_buf[0] = xBRIDGE_DAP_ID_DISCONNECT;
    ctx->resp_buf[1] = 0x00U; // OK
    return 2U;
}

static uint32_t handle_dap_swj_clock(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len)
{
    if ((req_len >= 5U) && (ctx->dap_ops->swj_clock != NULL))
    {
        uint32_t hz = (uint32_t)req[1] | ((uint32_t)req[2] << 8U) | ((uint32_t)req[3] << 16U) | ((uint32_t)req[4] << 24U);

        (void)ctx->dap_ops->swj_clock(ctx->dap_ctx, hz);
        ctx->swj_clock_hz = hz;
    }

    ctx->resp_buf[0] = xBRIDGE_DAP_ID_SWJ_CLOCK;
    ctx->resp_buf[1] = 0x00U; // OK
    return 2U;
}

static uint32_t handle_dap_swj_sequence(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len)
{
    if ((req_len >= 2U) && (ctx->dap_ops->swj_sequence != NULL))
    {
        uint32_t count = req[1];
        uint32_t bytes = (count + 7U) / 8U;

        if ((req_len >= (2U + bytes)))
        {
            (void)ctx->dap_ops->swj_sequence(ctx->dap_ctx, count, req + 2U);
        }
    }

    ctx->resp_buf[0] = xBRIDGE_DAP_ID_SWJ_SEQUENCE;
    ctx->resp_buf[1] = 0x00U; // OK
    return 2U;
}

static uint32_t handle_dap_reset_target(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len)
{
    (void)req;
    (void)req_len;

    if (ctx->dap_ops->reset_target != NULL)
    {
        (void)ctx->dap_ops->reset_target(ctx->dap_ctx);
    }

    ctx->resp_buf[0] = xBRIDGE_DAP_ID_RESET_TARGET;
    ctx->resp_buf[1] = 0x00U; // OK
    ctx->resp_buf[2] = 0x00U; // Execute - no reset extension
    return 3U;
}

static uint32_t handle_dap_delay(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len)
{
    if ((req_len >= 3U) && (ctx->dap_ops->delay_us != NULL))
    {
        uint32_t us = (uint32_t)req[1] | ((uint32_t)req[2] << 8U);
        (void)ctx->dap_ops->delay_us(ctx->dap_ctx, us);
    }

    ctx->resp_buf[0] = xBRIDGE_DAP_ID_DELAY;
    ctx->resp_buf[1] = 0x00U; // OK
    return 2U;
}

static uint32_t handle_dap_unknown(xBRIDGE_DAP_Context_t *ctx, const uint8_t *req, uint32_t req_len)
{
    (void)req_len;

    ctx->resp_buf[0] = req[0];
    ctx->resp_buf[1] = 0xFFU; // error - unknown command
    return 2U;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBRIDGE_DAP_Init(xBRIDGE_DAP_Context_t *ctx,
                           const xBRIDGE_USB_Ops_t *usb_ops,
                           void *usb_ctx,
                           const xBRIDGE_DAP_Peripheral_Ops_t *dap_ops,
                           void *dap_ctx)
{
    xASSERT(ctx != NULL, "ctx is NULL");
    xASSERT(usb_ops != NULL, "usb_ops is NULL");
    xASSERT(dap_ops != NULL, "dap_ops is NULL");

    if (ctx == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (usb_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (dap_ops == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    (void)memset(ctx, 0, sizeof(xBRIDGE_DAP_Context_t));

    ctx->usb_ops = usb_ops;
    ctx->usb_ctx = usb_ctx;
    ctx->dap_ops = dap_ops;
    ctx->dap_ctx = dap_ctx;
    ctx->connect_mode = (uint8_t)xBRIDGE_DAP_CONNECT_NONE;
    ctx->swj_clock_hz = 1000000U;
    ctx->state = xBRIDGE_STATE_IDLE;

    return xRETURN_OK;
}

xRETURN_t xBRIDGE_DAP_On_USB_Receive(xBRIDGE_DAP_Context_t *ctx, const uint8_t *data, uint32_t length)
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

    if (length == 0U)
    {
        return xRETURN_xERR_xBRIDGE_INVALID_ARGUMENT;
    }

    (void)memcpy(ctx->req_buf, data, (length < xBRIDGE_DAP_PACKET_BYTES) ? length : xBRIDGE_DAP_PACKET_BYTES);

    uint8_t cmd_id = ctx->req_buf[0];
    uint32_t resp_len = 0U;

    switch (cmd_id)
    {
    case xBRIDGE_DAP_ID_INFO:
        resp_len = handle_dap_info(ctx, ctx->req_buf, length);
        break;

    case xBRIDGE_DAP_ID_HOST_STATUS:
        resp_len = handle_dap_host_status(ctx, ctx->req_buf, length);
        break;

    case xBRIDGE_DAP_ID_CONNECT:
        resp_len = handle_dap_connect(ctx, ctx->req_buf, length);
        break;

    case xBRIDGE_DAP_ID_DISCONNECT:
        resp_len = handle_dap_disconnect(ctx, ctx->req_buf, length);
        break;

    case xBRIDGE_DAP_ID_SWJ_CLOCK:
        resp_len = handle_dap_swj_clock(ctx, ctx->req_buf, length);
        break;

    case xBRIDGE_DAP_ID_SWJ_SEQUENCE:
        resp_len = handle_dap_swj_sequence(ctx, ctx->req_buf, length);
        break;

    case xBRIDGE_DAP_ID_RESET_TARGET:
        resp_len = handle_dap_reset_target(ctx, ctx->req_buf, length);
        break;

    case xBRIDGE_DAP_ID_DELAY:
        resp_len = handle_dap_delay(ctx, ctx->req_buf, length);
        break;

    default:
        resp_len = handle_dap_unknown(ctx, ctx->req_buf, length);
        break;
    }

    if (resp_len == 0U)
    {
        return xRETURN_xERR_xBRIDGE_UNKNOWN_CMD;
    }

    return ctx->usb_ops->send(ctx->usb_ctx, ctx->resp_buf, resp_len);
}

// EOF /////////////////////////////////////////////////////////////////////////////
