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

// @file test_xtrace_cobs.c
// @brief Unit tests for xTRACE_COBS_Encode and xTRACE_COBS_Decode.
//

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "xtrace_cobs.h"

// -- Helpers -------------------------------------------------------------------

#define ENC_BUF 512U
#define DEC_BUF 512U

static uint8_t g_enc[ENC_BUF];
static uint8_t g_dec[DEC_BUF];

static void encode_then_decode(const uint8_t *src, uint32_t src_len)
{
    (void)memset(g_enc, 0xAA, sizeof(g_enc));
    (void)memset(g_dec, 0xBB, sizeof(g_dec));

    size_t enc_len = xTRACE_COBS_Encode(src, src_len, g_enc, ENC_BUF);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, enc_len);

    // Encoded output must not contain 0x00 except the trailing delimiter.
    for (uint32_t i = 0U; i < enc_len - 1U; i++)
    {
        TEST_ASSERT_NOT_EQUAL_UINT8(0x00U, g_enc[i]);
    }
    TEST_ASSERT_EQUAL_UINT8(0x00U, g_enc[enc_len - 1U]);

    // Decode (exclude trailing delimiter).
    size_t dec_len = xTRACE_COBS_Decode(g_enc, enc_len - 1U, g_dec, DEC_BUF, NULL);
    TEST_ASSERT_EQUAL_UINT32(src_len, dec_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(src, g_dec, src_len);
}

// -- setUp / tearDown ---------------------------------------------------------

void setUp(void)
{
}
void tearDown(void)
{
}

// -- Encode tests -------------------------------------------------------------

void test_cobs_encode_no_zeros_roundtrip(void)
{
    static const uint8_t src[] = {0x01U, 0x02U, 0x03U, 0x04U};
    encode_then_decode(src, sizeof(src));
}

void test_cobs_encode_with_zeros_roundtrip(void)
{
    static const uint8_t src[] = {0x01U, 0x00U, 0x02U, 0x00U, 0x03U};
    encode_then_decode(src, sizeof(src));
}

void test_cobs_encode_all_zeros_roundtrip(void)
{
    static const uint8_t src[] = {0x00U, 0x00U, 0x00U};
    encode_then_decode(src, sizeof(src));
}

void test_cobs_encode_single_zero_roundtrip(void)
{
    static const uint8_t src[] = {0x00U};
    encode_then_decode(src, sizeof(src));
}

void test_cobs_encode_single_nonzero_roundtrip(void)
{
    static const uint8_t src[] = {0xA5U};
    encode_then_decode(src, sizeof(src));
}

void test_cobs_encode_empty_input_produces_two_bytes(void)
{
    size_t enc_len = xTRACE_COBS_Encode(NULL, 0U, g_enc, ENC_BUF);
    // NULL src with 0 length: graceful return 0
    TEST_ASSERT_EQUAL_UINT32(0U, enc_len);

    // Valid empty input
    static const uint8_t empty[1] = {0U};
    enc_len = xTRACE_COBS_Encode(empty, 0U, g_enc, ENC_BUF);
    TEST_ASSERT_EQUAL_UINT32(2U, enc_len);    // overhead byte + delimiter
    TEST_ASSERT_EQUAL_UINT8(0x01U, g_enc[0]); // code = 1 (empty chunk)
    TEST_ASSERT_EQUAL_UINT8(0x00U, g_enc[1]); // delimiter
}

void test_cobs_encode_254_bytes_no_zeros_roundtrip(void)
{
    uint8_t src[254];
    for (uint32_t i = 0U; i < 254U; i++)
    {
        src[i] = (uint8_t)(i + 1U); // 1..254, no zeros
    }
    encode_then_decode(src, 254U);
}

void test_cobs_encode_255_bytes_spanning_two_chunks(void)
{
    uint8_t src[255];
    for (uint32_t i = 0U; i < 255U; i++)
    {
        src[i] = (uint8_t)(i + 1U); // 1..255, no zeros; chunk boundary at byte 254
    }
    encode_then_decode(src, 255U);
}

