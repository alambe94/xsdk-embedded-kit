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

// @file xusbd_dfu_mode_example.c
// @brief Standalone DFU-mode bootloader application callbacks.
//
// Simulated storage model (replace with real flash driver on hardware):
//
//   s_flash_bank[]  -- a RAM buffer that acts as the application flash partition.
//                      Writes are plain memcpy; erases are memset(0xFF).
//
// Download flow (host perspective):
//   DFU_DNLOAD blocks  ->  on_io_control(WRITE_BLOCK) each block
//   DFU_DNLOAD(len=0)  ->  on_io_control(MANIFEST) - verify + mark valid
//   GETSTATUS polling  ->  state machine moves to dfuMANIFEST_WAIT_RESET
//   USB reset          ->  dfu_bus_event -> pending_op = DETACH
//   on_io_control(DETACH) -> clear DFU boot flag, reset to application
//
// Upload flow (host perspective):
//   DFU_UPLOAD(block)  ->  on_io_control(READ_BLOCK) - return flash pointer
//
// Platform porting checklist:
//   [ ] Replace flash_erase()  with your HAL erase call (page/sector granularity)
//   [ ] Replace flash_write()  with your HAL program call
//   [ ] Replace flash_read()   with your HAL read or a direct pointer if memory-mapped
//   [ ] Replace platform_clear_dfu_boot_flag() / platform_trigger_reset() as in
//       the runtime example.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusbd_dfu.h"
#include "xusbd_dfu_mode_example.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// Simulated flash: erased state is 0xFF (matches real NOR flash).
static uint8_t s_flash_bank[DFU_MODE_FLASH_SIZE];

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t dfu_mode_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t dfu_mode_io_control(xUSBD_Class_Context_t *class_ctx,
                                     xUSBD_DFU_IO_CMD_t cmd,
                                     void *cmd_buff,
                                     uint32_t cmd_length,
                                     void **data_buff,
                                     uint32_t *data_length);
static xRETURN_t flash_erase(void);
static xRETURN_t flash_write(uint32_t offset, const uint8_t *data, uint32_t length);
static uint32_t firmware_checksum(uint32_t length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// Stub: clear the flag that caused the bootloader to start in DFU mode.
static void platform_clear_dfu_boot_flag(void)
{
    // *(volatile uint32_t *)BACKUP_SRAM_ADDR = APP_BOOT_MAGIC;
}

// Stub: trigger a system reset to jump to the application.
static void platform_trigger_reset(void)
{
    // NVIC_SystemReset();
}

// Erase the entire simulated flash bank (0xFF = erased NOR flash).
static xRETURN_t flash_erase(void)
{
    memset(s_flash_bank, 0xFFU, sizeof(s_flash_bank));
    return xRETURN_OK;
}

// Write a block of data at the given byte offset into the simulated flash.
// On real hardware: page-align offset, handle partial pages, call HAL_FLASH_Program.
static xRETURN_t flash_write(uint32_t offset, const uint8_t *data, uint32_t length)
{
    if (offset + length > sizeof(s_flash_bank))
    {
        return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
    }
    memcpy(&s_flash_bank[offset], data, length);
    return xRETURN_OK;
}

// Simple additive checksum over the written region.
// Replace with CRC-32 or SHA-256 for production use.
static uint32_t firmware_checksum(uint32_t length)
{
    uint32_t sum = 0U;
    for (uint32_t i = 0U; i < length; i++)
    {
        sum += s_flash_bank[i];
    }
    return sum;
}

static xRETURN_t dfu_mode_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    (void)class_ctx;

    switch (event)
    {
    case USB_DCD_CONNECT_RECEIVED:
        // Erase flash on each fresh connection so the host always starts with
        // a clean slate.  On real hardware you may choose to skip erasure here
        // and instead erase lazily per-sector during WRITE_BLOCK.
        (void)flash_erase();
        break;

    default:
        break;
    }

    return xRETURN_OK;
}

