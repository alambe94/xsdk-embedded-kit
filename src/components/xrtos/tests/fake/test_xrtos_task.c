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

// @file test_xrtos_task.c
// @brief Host tests for xRTOS_Kernel_Init, xRTOS_Task_Create,
//        xRTOS_Task_Exit, and xRTOS_Priority_Find_Free.

#include <stdint.h>
#include <setjmp.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_return.h"
#include "xrtos_task.h"
#include "xrtos_port_fake.h"

// FIXTURES ////////////////////////////////////////////////////////////////////

// Static kernel and task storage. Tests reinitialize these in setUp.
static xRTOS_Kernel_Context_t s_kernel_ctx;
static xRTOS_Task_Context_t s_task_ctx;
static uint32_t s_stack[64U];

static void dummy_task_entry(void *arg)
{
    (void)arg;
}

void setUp(void)
{
    // Re-initialize the kernel before each test.
    xRETURN_t ret = xRTOS_Kernel_Init(&s_kernel_ctx, &xrtos_fake_port_ops);
    (void)ret;
}

void tearDown(void)
{
}

// HELPERS /////////////////////////////////////////////////////////////////////

static xRTOS_Task_Config_t make_task_config(uint32_t task_id, uint32_t priority)
{
    xRTOS_Task_Config_t cfg;
    cfg.task_id = task_id;
    cfg.priority = priority;
    cfg.entry = dummy_task_entry;
    cfg.entry_arg = NULL;
    cfg.stack_mem = s_stack;
    cfg.stack_words = 64U;
    cfg.name = NULL;
    return cfg;
}

static void assert_kernel_init_rejects_port_ops(const xRTOS_Port_Ops_t *port_ops)
{
    xRTOS_Kernel_Context_t kernel_ctx;
    xRETURN_t ret = xRTOS_Kernel_Init(&kernel_ctx, port_ops);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_NULL_POINTER, ret);
}

// TESTS: xRTOS_Kernel_Init ////////////////////////////////////////////////////

void test_kernel_init_valid_args_returns_ok(void)
{
    xRTOS_Kernel_Context_t kernel_ctx;
    xRETURN_t ret = xRTOS_Kernel_Init(&kernel_ctx, &xrtos_fake_port_ops);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_TRUE(kernel_ctx.is_initialized);
}

void test_kernel_init_null_kernel_ctx_returns_null_pointer_error(void)
{
    xRETURN_t ret = xRTOS_Kernel_Init(NULL, &xrtos_fake_port_ops);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_NULL_POINTER, ret);
}

void test_kernel_init_null_port_ops_returns_null_pointer_error(void)
{
    xRTOS_Kernel_Context_t kernel_ctx;
    xRETURN_t ret = xRTOS_Kernel_Init(&kernel_ctx, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_NULL_POINTER, ret);
}

void test_kernel_init_null_port_callback_returns_null_pointer_error(void)
{
    xRTOS_Port_Ops_t ops = xrtos_fake_port_ops;

    ops.init_task_stack = NULL;
    assert_kernel_init_rejects_port_ops(&ops);

    ops = xrtos_fake_port_ops;
    ops.start_first_task = NULL;
    assert_kernel_init_rejects_port_ops(&ops);

    ops = xrtos_fake_port_ops;
    ops.yield = NULL;
    assert_kernel_init_rejects_port_ops(&ops);

    ops = xrtos_fake_port_ops;
    ops.disable_interrupts = NULL;
    assert_kernel_init_rejects_port_ops(&ops);

    ops = xrtos_fake_port_ops;
    ops.enable_interrupts = NULL;
    assert_kernel_init_rejects_port_ops(&ops);

    ops = xrtos_fake_port_ops;
    ops.is_in_isr = NULL;
    assert_kernel_init_rejects_port_ops(&ops);
}

void test_kernel_init_zeroes_task_tables(void)
{
    xRTOS_Kernel_Context_t kernel_ctx;
    (void)xRTOS_Kernel_Init(&kernel_ctx, &xrtos_fake_port_ops);
    for (uint32_t i = 0U; i < xRTOS_MAX_TASKS; i++)
    {
        TEST_ASSERT_NULL(kernel_ctx.task_table[i]);
    }
    for (uint32_t i = 0U; i < xRTOS_MAX_PRIORITIES; i++)
    {
        TEST_ASSERT_NULL(kernel_ctx.task_by_priority[i]);
    }
}

