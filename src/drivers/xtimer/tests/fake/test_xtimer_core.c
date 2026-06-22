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

// @file test_xtimer_core.c
// @brief Host unit tests for xTIMER core driver.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
#include "unity.h"
#include "xtimer.h"
#include "xtimer_fake.h"
#include <string.h>

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////
static xTIMER_Context_t s_timer_ctx;
static xTIMER_Fake_Context_t s_fake_ctx;
static xTIMER_Instance_t s_instance;
static xTIMER_Config_t s_config;

static bool s_callback_fired;
static xTIMER_Context_t *s_callback_ctx;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static void test_periodic_callback(xTIMER_Context_t *timer_ctx, void *user_ctx);

// SETUP & TEARDOWN ////////////////////////////////////////////////////////////////
void setUp(void)
{
    (void)memset(&s_timer_ctx, 0, sizeof(s_timer_ctx));
    (void)memset(&s_fake_ctx, 0, sizeof(s_fake_ctx));
    (void)memset(&s_config, 0, sizeof(s_config));

    s_instance.ops = &xTIMER_Fake_Driver_Ops;
    s_instance.driver_ctx = &s_fake_ctx;

    s_config.period_us = 1000U;
    s_config.module_clk_hz = 1000000U;

    s_callback_fired = false;
    s_callback_ctx = NULL;
}

void tearDown(void)
{
}

// PRIVATE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
static void test_periodic_callback(xTIMER_Context_t *timer_ctx, void *user_ctx)
{
    (void)user_ctx;
    s_callback_fired = true;
    s_callback_ctx = timer_ctx;
}

// TEST CASES //////////////////////////////////////////////////////////////////////
void test_TIMER_Init_NullPointerGuard(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xTIMER_NULL_POINTER, xTIMER_Init(NULL, &s_instance, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xTIMER_NULL_POINTER, xTIMER_Init(&s_timer_ctx, NULL, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xTIMER_NULL_POINTER, xTIMER_Init(&s_timer_ctx, &s_instance, NULL));
}

void test_TIMER_Init_Success(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Init(&s_timer_ctx, &s_instance, &s_config));
    TEST_ASSERT_TRUE(s_timer_ctx.is_initialized);
    TEST_ASSERT_EQUAL_PTR(&xTIMER_Fake_Driver_Ops, s_timer_ctx.ops);
    TEST_ASSERT_EQUAL_PTR(&s_fake_ctx, s_timer_ctx.driver_ctx);
}

void test_TIMER_Deinit_WithoutInit(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xTIMER_NOT_INITIALIZED, xTIMER_Deinit(&s_timer_ctx));
}

void test_TIMER_Deinit_Success(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Init(&s_timer_ctx, &s_instance, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Deinit(&s_timer_ctx));
    TEST_ASSERT_FALSE(s_timer_ctx.is_initialized);
}

void test_TIMER_StartStop(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Init(&s_timer_ctx, &s_instance, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Start(&s_timer_ctx));
    TEST_ASSERT_TRUE(s_fake_ctx.is_started);

    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Stop(&s_timer_ctx));
    TEST_ASSERT_FALSE(s_fake_ctx.is_started);
}

void test_TIMER_GetCount(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Init(&s_timer_ctx, &s_instance, &s_config));
    uint32_t count = 0;

    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Get_Count(&s_timer_ctx, &count));
    TEST_ASSERT_EQUAL(1U, count);

    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Get_Count(&s_timer_ctx, &count));
    TEST_ASSERT_EQUAL(2U, count);
}

void test_TIMER_PeriodicCallback(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Init(&s_timer_ctx, &s_instance, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Set_Periodic_Callback(&s_timer_ctx, test_periodic_callback, NULL));

    TEST_ASSERT_EQUAL(xRETURN_OK, xTIMER_Start(&s_timer_ctx));

    // Simulate hardware interrupt trigger
    xTIMER_Fake_Trigger_Interrupt(&s_fake_ctx);

    TEST_ASSERT_TRUE(s_callback_fired);
    TEST_ASSERT_EQUAL_PTR(&s_timer_ctx, s_callback_ctx);
}

// MAIN ////////////////////////////////////////////////////////////////////////////
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_TIMER_Init_NullPointerGuard);
    RUN_TEST(test_TIMER_Init_Success);
    RUN_TEST(test_TIMER_Deinit_WithoutInit);
    RUN_TEST(test_TIMER_Deinit_Success);
    RUN_TEST(test_TIMER_StartStop);
    RUN_TEST(test_TIMER_GetCount);
    RUN_TEST(test_TIMER_PeriodicCallback);

    return UNITY_END();
}
