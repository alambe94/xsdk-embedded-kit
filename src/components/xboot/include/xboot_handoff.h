// Copyright 2026 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xboot_handoff.h
// @brief Application handoff interface and wrapper declarations.
//

#ifndef XBOOT_HANDOFF_H
#define XBOOT_HANDOFF_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_core.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xBOOT_Port_Ops_t
    {
        xRETURN_t (*prepare_handoff)(void *port_ctx, uint32_t entry_address);
        void (*jump)(void *port_ctx, uint32_t entry_address);
        void (*reset)(void *port_ctx);
    } xBOOT_Port_Ops_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    /**
     * @brief Prepare the CPU/SoC state for handoff.
     * @param boot_ctx Bootloader context
     * @param entry_address Target application entry point address
     * @return xRETURN_t xRETURN_xBOOT_OK on success, error code otherwise
     */
    xRETURN_t xBOOT_Handoff_Prepare(xBOOT_Context_t *boot_ctx, uint32_t entry_address);

    /**
     * @brief Jump directly to the target application entry point.
     * @param boot_ctx Bootloader context
     * @param entry_address Target application entry point address
     */
    void xBOOT_Handoff_Jump(xBOOT_Context_t *boot_ctx, uint32_t entry_address);

    /**
     * @brief Trigger a system reset.
     * @param boot_ctx Bootloader context
     */
    void xBOOT_Handoff_Reset(xBOOT_Context_t *boot_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_HANDOFF_H
// EOF /////////////////////////////////////////////////////////////////////////////
