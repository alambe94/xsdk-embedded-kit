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

// @file xusbh_msc.c
// @brief USB host Mass Storage Class Bulk-Only Transport implementation.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xassert.h"
#include "xbytes.h"
#include "xusb_setup.h"
#include "xusbh_descriptor.h"
#include "xusbh_msc.h"

// MACROS /////////////////////////////////////////////////////////////////////////
#define USBH_CONTROL_ENDPOINT_ADDRESS       0U
#define USBH_ENDPOINT_HALT_FEATURE          0U
#define xUSBH_MSC_CBW_SIGNATURE_OFFSET      0U
#define xUSBH_MSC_CBW_TAG_OFFSET            4U
#define xUSBH_MSC_CBW_DATA_LENGTH_OFFSET    8U
#define xUSBH_MSC_CBW_FLAGS_OFFSET          12U
#define xUSBH_MSC_CBW_LUN_OFFSET            13U
#define xUSBH_MSC_CBW_CB_LENGTH_OFFSET      14U
#define xUSBH_MSC_CBW_CB_OFFSET             15U
#define xUSBH_MSC_CSW_SIGNATURE_OFFSET      0U
#define xUSBH_MSC_CSW_TAG_OFFSET            4U
#define xUSBH_MSC_CSW_DATA_RESIDUE_OFFSET   8U
#define xUSBH_MSC_CSW_STATUS_OFFSET         12U
#define xUSBH_MSC_SCSI_6_LENGTH             6U
#define xUSBH_MSC_SCSI_READ_WRITE_10_LENGTH 10U
#define xUSBH_MSC_SCSI_READ_CAPACITY_LENGTH 10U
#define xUSBH_MSC_READ_CAPACITY_LENGTH      8U
#define xUSBH_MSC_CSW_STATUS_PASSED         0U
#define xUSBH_MSC_CSW_STATUS_FAILED         1U
#define xUSBH_MSC_CSW_STATUS_PHASE_ERROR    2U

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t msc_match(const xUSBH_Interface_Context_t *interface_ctx, bool *is_match);
static xRETURN_t msc_start(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx);
static xRETURN_t msc_stop(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx);
static xRETURN_t msc_transfer_complete(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx, const xUSBH_Transfer_t *transfer);
static bool interface_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Interface_Context_t *interface_ctx, uint8_t *index);
static xUSBH_MSC_Instance_t *msc_instance_allocate(xUSBH_MSC_Context_t *msc_ctx);
static xUSBH_MSC_Instance_t *msc_instance_find(xUSBH_MSC_Context_t *msc_ctx, const xUSBH_Interface_Context_t *interface_ctx);
static xUSBH_Endpoint_Context_t *
msc_bulk_endpoint_find(xUSBH_Context_t *host_ctx, const xUSBH_Interface_Context_t *interface_ctx, bool is_in);
static xUSBH_MSC_Instance_t *msc_instance_find_by_lun(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun);
static uint32_t msc_next_tag_take(xUSBH_MSC_Instance_t *instance);
static xRETURN_t msc_transfer_prepare_and_submit(xUSBH_MSC_Context_t *msc_ctx,
                                                 xUSBH_MSC_Instance_t *instance,
                                                 const xUSBH_Endpoint_Context_t *endpoint,
                                                 uint8_t *data,
                                                 uint32_t length,
                                                 xUSBH_MSC_State_t state);
static xRETURN_t msc_owned_transfer_release(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance);
static void
msc_cbw_prepare(xUSBH_MSC_Instance_t *instance, uint8_t lun, const uint8_t *cdb, uint8_t cdb_length, uint32_t data_length, bool data_in);
static xRETURN_t msc_scsi_command_start(xUSBH_MSC_Context_t *msc_ctx,
                                        uint8_t lun,
                                        const uint8_t *cdb,
                                        uint8_t cdb_length,
                                        uint8_t *buffer,
                                        uint32_t data_length,
                                        bool data_in);
static xRETURN_t msc_block_operation_start(xUSBH_MSC_Context_t *msc_ctx,
                                           uint8_t lun,
                                           uint32_t lba,
                                           uint16_t block_count,
                                           uint8_t *buffer,
                                           uint32_t buffer_length,
                                           bool is_read);
