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

// @file xusbd_msc.c
// @brief xUSB Mass Storage Class (MSC) driver implementation.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "string.h"
#include "xusbd_return.h"
#include "xusbd_class.h"
#include "xusbd_msc.h"

#include "xusbd_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////
#define xUSBD_MSC_BOT_WAIT_CBW       0x00U
#define xUSBD_MSC_BOT_DATA_OUT       0x01U
#define xUSBD_MSC_BOT_DATA_IN        0x02U
#define xUSBD_MSC_BOT_SEND_CSW       0x03U
#define xUSBD_MSC_BOT_ERROR          0x04U
#define xUSBD_MSC_BOT_IDLE           0x05U
#define xUSBD_MSC_BOT_COMMAND_PASSED 0x00U
#define xUSBD_MSC_BOT_COMMAND_FAILED 0x01U
#define xUSBD_MSC_BOT_PHASE_ERROR    0x02U
#define xUSBD_MSC_BOT_OUT            0x00U
#define xUSBD_MSC_BOT_IN             0x80U

// TYPES //////////////////////////////////////////////////////////////////////////
typedef struct
{
    uint8_t *response;
    uint8_t *write_address;
    uint32_t response_length;
    uint32_t write_length;
} xUSBD_MSC_Command_Result_t;

typedef struct
{
    uint32_t data_length;
    uint8_t flags;
} xUSBD_MSC_CBW_Info_t;

typedef struct
{
    uint32_t block_offset;
    uint32_t number_of_blocks;
    uint32_t transfer_length;
} xUSBD_MSC_Block_Request_t;

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static inline void write_be32(uint8_t *p, uint32_t v);
static xRETURN_t msc_init_instance(xUSBD_Class_Context_t *class_ctx);
static uint32_t msc_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed);
static xRETURN_t msc_prepare_to_receive(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);
static xRETURN_t msc_transmit(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);
static xRETURN_t msc_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
static xRETURN_t msc_control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);
static xRETURN_t msc_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t msc_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t msc_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xUSBD_MSC_Callbacks_t *msc_callbacks(xUSBD_Class_Context_t *class_ctx);
static void msc_clear_sense(xUSBD_MSC_Context_t *ctx);
static void msc_set_sense(xUSBD_MSC_Context_t *ctx, uint8_t key, uint8_t asc, uint8_t ascq);
static void msc_prepare_csw(xUSBD_MSC_Context_t *ctx);
static void msc_set_csw_residue(xUSBD_MSC_Context_t *ctx, uint32_t residue);
static xRETURN_t msc_send_current_csw(xUSBD_Class_Context_t *class_ctx);
static xRETURN_t msc_start_cbw_receive(xUSBD_Class_Context_t *class_ctx);
static xRETURN_t msc_init_bulk_endpoints(xUSBD_Class_Context_t *class_ctx);
static xRETURN_t msc_reset_bulk_transport(xUSBD_Class_Context_t *class_ctx);
static uint8_t msc_rw10_request_decode(xUSBD_MSC_Context_t *ctx, xUSBD_MSC_Block_Request_t *request);
static void msc_read10_process(xUSBD_Class_Context_t *class_ctx,
                               xUSBD_MSC_Callbacks_t *callbacks,
                               uint32_t cbw_data_length,
                               xUSBD_MSC_Command_Result_t *result);
static void msc_write10_process(xUSBD_Class_Context_t *class_ctx,
                                xUSBD_MSC_Callbacks_t *callbacks,
                                uint32_t cbw_data_length,
                                xUSBD_MSC_Command_Result_t *result);
static xRETURN_t msc_cbw_received(xUSBD_Class_Context_t *class_ctx, xUSBD_MSC_Callbacks_t *callbacks, uint32_t length);
static xRETURN_t msc_cbw_validate(xUSBD_Class_Context_t *class_ctx, uint32_t length, xUSBD_MSC_CBW_Info_t *cbw_info, uint8_t *is_valid);
static bool msc_scsi_handle_data_commands(xUSBD_MSC_Context_t *ctx,
                                          xUSBD_MSC_Callbacks_t *callbacks,
                                          xUSBD_MSC_Command_Result_t *result,
                                          uint8_t opcode);
