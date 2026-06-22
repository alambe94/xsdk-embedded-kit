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

// @file test_xuart_core.c
// @brief Portable xUART core tests using the fake port.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
#include <string.h>
#include "unity.h"
#include "xuart.h"
#include "xuart_fake.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

static xUART_Fake_Context_t s_fake_ctx;
static xUART_Context_t s_uart_ctx;
static xUART_Start_Config_t s_start_config;
static xUART_Config_t s_config;

static bool s_event_fired;
static int s_last_event;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

void setUp(void);
void tearDown(void);
static void do_init_and_start(void);
static void test_on_event(xUART_Context_t *uart_ctx, xUART_Event_t event, const xUART_Event_Info_t *event_info, void *user_ctx);

// SETUP / TEARDOWN ////////////////////////////////////////////////////////////////

void setUp(void)
{
    xUART_Fake_Context_Init(&s_fake_ctx);
    (void)memset(&s_uart_ctx, 0, sizeof(s_uart_ctx));

    s_start_config.port = 1U;
    s_start_config.drv_ops = &xUART_Fake_Driver_Ops;
    s_start_config.drv_ctx = &s_fake_ctx;

    s_config.baud_rate = 115200U;
    s_config.data_bits = xUART_DATA_BITS_8;
    s_config.stop_bits = xUART_STOP_BITS_1;
    s_config.parity = xUART_PARITY_NONE;
    s_config.flow_control = xUART_FLOW_CONTROL_NONE;

    s_event_fired = false;
    s_last_event = 0xFF;
}

void tearDown(void)
{
}

// HELPERS /////////////////////////////////////////////////////////////////////////

static void do_init_and_start(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Start(&s_uart_ctx, &s_start_config));
}

static void test_on_event(xUART_Context_t *uart_ctx, xUART_Event_t event, const xUART_Event_Info_t *event_info, void *user_ctx)
{
    (void)uart_ctx;
    (void)event_info;
    (void)user_ctx;
    s_event_fired = true;
    s_last_event = (int)event;
}

// TESTS — Init ////////////////////////////////////////////////////////////////////

void test_Init_NullContext_ReturnsNullPointer(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NULL_POINTER, xUART_Init(NULL, &s_config));
}

void test_Init_NullConfig_ReturnsNullPointer(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NULL_POINTER, xUART_Init(&s_uart_ctx, NULL));
}

void test_Init_ValidArgs_SetsInitialized(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_TRUE(s_uart_ctx.is_initialized);
    TEST_ASSERT_FALSE(s_uart_ctx.is_started);
    // Init must not touch hardware — port ops not called yet
    TEST_ASSERT_EQUAL(0U, s_fake_ctx.init_count);
}

void test_Init_StoresConfig(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(115200U, s_uart_ctx.config.baud_rate);
    TEST_ASSERT_EQUAL(xUART_DATA_BITS_8, s_uart_ctx.config.data_bits);
}

// TESTS — Start ///////////////////////////////////////////////////////////////////

void test_Start_NullContext_ReturnsNullPointer(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NULL_POINTER, xUART_Start(NULL, &s_start_config));
}

void test_Start_NullStartConfig_ReturnsNullPointer(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NULL_POINTER, xUART_Start(&s_uart_ctx, NULL));
}

void test_Start_NotInitialized_ReturnsError(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NOT_INITIALIZED, xUART_Start(&s_uart_ctx, &s_start_config));
}

void test_Start_NullDrvOps_ReturnsError(void)
{
    xUART_Start_Config_t bad = s_start_config;
    bad.drv_ops = NULL;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_INVALID_ARG, xUART_Start(&s_uart_ctx, &bad));
}

void test_Start_NullDrvCtx_ReturnsError(void)
{
    xUART_Start_Config_t bad = s_start_config;
    bad.drv_ctx = NULL;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_INVALID_ARG, xUART_Start(&s_uart_ctx, &bad));
}

void test_Start_ValidArgs_ReturnsOk(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Start(&s_uart_ctx, &s_start_config));
    TEST_ASSERT_TRUE(s_uart_ctx.is_started);
    TEST_ASSERT_EQUAL(1U, s_fake_ctx.init_count);
    TEST_ASSERT_EQUAL(1U, s_fake_ctx.start_count);
    TEST_ASSERT_EQUAL(1U, s_uart_ctx.port);
}

void test_Start_AlreadyStarted_ReturnsError(void)
{
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_INVALID_STATE, xUART_Start(&s_uart_ctx, &s_start_config));
}

