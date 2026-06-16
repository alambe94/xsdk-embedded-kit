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

// @file test_init.c
// @brief Host tests for xUSBD_Init, xUSBD_Start, xUSBD_Stop, lifecycle
// state tracking, DCD failure propagation, and public state accessors.

#include "unity.h"
#include "test_helpers.h"
#include "xtrace.h"

#if xTRACE_ENABLE
static xTRACE_Context_t s_trace_ctx;
static uint8_t s_trace_buf[256U];

static xTRACE_Time_t trace_timestamp(void *ctx)
{
    (void)ctx;
    return 0U;
}

static void trace_context_init(void)
{
    xTRACE_Config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.buffer = s_trace_buf;
    cfg.capacity_bytes = (uint32_t)sizeof(s_trace_buf);
    cfg.timestamp_fn = trace_timestamp;
    cfg.timestamp_ctx = NULL;
    cfg.timestamp_hz = 1U;
    cfg.is_enabled = true;

    memset(s_trace_buf, 0, sizeof(s_trace_buf));
    memset(&s_trace_ctx, 0, sizeof(s_trace_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xTRACE_Init(&s_trace_ctx, &cfg, NULL, NULL));
}

static uint32_t leb128_read(const uint8_t *buf, uint32_t *pos)
{
    uint32_t value = 0U;
    uint32_t shift = 0U;

    while (true)
    {
        uint8_t byte = buf[(*pos)++];
        value |= ((uint32_t)(byte & 0x7FU)) << shift;
        shift += 7U;
        if ((byte & 0x80U) == 0U)
        {
            break;
        }
    }

    return value;
}

static void trace_positions_reset(void)
{
    s_trace_ctx.write_pos = 0U;
    s_trace_ctx.read_pos = 0U;
}

static bool trace_contains_event_arg(uint32_t target_id, uint32_t expected_arg)
{
    uint32_t pos = 0U;

    while (pos < s_trace_ctx.write_pos)
    {
        uint32_t record_len = leb128_read(s_trace_buf, &pos);
        (void)record_len;
        uint32_t event_id = leb128_read(s_trace_buf, &pos);
        uint32_t delta = leb128_read(s_trace_buf, &pos);
        uint32_t arg = leb128_read(s_trace_buf, &pos);
        (void)delta;

        if (event_id == target_id && arg == expected_arg)
        {
            return true;
        }
    }

    return false;
}
#endif

void setUp(void)
{
    RESET_ALL_DCD_FAKES();
}
void tearDown(void)
{
}

// TESTS: xUSBD_Init ///////////////////////////////////////////////////////////

void test_init_validates_public_config(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Init_Config_t config = {
        .speed = USB_SPEED_HIGH,
        .vendor_string = (const uint8_t *)"XE",
        .product_string = (const uint8_t *)"xUSB",
        .serial_number_string = (const uint8_t *)"0001",
        .vendor_id = 0x1209U,
        .product_id = 0x0001U,
    };

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Init(NULL, &config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Init(&device_ctx, NULL));

    config.vendor_string = NULL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Init(&device_ctx, &config));

    config.vendor_string = (const uint8_t *)"XE";
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Init(&device_ctx, &config));
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_INITIALIZED, device_ctx.lifecycle_state);
    TEST_ASSERT_EQUAL(USB_SPEED_HIGH, device_ctx.speed);

    USB_Device_Descriptor_t *descriptor = (USB_Device_Descriptor_t *)device_ctx.device_descriptor;
    TEST_ASSERT_EQUAL_UINT16(0x1209U, xLE16_TO_CPU(descriptor->idVendor));
    TEST_ASSERT_EQUAL_UINT16(0x0001U, xLE16_TO_CPU(descriptor->idProduct));
}

// TESTS: xUSBD_Start //////////////////////////////////////////////////////////

void test_lifecycle_tracks_init_and_registration(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_INITIALIZED, device_ctx.lifecycle_state);
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &normal_driver, NULL));
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_CLASSES_REGISTERED, device_ctx.lifecycle_state);
}