// TESTS: xRTOS_Task_Create /////////////////////////////////////////////

void test_task_create_static_valid_args_returns_ok(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    xRETURN_t ret = xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xRTOS_OK, ret);
}

void test_task_create_static_sets_state_ready(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_INT(xRTOS_TASK_STATE_READY, s_task_ctx.state);
}

void test_task_create_static_registers_in_task_table(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx, s_kernel_ctx.task_table[0U]);
}

void test_task_create_static_copies_task_id_and_priority(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(5U, 3U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_UINT32(5U, s_task_ctx.task_id);
    TEST_ASSERT_EQUAL_UINT32(3U, s_task_ctx.base_priority);
}

void test_task_create_static_writes_stack_canary(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRTOS_STACK_CANARY, s_stack[0U]);
}

void test_task_create_static_null_task_ctx_returns_null_pointer_error(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    xRETURN_t ret = xRTOS_Task_Create(NULL, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_NULL_POINTER, ret);
}

void test_task_create_static_null_config_returns_null_pointer_error(void)
{
    xRETURN_t ret = xRTOS_Task_Create(&s_task_ctx, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_NULL_POINTER, ret);
}

void test_task_create_shared_priority_succeeds(void)
{
    // Shared base priorities are always permitted. The ready list handles
    // them in FIFO order; cooperative yield or round-robin switching (when
    // xRTOS_CONFIG_ROUND_ROBIN_ENABLE=1) gives peer tasks CPU time.
    static xRTOS_Task_Context_t second_task_ctx;
    static uint32_t second_stack[64U];

    xRTOS_Task_Config_t cfg1 = make_task_config(0U, 5U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg1);

    xRTOS_Task_Config_t cfg2;
    cfg2.task_id = 1U;
    cfg2.priority = 5U;
    cfg2.entry = dummy_task_entry;
    cfg2.entry_arg = NULL;
    cfg2.stack_mem = second_stack;
    cfg2.stack_words = 64U;
    cfg2.name = NULL;

    xRETURN_t ret = xRTOS_Task_Create(&second_task_ctx, &cfg2);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xRTOS_OK, ret);
}

void test_task_create_static_task_id_in_use_returns_task_limit(void)
{
    static xRTOS_Task_Context_t second_task_ctx;
    static uint32_t second_stack[64U];

    xRTOS_Task_Config_t cfg1 = make_task_config(0U, 0U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg1);

    xRTOS_Task_Config_t cfg2;
    cfg2.task_id = 0U; // Same task_id - must be rejected.
    cfg2.priority = 1U;
    cfg2.entry = dummy_task_entry;
    cfg2.entry_arg = NULL;
    cfg2.stack_mem = second_stack;
    cfg2.stack_words = 64U;
    cfg2.name = NULL;

    xRETURN_t ret = xRTOS_Task_Create(&second_task_ctx, &cfg2);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_TASK_LIMIT, ret);
}

void test_task_create_static_sets_ready_bit_in_scheduler(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 7U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_map, 0U));
}

void test_task_create_static_idle_priority_requires_idle_task_id(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, xRTOS_IDLE_PRIORITY);
    xRETURN_t ret = xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

void test_task_create_static_idle_task_id_requires_idle_priority(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(xRTOS_IDLE_TASK_ID, 0U);
    xRETURN_t ret = xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

// TESTS: xRTOS_Kernel_Start ///////////////////////////////////////////////////

void test_kernel_start_without_idle_task_returns_invalid_state(void)
{
    // Kernel is initialized (setUp) but no idle task is registered.
    xRETURN_t ret = xRTOS_Kernel_Start();
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_INVALID_STATE, ret);
}

void test_kernel_start_with_idle_task_returns_ok_on_host(void)
{
    // The fake port returns from start_first_task so the success path is testable.
    static xRTOS_Task_Context_t idle_ctx;
    static uint32_t idle_stack[64U];

    xRTOS_Task_Config_t idle_cfg;
    idle_cfg.task_id = xRTOS_IDLE_TASK_ID;
    idle_cfg.priority = xRTOS_IDLE_PRIORITY;
    idle_cfg.entry = dummy_task_entry;
    idle_cfg.entry_arg = NULL;
    idle_cfg.stack_mem = idle_stack;
    idle_cfg.stack_words = 64U;
    idle_cfg.name = NULL;

    (void)xRTOS_Task_Create(&idle_ctx, &idle_cfg);

    xRETURN_t ret = xRTOS_Kernel_Start();
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_TRUE(s_kernel_ctx.scheduler.is_started);
    TEST_ASSERT_EQUAL_INT(xRTOS_TASK_STATE_RUNNING, idle_ctx.state);
}