static bool msc_scsi_handle_stub_commands(xUSBD_MSC_Context_t *ctx, uint8_t opcode);
static void msc_scsi_command_process(xUSBD_Class_Context_t *class_ctx,
                                     xUSBD_MSC_Callbacks_t *callbacks,
                                     uint32_t cbw_data_length,
                                     xUSBD_MSC_Command_Result_t *result);
static xRETURN_t msc_data_phase_start(xUSBD_Class_Context_t *class_ctx,
                                      uint32_t cbw_data_length,
                                      uint8_t cbw_flags,
                                      xUSBD_MSC_Command_Result_t *command_result);
static xRETURN_t msc_data_out_received(xUSBD_Class_Context_t *class_ctx, xUSBD_MSC_Callbacks_t *callbacks);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static inline void write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (v >> 24) & 0xFFU;
    p[1] = (v >> 16) & 0xFFU;
    p[2] = (v >> 8) & 0xFFU;
    p[3] = v & 0xFFU;
}

static xUSBD_MSC_Callbacks_t *msc_callbacks(xUSBD_Class_Context_t *class_ctx)
{
    return xUSBD_CLASS_CALLBACKS(class_ctx, xUSBD_MSC_Callbacks_t);
}

static xRETURN_t msc_init_instance(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    xRETURN_t s = xUSBD_Class_Allocate_Interface(class_ctx, &ctx->interface);
    if (s != xRETURN_OK)
    {
        return s;
    }
    s = xUSBD_Class_Allocate_Endpoint(class_ctx, USB_EP_DIR_IN, &ctx->in_ep);
    if (s != xRETURN_OK)
    {
        return s;
    }
    s = xUSBD_Class_Allocate_Endpoint(class_ctx, USB_EP_DIR_OUT, &ctx->out_ep);
    if (s != xRETURN_OK)
    {
        return s;
    }
    return xRETURN_OK;
}

static uint32_t msc_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed)
{
    uint16_t ep_mps = ep_max_mps(speed, USB_ENDP_TYPE_BULK);
    (void)xUSBD_Class_Set_EP_MPS(class_ctx, ep_mps);
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    uint8_t *ptr = buffer;

    ptr = build_interface_descriptor(ptr, ctx->interface, 0, 2, USB_CLASS_STORAGE, USB_MSC_SCSI_TRANSPARENT_COMMAND_SET, USB_MSC_BBB,
                                     class_ctx->interface_string_index);

    ptr = build_endpoint_descriptor(ptr, ctx->out_ep, USB_ENDP_TYPE_BULK, ep_mps, 0x00, speed, 0, 0, 0);
    ptr = build_endpoint_descriptor(ptr, ctx->in_ep, USB_ENDP_TYPE_BULK, ep_mps, 0x00, speed, 0, 0, 0);

    return (uint32_t)(ptr - buffer);
}

static xRETURN_t msc_prepare_to_receive(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    return xUSBD_Class_DCD_EP_Receive(class_ctx, ctx->out_ep, data, length);
}

static xRETURN_t msc_transmit(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    return xUSBD_Class_DCD_EP_Send(class_ctx, ctx->in_ep, data, length, false);
}

static void msc_clear_sense(xUSBD_MSC_Context_t *ctx)
{
    msc_set_sense(ctx, 0x00U, 0x00U, 0x00U);
}

static void msc_set_sense(xUSBD_MSC_Context_t *ctx, uint8_t key, uint8_t asc, uint8_t ascq)
{
    ctx->sense_key = key;
    ctx->sense_asc = asc;
    ctx->sense_ascq = ascq;
}

static void msc_prepare_csw(xUSBD_MSC_Context_t *ctx)
{
    memcpy(ctx->csw.dCSWSignature, "USBS", 4);
    memcpy(ctx->csw.dCSWTag, ctx->cbw.dCBWTag, sizeof(ctx->csw.dCSWTag));
}

static void msc_set_csw_residue(xUSBD_MSC_Context_t *ctx, uint32_t residue)
{
    memcpy(ctx->csw.dCSWDataResidue, &residue, sizeof(ctx->csw.dCSWDataResidue));
}

static xRETURN_t msc_send_current_csw(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    ctx->state = xUSBD_MSC_BOT_SEND_CSW;
    return msc_transmit(class_ctx, (uint8_t *)&ctx->csw, USB_MSC_BOT_CSW_LENGTH);
}

