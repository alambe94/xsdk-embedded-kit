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

// @file test_xfs_usbmsc.c
// @brief Host tests for the xFS USB MSC block-device adapter.

#include <string.h>

#include "unity.h"

#include "test_xusbh_helpers.h"
#include "xfs_block_usbmsc.h"
#include "xfs_core.h"
#include "xfs_defs.h"
#include "xfs_fat32.h"
#include "xusbh_msc.h"

// MACROS //////////////////////////////////////////////////////////////////////

#define TEST_USBMSC_SECTOR_COUNT         64U
#define TEST_USBMSC_STORAGE_SIZE         (XFS_SECTOR_SIZE * TEST_USBMSC_SECTOR_COUNT)
#define TEST_USBMSC_READ_CAPACITY_LENGTH 8U

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

static xUSBH_Context_t g_host;
static xUSBH_MSC_Context_t g_msc;
static xFS_USBMSC_Context_t g_usbmsc;
static uint8_t g_storage[TEST_USBMSC_STORAGE_SIZE];

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

static void host_init_register_msc_and_start(void);
static void allocate_msc_interface(xUSBH_Device_Context_t **device,
                                   xUSBH_Interface_Context_t **interface,
                                   xUSBH_Endpoint_Context_t **bulk_in,
                                   xUSBH_Endpoint_Context_t **bulk_out);
static void complete_transfer(xUSBH_Transfer_t *transfer, xUSBH_HCD_Transfer_Event_t transfer_event, uint32_t actual_length);
static void prepare_passed_csw(xUSBH_MSC_Instance_t *instance);
static uint32_t active_lba_get(void);
static void test_storage_prepare(void);
static xRETURN_t usbmsc_poll(void *poll_ctx);
static void usbmsc_context_prepare(void);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

static void host_init_register_msc_and_start(void)
{
    xUSBH_Class_Register_Config_t class_config = {
        .driver = xUSBH_MSC_Class(),
        .class_ctx = &g_msc,
    };

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&g_host, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_MSC_Init(&g_msc, &g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Register_Class(&g_host, &class_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(&g_host, &valid_start_config));
}

static void allocate_msc_interface(xUSBH_Device_Context_t **device,
                                   xUSBH_Interface_Context_t **interface,
                                   xUSBH_Endpoint_Context_t **bulk_in,
                                   xUSBH_Endpoint_Context_t **bulk_out)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Allocate(&g_host, 0U, device));
    (*device)->address = 4U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Interface_Allocate(&g_host, *device, interface));
    (*interface)->class_code = USB_CLASS_STORAGE;
    (*interface)->subclass = USB_MSC_SCSI_TRANSPARENT_COMMAND_SET;
    (*interface)->protocol = USB_MSC_BBB;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Endpoint_Allocate(&g_host, *interface, bulk_in));
    (*bulk_in)->endpoint_address = 0x81U;
    (*bulk_in)->endpoint_type = USB_ENDP_TYPE_BULK;
    (*bulk_in)->is_in = true;
    (*bulk_in)->max_packet_size = 64U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Endpoint_Allocate(&g_host, *interface, bulk_out));
    (*bulk_out)->endpoint_address = 0x01U;
    (*bulk_out)->endpoint_type = USB_ENDP_TYPE_BULK;
    (*bulk_out)->is_in = false;
    (*bulk_out)->max_packet_size = 64U;
    (*interface)->endpoint_count = 2U;
}

static void complete_transfer(xUSBH_Transfer_t *transfer, xUSBH_HCD_Transfer_Event_t transfer_event, uint32_t actual_length)
{
    xUSBH_HCD_Event_t event = {
        .type = xUSBH_HCD_EVENT_TYPE_TRANSFER,
        .transfer_event = transfer_event,
        .transfer = transfer,
    };

    transfer->actual_length = actual_length;
    g_fake_hcd.last_callback(g_fake_hcd.last_host_ctx, &event);
}

static void prepare_passed_csw(xUSBH_MSC_Instance_t *instance)
{
    uint8_t *csw = (uint8_t *)&instance->csw;

    xWrite_LE32(&csw[0U], USB_MSC_BOT_CSW_SIGNATURE);
    xWrite_LE32(&csw[4U], instance->active_tag);
    xWrite_LE32(&csw[8U], 0U);
    csw[12U] = 0U;
}

