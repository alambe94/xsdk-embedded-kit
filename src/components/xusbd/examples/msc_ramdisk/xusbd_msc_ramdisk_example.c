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

// @file xusbd_msc_ramdisk_example.c
// @brief Application-level hooks and configuration for USB MSC (mass storage).

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "string.h"
#include "xusbd_class.h"
#include "xusbd_msc.h"
#include "xusbd_msc_ramdisk_example.h"

// MACROS /////////////////////////////////////////////////////////////////////////
#define MSC_STORAGE_SIZE (48U * 1024U)

#define MSC_NUMBER_OF_LUN 1U

#define MSC_BLOCK_SIZE 512U

#define MSC_NUMBER_OF_BLOCKS (MSC_STORAGE_SIZE / MSC_BLOCK_SIZE)

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
//

__attribute__((aligned(4))) uint8_t MSC_RAM_Disk[MSC_STORAGE_SIZE];

__attribute__((aligned(4))) uint8_t MSC_Inquiry[] = {
    0x00, // Peripheral Device Type
    0x80, // Removable
    0x02, // Version (SPC-2)
    0x02, // Response Data Format
    0x1F, // Additional Length
    0x00, 0x00, 0x00, 'x', 'U', 'U', 'B', '-', 'M', 'S',
    'C', // Vendor (8 bytes)
    'R',  'A',  'M',  ' ', 'D', 'i', 's', 'k', ' ', 'X', ' ', ' ', ' ', ' ', ' ',
    ' ', // Product (16 bytes)
    '1',  '.',  '0',
    '0' // Revision (4 bytes)
};

xUSBD_MSC_Capacity_t msc_capacity = {
    .number_of_blocks = MSC_NUMBER_OF_BLOCKS,
    .block_size = MSC_BLOCK_SIZE,
};

uint32_t MSC_Max_LUN = MSC_NUMBER_OF_LUN;

static uint32_t MSC_App_Bus_Event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static uint32_t MSC_App_IO_Control(xUSBD_Class_Context_t *class_ctx,
                                   xUSBD_MSC_IO_CMD_t cmd,
                                   void *cmd_buff,
                                   uint32_t cmd_length,
                                   void **data_buff,
                                   uint32_t *data_length);

static uint32_t MSC_App_Bus_Event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    (void)class_ctx;
    (void)event;
    return xRETURN_OK;
}

static uint32_t MSC_App_IO_Control(xUSBD_Class_Context_t *class_ctx,
                                   xUSBD_MSC_IO_CMD_t cmd,
                                   void *cmd_buff,
                                   uint32_t cmd_length,
                                   void **data_buff,
                                   uint32_t *data_length)
{
    (void)class_ctx;
    (void)cmd_length;

    xUSBD_MSC_ADDR_t *address_data = NULL;

    switch (cmd)
    {
    case xUSBD_MSC_IO_CMD_GET_LUN:
        *data_buff = &MSC_Max_LUN;
        *data_length = 1;
        break;

    case xUSBD_MSC_IO_CMD_INQUIRY:
        MSC_Inquiry[25] = '0';
        *data_buff = MSC_Inquiry;
        *data_length = sizeof(MSC_Inquiry);
        break;

    case xUSBD_MSC_IO_CMD_GET_CAPACITY:
        *data_buff = &msc_capacity;
        *data_length = sizeof(msc_capacity);
        break;

    case xUSBD_MSC_IO_CMD_GET_READ_ADDR: /* fall through */
    case xUSBD_MSC_IO_CMD_GET_WRITE_ADDR:
        address_data = (xUSBD_MSC_ADDR_t *)cmd_buff;
        *data_buff = &MSC_RAM_Disk[(size_t)address_data->block_offset * MSC_BLOCK_SIZE];
        *data_length = (uint32_t)address_data->number_of_blocks * MSC_BLOCK_SIZE;
        break;

    default:
        break;
    }

    return xRETURN_OK;
}

static xUSBD_MSC_Callbacks_t callbacks = {
    .on_bus_event = MSC_App_Bus_Event,
    .on_io_control = MSC_App_IO_Control,
};

void xUSBD_MSC_App_Init(xUSBD_Class_Context_t *class_ctx)
{
    // Fill disk with incrementing pattern so host can verify read integrity
    for (uint32_t i = 0; i < MSC_STORAGE_SIZE; i++)
    {
        MSC_RAM_Disk[i] = (uint8_t)i;
    }

    (void)xUSBD_MSC_Set_Callbacks(class_ctx, &callbacks);
}

void xUSBD_MSC_App_Process(xUSBD_Class_Context_t *class_ctx)
{
    (void)class_ctx;
}
// EOF /////////////////////////////////////////////////////////////////////////////