void test_Start_PortInitFails_ReturnsError(void)
{
    s_fake_ctx.next_init_status = xRETURN_xERR_xUART_HARDWARE;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_HARDWARE, xUART_Start(&s_uart_ctx, &s_start_config));
    TEST_ASSERT_FALSE(s_uart_ctx.is_started);
}

void test_Start_PortStartFails_RollsBackAndReturnsError(void)
{
    s_fake_ctx.next_start_status = xRETURN_xERR_xUART_HARDWARE;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_HARDWARE, xUART_Start(&s_uart_ctx, &s_start_config));
    TEST_ASSERT_FALSE(s_uart_ctx.is_started);
    TEST_ASSERT_EQUAL(1U, s_fake_ctx.deinit_count); // rollback
}

// TESTS — Stop ////////////////////////////////////////////////////////////////////

void test_Stop_NotStarted_ReturnsError(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NOT_STARTED, xUART_Stop(&s_uart_ctx));
}

void test_Stop_ValidArgs_ReturnsOk(void)
{
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Stop(&s_uart_ctx));
    TEST_ASSERT_FALSE(s_uart_ctx.is_started);
    TEST_ASSERT_EQUAL(1U, s_fake_ctx.stop_count);
}

void test_Stop_WhileTxBusy_ReturnsError(void)
{
    do_init_and_start();
    s_uart_ctx.is_tx_busy = true;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_TX_BUSY, xUART_Stop(&s_uart_ctx));
}

void test_Stop_WhileRxBusy_ReturnsError(void)
{
    do_init_and_start();
    s_uart_ctx.is_rx_busy = true;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_RX_BUSY, xUART_Stop(&s_uart_ctx));
}

// TESTS — Deinit //////////////////////////////////////////////////////////////////

void test_Deinit_NullContext_ReturnsNullPointer(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NULL_POINTER, xUART_Deinit(NULL));
}

void test_Deinit_NotInitialized_ReturnsError(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NOT_INITIALIZED, xUART_Deinit(&s_uart_ctx));
}

void test_Deinit_WhileTxBusy_ReturnsError(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    s_uart_ctx.is_tx_busy = true;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_TX_BUSY, xUART_Deinit(&s_uart_ctx));
}

void test_Deinit_WhileRxBusy_ReturnsError(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    s_uart_ctx.is_rx_busy = true;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_RX_BUSY, xUART_Deinit(&s_uart_ctx));
}

void test_Deinit_AfterStart_CallsPortDeinit(void)
{
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Deinit(&s_uart_ctx));
    TEST_ASSERT_FALSE(s_uart_ctx.is_initialized);
    TEST_ASSERT_FALSE(s_uart_ctx.is_started);
    TEST_ASSERT_EQUAL(1U, s_fake_ctx.deinit_count);
}

void test_Deinit_WithoutStart_ClearsContext(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Deinit(&s_uart_ctx));
    TEST_ASSERT_FALSE(s_uart_ctx.is_initialized);
    TEST_ASSERT_EQUAL(0U, s_fake_ctx.deinit_count); // port deinit skipped
}

// TESTS — Transmit ////////////////////////////////////////////////////////////////

void test_Transmit_NullContext_ReturnsNullPointer(void)
{
    uint8_t buf[1] = {0U};
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NULL_POINTER, xUART_Transmit(NULL, buf, 1U, 100U));
}

void test_Transmit_NullBuffer_ReturnsNullPointer(void)
{
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NULL_POINTER, xUART_Transmit(&s_uart_ctx, NULL, 1U, 100U));
}

void test_Transmit_ZeroLength_ReturnsError(void)
{
    uint8_t buf[1] = {0U};
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_INVALID_ARG, xUART_Transmit(&s_uart_ctx, buf, 0U, 100U));
}

void test_Transmit_NotStarted_ReturnsError(void)
{
    uint8_t buf[4] = {0U, 1U, 2U, 3U};
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NOT_STARTED, xUART_Transmit(&s_uart_ctx, buf, sizeof(buf), 100U));
}

void test_Transmit_TxBusy_ReturnsError(void)
{
    uint8_t buf[4] = {0U};
    do_init_and_start();
    s_uart_ctx.is_tx_busy = true;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_TX_BUSY, xUART_Transmit(&s_uart_ctx, buf, sizeof(buf), 100U));
}

void test_Transmit_ValidArgs_ReturnsOk(void)
{
    uint8_t buf[4] = {0xAAU, 0xBBU, 0xCCU, 0xDDU};
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Transmit(&s_uart_ctx, buf, sizeof(buf), 100U));
    TEST_ASSERT_EQUAL(1U, s_fake_ctx.transmit_count);
    TEST_ASSERT_FALSE(s_uart_ctx.is_tx_busy);
}