static xRETURN_t msc_start_cbw_receive(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    ctx->state = xUSBD_MSC_BOT_WAIT_CBW;
    return msc_prepare_to_receive(class_ctx, (uint8_t *)&ctx->cbw, USB_MSC_BOT_CBW_LENGTH);
}

static xRETURN_t msc_init_bulk_endpoints(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    uint16_t ep_mps = 0U;
    xRETURN_t status = xUSBD_Class_Get_EP_MPS(class_ctx, &ep_mps);
    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xUSBD_Class_DCD_EP_Init(class_ctx, ctx->out_ep, USB_ENDP_TYPE_BULK, ep_mps);
    if (status != xRETURN_OK)
    {
        return status;
    }

    return xUSBD_Class_DCD_EP_Init(class_ctx, ctx->in_ep, USB_ENDP_TYPE_BULK, ep_mps);
}

static xRETURN_t msc_reset_bulk_transport(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    xRETURN_t status = msc_init_bulk_endpoints(class_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xUSBD_Class_DCD_EP_Clear_Stall(class_ctx, ctx->in_ep);
    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xUSBD_Class_DCD_EP_Clear_Stall(class_ctx, ctx->out_ep);
    if (status != xRETURN_OK)
    {
        return status;
    }

    msc_clear_sense(ctx);
    return msc_start_cbw_receive(class_ctx);
}

static uint8_t msc_rw10_request_decode(xUSBD_MSC_Context_t *ctx, xUSBD_MSC_Block_Request_t *request)
{
    request->block_offset = ((uint32_t)ctx->cbw.CBWCB[2] << 24) | ((uint32_t)ctx->cbw.CBWCB[3] << 16) | ((uint32_t)ctx->cbw.CBWCB[4] << 8) |
                            (uint32_t)ctx->cbw.CBWCB[5];
    request->number_of_blocks = ((uint32_t)ctx->cbw.CBWCB[7] << 8) | (uint32_t)ctx->cbw.CBWCB[8];
    request->transfer_length = request->number_of_blocks * ctx->msc_capacity.block_size;

    if (request->block_offset + request->number_of_blocks > ctx->msc_capacity.number_of_blocks)
    {
        msc_set_sense(ctx, 0x05U, 0x21U, 0x00U);
        return 0U;
    }

    return 1U;
}

static void msc_read10_process(xUSBD_Class_Context_t *class_ctx,
                               xUSBD_MSC_Callbacks_t *callbacks,
                               uint32_t cbw_data_length,
                               xUSBD_MSC_Command_Result_t *result)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    xUSBD_MSC_Block_Request_t block_request;

    if (!msc_rw10_request_decode(ctx, &block_request))
    {
        return;
    }

    if (cbw_data_length == block_request.transfer_length)
    {
        xUSBD_MSC_ADDR_t address_data;
        address_data.block_offset = block_request.block_offset;
        address_data.number_of_blocks = block_request.number_of_blocks;

        xRETURN_t callback_status =
            callbacks->on_io_control(class_ctx, xUSBD_MSC_IO_CMD_GET_READ_ADDR, &address_data, sizeof(xUSBD_MSC_ADDR_t),
                                     (void **)&result->response, &result->response_length);

        if (callback_status == xRETURN_OK)
        {
            ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        }
        else
        {
            msc_set_sense(ctx, 0x03U, 0x11U, 0x00U);
        }

        result->response_length = block_request.transfer_length;
    }
}

static void msc_write10_process(xUSBD_Class_Context_t *class_ctx,
                                xUSBD_MSC_Callbacks_t *callbacks,
                                uint32_t cbw_data_length,
                                xUSBD_MSC_Command_Result_t *result)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    xUSBD_MSC_Block_Request_t block_request;

    if (!msc_rw10_request_decode(ctx, &block_request))
    {
        return;
    }

    if (cbw_data_length == block_request.transfer_length)
    {
        xUSBD_MSC_ADDR_t address_data;
        address_data.block_offset = block_request.block_offset;
        address_data.number_of_blocks = block_request.number_of_blocks;

        xRETURN_t callback_status =
            callbacks->on_io_control(class_ctx, xUSBD_MSC_IO_CMD_GET_WRITE_ADDR, &address_data, sizeof(xUSBD_MSC_ADDR_t),
                                     (void **)&result->write_address, &result->write_length);

        if (callback_status == xRETURN_OK)
        {
            ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        }
        else
        {
            msc_set_sense(ctx, 0x03U, 0x0CU, 0x00U);
        }

        result->write_length = block_request.transfer_length;
    }
}

