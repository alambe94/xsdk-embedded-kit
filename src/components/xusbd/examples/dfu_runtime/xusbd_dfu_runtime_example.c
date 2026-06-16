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

// @file xusbd_dfu_runtime_example.c
// @brief DFU runtime-mode application callbacks.
//
// The runtime DFU interface lives alongside the normal application class
// (CDC, HID, MSC, ...).  Its only job is to catch DFU_DETACH and reboot the
// device into the standalone DFU bootloader.
//
// Platform porting:
//   1. Replace platform_set_dfu_boot_flag() with the mechanism your MCU uses
//      to communicate the desired boot mode across a reset (e.g. write a magic
//      word to a backup register, a specific SRAM address, or an external
//      EEPROM byte that the bootloader checks at startup).
//   2. Replace platform_trigger_reset() with NVIC_SystemReset(), watchdog
//      expiry, or whatever your HAL exposes.
//   3. Optionally implement on_io_control WRITE_BLOCK / READ_BLOCK if you want
//      the host to be able to download firmware directly to the running
//      application (uncommon; most designs defer that to the DFU-mode bootloader).

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusbd_dfu.h"
#include "xusbd_dfu_runtime_example.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t dfu_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t dfu_app_io_control(xUSBD_Class_Context_t *class_ctx,
                                    xUSBD_DFU_IO_CMD_t cmd,
                                    void *cmd_buff,
                                    uint32_t cmd_length,
                                    void **data_buff,
                                    uint32_t *data_length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// Stub: set a persistent flag that survives the upcoming reset so the
// bootloader knows to start in DFU mode instead of jumping to the app.
static void platform_set_dfu_boot_flag(void)
{
    // Example for Cortex-M with a backup SRAM/register:
    //   *(volatile uint32_t *)BACKUP_SRAM_ADDR = DFU_BOOT_MAGIC;
    // Example using a dedicated GPIO latch:
    //   HAL_GPIO_WritePin(DFU_FLAG_PORT, DFU_FLAG_PIN, GPIO_PIN_SET);
}

// Stub: trigger a system reset.  The boot flag set above will redirect the
// bootloader to DFU mode after the reset completes.
static void platform_trigger_reset(void)
{
    // NVIC_SystemReset();   // Cortex-M
    // WDT_force_expire();   // watchdog-based reset
}

static xRETURN_t dfu_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_DFU_App_Context_t *ctx = (xUSBD_DFU_App_Context_t *)app_context;

    switch (event)
    {
    case USB_DCD_CONNECT_RECEIVED:
        // The host re-enumerated us after the detach/reset cycle.
        ctx->reset_complete = 1U;
        break;

    case USB_DCD_DISCONNECT_RECEIVED:
        // Bus removed - if we are in APP_DETACH state the host will reassert
        // VBUS shortly to pick up the bootloader.
        ctx->reset_complete = 0U;
        break;

    default:
        break;
    }

    return xRETURN_OK;
}

static xRETURN_t dfu_app_io_control(xUSBD_Class_Context_t *class_ctx,
                                    xUSBD_DFU_IO_CMD_t cmd,
                                    void *cmd_buff,
                                    uint32_t cmd_length,
                                    void **data_buff,
                                    uint32_t *data_length)
{
    (void)class_ctx;
    (void)cmd_buff;
    (void)cmd_length;
    (void)data_buff;
    (void)data_length;

    switch (cmd)
    {
    case xUSBD_DFU_IO_CMD_DETACH:
        // DFU_DETACH was acknowledged; the USB status ZLP has been sent.
        // Mark the boot flag so the bootloader enters DFU mode after reset,
        // then trigger the reset.  This function does not return on real hardware.
        platform_set_dfu_boot_flag();
        platform_trigger_reset();
        return xRETURN_OK;

    case xUSBD_DFU_IO_CMD_WRITE_BLOCK:
    case xUSBD_DFU_IO_CMD_READ_BLOCK:
    case xUSBD_DFU_IO_CMD_MANIFEST:
    default:
        // Download/upload/manifest are handled by the standalone DFU-mode bootloader.
        // Returning an error here causes the driver to stall the corresponding request,
        // which is the correct behaviour when these are not supported at runtime.
        return xRETURN_xERR_xUSBD_APP_FEATURE_NOT_SUPPORTED;
    }
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xUSBD_DFU_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_App_Context_t *app_context)
{
    app_context->reset_complete = 0U;
    app_context->last_state = xUSBD_DFU_STATE_APP_IDLE;

    (void)xUSBD_Class_Set_App_Context(class_ctx, app_context);

    static xUSBD_DFU_Callbacks_t callbacks = {
        .on_bus_event = dfu_app_bus_event,
        .on_io_control = dfu_app_io_control,
    };
    (void)xUSBD_DFU_Set_Callbacks(class_ctx, &callbacks);
}

void xUSBD_DFU_App_Process(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_DFU_Pending_Op_t op = xUSBD_DFU_Get_Pending_Op(class_ctx);
    if (op != xUSBD_DFU_PENDING_NONE)
    {
        (void)xUSBD_DFU_Process_Pending_Op(class_ctx);
    }
}
// EOF /////////////////////////////////////////////////////////////////////////////
