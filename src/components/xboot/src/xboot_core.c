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

// @file xboot_core.c
// @brief Core bootloader lifecycle implementation (Init, Run).
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_core.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBOOT_Init(xBOOT_Context_t *boot_ctx, const xBOOT_Config_t *boot_config)
{
    if (boot_ctx == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (boot_config == NULL)
    {
        return xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    }

    // Initialize context fields from configuration parameters
    boot_ctx->storage_ops = boot_config->storage_ops;
    boot_ctx->storage_ctx = boot_config->storage_ctx;
    boot_ctx->port_ops = boot_config->port_ops;
    boot_ctx->port_ctx = boot_config->port_ctx;
    boot_ctx->is_recovery_requested = boot_config->force_recovery;
    boot_ctx->is_initialized = true;

    return xRETURN_xBOOT_OK;
}

xRETURN_t xBOOT_Run(xBOOT_Context_t *boot_ctx)
{
    if (boot_ctx == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (!boot_ctx->is_initialized)
    {
        return xRETURN_xERR_xBOOT_INVALID_STATE;
    }

    // Deterministic selection policy for Phase 2:
    // recovery takes absolute precedence.
    if (boot_ctx->is_recovery_requested)
    {
        return xRETURN_xERR_xBOOT_NO_BOOTABLE_IMAGE; // Placeholder: enter recovery / halt
    }

    return xRETURN_xBOOT_OK; // Placeholder: boot primary
}

// EOF /////////////////////////////////////////////////////////////////////////////