static xRETURN_t dfu_mode_io_control(xUSBD_Class_Context_t *class_ctx,
                                     xUSBD_DFU_IO_CMD_t cmd,
                                     void *cmd_buff,
                                     uint32_t cmd_length,
                                     void **data_buff,
                                     uint32_t *data_length)
{
    (void)cmd_length;

    void *app_ctx_ptr = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_ctx_ptr) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }
    xUSBD_DFU_Mode_App_Context_t *app_ctx = (xUSBD_DFU_Mode_App_Context_t *)app_ctx_ptr;

    switch (cmd)
    {
    case xUSBD_DFU_IO_CMD_WRITE_BLOCK:
    {
        // cmd_buff is xUSBD_DFU_Write_Block_t* carrying block_num, data, length.
        xUSBD_DFU_Write_Block_t *blk = (xUSBD_DFU_Write_Block_t *)cmd_buff;
        uint32_t offset = (uint32_t)blk->block_num * xUSBD_DFU_TRANSFER_SIZE;

        xRETURN_t status = flash_write(offset, blk->data, blk->length);
        if (status != xRETURN_OK)
        {
            return status;
        }

        app_ctx->bytes_written = offset + blk->length;
        return xRETURN_OK;
    }

    case xUSBD_DFU_IO_CMD_READ_BLOCK:
    {
        // cmd_buff is uint16_t* (block_num); data_buff/data_length are output.
        if (cmd_buff == NULL || data_buff == NULL || data_length == NULL)
        {
            return xRETURN_xERR_xUSBD_NULL_POINTER;
        }

        uint16_t block_num = *(uint16_t *)cmd_buff;
        uint32_t offset = (uint32_t)block_num * xUSBD_DFU_TRANSFER_SIZE;

        if (offset >= sizeof(s_flash_bank))
        {
            // Signal end-of-upload: return a zero-length block.
            *data_buff = NULL;
            *data_length = 0U;
            return xRETURN_OK;
        }

        uint32_t remaining = (uint32_t)sizeof(s_flash_bank) - offset;
        *data_buff = &s_flash_bank[offset];
        *data_length = (remaining < xUSBD_DFU_TRANSFER_SIZE) ? remaining : xUSBD_DFU_TRANSFER_SIZE;
        return xRETURN_OK;
    }

    case xUSBD_DFU_IO_CMD_MANIFEST:
    {
        // Verify the downloaded firmware before marking it valid.
        // On real hardware: verify CRC, check image header magic, or use a
        // hardware crypto engine.  Here we check that the flash is not blank.
        if (app_ctx->bytes_written == 0U)
        {
            return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
        }

        uint32_t checksum = firmware_checksum(app_ctx->bytes_written);
        if (checksum == 0U)
        {
            // All zeros -> blank / erased image, refuse to mark valid.
            return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
        }

        app_ctx->firmware_valid = true;
        return xRETURN_OK;
    }

    case xUSBD_DFU_IO_CMD_DETACH:
    {
        // MANIFEST_WAIT_RESET -> USB reset -> DETACH pending op fires here.
        // Clear the DFU boot flag so the bootloader restarts into the app.
        if (app_ctx->firmware_valid)
        {
            platform_clear_dfu_boot_flag();
        }

        // This call does not return on real hardware.
        platform_trigger_reset();
        return xRETURN_OK;
    }

    default:
        return xRETURN_xERR_xUSBD_APP_FEATURE_NOT_SUPPORTED;
    }
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xUSBD_DFU_Mode_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Mode_App_Context_t *app_context)
{
    app_context->bytes_written = 0U;
    app_context->firmware_valid = false;
    app_context->do_boot = 0U;

    memset(s_flash_bank, 0xFFU, sizeof(s_flash_bank));

    (void)xUSBD_Class_Set_App_Context(class_ctx, app_context);

    static xUSBD_DFU_Callbacks_t callbacks = {
        .on_bus_event = dfu_mode_bus_event,
        .on_io_control = dfu_mode_io_control,
    };
    (void)xUSBD_DFU_Set_Callbacks(class_ctx, &callbacks);
}

void xUSBD_DFU_Mode_App_Process(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_DFU_Pending_Op_t op = xUSBD_DFU_Get_Pending_Op(class_ctx);
    if (op != xUSBD_DFU_PENDING_NONE)
    {
        (void)xUSBD_DFU_Process_Pending_Op(class_ctx);
    }
}

const uint8_t *xUSBD_DFU_Mode_Get_Flash(uint32_t *size_out)
{
    if (size_out != NULL)
    {
        *size_out = (uint32_t)sizeof(s_flash_bank);
    }
    return s_flash_bank;
}
// EOF /////////////////////////////////////////////////////////////////////////////
