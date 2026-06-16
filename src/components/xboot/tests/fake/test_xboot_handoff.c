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

// @file test_xboot_handoff.c
// @brief Unit tests for xBOOT application handoff interface.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>

// SYSTEM INCLUDES
#include "unity.h"

// MODULE INCLUDES
#include "xboot_handoff.h"
#include "xboot_port_fake.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////
static xBOOT_Context_t g_boot_ctx;
static xBOOT_Port_Fake_Context_t g_fake_port_ctx;

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
void setUp(void);
void tearDown(void);
void test_xBOOT_Handoff_Prepare_Success(void);
void test_xBOOT_Handoff_Prepare_NullContext(void);
void test_xBOOT_Handoff_Prepare_NullOps(void);
void test_xBOOT_Handoff_Prepare_FailurePropagation(void);
void test_xBOOT_Handoff_Jump_Success(void);
void test_xBOOT_Handoff_Jump_NullContext(void);
void test_xBOOT_Handoff_Reset_Success(void);
void test_xBOOT_Handoff_Reset_NullContext(void);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void setUp(void)
{
    xBOOT_Port_Fake_Reset_Context(&g_fake_port_ctx);
    g_boot_ctx.port_ops = xBOOT_Port_Fake_Get_Ops();
    g_boot_ctx.port_ctx = &g_fake_port_ctx;
    g_boot_ctx.is_initialized = true;
    g_boot_ctx.is_recovery_requested = false;
}

void tearDown(void)
{
}

void test_xBOOT_Handoff_Prepare_Success(void)
{
    uint32_t entry = 0x70040000U;
    xRETURN_t status = xBOOT_Handoff_Prepare(&g_boot_ctx, entry);
    TEST_ASSERT_EQUAL(xRETURN_xBOOT_OK, status);
    TEST_ASSERT_TRUE(g_fake_port_ctx.is_prepare_called);
    TEST_ASSERT_EQUAL(entry, g_fake_port_ctx.recorded_entry_address);
}

void test_xBOOT_Handoff_Prepare_NullContext(void)
{
    xRETURN_t status = xBOOT_Handoff_Prepare(NULL, 0x70040000U);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xBOOT_NULL_POINTER, status);
}

void test_xBOOT_Handoff_Prepare_NullOps(void)
{
    g_boot_ctx.port_ops = NULL;
    xRETURN_t status = xBOOT_Handoff_Prepare(&g_boot_ctx, 0x70040000U);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xBOOT_NULL_POINTER, status);
}

void test_xBOOT_Handoff_Prepare_FailurePropagation(void)
{
    g_fake_port_ctx.prepare_fail_code = xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    xRETURN_t status = xBOOT_Handoff_Prepare(&g_boot_ctx, 0x70040000U);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xBOOT_INVALID_ARGUMENT, status);
    TEST_ASSERT_TRUE(g_fake_port_ctx.is_prepare_called);
}

void test_xBOOT_Handoff_Jump_Success(void)
{
    uint32_t entry = 0x70040000U;
    xBOOT_Handoff_Jump(&g_boot_ctx, entry);
    TEST_ASSERT_TRUE(g_fake_port_ctx.is_jump_called);
    TEST_ASSERT_EQUAL(entry, g_fake_port_ctx.recorded_entry_address);
}

void test_xBOOT_Handoff_Jump_NullContext(void)
{
    xBOOT_Handoff_Jump(NULL, 0x70040000U);
    TEST_ASSERT_FALSE(g_fake_port_ctx.is_jump_called);
}

void test_xBOOT_Handoff_Reset_Success(void)
{
    xBOOT_Handoff_Reset(&g_boot_ctx);
    TEST_ASSERT_TRUE(g_fake_port_ctx.is_reset_called);
}

void test_xBOOT_Handoff_Reset_NullContext(void)
{
    xBOOT_Handoff_Reset(NULL);
    TEST_ASSERT_FALSE(g_fake_port_ctx.is_reset_called);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_xBOOT_Handoff_Prepare_Success);
    RUN_TEST(test_xBOOT_Handoff_Prepare_NullContext);
    RUN_TEST(test_xBOOT_Handoff_Prepare_NullOps);
    RUN_TEST(test_xBOOT_Handoff_Prepare_FailurePropagation);
    RUN_TEST(test_xBOOT_Handoff_Jump_Success);
    RUN_TEST(test_xBOOT_Handoff_Jump_NullContext);
    RUN_TEST(test_xBOOT_Handoff_Reset_Success);
    RUN_TEST(test_xBOOT_Handoff_Reset_NullContext);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