static uint32_t active_lba_get(void)
{
    const uint8_t *cbw = (const uint8_t *)&g_msc.instances[0].cbw;

    return xRead_BE32(&cbw[17U]);
}

static void test_storage_prepare(void)
{
    uint8_t *boot = &g_storage[0U];

    (void)memset(g_storage, 0, sizeof(g_storage));

    boot[0U] = 0xEBU;
    boot[1U] = 0x58U;
    boot[2U] = 0x90U;
    xWrite_LE16(&boot[11U], XFS_SECTOR_SIZE);
    boot[13U] = 1U;
    xWrite_LE16(&boot[14U], 1U);
    boot[16U] = 1U;
    xWrite_LE32(&boot[32U], TEST_USBMSC_SECTOR_COUNT);
    xWrite_LE32(&boot[36U], 1U);
    xWrite_LE32(&boot[44U], 2U);
    xWrite_LE16(&boot[510U], 0xAA55U);
    xWrite_LE32(&g_storage[XFS_SECTOR_SIZE + (2U * FAT32_ENTRY_SIZE)], FAT32_EOC_MIN);
}

static xRETURN_t usbmsc_poll(void *poll_ctx)
{
    (void)poll_ctx;

    xUSBH_Transfer_t *transfer = g_fake_hcd.last_transfer;
    if ((transfer == NULL) || (transfer->is_submitted == false))
    {
        return xRETURN_OK;
    }

    if (transfer->has_setup == true)
    {
        complete_transfer(transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, transfer->length);
        (void)transfer;
    }
    else if ((transfer->endpoint_address == 0x01U) && (transfer->length > USB_MSC_BOT_CBW_LENGTH))
    {
        uint32_t offset = active_lba_get() * XFS_SECTOR_SIZE;
        (void)memcpy(&g_storage[offset], transfer->data, transfer->length);
        complete_transfer(transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, transfer->length);
    }
    else if ((transfer->endpoint_address == 0x81U) && (transfer->length == TEST_USBMSC_READ_CAPACITY_LENGTH))
    {
        xWrite_BE32(&transfer->data[0U], TEST_USBMSC_SECTOR_COUNT - 1U);
        xWrite_BE32(&transfer->data[4U], XFS_SECTOR_SIZE);
        complete_transfer(transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, transfer->length);
    }
    else if ((transfer->endpoint_address == 0x81U) && (transfer->length == USB_MSC_BOT_CSW_LENGTH))
    {
        prepare_passed_csw(&g_msc.instances[0]);
        complete_transfer(transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, transfer->length);
    }
    else if (transfer->endpoint_address == 0x81U)
    {
        uint32_t offset = active_lba_get() * XFS_SECTOR_SIZE;
        (void)memcpy(transfer->data, &g_storage[offset], transfer->length);
        complete_transfer(transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, transfer->length);
    }
    else
    {
        complete_transfer(transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, transfer->length);
    }

    return xRETURN_OK;
}

static void usbmsc_context_prepare(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;
    xUSBH_Endpoint_Context_t *bulk_out = NULL;

    host_init_register_msc_and_start();
    allocate_msc_interface(&device, &interface, &bulk_in, &bulk_out);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));

    g_usbmsc.msc_ctx = &g_msc;
    g_usbmsc.poll = usbmsc_poll;
    g_usbmsc.poll_ctx = NULL;
    g_usbmsc.max_poll_count = xFS_USBMSC_DEFAULT_MAX_POLL_COUNT;
    g_usbmsc.lun = 0U;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

void setUp(void)
{
    (void)memset(&g_host, 0, sizeof(g_host));
    (void)memset(&g_msc, 0, sizeof(g_msc));
    (void)memset(&g_usbmsc, 0, sizeof(g_usbmsc));
    reset_fake_hcd();
    test_storage_prepare();
}

void tearDown(void)
{
}

