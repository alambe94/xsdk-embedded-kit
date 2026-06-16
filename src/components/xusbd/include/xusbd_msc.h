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

// @file xusbd_msc.h
// @brief xUSB Mass Storage Class (MSC) driver interface.

#ifndef XUSBD_MSC_H
#define XUSBD_MSC_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusb_msc_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////
#define xUSBD_MSC_DESC_BASE_SIZE (USB_INTERFACE_DESC_LEN + (2U * USB_ENDPOINT_DESC_LEN))
#define xUSBD_MSC_DESC_SIZE(speed)                                                                                                         \
    (xUSBD_MSC_DESC_BASE_SIZE + (((speed) == USB_SPEED_SUPER) ? (2U * USB_SS_ENDPOINT_COMPANION_DESC_LEN) : 0U))

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xUSBD_MSC_Capacity_t
    {
        uint32_t number_of_blocks;
        uint32_t block_size;
    } xUSBD_MSC_Capacity_t;

    typedef struct xUSBD_MSC_Context_t
    {
        xUSBD_Class_Context_t class_ctx;
        uint8_t interface;
        uint8_t in_ep;
        uint8_t out_ep;

        uint8_t state;
        uint8_t sense_key;
        uint8_t sense_asc;
        uint8_t sense_ascq;
        uint8_t temp[64];
        USB_MSC_BOT_CBW_t cbw;
        USB_MSC_BOT_CSW_t csw;
        xUSBD_MSC_Capacity_t msc_capacity;
    } xUSBD_MSC_Context_t;

    typedef enum xUSBD_MSC_IO_CMD_t
    {
        xUSBD_MSC_IO_CMD_GET_LUN,
        xUSBD_MSC_IO_CMD_INQUIRY,
        xUSBD_MSC_IO_CMD_GET_CAPACITY,
        xUSBD_MSC_IO_CMD_GET_READ_ADDR,
        xUSBD_MSC_IO_CMD_GET_WRITE_ADDR,
        xUSBD_MSC_IO_CMD_WRITE_COMPLETE,
    } xUSBD_MSC_IO_CMD_t;

    typedef struct xUSBD_MSC_ADDR_t
    {
        uint32_t block_offset;
        uint32_t number_of_blocks;
    } xUSBD_MSC_ADDR_t;

    typedef struct xUSBD_MSC_Callbacks_t
    {
        xRETURN_t (*on_bus_event)(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
        xRETURN_t (*on_io_control)(xUSBD_Class_Context_t *class_ctx,
                                   xUSBD_MSC_IO_CMD_t cmd,
                                   void *cmd_buff,
                                   uint32_t cmd_length,
                                   void **data_buff,
                                   uint32_t *data_length);
    } xUSBD_MSC_Callbacks_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xUSBD_Class_Driver_t *xUSBD_MSC_Class(void);
    xRETURN_t xUSBD_MSC_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, xUSBD_MSC_Callbacks_t *callbacks);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_MSC_H
// EOF /////////////////////////////////////////////////////////////////////////////