static void msc_transfer_error_update(xUSBH_MSC_Instance_t *instance, const xUSBH_Transfer_t *transfer);
static bool msc_transfer_has_error(xUSBH_MSC_Instance_t *instance, const xUSBH_Transfer_t *transfer);
static xRETURN_t msc_command_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance);
static xRETURN_t msc_data_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance);
static xRETURN_t msc_csw_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance);
static xRETURN_t msc_reset_request_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance);
static xRETURN_t msc_clear_in_halt_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance);
static xRETURN_t msc_clear_out_halt_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance);
static xRETURN_t msc_control_transfer_submit(xUSBH_MSC_Context_t *msc_ctx,
                                             xUSBH_MSC_Instance_t *instance,
                                             const USB_Setup_Request_t *setup,
                                             xUSBH_MSC_State_t state);
static xRETURN_t msc_reset_recovery_start(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance);
static xRETURN_t msc_mass_storage_reset_submit(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance);
static xRETURN_t msc_clear_halt_submit(xUSBH_MSC_Context_t *msc_ctx,
                                       xUSBH_MSC_Instance_t *instance,
                                       const xUSBH_Endpoint_Context_t *endpoint,
                                       xUSBH_MSC_State_t state);
static void msc_success_data_parse(xUSBH_MSC_Instance_t *instance);

// MODULE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
xSTATIC_ASSERT(sizeof(xUSBH_MSC_BOT_CBW_t) == USB_MSC_BOT_CBW_LENGTH, "xUSBH MSC CBW wire size changed");
xSTATIC_ASSERT(sizeof(xUSBH_MSC_BOT_CSW_t) == USB_MSC_BOT_CSW_LENGTH, "xUSBH MSC CSW wire size changed");