static xRETURN_t msc_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    xUSBD_MSC_Callbacks_t *callbacks = msc_callbacks(class_ctx);
    const USB_Setup_Request_t *req = &class_ctx->device_ctx->request;

    if (xU16_LOW_BYTE(req->wIndex) != ctx->interface)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    if (callbacks == NULL)
    {
        return xRETURN_xERR_xUSBD_APP_NOT_INSTALLED;
    }

    xRETURN_t status = xRETURN_OK;
    uint32_t response_length = 0;
    uint32_t *max_lun = NULL;

    switch (req->bRequest)
    {
    case USB_MSC_REQ_GET_MAX_LUN:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_REQ_GET_MAX_LUN, "MSC get max lun");
        status = callbacks->on_io_control(class_ctx, xUSBD_MSC_IO_CMD_GET_LUN, NULL, 0, (void **)&max_lun, &response_length);
        if (status == xRETURN_OK)
        {
            ctx->temp[0] = *max_lun - 1;
            response->data = ctx->temp;
            response->length = 1;
        }
        break;
    default:
        status = xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
        break;
    }

    return status;
}

static xRETURN_t msc_control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    (void)data;
    (void)length;
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    const USB_Setup_Request_t *req = &class_ctx->device_ctx->request;

    if (xU16_LOW_BYTE(req->wIndex) != ctx->interface)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    xRETURN_t status = xRETURN_OK;

    switch (req->bRequest)
    {
    case USB_MSC_REQ_SET_BOMSR:
    {
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_REQ_RESET, "MSC reset");
        status = msc_reset_bulk_transport(class_ctx);
        if (status != xRETURN_OK)
        {
            return status;
        }
        break;
    }
    default:
        status = xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
        break;
    }

    return status;
}

static bool msc_scsi_handle_data_commands(xUSBD_MSC_Context_t *ctx,
                                          xUSBD_MSC_Callbacks_t *callbacks,
                                          xUSBD_MSC_Command_Result_t *result,
                                          uint8_t opcode)
{
    xRETURN_t callback_status = xRETURN_OK;

    switch (opcode)
    {
    case USB_MSC_REQUEST_SENSE:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_REQUEST_SENSE, "MSC request sense");
        memset(ctx->temp, 0, 18);
        ctx->temp[0] = 0x70;
        ctx->temp[7] = 0x0A;
        ctx->temp[2] = ctx->sense_key;
        ctx->temp[12] = ctx->sense_asc;
        ctx->temp[13] = ctx->sense_ascq;
        msc_clear_sense(ctx);
        result->response = ctx->temp;
        result->response_length = 18;
        ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        return true;
    case USB_MSC_INQUIRY:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_INQUIRY, "MSC inquiry");
        callback_status = callbacks->on_io_control((xUSBD_Class_Context_t *)ctx, xUSBD_MSC_IO_CMD_INQUIRY, NULL, 0,
                                                   (void **)&result->response, &result->response_length);
        if (callback_status == xRETURN_OK)
        {
            ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        }
        else
        {
            msc_set_sense(ctx, 0x05U, 0x20U, 0x00U);
        }
        return true;
    case USB_MSC_MODE_SENSE6:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_MODE_SENSE6, "MSC sense6");
        memset(ctx->temp, 0, 12);
        ctx->temp[0] = 0x0B;
        ctx->temp[3] = 0x08;
        write_be32(&ctx->temp[4], ctx->msc_capacity.number_of_blocks);
        write_be32(&ctx->temp[8], ctx->msc_capacity.block_size);
        result->response = ctx->temp;
        result->response_length = 12;
        ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        return true;
    case USB_MSC_READ_FORMAT_CAPACITIES:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_READ_FORMAT_CAPACITIES, "MSC format capacity");
        memset(ctx->temp, 0, 12);
        ctx->temp[3] = 0x08;
        write_be32(&ctx->temp[4], ctx->msc_capacity.number_of_blocks);
        ctx->temp[8] = 0x02;
        ctx->temp[9] = (ctx->msc_capacity.block_size >> 16) & 0xFFU;
        ctx->temp[10] = (ctx->msc_capacity.block_size >> 8) & 0xFFU;
        ctx->temp[11] = ctx->msc_capacity.block_size & 0xFFU;
        result->response = ctx->temp;
        result->response_length = 12;
        ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        return true;
    case USB_MSC_READ_CAPACITY:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_READ_CAPACITY, "MSC capacity");
        write_be32(&ctx->temp[0], ctx->msc_capacity.number_of_blocks - 1U);
        write_be32(&ctx->temp[4], ctx->msc_capacity.block_size);
        result->response = ctx->temp;
        result->response_length = 8;
        ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        return true;
    case USB_MSC_MODE_SENSE10:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_MODE_SENSE10, "MSC sense10");
        memset(ctx->temp, 0, 16);
        ctx->temp[1] = 0x0E;
        ctx->temp[7] = 0x08;
        write_be32(&ctx->temp[8], ctx->msc_capacity.number_of_blocks);
        write_be32(&ctx->temp[12], ctx->msc_capacity.block_size);
        result->response = ctx->temp;
        result->response_length = 16;
        ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        return true;
    default:
        return false;
    }
}

