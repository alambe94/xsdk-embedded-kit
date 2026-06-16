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

// @file xboot_handoff.c
// @brief Application handoff wrappers implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_handoff.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBOOT_Handoff_Prepare(xBOOT_Context_t *boot_ctx, uint32_t entry_address)
{
    if (boot_ctx == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if ((boot_ctx->port_ops == NULL) || (boot_ctx->port_ops->prepare_handoff == NULL))
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    return boot_ctx->port_ops->prepare_handoff(boot_ctx->port_ctx, entry_address);
}

void xBOOT_Handoff_Jump(xBOOT_Context_t *boot_ctx, uint32_t entry_address)
{
    if (boot_ctx != NULL)
    {
        if ((boot_ctx->port_ops != NULL) && (boot_ctx->port_ops->jump != NULL))
        {
            boot_ctx->port_ops->jump(boot_ctx->port_ctx, entry_address);
        }
    }
}

void xBOOT_Handoff_Reset(xBOOT_Context_t *boot_ctx)
{
    if (boot_ctx != NULL)
    {
        if ((boot_ctx->port_ops != NULL) && (boot_ctx->port_ops->reset != NULL))
        {
            boot_ctx->port_ops->reset(boot_ctx->port_ctx);
        }
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
