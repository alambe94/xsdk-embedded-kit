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

// @file test_xboot_policy.c
// @brief Host tests for xBOOT initialization and boot decision policy.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stdbool.h>

// SYSTEM INCLUDES
#include "unity.h"

// MODULE INCLUDES
#include "xboot_core.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void setUp(void)
{
}

void tearDown(void)
{
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void test_xboot_init_rejects_null_parameters(void)
{
    xBOOT_Context_t ctx;
    xBOOT_Config_t cfg = {0};

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_NULL_POINTER, xBOOT_Init(NULL, &cfg));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT, xBOOT_Init(&ctx, NULL));
}

void test_xboot_init_clears_context_and_configures_properties(void)
{
    xBOOT_Context_t ctx;
    xBOOT_Config_t cfg;

    cfg.storage_ops = (const struct xBOOT_Storage_Ops_t *)0x12345678U;
    cfg.storage_ctx = (void *)0x87654321U;
    cfg.port_ops = (const struct xBOOT_Port_Ops_t *)0xAAAABBBBU;
    cfg.port_ctx = (void *)0xCCCCDDDDU;
    cfg.force_recovery = true;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Init(&ctx, &cfg));
    TEST_ASSERT_TRUE(ctx.is_initialized);
    TEST_ASSERT_TRUE(ctx.is_recovery_requested);
    TEST_ASSERT_EQUAL_PTR(cfg.storage_ops, ctx.storage_ops);
    TEST_ASSERT_EQUAL_PTR(cfg.storage_ctx, ctx.storage_ctx);
    TEST_ASSERT_EQUAL_PTR(cfg.port_ops, ctx.port_ops);
    TEST_ASSERT_EQUAL_PTR(cfg.port_ctx, ctx.port_ctx);
}

void test_xboot_run_rejects_null_context(void)
{
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_NULL_POINTER, xBOOT_Run(NULL));
}

void test_xboot_run_rejects_uninitialized_context(void)
{
    xBOOT_Context_t ctx;
    ctx.is_initialized = false;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_STATE, xBOOT_Run(&ctx));
}

void test_xboot_run_selects_primary_when_no_recovery_requested(void)
{
    xBOOT_Context_t ctx;
    xBOOT_Config_t cfg;

    cfg.storage_ops = NULL;
    cfg.storage_ctx = NULL;
    cfg.port_ops = NULL;
    cfg.port_ctx = NULL;
    cfg.force_recovery = false;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Init(&ctx, &cfg));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Run(&ctx)); // boot primary
}

void test_xboot_run_enters_recovery_when_requested(void)
{
    xBOOT_Context_t ctx;
    xBOOT_Config_t cfg;

    cfg.storage_ops = NULL;
    cfg.storage_ctx = NULL;
    cfg.port_ops = NULL;
    cfg.port_ctx = NULL;
    cfg.force_recovery = true;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Init(&ctx, &cfg));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_NO_BOOTABLE_IMAGE, xBOOT_Run(&ctx)); // recovery
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_xboot_init_rejects_null_parameters);
    RUN_TEST(test_xboot_init_clears_context_and_configures_properties);
    RUN_TEST(test_xboot_run_rejects_null_context);
    RUN_TEST(test_xboot_run_rejects_uninitialized_context);
    RUN_TEST(test_xboot_run_selects_primary_when_no_recovery_requested);
    RUN_TEST(test_xboot_run_enters_recovery_when_requested);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