static bool msc_scsi_handle_stub_commands(xUSBD_MSC_Context_t *ctx, uint8_t opcode)
{
    (void)ctx;
    switch (opcode)
    {
    case USB_MSC_REWIND:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_REWIND, "MSC rewind");
        break;
    case USB_MSC_FORMAT:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_FORMAT, "MSC format");
        break;
    case USB_MSC_MODE_SELECT6:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_MODE_SELECT6, "MSC select6");
        break;
    case USB_MSC_RELEASE6:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_RELEASE6, "MSC release6");
        break;
    case USB_MSC_SEEK10:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_SEEK10, "MSC seek10");
        break;
    case USB_MSC_WRITE_AND_VERIFY10:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_WRITE_VERIFY10, "MSC write verify10");
        break;
    case USB_MSC_SYNCHRONIZE_CACHE:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_SYNCHRONIZE_CACHE, "MSC cache");
        break;
    case USB_MSC_READ12:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_READ12, "MSC read12");
        break;
    case USB_MSC_WRITE12:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_WRITE12, "MSC write12");
        break;
    case USB_MSC_SEND_DIAGNOSTIC:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_SEND_DIAGNOSTIC, "MSC diagnostic");
        break;
    default:
        return false;
    }
    return true;
}

static void msc_scsi_command_process(xUSBD_Class_Context_t *class_ctx,
                                     xUSBD_MSC_Callbacks_t *callbacks,
                                     uint32_t cbw_data_length,
                                     xUSBD_MSC_Command_Result_t *result)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    uint8_t opcode = ctx->cbw.CBWCB[0];

    if (msc_scsi_handle_data_commands(ctx, callbacks, result, opcode) == true)
    {
        return;
    }
    if (msc_scsi_handle_stub_commands(ctx, opcode) == true)
    {
        return;
    }

    switch (opcode)
    {
    case USB_MSC_TEST_UNIT_READY:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_TEST_UNIT_READY, "MSC ready");
        ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        break;
    case USB_MSC_START_STOP_UNIT:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_START_STOP_UNIT, "MSC stop unit");
        ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        break;
    case USB_MSC_PREVENT_ALLOW_MEDIUM_REMOVAL:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_PREVENT_REMOVAL, "MSC removal");
        ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        break;
    case USB_MSC_VERIFY10:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_VERIFY10, "MSC verify10");
        ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_PASSED;
        break;
    case USB_MSC_READ10:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_READ10, "MSC read10");
        msc_read10_process(class_ctx, callbacks, cbw_data_length, result);
        break;
    case USB_MSC_WRITE10:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_CMD_WRITE10, "MSC write");
        msc_write10_process(class_ctx, callbacks, cbw_data_length, result);
        break;
    default:
        xUSBD_LOG(xRETURN_xERR_xUSBD_MSC_CMD_UNKNOWN, "MSC unknown");
        break;
    }
}

