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

// @file xusbd_dfu.h
// @brief xUSB Device Firmware Upgrade (DFU) class driver interface.

#ifndef XUSBD_DFU_H
#define XUSBD_DFU_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusb_dfu_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

// MACROS //////////////////////////////////////////////////////////////////////
#ifndef xUSBD_DFU_TRANSFER_SIZE
#define xUSBD_DFU_TRANSFER_SIZE xUSBD_MAX_EP0_DATA_SIZE
#endif

#ifndef xUSBD_DFU_POLL_TIMEOUT_MS
#define xUSBD_DFU_POLL_TIMEOUT_MS 50U
#endif

#define xUSBD_DFU_DESC_SIZE (USB_INTERFACE_DESC_LEN + USB_DFU_FUNC_DESC_LEN)

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum xUSBD_DFU_State_t
    {
        xUSBD_DFU_STATE_APP_IDLE = 0,
        xUSBD_DFU_STATE_APP_DETACH = 1,
        xUSBD_DFU_STATE_DFU_IDLE = 2,
        xUSBD_DFU_STATE_DFU_DNLOAD_SYNC = 3,
        xUSBD_DFU_STATE_DFU_DNBUSY = 4,
        xUSBD_DFU_STATE_DFU_DNLOAD_IDLE = 5,
        xUSBD_DFU_STATE_DFU_MANIFEST_SYNC = 6,
        xUSBD_DFU_STATE_DFU_MANIFEST = 7,
        xUSBD_DFU_STATE_DFU_MANIFEST_WAIT_RESET = 8,
        xUSBD_DFU_STATE_DFU_UPLOAD_IDLE = 9,
        xUSBD_DFU_STATE_DFU_ERROR = 10,
    } xUSBD_DFU_State_t;

    typedef enum xUSBD_DFU_Status_t
    {
        xUSBD_DFU_STATUS_OK = 0x00,
        xUSBD_DFU_STATUS_ERR_TARGET = 0x01,       // file not for this device
        xUSBD_DFU_STATUS_ERR_FILE = 0x02,         // file failed checks
        xUSBD_DFU_STATUS_ERR_WRITE = 0x03,        // write failure
        xUSBD_DFU_STATUS_ERR_ERASE = 0x04,        // erase failure
        xUSBD_DFU_STATUS_ERR_CHECK_ERASED = 0x05, // erase verify failure
        xUSBD_DFU_STATUS_ERR_PROG = 0x06,         // program memory failure
        xUSBD_DFU_STATUS_ERR_VERIFY = 0x07,       // verify after program failed
        xUSBD_DFU_STATUS_ERR_ADDRESS = 0x08,      // address out of range
        xUSBD_DFU_STATUS_ERR_NOTDONE = 0x09,      // received less data than expected
        xUSBD_DFU_STATUS_ERR_FIRMWARE = 0x0A,     // device firmware corrupt
        xUSBD_DFU_STATUS_ERR_VENDOR = 0x0B,       // vendor-specific error
        xUSBD_DFU_STATUS_ERR_USBR = 0x0C,         // unexpected USB reset
        xUSBD_DFU_STATUS_ERR_POR = 0x0D,          // unexpected power-on reset
        xUSBD_DFU_STATUS_ERR_UNKNOWN = 0x0E,
        xUSBD_DFU_STATUS_ERR_STALLEDPKT = 0x0F, // stall during data phase
    } xUSBD_DFU_Status_t;

    typedef enum xUSBD_DFU_Pending_Op_t
    {
        xUSBD_DFU_PENDING_NONE = 0,
        xUSBD_DFU_PENDING_WRITE,    // flash a downloaded block
        xUSBD_DFU_PENDING_MANIFEST, // finalize firmware (verify / mark valid)
        xUSBD_DFU_PENDING_DETACH,   // reboot into / out of DFU mode
    } xUSBD_DFU_Pending_Op_t;

    typedef struct xUSBD_DFU_Context_t
    {
        xUSBD_Class_Context_t class_ctx;

        uint8_t interface;
        uint8_t protocol;       // USB_DFU_PROTOCOL_RUNTIME / _DFU
        uint16_t transfer_size; // advertised in wTransferSize; <= xUSBD_DFU_TRANSFER_SIZE

        xUSBD_DFU_State_t state;
        xUSBD_DFU_Status_t dfu_status;
        xUSBD_DFU_Pending_Op_t pending_op;

        uint16_t block_num;     // current download / upload block index (wValue)
        uint32_t dnload_length; // byte count of last received block

        // Download buffer - separates download payload from the shared EP0 control_data scratch.
        // Sized at xUSBD_DFU_TRANSFER_SIZE; must not exceed xUSBD_MAX_EP0_DATA_SIZE.
        uint8_t dnload_buffer[xUSBD_DFU_TRANSFER_SIZE];
    } xUSBD_DFU_Context_t;

    typedef enum xUSBD_DFU_IO_CMD_t
    {
        // Write one downloaded block to flash.
        // cmd_buff: xUSBD_DFU_Write_Block_t*   cmd_length: sizeof(xUSBD_DFU_Write_Block_t)
        // data_buff: NULL                     data_length: NULL
        xUSBD_DFU_IO_CMD_WRITE_BLOCK,

        // Read one block for upload (bitCanUpload must be set in bmAttributes).
        // cmd_buff: uint16_t* (block_num)     cmd_length: 2
        // data_buff: uint8_t** (read pointer) data_length: bytes available
        xUSBD_DFU_IO_CMD_READ_BLOCK,

        // Finalise download: verify CRC / mark image valid / switch boot bank.
        // cmd_buff: NULL  data_buff: NULL
        xUSBD_DFU_IO_CMD_MANIFEST,

        // Trigger mode switch: runtime->DFU (reboot to bootloader) or DFU->app.
        // cmd_buff: NULL  data_buff: NULL
        xUSBD_DFU_IO_CMD_DETACH,
    } xUSBD_DFU_IO_CMD_t;

    typedef struct xUSBD_DFU_Write_Block_t
    {
        uint16_t block_num;
        uint8_t *data;
        uint32_t length;
    } xUSBD_DFU_Write_Block_t;

    typedef struct xUSBD_DFU_Callbacks_t
    {
        // Optional bus-event notification (may be NULL).
        xRETURN_t (*on_bus_event)(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);

        // Flash I/O + control operations (must not be NULL).
        // Returns xRETURN_OK on success; any other value maps to xUSBD_DFU_STATUS_ERR_*.
        xRETURN_t (*on_io_control)(xUSBD_Class_Context_t *class_ctx,
                                   xUSBD_DFU_IO_CMD_t cmd,
                                   void *cmd_buff,
                                   uint32_t cmd_length,
                                   void **data_buff,
                                   uint32_t *data_length);
    } xUSBD_DFU_Callbacks_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // INLINE HELPERS //////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Return the class driver for DFU runtime mode (bInterfaceProtocol = 0x01).
    // Register this in normal application firmware alongside CDC/HID/MSC.
    // Handles only DFU_DETACH; all other DFU requests are stalled.
    // xUSBD_Class_Set_Interface_String(&ctx->class_ctx, "DFU Runtime") must be
    // called after registration to set the interface string descriptor.
    xUSBD_Class_Driver_t *xUSBD_DFU_Runtime_Class(void);

    // Return the class driver for DFU mode (bInterfaceProtocol = 0x02).
    // Register this when the device boots as a standalone DFU bootloader.
    // Handles the full DFU download / upload / manifest state machine.
    // xUSBD_Class_Set_Interface_String(&ctx->class_ctx, "DFU") must be
    // called after registration to set the interface string descriptor.
    xUSBD_Class_Driver_t *xUSBD_DFU_Mode_Class(void);

    // Wire callbacks after xUSBD_Class_Register().
    xRETURN_t xUSBD_DFU_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Callbacks_t *callbacks);

    // Override the advertised wTransferSize (default: xUSBD_DFU_TRANSFER_SIZE).
    // Call before xUSBD_Start().  size must be <= xUSBD_DFU_TRANSFER_SIZE.
    xRETURN_t xUSBD_DFU_Set_Transfer_Size(xUSBD_Class_Context_t *class_ctx, uint16_t size);

    // Query the pending deferred operation.  Call from the main loop after each
    // USB interrupt cycle; non-zero means xUSBD_DFU_Process_Pending_Op() is needed.
    xUSBD_DFU_Pending_Op_t xUSBD_DFU_Get_Pending_Op(xUSBD_Class_Context_t *class_ctx);

    // Execute the pending operation (flash write, manifest, or detach).
    // Must be called from the main loop, not from an ISR.
    // Returns xRETURN_OK; updates DFU state on completion or error.
    xRETURN_t xUSBD_DFU_Process_Pending_Op(xUSBD_Class_Context_t *class_ctx);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_DFU_H
// EOF /////////////////////////////////////////////////////////////////////////////