void test_cobs_encode_returns_zero_on_null_src(void)
{
    size_t enc_len = xTRACE_COBS_Encode(NULL, 4U, g_enc, ENC_BUF);
    TEST_ASSERT_EQUAL_UINT32(0U, enc_len);
}

void test_cobs_encode_returns_zero_on_null_dst(void)
{
    static const uint8_t src[] = {0x01U, 0x02U};
    size_t enc_len = xTRACE_COBS_Encode(src, sizeof(src), NULL, ENC_BUF);
    TEST_ASSERT_EQUAL_UINT32(0U, enc_len);
}

void test_cobs_encode_returns_zero_when_dst_too_small(void)
{
    static const uint8_t src[] = {0x01U, 0x02U, 0x03U};
    size_t enc_len = xTRACE_COBS_Encode(src, sizeof(src), g_enc, 1U);
    TEST_ASSERT_EQUAL_UINT32(0U, enc_len);
}

void test_cobs_encode_returns_zero_when_encoded_size_overflows(void)
{
    static const uint8_t src[] = {0x01U};
    size_t enc_len = xTRACE_COBS_Encode(src, SIZE_MAX, g_enc, ENC_BUF);
    TEST_ASSERT_EQUAL_UINT32(0U, enc_len);
}

void test_cobs_encode_output_has_no_interior_zeros(void)
{
    // Mix of zero and non-zero bytes to stress the overhead logic.
    static const uint8_t src[] = {0x01U, 0x00U, 0x02U, 0x03U, 0x00U, 0x04U, 0x00U, 0x00U, 0x05U};
    size_t enc_len = xTRACE_COBS_Encode(src, sizeof(src), g_enc, ENC_BUF);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, enc_len);

    for (uint32_t i = 0U; i < enc_len - 1U; i++)
    {
        TEST_ASSERT_NOT_EQUAL_MESSAGE(0x00U, g_enc[i], "Interior 0x00 found in COBS-encoded output");
    }
    TEST_ASSERT_EQUAL_UINT8(0x00U, g_enc[enc_len - 1U]);
}

// -- Decode tests -------------------------------------------------------------

void test_cobs_decode_returns_zero_on_null_src(void)
{
    size_t dec_len = xTRACE_COBS_Decode(NULL, 4U, g_dec, DEC_BUF, NULL);
    TEST_ASSERT_EQUAL_UINT32(0U, dec_len);
}

void test_cobs_decode_returns_zero_on_null_dst(void)
{
    static const uint8_t enc[] = {0x02U, 0xAAU, 0x00U};
    size_t dec_len = xTRACE_COBS_Decode(enc, 2U, NULL, DEC_BUF, NULL);
    TEST_ASSERT_EQUAL_UINT32(0U, dec_len);
}

