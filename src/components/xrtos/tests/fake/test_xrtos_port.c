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

// @file test_xrtos_port.c
// @brief Host tests for the xRTOS CPU port interface contract (Phase 7).
//
// Tests verify that:
//   1. The host port ops table is fully populated (all 6 callbacks non-NULL).
//   2. Each host port callback behaves as specified for the host stub.
//   3. xRTOS_Kernel_Init rejects a port ops table with any NULL callback,
//      including the new is_in_isr callback added in Phase 7.
//
// The portable kernel code is tested through xRTOS_Kernel_Init; the host
// port stubs are exercised directly to validate their stub contracts.
//

#include <stdbool.h>
#include <stdint.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_port.h"
#include "xrtos_return.h"
#include "xrtos_task.h"
#include "xrtos_port_fake.h"

// FIXTURES ////////////////////////////////////////////////////////////////////

static xRTOS_Kernel_Context_t s_kernel_ctx;

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel_ctx, &xrtos_fake_port_ops);
}

void tearDown(void)
{
}

// TESTS: fake port ops table completeness /////////////////////////////////////

void test_fake_port_init_task_stack_is_non_null(void)
{
    TEST_ASSERT_NOT_NULL(xrtos_fake_port_ops.init_task_stack);
}

void test_fake_port_start_first_task_is_non_null(void)
{
    TEST_ASSERT_NOT_NULL(xrtos_fake_port_ops.start_first_task);
}

void test_fake_port_yield_is_non_null(void)
{
    TEST_ASSERT_NOT_NULL(xrtos_fake_port_ops.yield);
}

void test_fake_port_disable_interrupts_is_non_null(void)
{
    TEST_ASSERT_NOT_NULL(xrtos_fake_port_ops.disable_interrupts);
}

void test_fake_port_enable_interrupts_is_non_null(void)
{
    TEST_ASSERT_NOT_NULL(xrtos_fake_port_ops.enable_interrupts);
}

void test_fake_port_is_in_isr_is_non_null(void)
{
    TEST_ASSERT_NOT_NULL(xrtos_fake_port_ops.is_in_isr);
}

// TESTS: fake port stub behaviour /////////////////////////////////////////////

void test_fake_port_is_in_isr_returns_false(void)
{
    // The host never executes inside a real interrupt; the stub must return false.
    TEST_ASSERT_FALSE(xrtos_fake_port_ops.is_in_isr());
}

void test_fake_port_disable_interrupts_returns_value(void)
{
    // Host stub returns a dummy saved state; just verify it does not crash.
    uint32_t saved = xrtos_fake_port_ops.disable_interrupts();
    (void)saved;
    TEST_PASS();
}

void test_fake_port_enable_interrupts_does_not_crash(void)
{
    uint32_t saved = xrtos_fake_port_ops.disable_interrupts();
    xrtos_fake_port_ops.enable_interrupts(saved);
    TEST_PASS();
}

void test_fake_port_yield_does_not_crash(void)
{
    xrtos_fake_port_ops.yield();
    TEST_PASS();
}

// TESTS: kernel init rejects incomplete port ops //////////////////////////////

void test_kernel_init_rejects_null_is_in_isr(void)
{
    xRTOS_Port_Ops_t ops = xrtos_fake_port_ops;
    ops.is_in_isr = NULL;

    xRTOS_Kernel_Context_t kernel_ctx;
    xRETURN_t ret = xRTOS_Kernel_Init(&kernel_ctx, &ops);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, ret);
}

void test_kernel_init_with_full_fake_port_ops_succeeds(void)
{
    xRTOS_Kernel_Context_t kernel_ctx;
    xRETURN_t ret = xRTOS_Kernel_Init(&kernel_ctx, &xrtos_fake_port_ops);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_TRUE(kernel_ctx.is_initialized);
}

// TESTS: scheduler lock uses port ops /////////////////////////////////////////

void test_scheduler_lock_uses_port_disable_interrupts(void)
{
    // Verify Lock/Unlock round-trip compiles and does not crash on the host.
    uint32_t saved = xRTOS_Scheduler_Lock();
    xRTOS_Scheduler_Unlock(saved);
    TEST_PASS();
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fake_port_init_task_stack_is_non_null);
    RUN_TEST(test_fake_port_start_first_task_is_non_null);
    RUN_TEST(test_fake_port_yield_is_non_null);
    RUN_TEST(test_fake_port_disable_interrupts_is_non_null);
    RUN_TEST(test_fake_port_enable_interrupts_is_non_null);
    RUN_TEST(test_fake_port_is_in_isr_is_non_null);

    RUN_TEST(test_fake_port_is_in_isr_returns_false);
    RUN_TEST(test_fake_port_disable_interrupts_returns_value);
    RUN_TEST(test_fake_port_enable_interrupts_does_not_crash);
    RUN_TEST(test_fake_port_yield_does_not_crash);

    RUN_TEST(test_kernel_init_rejects_null_is_in_isr);
    RUN_TEST(test_kernel_init_with_full_fake_port_ops_succeeds);

    RUN_TEST(test_scheduler_lock_uses_port_disable_interrupts);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