void test_trace_init_attaches_and_detaches_context(void)
{
#if xTRACE_ENABLE
    xUSBD_Device_Context_t device_ctx;

    test_device_init(&device_ctx);
    trace_context_init();

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Trace_Init(NULL, &s_trace_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Trace_Init(&device_ctx, &s_trace_ctx));
    TEST_ASSERT_EQUAL_PTR(&s_trace_ctx, device_ctx.trace_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Trace_Init(&device_ctx, NULL));
    TEST_ASSERT_NULL(device_ctx.trace_ctx);
#else
    TEST_IGNORE_MESSAGE("xTRACE_ENABLE is 0 - xUSBD trace context storage is compiled out");
#endif
}

void test_trace_records_bus_and_standard_control_events(void)
{
#if xTRACE_ENABLE
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_startable_device(&device_ctx, &class_ctx);
    trace_context_init();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Trace_Init(&device_ctx, &s_trace_ctx));
    trace_positions_reset();

    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));
    dcd_fire_event(&device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);
    TEST_ASSERT_TRUE(trace_contains_event_arg(xUSBD_TRACE_CODE_BUS_CONNECT, USB_SPEED_HIGH));

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_SET_ADDRESS;
    request.wValue = xCPU_TO_LE16(5U);
    request.wLength = xCPU_TO_LE16(0U);
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    uint32_t packed_set_address = ((uint32_t)request.bRequestType << 8U) | (uint32_t)request.bRequest;
    TEST_ASSERT_TRUE(trace_contains_event_arg(xUSBD_TRACE_CODE_CONTROL_REQUEST, packed_set_address));

    dcd_fire_event(&device_ctx, USB_DCD_DATA_SENT, 0x80U, NULL, 0U);
    TEST_ASSERT_TRUE(device_ctx.is_addressed);
    TEST_ASSERT_TRUE(trace_contains_event_arg(xUSBD_TRACE_CODE_SET_ADDRESS, 5U));

    request.bRequest = USB_REQ_SET_CONFIGURATION;
    request.wValue = xCPU_TO_LE16(1U);
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));
    dcd_fire_event(&device_ctx, USB_DCD_DATA_SENT, 0x80U, NULL, 0U);
    TEST_ASSERT_TRUE(device_ctx.is_configured);
    TEST_ASSERT_TRUE(trace_contains_event_arg(xUSBD_TRACE_CODE_SET_CONFIGURATION, 1U));

    dcd_fire_event(&device_ctx, USB_DCD_RESET_RECEIVED, 0U, NULL, 0U);
    TEST_ASSERT_FALSE(device_ctx.is_addressed);
    TEST_ASSERT_TRUE(trace_contains_event_arg(xUSBD_TRACE_CODE_BUS_RESET, USB_SPEED_HIGH));
#else
    TEST_IGNORE_MESSAGE("xTRACE_ENABLE is 0 - xUSBD trace emission is compiled out");
#endif
}

void test_trace_detach_stops_event_emission_without_changing_behavior(void)
{
#if xTRACE_ENABLE
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_startable_device(&device_ctx, &class_ctx);
    trace_context_init();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Trace_Init(&device_ctx, &s_trace_ctx));
    trace_positions_reset();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Trace_Init(&device_ctx, NULL));

    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));
    dcd_fire_event(&device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_TRUE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(USB_SPEED_HIGH, device_ctx.speed);
    TEST_ASSERT_FALSE(trace_contains_event_arg(xUSBD_TRACE_CODE_BUS_CONNECT, USB_SPEED_HIGH));
#else
    TEST_IGNORE_MESSAGE("xTRACE_ENABLE is 0 - xUSBD trace emission is compiled out");
#endif
}

