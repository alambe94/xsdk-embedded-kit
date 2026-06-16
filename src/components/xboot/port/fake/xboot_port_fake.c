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

// @file xboot_port_fake.c
// @brief Fake SoC port mock implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_port_fake.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t fake_prepare_handoff(void *port_ctx, uint32_t entry_address);
static void fake_jump(void *port_ctx, uint32_t entry_address);
static void fake_reset(void *port_ctx);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static xRETURN_t fake_prepare_handoff(void *port_ctx, uint32_t entry_address)
{
    if (port_ctx == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    xBOOT_Port_Fake_Context_t *ctx = (xBOOT_Port_Fake_Context_t *)port_ctx;
    ctx->is_prepare_called = true;
    ctx->recorded_entry_address = entry_address;

    if (ctx->prepare_fail_code != xRETURN_xBOOT_OK)
    {
        return ctx->prepare_fail_code;
    }

    return xRETURN_xBOOT_OK;
}

static void fake_jump(void *port_ctx, uint32_t entry_address)
{
    if (port_ctx != NULL)
    {
        xBOOT_Port_Fake_Context_t *ctx = (xBOOT_Port_Fake_Context_t *)port_ctx;
        ctx->is_jump_called = true;
        ctx->recorded_entry_address = entry_address;
    }
}

static void fake_reset(void *port_ctx)
{
    if (port_ctx != NULL)
    {
        xBOOT_Port_Fake_Context_t *ctx = (xBOOT_Port_Fake_Context_t *)port_ctx;
        ctx->is_reset_called = true;
    }
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static const xBOOT_Port_Ops_t fake_port_ops = {
    fake_prepare_handoff,
    fake_jump,
    fake_reset
};

const xBOOT_Port_Ops_t *xBOOT_Port_Fake_Get_Ops(void)
{
    return &fake_port_ops;
}

void xBOOT_Port_Fake_Reset_Context(xBOOT_Port_Fake_Context_t *ctx)
{
    if (ctx != NULL)
    {
        ctx->is_prepare_called = false;
        ctx->is_jump_called = false;
        ctx->is_reset_called = false;
        ctx->recorded_entry_address = 0U;
        ctx->prepare_fail_code = xRETURN_xBOOT_OK;
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
