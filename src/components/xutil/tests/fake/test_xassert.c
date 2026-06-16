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

// @file test_xassert.c
// @brief Host tests for xASSERT metadata routing.

#include <stdint.h>

#include "unity.h"

static uint32_t s_assert_count;
static const char *s_assert_file;
static int s_assert_line;
static const char *s_assert_expr;
static const char *s_assert_msg;

static void test_assert_handler(const char *file, int line, const char *expr, const char *msg);

#define xASSERT_HANDLER(file, line, expr, msg) test_assert_handler((file), (line), (expr), (msg))
#include "xassert.h"

static void test_assert_handler(const char *file, int line, const char *expr, const char *msg)
{
    s_assert_count++;
    s_assert_file = file;
    s_assert_line = line;
    s_assert_expr = expr;
    s_assert_msg = msg;
}

void setUp(void)
{
    s_assert_count = 0U;
    s_assert_file = NULL;
    s_assert_line = 0;
    s_assert_expr = NULL;
    s_assert_msg = NULL;
}

void tearDown(void)
{
}

void test_assert_true_does_not_call_handler(void)
{
    // cppcheck-suppress duplicateExpression
    xASSERT(1 == 1, "should pass");

    TEST_ASSERT_EQUAL_UINT32(0U, s_assert_count);
}

void test_assert_false_routes_metadata_to_handler(void)
{
    xASSERT(2 == 3, "should fail");

    TEST_ASSERT_EQUAL_UINT32(1U, s_assert_count);
    TEST_ASSERT_NOT_NULL(s_assert_file);
    TEST_ASSERT_TRUE(s_assert_line > 0);
    TEST_ASSERT_EQUAL_STRING("2 == 3", s_assert_expr);
    TEST_ASSERT_EQUAL_STRING("should fail", s_assert_msg);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_assert_true_does_not_call_handler);
    RUN_TEST(test_assert_false_routes_metadata_to_handler);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