void test_start_validates_public_config(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    xUSBD_Start_Config_t config;

    prepare_startable_device(&device_ctx, &class_ctx);

    config.port = 0U;
    config.dcd_ops = &fake_dcd_ops;
    config.dcd_ctx = &g_dcd_ctx_sentinel;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Start(NULL, &config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Start(&device_ctx, NULL));

    config.dcd_ops = NULL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Start(&device_ctx, &config));

    config.dcd_ops = &fake_dcd_ops;
    config.dcd_ctx = NULL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Start(&device_ctx, &config));

    config.dcd_ctx = &g_dcd_ctx_sentinel;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Start(&device_ctx, &config));
    TEST_ASSERT_EQUAL_UINT8(0U, device_ctx.port);
    TEST_ASSERT_TRUE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STARTED, device_ctx.lifecycle_state);
}

void test_start_rejected_before_class_registration(void)
{
    xUSBD_Device_Context_t device_ctx;

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_INVALID_CONFIGURATION, test_device_start(&device_ctx));
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_init_fake.call_count);
    TEST_ASSERT_FALSE(device_ctx.is_started);
}

void test_start_propagates_dcd_init_failure(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_startable_device(&device_ctx, &class_ctx);
    fake_dcd_init_fake.return_val = xRETURN_xERR_xUSBD_DCD_INVALID_PORT;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_INVALID_PORT, test_device_start(&device_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_init_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_set_callback_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_connect_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_enable_interrupts_fake.call_count);
    TEST_ASSERT_FALSE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_CLASSES_REGISTERED, device_ctx.lifecycle_state);
}

void test_start_propagates_dcd_callback_failure(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_startable_device(&device_ctx, &class_ctx);
    fake_dcd_set_callback_fake.return_val = xRETURN_xERR_xUSBD_DCD_NULL_POINTER;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_NULL_POINTER, test_device_start(&device_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_init_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_set_callback_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_connect_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_enable_interrupts_fake.call_count);
    TEST_ASSERT_FALSE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_CLASSES_REGISTERED, device_ctx.lifecycle_state);
}

void test_start_propagates_dcd_connect_failure(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_startable_device(&device_ctx, &class_ctx);
    fake_dcd_connect_fake.return_val = xRETURN_xERR_xUSBD_DCD_INVALID_PORT;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_INVALID_PORT, test_device_start(&device_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_init_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_set_callback_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_connect_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_enable_interrupts_fake.call_count);
    TEST_ASSERT_FALSE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_CLASSES_REGISTERED, device_ctx.lifecycle_state);
}

void test_start_propagates_dcd_interrupt_failure(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_startable_device(&device_ctx, &class_ctx);
    fake_dcd_enable_interrupts_fake.return_val = xRETURN_xERR_xUSBD_DCD_INVALID_PORT;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_INVALID_PORT, test_device_start(&device_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_init_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_set_callback_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_connect_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_enable_interrupts_fake.call_count);
    TEST_ASSERT_FALSE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_CLASSES_REGISTERED, device_ctx.lifecycle_state);
}

void test_start_succeeds_after_dcd_success(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_startable_device(&device_ctx, &class_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_init_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_set_callback_fake.call_count);
    TEST_ASSERT_EQUAL_PTR(xUSBD_DCD_Event_Callback, fake_dcd_set_callback_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_connect_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_enable_interrupts_fake.call_count);
    TEST_ASSERT_TRUE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STARTED, device_ctx.lifecycle_state);
}