void test_cobs_decode_stops_at_unexpected_zero(void)
{
    // Sequence: code=2 (1 data byte 0xAA + implicit zero), then unexpected 0x00.
    // The decoder correctly produces the implicit zero from the first chunk
    // and then stops when it encounters the 0x00 code byte for the second chunk.
    // The 0xBB byte after the 0x00 must NOT appear in the output.
    static const uint8_t corrupt[] = {0x02U, 0xAAU, 0x00U, 0x02U, 0xBBU};
    size_t dec_len = xTRACE_COBS_Decode(corrupt, sizeof(corrupt), g_dec, DEC_BUF, NULL);
    TEST_ASSERT_EQUAL_UINT32(2U, dec_len); // 0xAA + implicit 0x00 from chunk 1
    TEST_ASSERT_EQUAL_UINT8(0xAAU, g_dec[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00U, g_dec[1]);
    // 0xBB from the second chunk was not decoded.
}

void test_cobs_decode_empty_encoded_gives_empty_output(void)
{
    static const uint8_t enc[] = {0x01U}; // code=1, run=0, no data, no implicit zero
    size_t dec_len = xTRACE_COBS_Decode(enc, sizeof(enc), g_dec, DEC_BUF, NULL);
    TEST_ASSERT_EQUAL_UINT32(0U, dec_len);
}

void test_cobs_decode_truncated_flag_set_when_dst_too_small(void)
{
    // Encode 4 bytes, decode into a 2-byte buffer - decode is truncated.
    static const uint8_t src[] = {0x01U, 0x02U, 0x03U, 0x04U};
    uint8_t enc[xTRACE_COBS_MAX_ENCODED_SIZE(sizeof(src))];
    size_t enc_len = xTRACE_COBS_Encode(src, sizeof(src), enc, sizeof(enc));
    TEST_ASSERT_GREATER_THAN_UINT32(0U, enc_len);

    uint8_t small_dst[2];
    bool was_truncated = false;
    xTRACE_COBS_Decode(enc, enc_len - 1U, small_dst, sizeof(small_dst), &was_truncated);
    TEST_ASSERT_TRUE(was_truncated);
}

void test_cobs_decode_truncated_flag_clear_on_clean_decode(void)
{
    // Clean roundtrip: truncated must be false.
    static const uint8_t src[] = {0x01U, 0x02U, 0x03U};
    uint8_t enc[xTRACE_COBS_MAX_ENCODED_SIZE(sizeof(src))];
    size_t enc_len = xTRACE_COBS_Encode(src, sizeof(src), enc, sizeof(enc));
    TEST_ASSERT_GREATER_THAN_UINT32(0U, enc_len);

    bool was_truncated = true; // initialise to true; expect to be cleared
    xTRACE_COBS_Decode(enc, enc_len - 1U, g_dec, DEC_BUF, &was_truncated);
    TEST_ASSERT_FALSE(was_truncated);
}

// -- MAX_ENCODED_SIZE macro ----------------------------------------------------

void test_cobs_max_encoded_size_fits_actual_output(void)
{
    for (uint32_t n = 0U; n <= 300U; n++)
    {
        uint8_t src[300] = {0};
        size_t max_size = xTRACE_COBS_MAX_ENCODED_SIZE(n);
        uint8_t dst_buf[600];
        size_t actual = xTRACE_COBS_Encode(src, n, dst_buf, sizeof(dst_buf));
        TEST_ASSERT_TRUE_MESSAGE(actual <= max_size, "xTRACE_COBS_MAX_ENCODED_SIZE underestimates actual encoded size");
    }
}

// -- Main ---------------------------------------------------------------------

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_cobs_encode_no_zeros_roundtrip);
    RUN_TEST(test_cobs_encode_with_zeros_roundtrip);
    RUN_TEST(test_cobs_encode_all_zeros_roundtrip);
    RUN_TEST(test_cobs_encode_single_zero_roundtrip);
    RUN_TEST(test_cobs_encode_single_nonzero_roundtrip);
    RUN_TEST(test_cobs_encode_empty_input_produces_two_bytes);
    RUN_TEST(test_cobs_encode_254_bytes_no_zeros_roundtrip);
    RUN_TEST(test_cobs_encode_255_bytes_spanning_two_chunks);
    RUN_TEST(test_cobs_encode_returns_zero_on_null_src);
    RUN_TEST(test_cobs_encode_returns_zero_on_null_dst);
    RUN_TEST(test_cobs_encode_returns_zero_when_dst_too_small);
    RUN_TEST(test_cobs_encode_returns_zero_when_encoded_size_overflows);
    RUN_TEST(test_cobs_encode_output_has_no_interior_zeros);
    RUN_TEST(test_cobs_decode_returns_zero_on_null_src);
    RUN_TEST(test_cobs_decode_returns_zero_on_null_dst);
    RUN_TEST(test_cobs_decode_stops_at_unexpected_zero);
    RUN_TEST(test_cobs_decode_empty_encoded_gives_empty_output);
    RUN_TEST(test_cobs_decode_truncated_flag_set_when_dst_too_small);
    RUN_TEST(test_cobs_decode_truncated_flag_clear_on_clean_decode);
    RUN_TEST(test_cobs_max_encoded_size_fits_actual_output);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
