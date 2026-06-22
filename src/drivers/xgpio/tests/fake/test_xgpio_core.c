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

// @file test_xgpio_core.c
// @brief Public xGPIO core tests using the fake port.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
#include <string.h>
#include "unity.h"
#include "xgpio.h"
#include "xgpio_fake.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////
static xGPIO_Fake_Context_t s_fake_ctx;
static xGPIO_Context_t s_gpio_ctx;
static xGPIO_Instance_t s_instance;
static xGPIO_Config_t s_config;

static xGPIO_Context_t *s_callback_ctx;
static void *s_callback_user_ctx;
static bool s_callback_fired;
static uint32_t s_callback_pin;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
void setUp(void);
void tearDown(void);
static void do_init(void);
static void test_interrupt_handler(xGPIO_Context_t *gpio_ctx, uint32_t pin, void *user_ctx);

// SETUP / TEARDOWN ////////////////////////////////////////////////////////////////

void setUp(void)
{
    (void)memset(&s_fake_ctx, 0, sizeof(s_fake_ctx));
    (void)memset(&s_gpio_ctx, 0, sizeof(s_gpio_ctx));

    s_instance.ops = &xGPIO_Fake_Driver_Ops;
    s_instance.driver_ctx = &s_fake_ctx;

    s_config.reserved = 0U;

    s_callback_fired = false;
    s_callback_pin = 0xFFFFFFFFU;
    s_callback_ctx = NULL;
    s_callback_user_ctx = NULL;
}

void tearDown(void)
{
}

// HELPERS /////////////////////////////////////////////////////////////////////////

static void do_init(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Init(&s_gpio_ctx, &s_instance, &s_config));
}

static void test_interrupt_handler(xGPIO_Context_t *gpio_ctx, uint32_t pin, void *user_ctx)
{
    s_callback_fired = true;
    s_callback_pin = pin;
    s_callback_ctx = gpio_ctx;
    s_callback_user_ctx = user_ctx;
}

// TESTS — Init / Deinit ///////////////////////////////////////////////////////////

void test_Init_NullContext_ReturnsNullPointer(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xGPIO_NULL_POINTER, xGPIO_Init(NULL, &s_instance, &s_config));
}

void test_Init_NullInstance_ReturnsNullPointer(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xGPIO_NULL_POINTER, xGPIO_Init(&s_gpio_ctx, NULL, &s_config));
}

void test_Init_NullOps_ReturnsNullPointer(void)
{
    xGPIO_Instance_t bad_inst = s_instance;
    bad_inst.ops = NULL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xGPIO_NULL_POINTER, xGPIO_Init(&s_gpio_ctx, &bad_inst, &s_config));
}

void test_Init_ValidArgs_SetsInitialized(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Init(&s_gpio_ctx, &s_instance, &s_config));
    TEST_ASSERT_TRUE(s_gpio_ctx.is_initialized);
    TEST_ASSERT_TRUE(s_fake_ctx.is_initialized);
}

void test_Deinit_NotInitialized_ReturnsError(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xGPIO_NOT_INITIALIZED, xGPIO_Deinit(&s_gpio_ctx));
}

void test_Deinit_ValidArgs_ClearsInitialized(void)
{
    do_init();
    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Deinit(&s_gpio_ctx));
    TEST_ASSERT_FALSE(s_gpio_ctx.is_initialized);
    TEST_ASSERT_FALSE(s_fake_ctx.is_initialized);
}

// TESTS — Pin Operations //////////////////////////////////////////////////////////

void test_ConfigurePin_ValidArgs_ConfiguresPin(void)
{
    do_init();
    xGPIO_Pin_Config_t pin_cfg = {
        .mode = xGPIO_PIN_MODE_OUTPUT_PUSH_PULL, .speed = xGPIO_PIN_SPEED_VERY_HIGH, .pull = xGPIO_PIN_PULL_NONE, .alternate_function = 0U};

    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Configure_Pin(&s_gpio_ctx, 5U, &pin_cfg));
    TEST_ASSERT_EQUAL(xGPIO_PIN_MODE_OUTPUT_PUSH_PULL, s_fake_ctx.pins_config[5U].mode);
    TEST_ASSERT_EQUAL(xGPIO_PIN_SPEED_VERY_HIGH, s_fake_ctx.pins_config[5U].speed);
}