void test_kernel_init_after_start_returns_invalid_state(void)
{
    static xRTOS_Task_Context_t idle_ctx;
    static uint32_t idle_stack[64U];
    xRTOS_Kernel_Context_t other_kernel;

    xRTOS_Task_Config_t idle_cfg;
    idle_cfg.task_id = xRTOS_IDLE_TASK_ID;
    idle_cfg.priority = xRTOS_IDLE_PRIORITY;
    idle_cfg.entry = dummy_task_entry;
    idle_cfg.entry_arg = NULL;
    idle_cfg.stack_mem = idle_stack;
    idle_cfg.stack_words = 64U;
    idle_cfg.name = NULL;

    (void)xRTOS_Task_Create(&idle_ctx, &idle_cfg);
    (void)xRTOS_Kernel_Start();

    xRETURN_t ret = xRTOS_Kernel_Init(&other_kernel, &xrtos_fake_port_ops);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_INVALID_STATE, ret);
    TEST_ASSERT_TRUE(s_kernel_ctx.scheduler.is_started);
}

// TESTS: xRTOS_Priority_Find_Free /////////////////////////////////////////////

void test_priority_find_free_exact_free_priority_returns_ok(void)
{
    uint32_t assigned = 0xFFU;
    xRETURN_t ret = xRTOS_Priority_Find_Free(5U, xRTOS_PRIORITY_SEARCH_MODE_EXACT, &assigned);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(5U, assigned);
}

void test_priority_find_free_exact_used_priority_returns_priority_in_use(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 5U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);

    uint32_t assigned = 0xFFU;
    xRETURN_t ret = xRTOS_Priority_Find_Free(5U, xRTOS_PRIORITY_SEARCH_MODE_EXACT, &assigned);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_PRIORITY_IN_USE, ret);
}

void test_priority_find_free_toward_higher_finds_free_slot(void)
{
    // Fill priorities 5 and 4; preferred=5. Should find 3.
    static xRTOS_Task_Context_t t4, t5;
    static uint32_t s4[64U], s5[64U];

    xRTOS_Task_Config_t c5 = {5U, 5U, dummy_task_entry, NULL, s5, 64U, NULL};
    xRTOS_Task_Config_t c4 = {4U, 4U, dummy_task_entry, NULL, s4, 64U, NULL};
    (void)xRTOS_Task_Create(&t5, &c5);
    (void)xRTOS_Task_Create(&t4, &c4);

    uint32_t assigned = 0xFFU;
    xRETURN_t ret = xRTOS_Priority_Find_Free(5U, xRTOS_PRIORITY_SEARCH_MODE_TOWARD_HIGHER, &assigned);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(3U, assigned);
}

void test_priority_find_free_toward_lower_finds_free_slot(void)
{
    // Fill priorities 5 and 6; preferred=5. Should find 7.
    static xRTOS_Task_Context_t t5, t6;
    static uint32_t s5[64U], s6[64U];

    xRTOS_Task_Config_t c5 = {5U, 5U, dummy_task_entry, NULL, s5, 64U, NULL};
    xRTOS_Task_Config_t c6 = {6U, 6U, dummy_task_entry, NULL, s6, 64U, NULL};
    (void)xRTOS_Task_Create(&t5, &c5);
    (void)xRTOS_Task_Create(&t6, &c6);

    uint32_t assigned = 0xFFU;
    xRETURN_t ret = xRTOS_Priority_Find_Free(5U, xRTOS_PRIORITY_SEARCH_MODE_TOWARD_LOWER, &assigned);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(7U, assigned);
}

void test_priority_find_free_null_out_ptr_returns_null_pointer_error(void)
{
    xRETURN_t ret = xRTOS_Priority_Find_Free(5U, xRTOS_PRIORITY_SEARCH_MODE_EXACT, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_NULL_POINTER, ret);
}