static xRETURN_t msc_match(const xUSBH_Interface_Context_t *interface_ctx, bool *is_match)
{
    if ((interface_ctx == NULL) || (is_match == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    *is_match = (interface_ctx->class_code == USB_CLASS_STORAGE) && (interface_ctx->subclass == USB_MSC_SCSI_TRANSPARENT_COMMAND_SET) &&
                (interface_ctx->protocol == USB_MSC_BBB);

    return xRETURN_OK;
}

static xRETURN_t msc_start(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx)
{
    xUSBH_MSC_Context_t *msc_ctx = (xUSBH_MSC_Context_t *)class_ctx;

    if ((interface_ctx == NULL) || (msc_ctx == NULL) || (msc_ctx->host_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xUSBH_Endpoint_Context_t *bulk_in = msc_bulk_endpoint_find(msc_ctx->host_ctx, interface_ctx, true);
    xUSBH_Endpoint_Context_t *bulk_out = msc_bulk_endpoint_find(msc_ctx->host_ctx, interface_ctx, false);
    if ((bulk_in == NULL) || (bulk_out == NULL))
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    xUSBH_MSC_Instance_t *instance = msc_instance_allocate(msc_ctx);
    if (instance == NULL)
    {
        return xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED;
    }

    instance->state = xUSBH_MSC_STATE_READY;
    instance->error = xUSBH_MSC_ERROR_NONE;
    instance->interface_ctx = interface_ctx;
    instance->bulk_in_endpoint = bulk_in;
    instance->bulk_out_endpoint = bulk_out;
    instance->next_tag = 1U;
    instance->block_size = xUSBH_MSC_BLOCK_SIZE;
    instance->lun = 0U;

    return xRETURN_OK;
}

static xRETURN_t msc_stop(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx)
{
    xUSBH_MSC_Context_t *msc_ctx = (xUSBH_MSC_Context_t *)class_ctx;

    if ((interface_ctx == NULL) || (msc_ctx == NULL) || (msc_ctx->host_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xUSBH_MSC_Instance_t *instance = msc_instance_find(msc_ctx, interface_ctx);
    if (instance == NULL)
    {
        return xRETURN_OK;
    }

    if (instance->transfer != NULL)
    {
        xRETURN_t status = xUSBH_Transfer_Release(msc_ctx->host_ctx, instance->transfer);
        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    (void)memset(instance, 0, sizeof(*instance));

    return xRETURN_OK;
}

static xRETURN_t msc_transfer_complete(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx, const xUSBH_Transfer_t *transfer)
{
    xUSBH_MSC_Context_t *msc_ctx = (xUSBH_MSC_Context_t *)class_ctx;

    if ((interface_ctx == NULL) || (msc_ctx == NULL) || (transfer == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xUSBH_MSC_Instance_t *instance = msc_instance_find(msc_ctx, interface_ctx);
    if (instance != NULL)
    {
        if (transfer == instance->transfer)
        {
            if (msc_transfer_has_error(instance, transfer) == true)
            {
                return msc_owned_transfer_release(msc_ctx, instance);
            }

            switch (instance->state)
            {
            case xUSBH_MSC_STATE_COMMAND:
                return msc_command_complete(msc_ctx, instance);

            case xUSBH_MSC_STATE_DATA_IN:
            case xUSBH_MSC_STATE_DATA_OUT:
                return msc_data_complete(msc_ctx, instance);

            case xUSBH_MSC_STATE_STATUS:
                return msc_csw_complete(msc_ctx, instance);

            case xUSBH_MSC_STATE_RESET_REQUEST:
                return msc_reset_request_complete(msc_ctx, instance);

            case xUSBH_MSC_STATE_CLEAR_IN_HALT:
                return msc_clear_in_halt_complete(msc_ctx, instance);

            case xUSBH_MSC_STATE_CLEAR_OUT_HALT:
                return msc_clear_out_halt_complete(msc_ctx, instance);

            default:
                return xRETURN_OK;
            }
        }

        msc_transfer_error_update(instance, transfer);
    }

    return xRETURN_OK;
}

static bool interface_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Interface_Context_t *interface_ctx, uint8_t *index)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_INTERFACES; i++)
    {
        if (&host_ctx->interfaces[i] == interface_ctx)
        {
            *index = i;
            return true;
        }
    }

    return false;
}

static xUSBH_MSC_Instance_t *msc_instance_allocate(xUSBH_MSC_Context_t *msc_ctx)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MSC_MAX_INSTANCES; i++)
    {
        if (msc_ctx->instances[i].is_allocated == false)
        {
            (void)memset(&msc_ctx->instances[i], 0, sizeof(msc_ctx->instances[i]));
            msc_ctx->instances[i].is_allocated = true;
            return &msc_ctx->instances[i];
        }
    }

    return NULL;
}

static xUSBH_MSC_Instance_t *msc_instance_find(xUSBH_MSC_Context_t *msc_ctx, const xUSBH_Interface_Context_t *interface_ctx)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MSC_MAX_INSTANCES; i++)
    {
        if ((msc_ctx->instances[i].is_allocated == true) && (msc_ctx->instances[i].interface_ctx == interface_ctx))
        {
            return &msc_ctx->instances[i];
        }
    }

    return NULL;
}

static xUSBH_Endpoint_Context_t *
msc_bulk_endpoint_find(xUSBH_Context_t *host_ctx, const xUSBH_Interface_Context_t *interface_ctx, bool is_in)
{
    uint8_t interface_index = 0U;
    uint8_t i;

    if (interface_index_get(host_ctx, interface_ctx, &interface_index) == false)
    {
        return NULL;
    }

    for (i = 0U; i < xUSBH_MAX_ENDPOINTS; i++)
    {
        if ((host_ctx->endpoints[i].is_allocated == true) && (host_ctx->endpoints[i].interface_index == interface_index) &&
            (host_ctx->endpoints[i].endpoint_type == USB_ENDP_TYPE_BULK) && (host_ctx->endpoints[i].is_in == is_in))
        {
            return &host_ctx->endpoints[i];
        }
    }

    return NULL;
}

static xUSBH_MSC_Instance_t *msc_instance_find_by_lun(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MSC_MAX_INSTANCES; i++)
    {
        if ((msc_ctx->instances[i].is_allocated == true) && (msc_ctx->instances[i].lun == lun))
        {
            return &msc_ctx->instances[i];
        }
    }

    return NULL;
}

static uint32_t msc_next_tag_take(xUSBH_MSC_Instance_t *instance)
{
    uint32_t tag = instance->next_tag;

    instance->next_tag++;
    if (instance->next_tag == 0U)
    {
        instance->next_tag = 1U;
    }

    return tag;
}

static xRETURN_t msc_transfer_prepare_and_submit(xUSBH_MSC_Context_t *msc_ctx,
                                                 xUSBH_MSC_Instance_t *instance,
                                                 const xUSBH_Endpoint_Context_t *endpoint,
                                                 uint8_t *data,
                                                 uint32_t length,
                                                 xUSBH_MSC_State_t state)
{
    xASSERT(msc_ctx != NULL, "msc_ctx is NULL");
    xASSERT(instance != NULL, "instance is NULL");
    xASSERT(endpoint != NULL, "endpoint is NULL");
    xASSERT(endpoint->device_index < xUSBH_MAX_DEVICES, "device_index out of range");
    xASSERT(instance->transfer != NULL, "transfer is NULL");

    xUSBH_Transfer_t *transfer = instance->transfer;
    xUSBH_Device_Context_t *device = &msc_ctx->host_ctx->devices[endpoint->device_index];

    transfer->device_address = device->address;
    transfer->endpoint_address = endpoint->endpoint_address;
    transfer->endpoint_type = endpoint->endpoint_type;
    transfer->interval = endpoint->interval;
    transfer->has_setup = false;
    transfer->last_event = xUSBH_HCD_TRANSFER_EVENT_COMPLETE;
    transfer->data = data;
    transfer->length = length;
    transfer->actual_length = 0U;
    transfer->user_ctx = instance;

    instance->state = state;

    return xUSBH_Transfer_Submit(msc_ctx->host_ctx, transfer);
}

static xRETURN_t msc_owned_transfer_release(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance)
{
    xRETURN_t status = xRETURN_OK;

    if (instance->transfer != NULL)
    {
        status = xUSBH_Transfer_Release(msc_ctx->host_ctx, instance->transfer);
        instance->transfer = NULL;
    }

    instance->data_buffer = NULL;
    instance->data_length = 0U;
    instance->active_tag = 0U;
    instance->active_opcode = 0U;
    instance->capacity = NULL;
    instance->active_data_in = false;

    return status;
}

static void
msc_cbw_prepare(xUSBH_MSC_Instance_t *instance, uint8_t lun, const uint8_t *cdb, uint8_t cdb_length, uint32_t data_length, bool data_in)
{
    uint8_t *cbw = (uint8_t *)&instance->cbw;
    uint8_t *cbw_cdb = &cbw[xUSBH_MSC_CBW_CB_OFFSET];

    (void)memset(&instance->cbw, 0, sizeof(instance->cbw));

    uint32_t tag = msc_next_tag_take(instance);
    instance->active_tag = tag;
    instance->active_opcode = cdb[0U];
    instance->active_data_in = data_in;

    xWrite_LE32(&cbw[xUSBH_MSC_CBW_SIGNATURE_OFFSET], USB_MSC_BOT_CBW_SIGNATURE);
    xWrite_LE32(&cbw[xUSBH_MSC_CBW_TAG_OFFSET], tag);
    xWrite_LE32(&cbw[xUSBH_MSC_CBW_DATA_LENGTH_OFFSET], data_length);
    cbw[xUSBH_MSC_CBW_FLAGS_OFFSET] = data_in ? xUSBH_MSC_BOT_CBW_FLAG_IN : xUSBH_MSC_BOT_CBW_FLAG_OUT;
    cbw[xUSBH_MSC_CBW_LUN_OFFSET] = lun;
    cbw[xUSBH_MSC_CBW_CB_LENGTH_OFFSET] = cdb_length;

    (void)memcpy(cbw_cdb, cdb, cdb_length);
}

static xRETURN_t msc_scsi_command_start(xUSBH_MSC_Context_t *msc_ctx,
                                        uint8_t lun,
                                        const uint8_t *cdb,
                                        uint8_t cdb_length,
                                        uint8_t *buffer,
                                        uint32_t data_length,
                                        bool data_in)
{
    if ((msc_ctx == NULL) || (cdb == NULL) || ((data_length > 0U) && (buffer == NULL)))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (msc_ctx->host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if ((lun > xUSBH_MSC_MAX_LUN) || (cdb_length == 0U) || (cdb_length > xUSBH_MSC_BOT_CBW_CB_SIZE))
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    xUSBH_MSC_Instance_t *instance = msc_instance_find_by_lun(msc_ctx, lun);
    if (instance == NULL)
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }
    if ((instance->state != xUSBH_MSC_STATE_READY) || (instance->transfer != NULL))
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    xRETURN_t status = xUSBH_Transfer_Allocate(msc_ctx->host_ctx, &instance->transfer);
    if (status != xRETURN_OK)
    {
        return status;
    }

    instance->data_buffer = buffer;
    instance->data_length = data_length;
    instance->error = xUSBH_MSC_ERROR_NONE;
    msc_cbw_prepare(instance, lun, cdb, cdb_length, data_length, data_in);

    status = msc_transfer_prepare_and_submit(msc_ctx, instance, instance->bulk_out_endpoint, (uint8_t *)&instance->cbw,
                                             USB_MSC_BOT_CBW_LENGTH, xUSBH_MSC_STATE_COMMAND);
    if (status != xRETURN_OK)
    {
        (void)msc_owned_transfer_release(msc_ctx, instance);
        instance->state = xUSBH_MSC_STATE_READY;
        return status;
    }

    return xRETURN_OK;
}

static xRETURN_t msc_block_operation_start(xUSBH_MSC_Context_t *msc_ctx,
                                           uint8_t lun,
                                           uint32_t lba,
                                           uint16_t block_count,
                                           uint8_t *buffer,
                                           uint32_t buffer_length,
                                           bool is_read)
{
    uint8_t cdb[xUSBH_MSC_SCSI_READ_WRITE_10_LENGTH] = {0};

    if ((msc_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (msc_ctx->host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if ((lun > xUSBH_MSC_MAX_LUN) || (block_count == 0U))
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    xUSBH_MSC_Instance_t *instance = msc_instance_find_by_lun(msc_ctx, lun);
    if (instance == NULL)
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }
    if ((instance->state != xUSBH_MSC_STATE_READY) || (instance->transfer != NULL))
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }
    if ((instance->block_size == 0U) || ((uint32_t)block_count > (UINT32_MAX / instance->block_size)))
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    uint32_t data_length = (uint32_t)block_count * instance->block_size;
    if (buffer_length < data_length)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    cdb[0U] = is_read ? USB_MSC_READ10 : USB_MSC_WRITE10;
    xWrite_BE32(&cdb[2U], lba);
    xWrite_BE16(&cdb[7U], block_count);

    return msc_scsi_command_start(msc_ctx, lun, cdb, sizeof(cdb), buffer, data_length, is_read);
}

static void msc_transfer_error_update(xUSBH_MSC_Instance_t *instance, const xUSBH_Transfer_t *transfer)
{
    if (transfer->last_event == xUSBH_HCD_TRANSFER_EVENT_STALLED)
    {
        instance->state = xUSBH_MSC_STATE_RESET_RECOVERY;
        instance->error = xUSBH_MSC_ERROR_STALL;
    }
    else if ((transfer->last_event == xUSBH_HCD_TRANSFER_EVENT_SHORT) || (transfer->actual_length < transfer->length))
    {
        instance->state = xUSBH_MSC_STATE_ERROR;
        instance->error = xUSBH_MSC_ERROR_SHORT_TRANSFER;
    }
    else if (transfer->last_event != xUSBH_HCD_TRANSFER_EVENT_COMPLETE)
    {
        instance->state = xUSBH_MSC_STATE_RESET_RECOVERY;
        instance->error = xUSBH_MSC_ERROR_PHASE_ERROR;
    }
    else
    {
        instance->error = xUSBH_MSC_ERROR_NONE;
    }
}

static xRETURN_t msc_control_transfer_submit(xUSBH_MSC_Context_t *msc_ctx,
                                             xUSBH_MSC_Instance_t *instance,
                                             const USB_Setup_Request_t *setup,
                                             xUSBH_MSC_State_t state)
{
    xASSERT(msc_ctx != NULL, "msc_ctx is NULL");
    xASSERT(instance != NULL, "instance is NULL");
    xASSERT(setup != NULL, "setup is NULL");
    xASSERT(instance->interface_ctx != NULL, "interface_ctx is NULL");
    xASSERT(instance->interface_ctx->device_index < xUSBH_MAX_DEVICES, "device_index out of range");
    xASSERT(instance->transfer != NULL, "transfer is NULL");

    xUSBH_Transfer_t *transfer = instance->transfer;
    xUSBH_Device_Context_t *device = &msc_ctx->host_ctx->devices[instance->interface_ctx->device_index];

    transfer->device_address = device->address;
    transfer->endpoint_address = USBH_CONTROL_ENDPOINT_ADDRESS;
    transfer->endpoint_type = USB_ENDP_TYPE_CTRL;
    transfer->interval = 0U;
    transfer->has_setup = true;
    transfer->setup = *setup;
    transfer->last_event = xUSBH_HCD_TRANSFER_EVENT_COMPLETE;
    transfer->data = NULL;
    transfer->length = 0U;
    transfer->actual_length = 0U;
    transfer->user_ctx = instance;

    instance->state = state;

    return xUSBH_Transfer_Submit(msc_ctx->host_ctx, transfer);
}

static xRETURN_t msc_reset_recovery_start(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance)
{
    xRETURN_t status = xUSBH_Transfer_Allocate(msc_ctx->host_ctx, &instance->transfer);
    if (status != xRETURN_OK)
    {
        return status;
    }

    status = msc_mass_storage_reset_submit(msc_ctx, instance);
    if (status != xRETURN_OK)
    {
        (void)msc_owned_transfer_release(msc_ctx, instance);
        instance->state = xUSBH_MSC_STATE_RESET_RECOVERY;
        return status;
    }

    return xRETURN_OK;
}

static xRETURN_t msc_mass_storage_reset_submit(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance)
{
    USB_Setup_Request_t setup = {0};

    setup.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
    setup.bRequest = USB_MSC_REQ_SET_BOMSR;
    xUSB_Setup_Set_Value(&setup, 0U);
    xUSB_Setup_Set_Index(&setup, instance->interface_ctx->interface_number);
    xUSB_Setup_Set_Length(&setup, 0U);

    return msc_control_transfer_submit(msc_ctx, instance, &setup, xUSBH_MSC_STATE_RESET_REQUEST);
}

static xRETURN_t msc_clear_halt_submit(xUSBH_MSC_Context_t *msc_ctx,
                                       xUSBH_MSC_Instance_t *instance,
                                       const xUSBH_Endpoint_Context_t *endpoint,
                                       xUSBH_MSC_State_t state)
{
    USB_Setup_Request_t setup = {0};

    if (endpoint == NULL)
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    xRETURN_t status =
        xUSBH_Setup_Build_Clear_Feature(&setup, USB_REQ_RECIPIENT_ENDPOINT, USBH_ENDPOINT_HALT_FEATURE, endpoint->endpoint_address);
    if (status != xRETURN_OK)
    {
        return status;
    }

    return msc_control_transfer_submit(msc_ctx, instance, &setup, state);
}

static bool msc_transfer_has_error(xUSBH_MSC_Instance_t *instance, const xUSBH_Transfer_t *transfer)
{
    bool has_error = (transfer->last_event != xUSBH_HCD_TRANSFER_EVENT_COMPLETE) || (transfer->actual_length < transfer->length);

    if (has_error == true)
    {
        msc_transfer_error_update(instance, transfer);
    }

    return has_error;
}

static xRETURN_t msc_command_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance)
{
    if (instance->data_length == 0U)
    {
        return msc_data_complete(msc_ctx, instance);
    }

    xUSBH_Endpoint_Context_t *endpoint = instance->active_data_in ? instance->bulk_in_endpoint : instance->bulk_out_endpoint;
    xUSBH_MSC_State_t state = instance->active_data_in ? xUSBH_MSC_STATE_DATA_IN : xUSBH_MSC_STATE_DATA_OUT;

    return msc_transfer_prepare_and_submit(msc_ctx, instance, endpoint, instance->data_buffer, instance->data_length, state);
}

static xRETURN_t msc_data_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance)
{
    (void)memset(&instance->csw, 0, sizeof(instance->csw));
    return msc_transfer_prepare_and_submit(msc_ctx, instance, instance->bulk_in_endpoint, (uint8_t *)&instance->csw, USB_MSC_BOT_CSW_LENGTH,
                                           xUSBH_MSC_STATE_STATUS);
}

