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

// @file test_xboot_image.c
// @brief Host tests for xBOOT image header parsing and bounds validation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES
#include "unity.h"

// MODULE INCLUDES
#include "xboot_image.h"
#include "xboot_verify.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////
#define TEST_MEM_START 0x70040000U
#define TEST_MEM_SIZE  (768 * 1024U) // 768 KB

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////
static uint8_t g_header_buf[sizeof(xBOOT_Image_Header_t)];

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static void helper_fill_valid_image_header(uint8_t *buf)
{
    xBOOT_Image_Header_t header;
    (void)memset(&header, 0, sizeof(xBOOT_Image_Header_t));

    header.magic = xBOOT_IMAGE_MAGIC;
    header.header_version = xBOOT_IMAGE_HEADER_VERSION_1;
    header.header_size = sizeof(xBOOT_Image_Header_t);
    header.image_size = 0x1000U;
    header.load_address = 0x70040000U;
    header.entry_address = 0x70040000U;
    header.version_major = 1U;
    header.version_minor = 0U;
    header.version_patch = 0U;
    header.build_number = 100U;
    header.flags = 0U;
    header.payload_crc32 = 0x12345678U;

    header.header_crc32 =
        xBOOT_CRC32_Calculate((const uint8_t *)&header, offsetof(xBOOT_Image_Header_t, header_crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;

    (void)memcpy(buf, &header, sizeof(xBOOT_Image_Header_t));
}

void setUp(void)
{
    (void)memset(g_header_buf, 0, sizeof(g_header_buf));
}

void tearDown(void)
{
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void test_image_header_parse_rejects_null_arguments(void)
{
    xBOOT_Image_Header_t header;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_NULL_POINTER, xBOOT_Image_Header_Parse(NULL, sizeof(g_header_buf), &header));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_NULL_POINTER, xBOOT_Image_Header_Parse(g_header_buf, sizeof(g_header_buf), NULL));
}

void test_image_header_parse_rejects_insufficient_buffer_size(void)
{
    xBOOT_Image_Header_t header;
    helper_fill_valid_image_header(g_header_buf);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT,
                             xBOOT_Image_Header_Parse(g_header_buf, sizeof(xBOOT_Image_Header_t) - 1U, &header));
}

