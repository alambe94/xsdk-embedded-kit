// Copyright 2022 alambe94
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

// @file test_xspi_core.c
// @brief Host tests for the xSPI portable driver core (simplified).
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "unity.h"
#include "xspi.h"
#include "xspi_fake.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static void fill_default_config(xSPI_Config_t *config);
static void fill_device(xSPI_Context_t *context, xSPI_Device_t *device);
static void fill_transaction(const uint8_t *tx_buffer, uint8_t *rx_buffer, uint32_t length, xSPI_Transaction_t *transaction);
static void setup_started_bus(xSPI_Fake_Context_t *fake_ctx, xSPI_Context_t *context, xSPI_Device_t *device);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static void fill_default_config(xSPI_Config_t *config)
{
    config->default_clock_hz = 1000000U;
    config->default_mode_flags = xSPI_MODE_0;
    config->bits_per_word = 8U;
    config->bit_order = xSPI_BIT_ORDER_MSB_FIRST;
}

static void fill_device(xSPI_Context_t *context, xSPI_Device_t *device)
{
    device->bus_ctx = context;
    device->chip_select = 2U;
    device->mode_flags = xSPI_MODE_0;
    device->max_clock_hz = 12000000U;
}

static void fill_transaction(const uint8_t *tx_buffer, uint8_t *rx_buffer, uint32_t length, xSPI_Transaction_t *transaction)
{
    transaction->tx_buffer = tx_buffer;
    transaction->rx_buffer = rx_buffer;
    transaction->length = length;
    transaction->clock_hz = 1000000U;
    transaction->timeout_ms = 10U;
    transaction->bits_per_word = 8U;
}

static void setup_started_bus(xSPI_Fake_Context_t *fake_ctx, xSPI_Context_t *context, xSPI_Device_t *device)
{
    xSPI_Instance_t instance;
    xSPI_Config_t config;
    xRETURN_t status;

    xSPI_Fake_Context_Init(fake_ctx);
    fill_default_config(&config);

    instance.ops = &xSPI_Fake_Driver_Ops;
    instance.driver_ctx = fake_ctx;

    status = xSPI_Init(context, &instance, &config);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, status);

    status = xSPI_Start(context);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, status);

    fill_device(context, device);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void setUp(void)
{
}

void tearDown(void)
{
}

void test_lifecycle_updates_core_and_port_state(void)
{
    xSPI_Fake_Context_t fake_ctx;
    xSPI_Context_t context;
    xSPI_Instance_t instance;
    xSPI_Config_t config;
    xRETURN_t status;

    xSPI_Fake_Context_Init(&fake_ctx);
    fill_default_config(&config);
    instance.ops = &xSPI_Fake_Driver_Ops;
    instance.driver_ctx = &fake_ctx;

    status = xSPI_Init(&context, &instance, &config);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, status);
    TEST_ASSERT_TRUE(context.is_initialized);
    TEST_ASSERT_TRUE(fake_ctx.is_initialized);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.init_count);

    status = xSPI_Start(&context);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, status);
    TEST_ASSERT_TRUE(context.is_started);
    TEST_ASSERT_TRUE(fake_ctx.is_started);

    status = xSPI_Stop(&context);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, status);
    TEST_ASSERT_FALSE(context.is_started);
    TEST_ASSERT_FALSE(fake_ctx.is_started);

    status = xSPI_Deinit(&context);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, status);
    TEST_ASSERT_FALSE(context.is_initialized);
    TEST_ASSERT_FALSE(fake_ctx.is_initialized);
}

void test_transfer_requires_started_bus(void)
{
    xSPI_Fake_Context_t fake_ctx;
    xSPI_Context_t context;
    xSPI_Instance_t instance;
    xSPI_Device_t device;
    xSPI_Config_t config;
    xSPI_Transaction_t transaction;
    const uint8_t tx_data[1] = {0};
    uint8_t rx_data[1];
    xRETURN_t status;

    xSPI_Fake_Context_Init(&fake_ctx);
    fill_default_config(&config);
    fill_transaction(tx_data, rx_data, 1U, &transaction);
    instance.ops = &xSPI_Fake_Driver_Ops;
    instance.driver_ctx = &fake_ctx;

    status = xSPI_Init(&context, &instance, &config);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, status);

    fill_device(&context, &device);

    status = xSPI_Transfer(&device, &transaction);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSPI_NOT_STARTED, status);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_ctx.transfer_count);
}