static xRETURN_t msc_csw_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance)
{
    const uint8_t *csw = (const uint8_t *)&instance->csw;
    uint32_t signature = xRead_LE32(&csw[xUSBH_MSC_CSW_SIGNATURE_OFFSET]);
    uint32_t tag = xRead_LE32(&csw[xUSBH_MSC_CSW_TAG_OFFSET]);
    uint32_t residue = xRead_LE32(&csw[xUSBH_MSC_CSW_DATA_RESIDUE_OFFSET]);
    uint8_t status_byte = csw[xUSBH_MSC_CSW_STATUS_OFFSET];

    if ((signature != USB_MSC_BOT_CSW_SIGNATURE) || (tag != instance->active_tag) || (status_byte == xUSBH_MSC_CSW_STATUS_PHASE_ERROR))
    {
        instance->state = xUSBH_MSC_STATE_RESET_RECOVERY;
        instance->error = xUSBH_MSC_ERROR_PHASE_ERROR;
        return msc_owned_transfer_release(msc_ctx, instance);
    }

    if (status_byte == xUSBH_MSC_CSW_STATUS_FAILED)
    {
        instance->state = xUSBH_MSC_STATE_ERROR;
        instance->error = xUSBH_MSC_ERROR_COMMAND_FAILED;
        return msc_owned_transfer_release(msc_ctx, instance);
    }

    if ((status_byte != xUSBH_MSC_CSW_STATUS_PASSED) || (residue != 0U))
    {
        instance->state = xUSBH_MSC_STATE_ERROR;
        instance->error = xUSBH_MSC_ERROR_SHORT_TRANSFER;
        return msc_owned_transfer_release(msc_ctx, instance);
    }

    instance->state = xUSBH_MSC_STATE_READY;
    instance->error = xUSBH_MSC_ERROR_NONE;
    msc_success_data_parse(instance);
    return msc_owned_transfer_release(msc_ctx, instance);
}