void test_image_header_parse_rejects_bad_magic(void)
{
    xBOOT_Image_Header_t header;
    helper_fill_valid_image_header(g_header_buf);

    uint32_t bad_magic = 0xDEADBEEFU;
    (void)memcpy(&g_header_buf[offsetof(xBOOT_Image_Header_t, magic)], &bad_magic, sizeof(uint32_t));

    // Recompute CRC to isolate magic check
    uint32_t crc = xBOOT_CRC32_Calculate(g_header_buf, offsetof(xBOOT_Image_Header_t, header_crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    (void)memcpy(&g_header_buf[offsetof(xBOOT_Image_Header_t, header_crc32)], &crc, sizeof(uint32_t));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_IMAGE, xBOOT_Image_Header_Parse(g_header_buf, sizeof(g_header_buf), &header));
}

void test_image_header_parse_rejects_bad_version(void)
{
    xBOOT_Image_Header_t header;
    helper_fill_valid_image_header(g_header_buf);

    uint32_t bad_version = 99U;
    (void)memcpy(&g_header_buf[offsetof(xBOOT_Image_Header_t, header_version)], &bad_version, sizeof(uint32_t));

    // Recompute CRC
    uint32_t crc = xBOOT_CRC32_Calculate(g_header_buf, offsetof(xBOOT_Image_Header_t, header_crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    (void)memcpy(&g_header_buf[offsetof(xBOOT_Image_Header_t, header_crc32)], &crc, sizeof(uint32_t));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_IMAGE, xBOOT_Image_Header_Parse(g_header_buf, sizeof(g_header_buf), &header));
}

void test_image_header_parse_rejects_bad_header_size(void)
{
    xBOOT_Image_Header_t header;
    helper_fill_valid_image_header(g_header_buf);

    uint32_t bad_size = sizeof(xBOOT_Image_Header_t) + 4U;
    (void)memcpy(&g_header_buf[offsetof(xBOOT_Image_Header_t, header_size)], &bad_size, sizeof(uint32_t));

    // Recompute CRC
    uint32_t crc = xBOOT_CRC32_Calculate(g_header_buf, offsetof(xBOOT_Image_Header_t, header_crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    (void)memcpy(&g_header_buf[offsetof(xBOOT_Image_Header_t, header_crc32)], &crc, sizeof(uint32_t));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_IMAGE, xBOOT_Image_Header_Parse(g_header_buf, sizeof(g_header_buf), &header));
}

void test_image_header_parse_rejects_bad_crc(void)
{
    xBOOT_Image_Header_t header;
    helper_fill_valid_image_header(g_header_buf);

    g_header_buf[offsetof(xBOOT_Image_Header_t, header_crc32)] ^= 0xFFU;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_CRC_MISMATCH, xBOOT_Image_Header_Parse(g_header_buf, sizeof(g_header_buf), &header));
}

void test_image_header_parse_success(void)
{
    xBOOT_Image_Header_t header;
    helper_fill_valid_image_header(g_header_buf);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Image_Header_Parse(g_header_buf, sizeof(g_header_buf), &header));
    TEST_ASSERT_EQUAL_UINT32(xBOOT_IMAGE_MAGIC, header.magic);
    TEST_ASSERT_EQUAL_UINT32(xBOOT_IMAGE_HEADER_VERSION_1, header.header_version);
    TEST_ASSERT_EQUAL_UINT32(0x70040000U, header.load_address);
    TEST_ASSERT_EQUAL_UINT32(0x70040000U, header.entry_address);
}

void test_image_bounds_validation(void)
{
    xBOOT_Image_Header_t header;
    (void)memset(&header, 0, sizeof(xBOOT_Image_Header_t));

    header.load_address = TEST_MEM_START;
    header.image_size = 0x1000U;
    header.entry_address = TEST_MEM_START + 0x100U;

    // 1. Success case
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Image_Validate_Bounds(&header, TEST_MEM_START, TEST_MEM_SIZE));

    // 2. Reject NULL header
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_NULL_POINTER, xBOOT_Image_Validate_Bounds(NULL, TEST_MEM_START, TEST_MEM_SIZE));

    // 3. Reject invalid memory size
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT, xBOOT_Image_Validate_Bounds(&header, TEST_MEM_START, 0));

    // 4. Reject load address below memory start
    header.load_address = TEST_MEM_START - 4U;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_IMAGE, xBOOT_Image_Validate_Bounds(&header, TEST_MEM_START, TEST_MEM_SIZE));

    // 5. Reject image exceeding memory limit
    header.load_address = TEST_MEM_START;
    header.image_size = TEST_MEM_SIZE + 4U;
    header.entry_address = TEST_MEM_START;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_IMAGE, xBOOT_Image_Validate_Bounds(&header, TEST_MEM_START, TEST_MEM_SIZE));

    // 6. Reject load address + size wrapping/overflowing memory end
    header.load_address = TEST_MEM_START + TEST_MEM_SIZE - 0x100U;
    header.image_size = 0x200U;
    header.entry_address = header.load_address;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_IMAGE, xBOOT_Image_Validate_Bounds(&header, TEST_MEM_START, TEST_MEM_SIZE));

    // 7. Reject entry point outside loaded image span (below load address)
    header.load_address = TEST_MEM_START + 0x1000U;
    header.image_size = 0x1000U;
    header.entry_address = header.load_address - 4U;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_IMAGE, xBOOT_Image_Validate_Bounds(&header, TEST_MEM_START, TEST_MEM_SIZE));

    // 8. Reject entry point outside loaded image span (above image end)
    header.entry_address = header.load_address + header.image_size;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_IMAGE, xBOOT_Image_Validate_Bounds(&header, TEST_MEM_START, TEST_MEM_SIZE));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_image_header_parse_rejects_null_arguments);
    RUN_TEST(test_image_header_parse_rejects_insufficient_buffer_size);
    RUN_TEST(test_image_header_parse_rejects_bad_magic);
    RUN_TEST(test_image_header_parse_rejects_bad_version);
    RUN_TEST(test_image_header_parse_rejects_bad_header_size);
    RUN_TEST(test_image_header_parse_rejects_bad_crc);
    RUN_TEST(test_image_header_parse_success);
    RUN_TEST(test_image_bounds_validation);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