void test_Transmit_PortFails_ClearsBusyAndReturnsError(void)
{
    uint8_t buf[4] = {0U};
    s_fake_ctx.next_transmit_status = xRETURN_xERR_xUART_TIMEOUT;
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_TIMEOUT, xUART_Transmit(&s_uart_ctx, buf, sizeof(buf), 100U));
    TEST_ASSERT_FALSE(s_uart_ctx.is_tx_busy);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_TIMEOUT, s_uart_ctx.last_tx_error);
}

// TESTS — Receive /////////////////////////////////////////////////////////////////

void test_Receive_NotStarted_ReturnsError(void)
{
    uint8_t buf[4] = {0U};
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NOT_STARTED, xUART_Receive(&s_uart_ctx, buf, sizeof(buf), 100U));
}

void test_Receive_RxBusy_ReturnsError(void)
{
    uint8_t buf[4] = {0U};
    do_init_and_start();
    s_uart_ctx.is_rx_busy = true;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_RX_BUSY, xUART_Receive(&s_uart_ctx, buf, sizeof(buf), 100U));
}

void test_Receive_ValidArgs_FillsBuf(void)
{
    uint8_t buf[4] = {0U};
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Receive(&s_uart_ctx, buf, sizeof(buf), 100U));
    TEST_ASSERT_EQUAL(1U, s_fake_ctx.receive_count);
    TEST_ASSERT_FALSE(s_uart_ctx.is_rx_busy);
    TEST_ASSERT_EACH_EQUAL_UINT8(xUART_FAKE_RX_FILL_BYTE, buf, sizeof(buf));
}

void test_Receive_PortFails_ClearsBusyAndReturnsError(void)
{
    uint8_t buf[4] = {0U};
    s_fake_ctx.next_receive_status = xRETURN_xERR_xUART_TIMEOUT;
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_TIMEOUT, xUART_Receive(&s_uart_ctx, buf, sizeof(buf), 100U));
    TEST_ASSERT_FALSE(s_uart_ctx.is_rx_busy);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_TIMEOUT, s_uart_ctx.last_rx_error);
}

// TESTS — Transmit_Async //////////////////////////////////////////////////////////

void test_Transmit_Async_ValidArgs_SetsTxBusy(void)
{
    uint8_t buf[4] = {0xAAU, 0xBBU, 0xCCU, 0xDDU};
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Transmit_Async(&s_uart_ctx, buf, sizeof(buf)));
    TEST_ASSERT_TRUE(s_uart_ctx.is_tx_busy);
    TEST_ASSERT_EQUAL(1U, s_fake_ctx.transmit_async_count);
}

void test_Transmit_Async_TxBusy_ReturnsError(void)
{
    uint8_t buf[4] = {0U};
    do_init_and_start();
    s_uart_ctx.is_tx_busy = true;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_TX_BUSY, xUART_Transmit_Async(&s_uart_ctx, buf, sizeof(buf)));
}

void test_Transmit_Async_PortFails_ClearsBusy(void)
{
    uint8_t buf[4] = {0U};
    s_fake_ctx.next_transmit_async_status = xRETURN_xERR_xUART_HARDWARE;
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_HARDWARE, xUART_Transmit_Async(&s_uart_ctx, buf, sizeof(buf)));
    TEST_ASSERT_FALSE(s_uart_ctx.is_tx_busy);
}

// TESTS — Receive_Async ///////////////////////////////////////////////////////////

void test_Receive_Async_ValidArgs_SetsRxBusy(void)
{
    uint8_t buf[4] = {0U};
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Receive_Async(&s_uart_ctx, buf, sizeof(buf)));
    TEST_ASSERT_TRUE(s_uart_ctx.is_rx_busy);
    TEST_ASSERT_EQUAL(1U, s_fake_ctx.receive_async_count);
}

void test_Receive_Async_RxBusy_ReturnsError(void)
{
    uint8_t buf[4] = {0U};
    do_init_and_start();
    s_uart_ctx.is_rx_busy = true;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_RX_BUSY, xUART_Receive_Async(&s_uart_ctx, buf, sizeof(buf)));
}

void test_Receive_Async_PortFails_ClearsBusy(void)
{
    uint8_t buf[4] = {0U};
    s_fake_ctx.next_receive_async_status = xRETURN_xERR_xUART_HARDWARE;
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_HARDWARE, xUART_Receive_Async(&s_uart_ctx, buf, sizeof(buf)));
    TEST_ASSERT_FALSE(s_uart_ctx.is_rx_busy);
}