static xRETURN_t msc_reset_request_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance)
{
    xRETURN_t status = msc_clear_halt_submit(msc_ctx, instance, instance->bulk_in_endpoint, xUSBH_MSC_STATE_CLEAR_IN_HALT);
    if (status != xRETURN_OK)
    {
        instance->state = xUSBH_MSC_STATE_RESET_RECOVERY;
        instance->error = xUSBH_MSC_ERROR_RESET_RECOVERY_REQUIRED;
        (void)msc_owned_transfer_release(msc_ctx, instance);
    }

    return status;
}

static xRETURN_t msc_clear_in_halt_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance)
{
    xRETURN_t status = msc_clear_halt_submit(msc_ctx, instance, instance->bulk_out_endpoint, xUSBH_MSC_STATE_CLEAR_OUT_HALT);
    if (status != xRETURN_OK)
    {
        instance->state = xUSBH_MSC_STATE_RESET_RECOVERY;
        instance->error = xUSBH_MSC_ERROR_RESET_RECOVERY_REQUIRED;
        (void)msc_owned_transfer_release(msc_ctx, instance);
    }

    return status;
}

static xRETURN_t msc_clear_out_halt_complete(xUSBH_MSC_Context_t *msc_ctx, xUSBH_MSC_Instance_t *instance)
{
    instance->state = xUSBH_MSC_STATE_READY;
    instance->error = xUSBH_MSC_ERROR_NONE;
    return msc_owned_transfer_release(msc_ctx, instance);
}

