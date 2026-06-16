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

// @file xusbd_dfu_runtime_example.h
// @brief Application-level hooks and configuration for USB DFU.

#ifndef XUSBD_DFU_RUNTIME_EXAMPLE_H
#define XUSBD_DFU_RUNTIME_EXAMPLE_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_dfu.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        volatile uint8_t reset_complete;
        xUSBD_DFU_State_t last_state;
    } xUSBD_DFU_App_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // Wire DFU callbacks and store app_context for use by Process.
    // Call once after xUSBD_Class_Register() and before xUSBD_Start().
    void xUSBD_DFU_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_App_Context_t *app_context);

    // Poll for pending DFU operations and dispatch them.
    // Call from the main loop; must not be called from an ISR.
    void xUSBD_DFU_App_Process(xUSBD_Class_Context_t *class_ctx);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_DFU_RUNTIME_EXAMPLE_H
// EOF /////////////////////////////////////////////////////////////////////////////