// TESTS — Callback / event sink ///////////////////////////////////////////////////

void test_Callback_TxComplete_ClearsBusyAndFiresEvent(void)
{
    uint8_t buf[4] = {0U};
    do_init_and_start();
    xUART_Callbacks_t cbs = {.on_event = test_on_event};
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Set_Callback(&s_uart_ctx, &cbs, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Transmit_Async(&s_uart_ctx, buf, sizeof(buf)));
    TEST_ASSERT_TRUE(s_uart_ctx.is_tx_busy);

    xUART_Fake_Fire_Tx_Complete(&s_fake_ctx, sizeof(buf));

    TEST_ASSERT_FALSE(s_uart_ctx.is_tx_busy);
    TEST_ASSERT_TRUE(s_event_fired);
    TEST_ASSERT_EQUAL(xUART_EVENT_TX_COMPLETE, s_last_event);
}

void test_Callback_RxComplete_ClearsBusyAndFiresEvent(void)
{
    uint8_t buf[4] = {0U};
    do_init_and_start();
    xUART_Callbacks_t cbs = {.on_event = test_on_event};
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Set_Callback(&s_uart_ctx, &cbs, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Receive_Async(&s_uart_ctx, buf, sizeof(buf)));
    TEST_ASSERT_TRUE(s_uart_ctx.is_rx_busy);

    xUART_Fake_Fire_Rx_Complete(&s_fake_ctx, sizeof(buf));

    TEST_ASSERT_FALSE(s_uart_ctx.is_rx_busy);
    TEST_ASSERT_TRUE(s_event_fired);
    TEST_ASSERT_EQUAL(xUART_EVENT_RX_COMPLETE, s_last_event);
}

// TESTS — Abort ///////////////////////////////////////////////////////////////////

void test_Abort_Tx_WhileNotBusy_ReturnsOk(void)
{
    do_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Abort_Tx(&s_uart_ctx));
    TEST_ASSERT_EQUAL(0U, s_fake_ctx.abort_tx_count); // no-op, port not called
}

void test_Abort_Tx_WhileBusy_FiresAbortedEvent(void)
{
    uint8_t buf[4] = {0U};
    do_init_and_start();
    xUART_Callbacks_t cbs = {.on_event = test_on_event};
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Set_Callback(&s_uart_ctx, &cbs, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Transmit_Async(&s_uart_ctx, buf, sizeof(buf)));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Abort_Tx(&s_uart_ctx));
    TEST_ASSERT_EQUAL(1U, s_fake_ctx.abort_tx_count);
    TEST_ASSERT_FALSE(s_uart_ctx.is_tx_busy);
    TEST_ASSERT_TRUE(s_event_fired);
    TEST_ASSERT_EQUAL(xUART_EVENT_TX_ABORTED, s_last_event);
}

void test_Abort_Rx_WhileBusy_FiresAbortedEvent(void)
{
    uint8_t buf[4] = {0U};
    do_init_and_start();
    xUART_Callbacks_t cbs = {.on_event = test_on_event};
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Set_Callback(&s_uart_ctx, &cbs, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Receive_Async(&s_uart_ctx, buf, sizeof(buf)));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Abort_Rx(&s_uart_ctx));
    TEST_ASSERT_EQUAL(1U, s_fake_ctx.abort_rx_count);
    TEST_ASSERT_FALSE(s_uart_ctx.is_rx_busy);
    TEST_ASSERT_TRUE(s_event_fired);
    TEST_ASSERT_EQUAL(xUART_EVENT_RX_ABORTED, s_last_event);
}

void test_SetCallback_NullContext_ReturnsNullPointer(void)
{
    xUART_Callbacks_t cbs = {.on_event = test_on_event};
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NULL_POINTER, xUART_Set_Callback(NULL, &cbs, NULL));
}

void test_SetCallback_NotInitialized_ReturnsError(void)
{
    xUART_Callbacks_t cbs = {.on_event = test_on_event};
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NOT_INITIALIZED, xUART_Set_Callback(&s_uart_ctx, &cbs, NULL));
}

void test_SetCallback_WhileBusy_ReturnsError(void)
{
    uint8_t buf[4] = {0U};
    do_init_and_start();
    xUART_Callbacks_t cbs = {.on_event = test_on_event};
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Transmit_Async(&s_uart_ctx, buf, sizeof(buf)));
    TEST_ASSERT_TRUE(s_uart_ctx.is_tx_busy);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_TX_BUSY, xUART_Set_Callback(&s_uart_ctx, &cbs, NULL));
}

