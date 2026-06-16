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

// @file xusbd_dfu_mode_example.h
// @brief Standalone DFU-mode bootloader application hooks.
//
// This example demonstrates a complete DFU download/upload/manifest flow.
// It uses a RAM buffer to simulate flash storage so the code compiles and
// runs without any platform-specific flash driver.  Replace the simulated
// storage with real flash read/write/erase calls to deploy on hardware.
//
// Usage:
//   1. Boot into this firmware instead of the normal application
//      (e.g. when a magic word is set in backup SRAM by the runtime DFU).
//   2. Call xUSBD_DFU_Mode_App_Init() after xUSBD_Class_Register().
//   3. Call xUSBD_DFU_Mode_App_Process() from the main loop after each USB
//      interrupt cycle.

#ifndef XUSBD_DFU_MODE_EXAMPLE_H
#define XUSBD_DFU_MODE_EXAMPLE_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// MODULE INCLUDES
#include "xusbd_dfu.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

// Size of the simulated flash bank in bytes.
// Must be a multiple of xUSBD_DFU_TRANSFER_SIZE.
#ifndef DFU_MODE_FLASH_SIZE
#define DFU_MODE_FLASH_SIZE (128U * 1024U)
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        uint32_t bytes_written;   // total bytes received across all DNLOAD blocks
        bool firmware_valid;      // set to true by manifest if checksum passes
        volatile uint8_t do_boot; // set by DETACH pending op; main loop acts on it
    } xUSBD_DFU_Mode_App_Context_t;

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Wire DFU-mode callbacks and store app_context.
    // Call after xUSBD_Class_Register() and before xUSBD_Start().
    void xUSBD_DFU_Mode_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Mode_App_Context_t *app_context);

    // Dispatch any pending deferred operation (flash write, manifest, reboot).
    // Call from the main loop; must not be called from an ISR.
    void xUSBD_DFU_Mode_App_Process(xUSBD_Class_Context_t *class_ctx);

    // Expose the simulated flash bank for inspection (e.g. by tests).
    const uint8_t *xUSBD_DFU_Mode_Get_Flash(uint32_t *size_out);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_DFU_MODE_EXAMPLE_H
// EOF /////////////////////////////////////////////////////////////////////////////
