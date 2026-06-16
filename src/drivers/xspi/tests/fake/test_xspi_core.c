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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lifecycle_updates_core_and_port_state);
    RUN_TEST(test_transfer_requires_started_bus);
    RUN_TEST(test_blocking_transfer_records_device_and_copies_data);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