void test_PinWrite_ValidArgs_WritesLevel(void)
{
    do_init();
    xGPIO_Pin_Config_t pin_cfg = {
        .mode = xGPIO_PIN_MODE_OUTPUT_PUSH_PULL, .speed = xGPIO_PIN_SPEED_LOW, .pull = xGPIO_PIN_PULL_NONE, .alternate_function = 0U};

    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Configure_Pin(&s_gpio_ctx, 2U, &pin_cfg));

    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Pin_Write(&s_gpio_ctx, 2U, true));
    TEST_ASSERT_TRUE(s_fake_ctx.pins_level[2U]);

    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Pin_Write(&s_gpio_ctx, 2U, false));
    TEST_ASSERT_FALSE(s_fake_ctx.pins_level[2U]);
}

void test_PinRead_ValidArgs_ReadsLevel(void)
{
    do_init();
    xGPIO_Pin_Config_t pin_cfg = {
        .mode = xGPIO_PIN_MODE_INPUT, .speed = xGPIO_PIN_SPEED_LOW, .pull = xGPIO_PIN_PULL_NONE, .alternate_function = 0U};
    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Configure_Pin(&s_gpio_ctx, 3U, &pin_cfg));

    xGPIO_Fake_Set_Input_Level(&s_fake_ctx, 3U, true);
    bool val = false;
    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Pin_Read(&s_gpio_ctx, 3U, &val));
    TEST_ASSERT_TRUE(val);

    xGPIO_Fake_Set_Input_Level(&s_fake_ctx, 3U, false);
    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Pin_Read(&s_gpio_ctx, 3U, &val));
    TEST_ASSERT_FALSE(val);
}

void test_PinToggle_ValidArgs_TogglesLevel(void)
{
    do_init();
    xGPIO_Pin_Config_t pin_cfg = {
        .mode = xGPIO_PIN_MODE_OUTPUT_PUSH_PULL, .speed = xGPIO_PIN_SPEED_LOW, .pull = xGPIO_PIN_PULL_NONE, .alternate_function = 0U};
    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Configure_Pin(&s_gpio_ctx, 4U, &pin_cfg));

    s_fake_ctx.pins_level[4U] = false;
    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Pin_Toggle(&s_gpio_ctx, 4U));
    TEST_ASSERT_TRUE(s_fake_ctx.pins_level[4U]);

    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Pin_Toggle(&s_gpio_ctx, 4U));
    TEST_ASSERT_FALSE(s_fake_ctx.pins_level[4U]);
}

void test_InterruptCallback_FiresCallback(void)
{
    do_init();
    void *dummy_user_ctx = (void *)0x12345678U;
    s_callback_fired = false;
    s_callback_pin = 0xFFFFFFFFU;
    s_callback_ctx = NULL;
    s_callback_user_ctx = NULL;

    TEST_ASSERT_EQUAL(xRETURN_OK, xGPIO_Set_Interrupt_Callback(&s_gpio_ctx, 8U, test_interrupt_handler, dummy_user_ctx));

    xGPIO_Fake_Trigger_Interrupt(&s_fake_ctx, 8U);
    TEST_ASSERT_TRUE(s_callback_fired);
    TEST_ASSERT_EQUAL(8U, s_callback_pin);
    TEST_ASSERT_EQUAL_PTR(&s_gpio_ctx, s_callback_ctx);
    TEST_ASSERT_EQUAL_PTR(dummy_user_ctx, s_callback_user_ctx);
}

// MAIN ////////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Init_NullContext_ReturnsNullPointer);
    RUN_TEST(test_Init_NullInstance_ReturnsNullPointer);
    RUN_TEST(test_Init_NullOps_ReturnsNullPointer);
    RUN_TEST(test_Init_ValidArgs_SetsInitialized);

    RUN_TEST(test_Deinit_NotInitialized_ReturnsError);
    RUN_TEST(test_Deinit_ValidArgs_ClearsInitialized);

    RUN_TEST(test_ConfigurePin_ValidArgs_ConfiguresPin);
    RUN_TEST(test_PinWrite_ValidArgs_WritesLevel);
    RUN_TEST(test_PinRead_ValidArgs_ReadsLevel);
    RUN_TEST(test_PinToggle_ValidArgs_TogglesLevel);
    RUN_TEST(test_InterruptCallback_FiresCallback);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
