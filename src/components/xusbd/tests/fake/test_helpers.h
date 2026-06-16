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

// @file test_helpers.h
// @brief Shared test infrastructure for xusb host tests.
// All functions are static inline to suppress unused-function warnings when
// a test file does not exercise every helper.

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdbool.h>
#include <string.h>

#include "unity.h"
#include "fff.h"

#include "xusbd_class.h"
#include "xusbd_core.h"
#include "xusbd_return.h"
#include "xusbd_std.h"

// fff macro expansions generate trailing ';' constructs that -Wpedantic rejects.
// Suppress for this section only; all other test code still compiles pedantically.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

// FFF GLOBALS - one definition per executable (safe: each test is one .c file)
DEFINE_FFF_GLOBALS;

// FAKE DCD FUNCTIONS ///////////////////////////////////////////////////////////

FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_init, void *, USB_Speed_t, void *);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_set_callback, void *, xUSBD_DCD_Event_Callback_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_connect, void *);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_enable_interrupts, void *);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_disable_interrupts, void *);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_disconnect, void *);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_deinit, void *);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_set_address, void *, uint8_t);
FAKE_VALUE_FUNC(USB_Speed_t, fake_dcd_get_speed, void *);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_ep_init, void *, uint8_t, uint8_t, uint16_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_ep_deinit, void *, uint8_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_ep_receive, void *, uint8_t, uint8_t *, uint32_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_ep_send, void *, uint8_t, uint8_t *, uint32_t, bool);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_ep_transfer_queue, void *, const xUSBD_DCD_Transfer_t *);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_ep_stall, void *, uint8_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_ep_clear_stall, void *, uint8_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_dcd_set_remote_wakeup, void *, bool);

#pragma GCC diagnostic pop

// DCD OPS TABLE ///////////////////////////////////////////////////////////////

static xUSBD_DCD_Ops_t fake_dcd_ops = {
    .init = fake_dcd_init,
    .set_event_callback = fake_dcd_set_callback,
    .connect = fake_dcd_connect,
    .disconnect = fake_dcd_disconnect,
    .enable_interrupts = fake_dcd_enable_interrupts,
    .disable_interrupts = fake_dcd_disable_interrupts,
    .deinit = fake_dcd_deinit,
    .set_address = fake_dcd_set_address,
    .set_remote_wakeup = fake_dcd_set_remote_wakeup,
    .get_speed = fake_dcd_get_speed,
    .ep_init = fake_dcd_ep_init,
    .ep_deinit = fake_dcd_ep_deinit,
    .ep_receive = fake_dcd_ep_receive,
    .ep_send = fake_dcd_ep_send,
    .ep_transfer_queue = fake_dcd_ep_transfer_queue,
    .ep_stall = fake_dcd_ep_stall,
    .ep_clear_stall = fake_dcd_ep_clear_stall,
};

// Shorthand for firing DCD events through the registered callback.
// Valid after xUSBD_Start() has been called.
#define dcd_fire_event fake_dcd_set_callback_fake.arg1_val

// Reset all DCD fakes to zero state and configure sensible defaults.
// Call from setUp() before each test.
#define RESET_ALL_DCD_FAKES()                                                                                                              \
    do                                                                                                                                     \
    {                                                                                                                                      \
        RESET_FAKE(fake_dcd_init);                                                                                                         \
        RESET_FAKE(fake_dcd_set_callback);                                                                                                 \
        RESET_FAKE(fake_dcd_connect);                                                                                                      \
        RESET_FAKE(fake_dcd_enable_interrupts);                                                                                            \
        RESET_FAKE(fake_dcd_disable_interrupts);                                                                                           \
        RESET_FAKE(fake_dcd_disconnect);                                                                                                   \
        RESET_FAKE(fake_dcd_deinit);                                                                                                       \
        RESET_FAKE(fake_dcd_set_address);                                                                                                  \
        RESET_FAKE(fake_dcd_get_speed);                                                                                                    \
        RESET_FAKE(fake_dcd_ep_init);                                                                                                      \
        RESET_FAKE(fake_dcd_ep_deinit);                                                                                                    \
        RESET_FAKE(fake_dcd_ep_receive);                                                                                                   \
        RESET_FAKE(fake_dcd_ep_send);                                                                                                      \
        RESET_FAKE(fake_dcd_ep_transfer_queue);                                                                                            \
        RESET_FAKE(fake_dcd_ep_stall);                                                                                                     \
        RESET_FAKE(fake_dcd_ep_clear_stall);                                                                                               \
        RESET_FAKE(fake_dcd_set_remote_wakeup);                                                                                            \
        FFF_RESET_HISTORY();                                                                                                               \
        fake_dcd_get_speed_fake.return_val = USB_SPEED_HIGH;                                                                               \
    } while (0)

// NORMAL DRIVER (single interface, no callbacks) //////////////////////////////

static inline xRETURN_t one_interface_init(xUSBD_Class_Context_t *class_ctx)
{
    uint8_t interface = 0U;
    return xUSBD_Class_Allocate_Interface(class_ctx, &interface);
}

static inline uint32_t normal_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed)
{
    (void)speed;
    return (uint32_t)(build_interface_descriptor(buffer, class_ctx->first_interface, 0U, 0U, USB_CLASS_VENDOR, 0U, 0U, 0U) - buffer);
}

static xUSBD_Class_Driver_t normal_driver = {
    .init_instance = one_interface_init,
    .build_descriptor = normal_build_descriptor,
};

// DEVICE SETUP HELPERS ////////////////////////////////////////////////////////

// Sentinel passed as dcd_ctx - non-NULL so xUSBD_Start does not reject it.
// fff fakes record it in arg0_val but do not dereference it.
static int g_dcd_ctx_sentinel = 0;

static inline void test_device_init(xUSBD_Device_Context_t *device_ctx)
{
    xUSBD_Init_Config_t config = {
        .speed = USB_SPEED_HIGH,
        .vendor_string = (const uint8_t *)"XE",
        .product_string = (const uint8_t *)"xUSB",
        .serial_number_string = (const uint8_t *)"0001",
        .vendor_id = 0x1209U,
        .product_id = 0x0001U,
    };
    (void)xUSBD_Init(device_ctx, &config);
}

static inline xRETURN_t test_class_register(xUSBD_Device_Context_t *device_ctx,
                                            xUSBD_Class_Context_t *class_ctx,
                                            xUSBD_Class_Driver_t *class_driver,
                                            void *user_data)
{
    (void)user_data;
    return xUSBD_Class_Register(device_ctx, class_ctx, class_driver);
}

static inline xRETURN_t test_device_start(xUSBD_Device_Context_t *device_ctx)
{
    xUSBD_Start_Config_t config = {
        .port = 0U,
        .dcd_ops = &fake_dcd_ops,
        .dcd_ctx = &g_dcd_ctx_sentinel,
    };
    return xUSBD_Start(device_ctx, &config);
}

static inline void prepare_startable_device(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Context_t *class_ctx)
{
    memset(class_ctx, 0, sizeof(*class_ctx));
    test_device_init(device_ctx);
    (void)test_class_register(device_ctx, class_ctx, &normal_driver, NULL);
}

static inline void prepare_started_device(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Context_t *class_ctx)
{
    prepare_startable_device(device_ctx, class_ctx);
    (void)test_device_start(device_ctx);
}

#endif // TEST_HELPERS_H
// EOF /////////////////////////////////////////////////////////////////////////