void test_priority_find_free_excludes_idle_priority(void)
{
    // preferred_priority == xRTOS_IDLE_PRIORITY must be rejected.
    uint32_t assigned = 0xFFU;
    xRETURN_t ret = xRTOS_Priority_Find_Free(xRTOS_IDLE_PRIORITY, xRTOS_PRIORITY_SEARCH_MODE_EXACT, &assigned);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

void test_priority_find_free_toward_higher_all_used_returns_priority_in_use(void)
{
    // Fill priorities 0-2; preferred=2. No free slot toward higher (0).
    static xRTOS_Task_Context_t t0, t1, t2;
    static uint32_t s0[64U], s1[64U], s2[64U];

    xRTOS_Task_Config_t c0 = {0U, 0U, dummy_task_entry, NULL, s0, 64U, NULL};
    xRTOS_Task_Config_t c1 = {1U, 1U, dummy_task_entry, NULL, s1, 64U, NULL};
    xRTOS_Task_Config_t c2 = {2U, 2U, dummy_task_entry, NULL, s2, 64U, NULL};
    (void)xRTOS_Task_Create(&t0, &c0);
    (void)xRTOS_Task_Create(&t1, &c1);
    (void)xRTOS_Task_Create(&t2, &c2);

    uint32_t assigned = 0xFFU;
    xRETURN_t ret = xRTOS_Priority_Find_Free(2U, xRTOS_PRIORITY_SEARCH_MODE_TOWARD_HIGHER, &assigned);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_PRIORITY_IN_USE, ret);
}

// TESTS: validate_task_config error paths //////////////////////////////////////

void test_task_create_out_of_range_task_id_returns_invalid_argument(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(xRTOS_MAX_TASKS, 0U);
    xRETURN_t ret = xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

void test_task_create_out_of_range_priority_returns_invalid_argument(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, xRTOS_MAX_PRIORITIES);
    xRETURN_t ret = xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

void test_task_create_null_entry_returns_null_pointer_error(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    cfg.entry = NULL;
    xRETURN_t ret = xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_NULL_POINTER, ret);
}

void test_task_create_null_stack_mem_returns_null_pointer_error(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    cfg.stack_mem = NULL;
    xRETURN_t ret = xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_NULL_POINTER, ret);
}

void test_task_create_stack_too_small_returns_invalid_argument(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    cfg.stack_words = xRTOS_CONFIG_MIN_STACK_WORDS - 1U;
    xRETURN_t ret = xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

void test_priority_find_free_toward_lower_all_used_returns_priority_in_use(void)
{
    // Fill priorities 0-2; preferred=0. No free slot toward lower (up to idle).
    static xRTOS_Task_Context_t t0, t1, t2;
    static uint32_t s0[64U], s1[64U], s2[64U];

    xRTOS_Task_Config_t c0 = {0U, 0U, dummy_task_entry, NULL, s0, 64U, NULL};
    xRTOS_Task_Config_t c1 = {1U, 1U, dummy_task_entry, NULL, s1, 64U, NULL};
    xRTOS_Task_Config_t c2 = {2U, 2U, dummy_task_entry, NULL, s2, 64U, NULL};
    (void)xRTOS_Task_Create(&t0, &c0);
    (void)xRTOS_Task_Create(&t1, &c1);
    (void)xRTOS_Task_Create(&t2, &c2);

    // Fill remaining user priorities so every slot is occupied.
    static xRTOS_Task_Context_t t_rest[xRTOS_MAX_PRIORITIES - 4U];
    static uint32_t s_rest[xRTOS_MAX_PRIORITIES - 4U][64U];
    for (uint32_t p = 3U; p <= xRTOS_LOWEST_USER_PRIORITY; p++)
    {
        xRTOS_Task_Config_t cfg = {p, p, dummy_task_entry, NULL, s_rest[p - 3U], 64U, NULL};
        (void)xRTOS_Task_Create(&t_rest[p - 3U], &cfg);
    }

    uint32_t assigned = 0xFFU;
    xRETURN_t ret = xRTOS_Priority_Find_Free(0U, xRTOS_PRIORITY_SEARCH_MODE_TOWARD_LOWER, &assigned);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xRTOS_PRIORITY_IN_USE, ret);
}

static jmp_buf s_exit_jmp;
static void test_yield_jmp(void)
{
    longjmp(s_exit_jmp, 1);
}