void test_Abort_Tx_NotStarted_ReturnsError(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUART_Init(&s_uart_ctx, &s_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUART_NOT_STARTED, xUART_Abort_Tx(&s_uart_ctx));
}

// MAIN ////////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    // Init
    RUN_TEST(test_Init_NullContext_ReturnsNullPointer);
    RUN_TEST(test_Init_NullConfig_ReturnsNullPointer);
    RUN_TEST(test_Init_ValidArgs_SetsInitialized);
    RUN_TEST(test_Init_StoresConfig);

    // Start
    RUN_TEST(test_Start_NullContext_ReturnsNullPointer);
    RUN_TEST(test_Start_NullStartConfig_ReturnsNullPointer);
    RUN_TEST(test_Start_NotInitialized_ReturnsError);
    RUN_TEST(test_Start_NullDrvOps_ReturnsError);
    RUN_TEST(test_Start_NullDrvCtx_ReturnsError);
    RUN_TEST(test_Start_ValidArgs_ReturnsOk);
    RUN_TEST(test_Start_AlreadyStarted_ReturnsError);
    RUN_TEST(test_Start_PortInitFails_ReturnsError);
    RUN_TEST(test_Start_PortStartFails_RollsBackAndReturnsError);

    // Stop
    RUN_TEST(test_Stop_NotStarted_ReturnsError);
    RUN_TEST(test_Stop_ValidArgs_ReturnsOk);
    RUN_TEST(test_Stop_WhileTxBusy_ReturnsError);
    RUN_TEST(test_Stop_WhileRxBusy_ReturnsError);

    // Deinit
    RUN_TEST(test_Deinit_NullContext_ReturnsNullPointer);
    RUN_TEST(test_Deinit_NotInitialized_ReturnsError);
    RUN_TEST(test_Deinit_WhileTxBusy_ReturnsError);
    RUN_TEST(test_Deinit_WhileRxBusy_ReturnsError);
    RUN_TEST(test_Deinit_AfterStart_CallsPortDeinit);
    RUN_TEST(test_Deinit_WithoutStart_ClearsContext);

    // Transmit
    RUN_TEST(test_Transmit_NullContext_ReturnsNullPointer);
    RUN_TEST(test_Transmit_NullBuffer_ReturnsNullPointer);
    RUN_TEST(test_Transmit_ZeroLength_ReturnsError);
    RUN_TEST(test_Transmit_NotStarted_ReturnsError);
    RUN_TEST(test_Transmit_TxBusy_ReturnsError);
    RUN_TEST(test_Transmit_ValidArgs_ReturnsOk);
    RUN_TEST(test_Transmit_PortFails_ClearsBusyAndReturnsError);

    // Receive
    RUN_TEST(test_Receive_NotStarted_ReturnsError);
    RUN_TEST(test_Receive_RxBusy_ReturnsError);
    RUN_TEST(test_Receive_ValidArgs_FillsBuf);
    RUN_TEST(test_Receive_PortFails_ClearsBusyAndReturnsError);

    // Transmit_Async
    RUN_TEST(test_Transmit_Async_ValidArgs_SetsTxBusy);
    RUN_TEST(test_Transmit_Async_TxBusy_ReturnsError);
    RUN_TEST(test_Transmit_Async_PortFails_ClearsBusy);

    // Receive_Async
    RUN_TEST(test_Receive_Async_ValidArgs_SetsRxBusy);
    RUN_TEST(test_Receive_Async_RxBusy_ReturnsError);
    RUN_TEST(test_Receive_Async_PortFails_ClearsBusy);

    // Callback
    RUN_TEST(test_Callback_TxComplete_ClearsBusyAndFiresEvent);
    RUN_TEST(test_Callback_RxComplete_ClearsBusyAndFiresEvent);

    // Set_Callback
    RUN_TEST(test_SetCallback_NullContext_ReturnsNullPointer);
    RUN_TEST(test_SetCallback_NotInitialized_ReturnsError);
    RUN_TEST(test_SetCallback_WhileBusy_ReturnsError);

    // Abort
    RUN_TEST(test_Abort_Tx_WhileNotBusy_ReturnsOk);
    RUN_TEST(test_Abort_Tx_WhileBusy_FiresAbortedEvent);
    RUN_TEST(test_Abort_Rx_WhileBusy_FiresAbortedEvent);
    RUN_TEST(test_Abort_Tx_NotStarted_ReturnsError);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