void test_lifecycle_tracks_configuration(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_startable_device(&device_ctx, &class_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));
    TEST_ASSERT_NOT_NULL(fake_dcd_set_callback_fake.arg1_val);
    dcd_fire_event(&device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STARTED, device_ctx.lifecycle_state);

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_SET_ADDRESS;
    request.wValue = xCPU_TO_LE16(5U);
    request.wLength = xCPU_TO_LE16(0U);
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));
    dcd_fire_event(&device_ctx, USB_DCD_DATA_SENT, 0x80U, NULL, 0U);
    TEST_ASSERT_TRUE(device_ctx.is_addressed);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STARTED, device_ctx.lifecycle_state);

    request.bRequest = USB_REQ_SET_CONFIGURATION;
    request.wValue = xCPU_TO_LE16(1U);
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));
    dcd_fire_event(&device_ctx, USB_DCD_DATA_SENT, 0x80U, NULL, 0U);
    TEST_ASSERT_TRUE(device_ctx.is_configured);
    TEST_ASSERT_EQUAL_UINT8(1U, device_ctx.configuration_value);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_CONFIGURED, device_ctx.lifecycle_state);

    request.wValue = xCPU_TO_LE16(0U);
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));
    dcd_fire_event(&device_ctx, USB_DCD_DATA_SENT, 0x80U, NULL, 0U);
    TEST_ASSERT_FALSE(device_ctx.is_configured);
    TEST_ASSERT_EQUAL_UINT8(0U, device_ctx.configuration_value);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STARTED, device_ctx.lifecycle_state);
}

// TESTS: state accessors //////////////////////////////////////////////////////

void test_public_state_accessors_validate_arguments(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Lifecycle_State_t state;
    USB_DCD_Link_State_t link_state;
    bool bool_value;
    uint8_t byte_value;

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Get_Lifecycle_State(NULL, &state));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Get_Lifecycle_State(&device_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Is_Started(NULL, &bool_value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Is_Started(&device_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Is_Configured(NULL, &bool_value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Is_Configured(&device_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Get_Address(NULL, &byte_value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Get_Address(&device_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Get_Configuration_Value(NULL, &byte_value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Get_Configuration_Value(&device_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Get_Link_State(NULL, &link_state));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Get_Link_State(&device_ctx, NULL));
}

void test_public_state_accessors_report_runtime_state(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    xUSBD_Lifecycle_State_t state = xUSBD_LIFECYCLE_CREATED;
    USB_DCD_Link_State_t link_state = USB_DCD_LINK_STATE_U0;
    bool bool_value = true;
    uint8_t byte_value = 0xFFU;

    prepare_startable_device(&device_ctx, &class_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Get_Lifecycle_State(&device_ctx, &state));
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_CLASSES_REGISTERED, state);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Is_Started(&device_ctx, &bool_value));
    TEST_ASSERT_FALSE(bool_value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Get_Link_State(&device_ctx, &link_state));
    TEST_ASSERT_EQUAL(USB_DCD_LINK_STATE_UNKNOWN, link_state);

    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Get_Lifecycle_State(&device_ctx, &state));
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STARTED, state);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Is_Started(&device_ctx, &bool_value));
    TEST_ASSERT_TRUE(bool_value);

    device_ctx.is_addressed = true;
    device_ctx.is_configured = true;
    device_ctx.address_value = 5U;
    device_ctx.configuration_value = 1U;
    device_ctx.link_state = USB_DCD_LINK_STATE_U2;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Is_Configured(&device_ctx, &bool_value));
    TEST_ASSERT_TRUE(bool_value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Get_Address(&device_ctx, &byte_value));
    TEST_ASSERT_EQUAL_UINT8(5U, byte_value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Get_Configuration_Value(&device_ctx, &byte_value));
    TEST_ASSERT_EQUAL_UINT8(1U, byte_value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Get_Link_State(&device_ctx, &link_state));
    TEST_ASSERT_EQUAL(USB_DCD_LINK_STATE_U2, link_state);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Stop(&device_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Get_Lifecycle_State(&device_ctx, &state));
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STOPPED, state);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Is_Started(&device_ctx, &bool_value));
    TEST_ASSERT_FALSE(bool_value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Is_Configured(&device_ctx, &bool_value));
    TEST_ASSERT_FALSE(bool_value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Get_Address(&device_ctx, &byte_value));
    TEST_ASSERT_EQUAL_UINT8(0U, byte_value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Get_Configuration_Value(&device_ctx, &byte_value));
    TEST_ASSERT_EQUAL_UINT8(0U, byte_value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Get_Link_State(&device_ctx, &link_state));
    TEST_ASSERT_EQUAL(USB_DCD_LINK_STATE_DISABLED, link_state);
}