void test_xfs_usbmsc_init_validates_context_and_reads_capacity(void)
{
    xUSBH_MSC_Capacity_t capacity = {0};
    bool is_ready = true;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, gxFS_USBMSC_Driver.init(NULL));
    usbmsc_context_prepare();

    TEST_ASSERT_EQUAL(xRETURN_OK, gxFS_USBMSC_Driver.init(&g_usbmsc));
    TEST_ASSERT_TRUE(g_usbmsc.is_initialized);
    TEST_ASSERT_TRUE(g_usbmsc.is_media_present);
    TEST_ASSERT_EQUAL_UINT32(TEST_USBMSC_SECTOR_COUNT, g_usbmsc.sector_count);
    TEST_ASSERT_EQUAL_UINT32(XFS_SECTOR_SIZE, g_usbmsc.sector_size);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_USBMSC_Is_Ready(&g_usbmsc, &is_ready));
    TEST_ASSERT_TRUE(is_ready);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_USBMSC_Get_Capacity(&g_usbmsc, &capacity));
    TEST_ASSERT_EQUAL_UINT32(TEST_USBMSC_SECTOR_COUNT, capacity.block_count);
    TEST_ASSERT_EQUAL_UINT32(XFS_SECTOR_SIZE, capacity.block_size);
}

void test_xfs_usbmsc_reads_and_writes_sectors_synchronously(void)
{
    uint8_t buffer[XFS_SECTOR_SIZE] = {0};

    usbmsc_context_prepare();
    TEST_ASSERT_EQUAL(xRETURN_OK, gxFS_USBMSC_Driver.init(&g_usbmsc));

    TEST_ASSERT_EQUAL(xRETURN_OK, gxFS_USBMSC_Driver.read_sector(&g_usbmsc, 0U, buffer, 1U));
    TEST_ASSERT_EQUAL_UINT8(0xEBU, buffer[0U]);
    TEST_ASSERT_EQUAL_UINT16(0xAA55U, xRead_LE16(&buffer[510U]));

    buffer[0U] = 0xA5U;
    TEST_ASSERT_EQUAL(xRETURN_OK, gxFS_USBMSC_Driver.write_sector(&g_usbmsc, 10U, buffer, 1U));
    TEST_ASSERT_EQUAL_UINT8(0xA5U, g_storage[(size_t)10U * XFS_SECTOR_SIZE]);
}

void test_xfs_usbmsc_mounts_fat32_volume(void)
{
    xFS_Context_t fs_ctx = {0};

    usbmsc_context_prepare();

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_USBMSC_Driver, &g_usbmsc));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Mount(&fs_ctx));
    TEST_ASSERT_EQUAL_UINT32(XFS_SECTOR_SIZE, fs_ctx.bytes_per_sector);
    TEST_ASSERT_EQUAL_UINT32(TEST_USBMSC_SECTOR_COUNT - 2U, fs_ctx.total_clusters);
}

void test_xfs_usbmsc_disconnect_returns_block_failures(void)
{
    xUSBH_Device_Context_t *device = NULL;
    uint8_t buffer[XFS_SECTOR_SIZE] = {0};

    usbmsc_context_prepare();
    TEST_ASSERT_EQUAL(xRETURN_OK, gxFS_USBMSC_Driver.init(&g_usbmsc));
    device = &g_host.devices[0];
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Release(&g_host, device));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_IO, gxFS_USBMSC_Driver.read_sector(&g_usbmsc, 0U, buffer, 1U));
}

void test_xfs_usbmsc_rejects_invalid_io_ranges(void)
{
    uint8_t buffer[XFS_SECTOR_SIZE] = {0};

    usbmsc_context_prepare();
    TEST_ASSERT_EQUAL(xRETURN_OK, gxFS_USBMSC_Driver.init(&g_usbmsc));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, gxFS_USBMSC_Driver.read_sector(&g_usbmsc, 0U, NULL, 1U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, gxFS_USBMSC_Driver.read_sector(&g_usbmsc, 0U, buffer, 0U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_OUT_OF_RANGE, gxFS_USBMSC_Driver.write_sector(&g_usbmsc, TEST_USBMSC_SECTOR_COUNT, buffer, 1U));
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_xfs_usbmsc_init_validates_context_and_reads_capacity);
    RUN_TEST(test_xfs_usbmsc_reads_and_writes_sectors_synchronously);
    RUN_TEST(test_xfs_usbmsc_mounts_fat32_volume);
    RUN_TEST(test_xfs_usbmsc_disconnect_returns_block_failures);
    RUN_TEST(test_xfs_usbmsc_rejects_invalid_io_ranges);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