static xRETURN_t msc_data_phase_start(xUSBD_Class_Context_t *class_ctx,
                                      uint32_t cbw_data_length,
                                      uint8_t cbw_flags,
                                      xUSBD_MSC_Command_Result_t *command_result)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    uint32_t residue = 0;
    xRETURN_t status;

    if (cbw_data_length == 0U)
    {
        msc_set_csw_residue(ctx, 0U);
        return msc_send_current_csw(class_ctx);
    }

    if (cbw_flags & xUSBD_MSC_BOT_IN)
    {
        if (command_result->response_length > cbw_data_length)
        {
            command_result->response_length = cbw_data_length;
        }
        if (command_result->response_length < cbw_data_length)
        {
            residue = cbw_data_length - command_result->response_length;
        }

        ctx->state = xUSBD_MSC_BOT_DATA_IN;
        status = msc_transmit(class_ctx, command_result->response, command_result->response_length);
    }
    else
    {
        if (command_result->write_length > cbw_data_length)
        {
            command_result->write_length = cbw_data_length;
        }
        if (command_result->write_length < cbw_data_length)
        {
            residue = cbw_data_length - command_result->write_length;
        }

        ctx->state = xUSBD_MSC_BOT_DATA_OUT;
        status = msc_prepare_to_receive(class_ctx, command_result->write_address, command_result->write_length);
    }

    if (status != xRETURN_OK)
    {
        return status;
    }

    msc_set_csw_residue(ctx, residue);
    return xRETURN_OK;
}

static xRETURN_t msc_cbw_validate(xUSBD_Class_Context_t *class_ctx, uint32_t length, xUSBD_MSC_CBW_Info_t *cbw_info, uint8_t *is_valid)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    uint32_t signature;
    xRETURN_t status;

    memcpy(&signature, ctx->cbw.dCBWSignature, sizeof(signature));
    memcpy(&cbw_info->data_length, ctx->cbw.dCBWDataLength, sizeof(cbw_info->data_length));
    cbw_info->flags = ctx->cbw.bmCBWFlags;

    *is_valid = (length == USB_MSC_BOT_CBW_LENGTH && signature == USB_MSC_BOT_CBW_SIGNATURE) ? 1U : 0U;
    if (*is_valid)
    {
        return xRETURN_OK;
    }

    ctx->state = xUSBD_MSC_BOT_ERROR;
    status = xUSBD_Class_DCD_EP_Stall(class_ctx, ctx->in_ep);
    if (status == xRETURN_OK)
    {
        status = xUSBD_Class_DCD_EP_Stall(class_ctx, ctx->out_ep);
    }
    if (status != xRETURN_OK)
    {
        return status;
    }

    xUSBD_LOG(xRETURN_xERR_xUSBD_MSC_CBW_SIGNATURE, "dCBWSignature failed");
    return xRETURN_OK;
}

static xRETURN_t msc_cbw_received(xUSBD_Class_Context_t *class_ctx, xUSBD_MSC_Callbacks_t *callbacks, uint32_t length)
{
    xRETURN_t status;
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;

    xUSBD_MSC_Command_Result_t command_result = {0};
    xUSBD_MSC_CBW_Info_t cbw_info = {0};
    uint8_t is_cbw_valid = 0U;

    ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_FAILED;

    status = msc_cbw_validate(class_ctx, length, &cbw_info, &is_cbw_valid);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (!is_cbw_valid)
    {
        return xRETURN_OK;
    }

    msc_scsi_command_process(class_ctx, callbacks, cbw_info.data_length, &command_result);

    msc_prepare_csw(ctx);

    if (ctx->csw.bCSWStatus == xUSBD_MSC_BOT_COMMAND_PASSED)
    {
        status = msc_data_phase_start(class_ctx, cbw_info.data_length, cbw_info.flags, &command_result);
        if (status != xRETURN_OK)
        {
            return status;
        }
    }
    else
    {
        // SCSI command failed: report residue = full requested length and send FAILED csw
        xUSBD_LOG(xRETURN_xERR_xUSBD_MSC_BOT_CMD_FAILED, "bot cmd failed");
        msc_set_csw_residue(ctx, cbw_info.data_length);
        status = msc_send_current_csw(class_ctx);
        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    return xRETURN_OK;
}

static xRETURN_t msc_data_out_received(xUSBD_Class_Context_t *class_ctx, xUSBD_MSC_Callbacks_t *callbacks)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    xUSBD_LOG(xRETURN_xMSG_xUSBD_MSC_BOT_OUT, "bot out");

    uint32_t write_status = callbacks->on_io_control(class_ctx, xUSBD_MSC_IO_CMD_WRITE_COMPLETE, NULL, 0, NULL, NULL);
    if (write_status != xRETURN_OK)
    {
        ctx->csw.bCSWStatus = xUSBD_MSC_BOT_COMMAND_FAILED;
        msc_set_sense(ctx, 0x03U, 0x0CU, 0x00U);
    }

    return msc_send_current_csw(class_ctx);
}