void test_reset_event_clears_bus_state(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_started_device(&device_ctx, &class_ctx);

    device_ctx.is_addressed = true;
    device_ctx.address_value = 5U;
    device_ctx.is_configured = true;
    device_ctx.configuration_value = 1U;
    device_ctx.lifecycle_state = xUSBD_LIFECYCLE_CONFIGURED;

    dcd_fire_event(&device_ctx, USB_DCD_RESET_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_FALSE(device_ctx.is_addressed);
    TEST_ASSERT_EQUAL_UINT8(0U, device_ctx.address_value);
    TEST_ASSERT_FALSE(device_ctx.is_configured);
    TEST_ASSERT_EQUAL_UINT8(0U, device_ctx.configuration_value);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STARTED, device_ctx.lifecycle_state);
}

// TESTS: xUSBD_Stop ///////////////////////////////////////////////////////////

void test_stop_rejects_null_and_not_started(void)
{
    xUSBD_Device_Context_t device_ctx;

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Stop(NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NOT_INITIALIZED, xUSBD_Stop(&device_ctx));
}

void test_stop_propagates_dcd_disable_failure(void)
{
    // Stop() is best-effort: all three DCD teardown calls are always made and the
    // software context is always reset to STOPPED, even when a DCD step fails.
    // The first failure code is returned so the caller knows something went wrong.
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_started_device(&device_ctx, &class_ctx);
    fake_dcd_disable_interrupts_fake.return_val = xRETURN_xERR_xUSBD_DCD_INVALID_PORT;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_INVALID_PORT, xUSBD_Stop(&device_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_disable_interrupts_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_disconnect_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_deinit_fake.call_count);
    TEST_ASSERT_FALSE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STOPPED, device_ctx.lifecycle_state);
}

void test_stop_propagates_dcd_disconnect_failure(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_started_device(&device_ctx, &class_ctx);
    fake_dcd_disconnect_fake.return_val = xRETURN_xERR_xUSBD_DCD_INVALID_PORT;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_INVALID_PORT, xUSBD_Stop(&device_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_disable_interrupts_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_disconnect_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_deinit_fake.call_count);
    TEST_ASSERT_FALSE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STOPPED, device_ctx.lifecycle_state);
}

void test_stop_propagates_dcd_deinit_failure(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_started_device(&device_ctx, &class_ctx);
    fake_dcd_deinit_fake.return_val = xRETURN_xERR_xUSBD_DCD_INVALID_PORT;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_INVALID_PORT, xUSBD_Stop(&device_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_disable_interrupts_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_disconnect_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_deinit_fake.call_count);
    TEST_ASSERT_FALSE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STOPPED, device_ctx.lifecycle_state);
}

void test_stop_transitions_to_stopped_after_dcd_success(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_started_device(&device_ctx, &class_ctx);

    device_ctx.is_addressed = true;
    device_ctx.is_configured = true;
    device_ctx.address_value = 5U;
    device_ctx.configuration_value = 1U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Stop(&device_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_disable_interrupts_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_disconnect_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_deinit_fake.call_count);
    TEST_ASSERT_FALSE(device_ctx.is_started);
    TEST_ASSERT_FALSE(device_ctx.is_addressed);
    TEST_ASSERT_FALSE(device_ctx.is_configured);
    TEST_ASSERT_EQUAL_UINT8(0U, device_ctx.address_value);
    TEST_ASSERT_EQUAL_UINT8(0U, device_ctx.configuration_value);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STOPPED, device_ctx.lifecycle_state);
    TEST_ASSERT_NULL(device_ctx.dcd_ops);
    TEST_ASSERT_NULL(device_ctx.dcd_ctx);
}

// TESTS: lifecycle edge cases /////////////////////////////////////////////////