void test_task_exit_clears_registry_slots(void)
{
    // Initialize custom port ops with our mock yield
    xRTOS_Port_Ops_t custom_ops = xrtos_fake_port_ops;
    custom_ops.yield = test_yield_jmp;

    // Reset kernel with our custom port ops
    (void)xRTOS_Kernel_Init(&s_kernel_ctx, &custom_ops);

    // Create Idle task (required to start)
    xRTOS_Task_Config_t idle_cfg = make_task_config(xRTOS_IDLE_TASK_ID, xRTOS_IDLE_PRIORITY);
    (void)xRTOS_Task_Create(&s_task_ctx, &idle_cfg);

    // Create a dummy task
    static xRTOS_Task_Context_t dummy_ctx;
    static uint32_t dummy_stack[64] = {0};
    xRTOS_Task_Config_t dummy_cfg = {
        .task_id = 2U, .priority = 4U, .entry = dummy_task_entry, .entry_arg = NULL, .stack_mem = dummy_stack, .stack_words = 64};
    (void)xRTOS_Task_Create(&dummy_ctx, &dummy_cfg);

    // Force current task to be dummy task
    s_kernel_ctx.scheduler.current_task_id = 2U;

    // Call exit and catch the yield jump
    if (setjmp(s_exit_jmp) == 0)
    {
        xRTOS_Task_Exit();
    }

    // After exit, check that registry slots are cleared!
    TEST_ASSERT_NULL(s_kernel_ctx.task_table[2U]);
    TEST_ASSERT_NULL(s_kernel_ctx.task_by_priority[4U]);

    // Check state is terminated
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_TERMINATED, dummy_ctx.state);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_kernel_init_valid_args_returns_ok);
    RUN_TEST(test_kernel_init_null_kernel_ctx_returns_null_pointer_error);
    RUN_TEST(test_kernel_init_null_port_ops_returns_null_pointer_error);
    RUN_TEST(test_kernel_init_null_port_callback_returns_null_pointer_error);
    RUN_TEST(test_kernel_init_zeroes_task_tables);

    RUN_TEST(test_task_create_static_valid_args_returns_ok);
    RUN_TEST(test_task_create_static_sets_state_ready);
    RUN_TEST(test_task_create_static_registers_in_task_table);
    RUN_TEST(test_task_create_static_copies_task_id_and_priority);
    RUN_TEST(test_task_create_static_writes_stack_canary);
    RUN_TEST(test_task_create_static_null_task_ctx_returns_null_pointer_error);
    RUN_TEST(test_task_create_static_null_config_returns_null_pointer_error);
    RUN_TEST(test_task_create_shared_priority_succeeds);
    RUN_TEST(test_task_create_static_task_id_in_use_returns_task_limit);
    RUN_TEST(test_task_create_static_sets_ready_bit_in_scheduler);
    RUN_TEST(test_task_create_static_idle_priority_requires_idle_task_id);
    RUN_TEST(test_task_create_out_of_range_task_id_returns_invalid_argument);
    RUN_TEST(test_task_create_out_of_range_priority_returns_invalid_argument);
    RUN_TEST(test_task_create_null_entry_returns_null_pointer_error);
    RUN_TEST(test_task_create_null_stack_mem_returns_null_pointer_error);
    RUN_TEST(test_task_create_stack_too_small_returns_invalid_argument);
    RUN_TEST(test_task_create_static_idle_task_id_requires_idle_priority);

    RUN_TEST(test_kernel_start_without_idle_task_returns_invalid_state);
    RUN_TEST(test_kernel_start_with_idle_task_returns_ok_on_host);
    RUN_TEST(test_kernel_init_after_start_returns_invalid_state);

    RUN_TEST(test_priority_find_free_exact_free_priority_returns_ok);
    RUN_TEST(test_priority_find_free_exact_used_priority_returns_priority_in_use);
    RUN_TEST(test_priority_find_free_toward_higher_finds_free_slot);
    RUN_TEST(test_priority_find_free_toward_lower_finds_free_slot);
    RUN_TEST(test_priority_find_free_null_out_ptr_returns_null_pointer_error);
    RUN_TEST(test_priority_find_free_excludes_idle_priority);
    RUN_TEST(test_priority_find_free_toward_higher_all_used_returns_priority_in_use);
    RUN_TEST(test_priority_find_free_toward_lower_all_used_returns_priority_in_use);
    RUN_TEST(test_task_exit_clears_registry_slots);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