static void msc_success_data_parse(xUSBH_MSC_Instance_t *instance)
{
    if ((instance->active_opcode == USB_MSC_READ_CAPACITY) && (instance->capacity != NULL))
    {
        uint32_t last_lba = xRead_BE32(&instance->capacity_buffer[0U]);
        uint32_t block_size = xRead_BE32(&instance->capacity_buffer[4U]);

        instance->capacity->block_count = (last_lba == UINT32_MAX) ? UINT32_MAX : (last_lba + 1U);
        instance->capacity->block_size = block_size;
        instance->block_size = block_size;
    }
}

// PUBLIC FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
xRETURN_t xUSBH_MSC_Init(xUSBH_MSC_Context_t *msc_ctx, xUSBH_Context_t *host_ctx)
{
    if ((msc_ctx == NULL) || (host_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    (void)memset(msc_ctx, 0, sizeof(*msc_ctx));
    msc_ctx->host_ctx = host_ctx;

    return xRETURN_OK;
}

const xUSBH_Class_Driver_t *xUSBH_MSC_Class(void)
{
    static const xUSBH_Class_Driver_t driver = {
        .match = msc_match,
        .start = msc_start,
        .stop = msc_stop,
        .transfer_complete = msc_transfer_complete,
    };

    return &driver;
}

xRETURN_t xUSBH_MSC_Read_Blocks(xUSBH_MSC_Context_t *msc_ctx,
                                uint8_t lun,
                                uint32_t lba,
                                uint16_t block_count,
                                uint8_t *buffer,
                                uint32_t buffer_length)
{
    return msc_block_operation_start(msc_ctx, lun, lba, block_count, buffer, buffer_length, true);
}

xRETURN_t xUSBH_MSC_Write_Blocks(xUSBH_MSC_Context_t *msc_ctx,
                                 uint8_t lun,
                                 uint32_t lba,
                                 uint16_t block_count,
                                 const uint8_t *buffer,
                                 uint32_t buffer_length)
{
    // HCD transfer buffers are currently typed as mutable for DMA ownership.
    // WRITE10 does not modify caller data.
    return msc_block_operation_start(msc_ctx, lun, lba, block_count, (uint8_t *)buffer, buffer_length, false);
}

xRETURN_t xUSBH_MSC_Inquiry(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun, uint8_t *buffer, uint32_t buffer_length)
{
    uint8_t cdb[xUSBH_MSC_SCSI_6_LENGTH] = {0};

    if ((msc_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (buffer_length < xUSBH_MSC_INQUIRY_LENGTH)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    cdb[0U] = USB_MSC_INQUIRY;
    cdb[4U] = xUSBH_MSC_INQUIRY_LENGTH;

    return msc_scsi_command_start(msc_ctx, lun, cdb, sizeof(cdb), buffer, xUSBH_MSC_INQUIRY_LENGTH, true);
}

xRETURN_t xUSBH_MSC_Read_Capacity(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun, xUSBH_MSC_Capacity_t *capacity)
{
    uint8_t cdb[xUSBH_MSC_SCSI_READ_CAPACITY_LENGTH] = {0};

    if ((msc_ctx == NULL) || (capacity == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (msc_ctx->host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (lun > xUSBH_MSC_MAX_LUN)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    cdb[0U] = USB_MSC_READ_CAPACITY;

    xUSBH_MSC_Instance_t *instance = msc_instance_find_by_lun(msc_ctx, lun);
    if (instance == NULL)
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    xRETURN_t status =
        msc_scsi_command_start(msc_ctx, lun, cdb, sizeof(cdb), instance->capacity_buffer, xUSBH_MSC_READ_CAPACITY_LENGTH, true);
    if (status == xRETURN_OK)
    {
        instance->capacity = capacity;
    }

    return status;
}

xRETURN_t xUSBH_MSC_Test_Unit_Ready(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun)
{
    uint8_t cdb[xUSBH_MSC_SCSI_6_LENGTH] = {0};

    cdb[0U] = USB_MSC_TEST_UNIT_READY;

    return msc_scsi_command_start(msc_ctx, lun, cdb, sizeof(cdb), NULL, 0U, false);
}

xRETURN_t xUSBH_MSC_Request_Sense(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun, uint8_t *buffer, uint32_t buffer_length)
{
    uint8_t cdb[xUSBH_MSC_SCSI_6_LENGTH] = {0};

    if ((msc_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (buffer_length < xUSBH_MSC_REQUEST_SENSE_LENGTH)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    cdb[0U] = USB_MSC_REQUEST_SENSE;
    cdb[4U] = xUSBH_MSC_REQUEST_SENSE_LENGTH;

    return msc_scsi_command_start(msc_ctx, lun, cdb, sizeof(cdb), buffer, xUSBH_MSC_REQUEST_SENSE_LENGTH, true);
}

xRETURN_t xUSBH_MSC_Reset_Recovery(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun)
{
    if (msc_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (msc_ctx->host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (lun > xUSBH_MSC_MAX_LUN)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    xUSBH_MSC_Instance_t *instance = msc_instance_find_by_lun(msc_ctx, lun);
    if (instance == NULL)
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }
    if ((instance->state != xUSBH_MSC_STATE_RESET_RECOVERY) || (instance->transfer != NULL))
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    instance->error = xUSBH_MSC_ERROR_RESET_RECOVERY_REQUIRED;
    return msc_reset_recovery_start(msc_ctx, instance);
}

// EOF /////////////////////////////////////////////////////////////////////////////