static xRETURN_t msc_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)data;
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    xUSBD_MSC_Callbacks_t *callbacks = msc_callbacks(class_ctx);

    if (ep_addr != ctx->out_ep)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    if (callbacks == NULL)
    {
        return xRETURN_xERR_xUSBD_APP_NOT_INSTALLED;
    }

    if (ctx->state == xUSBD_MSC_BOT_WAIT_CBW)
    {
        return msc_cbw_received(class_ctx, callbacks, length);
    }

    if (ctx->state == xUSBD_MSC_BOT_DATA_OUT)
    {
        return msc_data_out_received(class_ctx, callbacks);
    }

    return xRETURN_OK;
}

static xRETURN_t msc_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)data;
    (void)length;
    xRETURN_t status;
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;

    if (ep_addr != ctx->in_ep)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    if (ctx->state == xUSBD_MSC_BOT_SEND_CSW)
    {
        status = msc_start_cbw_receive(class_ctx);
        if (status != xRETURN_OK)
        {
            return status;
        }
    }
    else if (ctx->state == xUSBD_MSC_BOT_DATA_IN)
    {
        status = msc_send_current_csw(class_ctx);
        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    return xRETURN_OK;
}

static xRETURN_t msc_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    xUSBD_MSC_Context_t *ctx = (xUSBD_MSC_Context_t *)class_ctx;
    xUSBD_MSC_Callbacks_t *callbacks = msc_callbacks(class_ctx);

    xRETURN_t status = xRETURN_OK;

    if (callbacks == NULL)
    {
        return xRETURN_xERR_xUSBD_APP_NOT_INSTALLED;
    }

    // Forward BUS events to APP
    status = callbacks->on_bus_event(class_ctx, event);

    if (event == USB_DCD_CONNECT_RECEIVED || event == USB_DCD_SPEED_CHANGE_RECEIVED)
    {
        xRETURN_t dcd_status = msc_init_bulk_endpoints(class_ctx);
        if (dcd_status != xRETURN_OK)
        {
            return dcd_status;
        }

        xUSBD_MSC_Capacity_t *cap = NULL;
        uint32_t cap_length = 0;
        if (callbacks->on_io_control(class_ctx, xUSBD_MSC_IO_CMD_GET_CAPACITY, NULL, 0, (void **)&cap, &cap_length) == xRETURN_OK &&
            cap != NULL)
        {
            ctx->msc_capacity = *cap;
        }

        msc_clear_sense(ctx);
        dcd_status = msc_start_cbw_receive(class_ctx);
        if (dcd_status != xRETURN_OK)
        {
            return dcd_status;
        }
    }
    else if (event == USB_DCD_RESET_RECEIVED || event == USB_DCD_DISCONNECT_RECEIVED)
    {
        ctx->state = xUSBD_MSC_BOT_IDLE;
    }

    return status;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
xUSBD_Class_Driver_t *xUSBD_MSC_Class(void)
{
    static xUSBD_Class_Driver_t s_driver = {
        .init_instance = msc_init_instance,
        .build_descriptor = msc_build_descriptor,
        .bus_event = msc_bus_event,
        .control_in_request = msc_control_in_request,
        .control_out_request = msc_control_out_request,
        .data_received = msc_data_received,
        .transmit_complete = msc_transmit_complete,
    };
    return &s_driver;
}

xRETURN_t xUSBD_MSC_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, xUSBD_MSC_Callbacks_t *callbacks)
{
    return xUSBD_Class_Set_Callbacks(class_ctx, callbacks);
}
// EOF /////////////////////////////////////////////////////////////////////////////
