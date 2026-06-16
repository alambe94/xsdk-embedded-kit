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

// @file test_xlog.c
// @brief Host tests for xLOG backend routing.

#include <stdint.h>

#include "unity.h"

static uint32_t s_error_count;
static uint32_t s_status_count;
static uint32_t s_message_count;
static uint32_t s_last_code;

static void test_log_error(uint32_t code);
static void test_log_status(uint32_t code);
static void test_log_message(uint32_t code);

#define xLOG_ERROR(code, ...)   test_log_error((uint32_t)(code))
#define xLOG_STATUS(code)       test_log_status((uint32_t)(code))
#define xLOG_MESSAGE(code, ...) test_log_message((uint32_t)(code))

#include "xlog.h"

static void call_error_log(uint32_t code)
{
#if (xLOG_LEVEL_ERROR >= xLOG_LEVEL_MESSAGE)
    xLOG_MESSAGE(code, "error %u", 0U);
#elif (xLOG_LEVEL_ERROR >= xLOG_LEVEL_STATUS)
    do
    {
        xLOG_STATUS(code);
    } while (0);
#elif (xLOG_LEVEL_ERROR >= xLOG_LEVEL_ERROR)
    xLOG_ERROR(code, "error %u", 0U);
#else
    (void)(code);
#endif
}

static void call_status_log(uint32_t code)
{
#if (xLOG_LEVEL_STATUS >= xLOG_LEVEL_MESSAGE)
    xLOG_MESSAGE(code, "unused");
#elif (xLOG_LEVEL_STATUS >= xLOG_LEVEL_STATUS)
    do
    {
        xLOG_STATUS(code);
    } while (0);
#elif (xLOG_LEVEL_STATUS >= xLOG_LEVEL_ERROR)
    xLOG_ERROR(code, "unused");
#else
    (void)(code);
#endif
}

static void call_message_log(uint32_t code)
{
#if (xLOG_LEVEL_MESSAGE >= xLOG_LEVEL_MESSAGE)
    xLOG_MESSAGE(code, "message %u", 7U);
#elif (xLOG_LEVEL_MESSAGE >= xLOG_LEVEL_STATUS)
    do
    {
        xLOG_STATUS(code);
    } while (0);
#elif (xLOG_LEVEL_MESSAGE >= xLOG_LEVEL_ERROR)
    xLOG_ERROR(code, "message %u", 7U);
#else
    (void)(code);
#endif
}

static void test_log_error(uint32_t code)
{
    s_error_count++;
    s_last_code = code;
}

static void test_log_status(uint32_t code)
{
    s_status_count++;
    s_last_code = code;
}

static void test_log_message(uint32_t code)
{
    s_message_count++;
    s_last_code = code;
}

void setUp(void)
{
    s_error_count = 0U;
    s_status_count = 0U;
    s_message_count = 0U;
    s_last_code = 0U;
}

void tearDown(void)
{
}

void test_status_level_routes_to_status_backend(void)
{
    call_status_log(0x12345678U);

    TEST_ASSERT_EQUAL_UINT32(1U, s_status_count);
    TEST_ASSERT_EQUAL_UINT32(0U, s_message_count);
    TEST_ASSERT_EQUAL_UINT32(0x12345678U, s_last_code);
}

void test_message_level_routes_to_message_backend(void)
{
    call_message_log(0x87654321U);

    TEST_ASSERT_EQUAL_UINT32(0U, s_status_count);
    TEST_ASSERT_EQUAL_UINT32(1U, s_message_count);
    TEST_ASSERT_EQUAL_UINT32(0x87654321U, s_last_code);
}

void test_error_level_routes_to_error_backend(void)
{
    call_error_log(0xDEADBEEFU);

    TEST_ASSERT_EQUAL_UINT32(1U, s_error_count);
    TEST_ASSERT_EQUAL_UINT32(0U, s_status_count);
    TEST_ASSERT_EQUAL_UINT32(0U, s_message_count);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFU, s_last_code);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_error_level_routes_to_error_backend);
    RUN_TEST(test_status_level_routes_to_status_backend);
    RUN_TEST(test_message_level_routes_to_message_backend);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