void test_blocking_transfer_records_device_and_copies_data(void)
{
    xSPI_Fake_Context_t fake_ctx;
    xSPI_Context_t context;
    xSPI_Device_t device;
    xSPI_Transaction_t transaction;
    const uint8_t tx_data[4] = {0x11U, 0x22U, 0x33U, 0x44U};
    uint8_t rx_data[4] = {0U, 0U, 0U, 0U};
    xRETURN_t status;

    setup_started_bus(&fake_ctx, &context, &device);
    fill_transaction(tx_data, rx_data, 4U, &transaction);

    status = xSPI_Transfer(&device, &transaction);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, status);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx_data, rx_data, 4U);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.transfer_count);
    TEST_ASSERT_EQUAL_UINT32(device.chip_select, fake_ctx.last_chip_select);
    TEST_ASSERT_EQUAL_UINT32(4U, fake_ctx.last_length);
}

static uint32_t g_callback_events = 0U;
static xSPI_Event_t g_last_event;
static xSPI_Event_Info_t g_last_event_info;
static void *g_last_user_ctx = NULL;

static void spi_test_event_cb(xSPI_Context_t *spi_ctx, xSPI_Event_t event, const xSPI_Event_Info_t *event_info, void *user_ctx)
{
    (void)spi_ctx;
    g_callback_events++;
    g_last_event = event;
    if (event_info != NULL)
    {
        g_last_event_info = *event_info;
    }
    g_last_user_ctx = user_ctx;
}

void test_callback_registration_and_propagation(void)
{
    xSPI_Fake_Context_t fake_ctx;
    xSPI_Context_t context;
    xSPI_Device_t device;
    xSPI_Callbacks_t callbacks;
    xSPI_Event_Info_t info;
    xRETURN_t status;
    void *dummy_user_ctx = (void *)0xDEADBEEFU;

    setup_started_bus(&fake_ctx, &context, &device);

    // Initial state
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.set_event_callback_count);
    TEST_ASSERT_NOT_NULL(fake_ctx.registered_callback);
    TEST_ASSERT_EQUAL_PTR(&context, fake_ctx.registered_callback_ctx);

    // Register callbacks
    callbacks.on_event = spi_test_event_cb;
    g_callback_events = 0U;
    g_last_user_ctx = NULL;

    status = xSPI_Set_Callback(&context, &callbacks, dummy_user_ctx);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, status);
    TEST_ASSERT_EQUAL_PTR(spi_test_event_cb, context.callbacks.on_event);
    TEST_ASSERT_EQUAL_PTR(dummy_user_ctx, context.user_ctx);

    // Call fake callback propagation
    info.error_code = xRETURN_OK;
    info.bytes_transferred = 100U;
    fake_ctx.registered_callback(fake_ctx.registered_callback_ctx, xSPI_EVENT_TRANSFER_COMPLETE, &info);

    TEST_ASSERT_EQUAL_UINT32(1U, g_callback_events);
    TEST_ASSERT_EQUAL(xSPI_EVENT_TRANSFER_COMPLETE, (int)g_last_event);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, g_last_event_info.error_code);
    TEST_ASSERT_EQUAL_UINT32(100U, g_last_event_info.bytes_transferred);
    TEST_ASSERT_EQUAL_PTR(dummy_user_ctx, g_last_user_ctx);

    // Deregister callbacks
    status = xSPI_Set_Callback(&context, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, status);
    TEST_ASSERT_NULL(context.callbacks.on_event);
    TEST_ASSERT_NULL(context.user_ctx);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lifecycle_updates_core_and_port_state);
    RUN_TEST(test_transfer_requires_started_bus);
    RUN_TEST(test_blocking_transfer_records_device_and_copies_data);
    RUN_TEST(test_callback_registration_and_propagation);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