void test_reinit_after_stop_succeeds(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_started_device(&device_ctx, &class_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Stop(&device_ctx));
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STOPPED, device_ctx.lifecycle_state);

    // Reset fakes to count only the second start
    RESET_ALL_DCD_FAKES();

    xUSBD_Class_Context_t class_ctx2 = {0};
    test_device_init(&device_ctx);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_INITIALIZED, device_ctx.lifecycle_state);
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx2, &normal_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STARTED, device_ctx.lifecycle_state);
    TEST_ASSERT_TRUE(device_ctx.is_started);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_init_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_connect_fake.call_count);
}

void test_invalid_setup_packet_handled_gracefully(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    uint8_t dummy[4] = {0};

    prepare_started_device(&device_ctx, &class_ctx);
    TEST_ASSERT_NOT_NULL(fake_dcd_set_callback_fake.arg1_val);

    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0U, dummy, sizeof(dummy));
    TEST_ASSERT_TRUE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STARTED, device_ctx.lifecycle_state);

    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0U, NULL, 8U);
    TEST_ASSERT_TRUE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STARTED, device_ctx.lifecycle_state);
}

void test_connect_then_reset_clears_bus_state(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_started_device(&device_ctx, &class_ctx);

    dcd_fire_event(&device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);
    TEST_ASSERT_EQUAL(USB_SPEED_HIGH, device_ctx.speed);

    device_ctx.is_addressed = true;
    device_ctx.address_value = 7U;
    device_ctx.is_configured = true;
    device_ctx.configuration_value = 1U;
    device_ctx.lifecycle_state = xUSBD_LIFECYCLE_CONFIGURED;

    dcd_fire_event(&device_ctx, USB_DCD_RESET_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_FALSE(device_ctx.is_addressed);
    TEST_ASSERT_EQUAL_UINT8(0U, device_ctx.address_value);
    TEST_ASSERT_FALSE(device_ctx.is_configured);
    TEST_ASSERT_EQUAL_UINT8(0U, device_ctx.configuration_value);
    TEST_ASSERT_EQUAL(xUSBD_LIFECYCLE_STARTED, device_ctx.lifecycle_state);
    TEST_ASSERT_TRUE(device_ctx.is_started);
    TEST_ASSERT_EQUAL(USB_SPEED_HIGH, device_ctx.speed);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_validates_public_config);
    RUN_TEST(test_lifecycle_tracks_init_and_registration);
    RUN_TEST(test_trace_init_attaches_and_detaches_context);
    RUN_TEST(test_trace_records_bus_and_standard_control_events);
    RUN_TEST(test_trace_detach_stops_event_emission_without_changing_behavior);
    RUN_TEST(test_start_validates_public_config);
    RUN_TEST(test_start_rejected_before_class_registration);
    RUN_TEST(test_start_propagates_dcd_init_failure);
    RUN_TEST(test_start_propagates_dcd_callback_failure);
    RUN_TEST(test_start_propagates_dcd_connect_failure);
    RUN_TEST(test_start_propagates_dcd_interrupt_failure);
    RUN_TEST(test_start_succeeds_after_dcd_success);
    RUN_TEST(test_lifecycle_tracks_configuration);
    RUN_TEST(test_public_state_accessors_validate_arguments);
    RUN_TEST(test_public_state_accessors_report_runtime_state);
    RUN_TEST(test_reset_event_clears_bus_state);
    RUN_TEST(test_stop_rejects_null_and_not_started);
    RUN_TEST(test_stop_propagates_dcd_disable_failure);
    RUN_TEST(test_stop_propagates_dcd_disconnect_failure);
    RUN_TEST(test_stop_propagates_dcd_deinit_failure);
    RUN_TEST(test_stop_transitions_to_stopped_after_dcd_success);
    RUN_TEST(test_reinit_after_stop_succeeds);
    RUN_TEST(test_invalid_setup_packet_handled_gracefully);
    RUN_TEST(test_connect_then_reset_clears_bus_state);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////
