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

// @file xtrace_cobs.c
// @brief COBS encoder and decoder for xTrace transport framing.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xtrace_cobs.h"

// MODULE MACROS ///////////////////////////////////////////////////////////////////

// Maximum non-zero bytes per COBS chunk (RFC: 254).
#define COBS_MAX_CHUNK 254U

// MODULE TYPES ////////////////////////////////////////////////////////////////////

// MODULE VARIABLES ////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// MODULE FUNCTION PROTOTYPES //////////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

size_t xTRACE_COBS_Encode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_cap)
{
    if ((src == NULL) || (dst == NULL))
    {
        return 0U;
    }
    if (src_len > (SIZE_MAX - (src_len / COBS_MAX_CHUNK) - 2U))
    {
        return 0U;
    }
    if (dst_cap < xTRACE_COBS_MAX_ENCODED_SIZE(src_len))
    {
        return 0U;
    }

    size_t src_idx = 0U;
    size_t dst_idx = 0U;
    size_t overhead_pos = 0U; // position in dst where the current code byte lives
    uint8_t code = 1U;        // number of bytes in the current chunk (including code byte)

    dst[dst_idx] = 0U; // placeholder - will be overwritten with the first code byte
    dst_idx++;

    while (src_idx < src_len)
    {
        uint8_t b = src[src_idx];
        src_idx++;

        if (b == 0U)
        {
            // Zero in input: close the current chunk, start a new one.
            dst[overhead_pos] = code;
            overhead_pos = dst_idx;
            dst[dst_idx] = 0U; // placeholder for next code byte
            dst_idx++;
            code = 1U;
        }
        else
        {
            dst[dst_idx] = b;
            dst_idx++;
            code++;

            if (code == (uint8_t)(COBS_MAX_CHUNK + 1U))
            {
                // Chunk full (254 non-zero data bytes): close and start a new one.
                dst[overhead_pos] = code;
                overhead_pos = dst_idx;
                dst[dst_idx] = 0U; // placeholder
                dst_idx++;
                code = 1U;
            }
        }
    }

    // Close the final chunk and append the frame delimiter.
    dst[overhead_pos] = code;
    dst[dst_idx] = 0U; // 0x00 frame delimiter
    dst_idx++;

    return dst_idx;
}

size_t xTRACE_COBS_Decode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_cap, bool *truncated)
{
    if ((src == NULL) || (dst == NULL))
    {
        if (truncated != NULL)
        {
            *truncated = false;
        }
        return 0U;
    }

    size_t src_idx = 0U;
    size_t dst_idx = 0U;
    bool is_truncated = false;

    while (src_idx < src_len)
    {
        uint8_t code = src[src_idx];
        size_t run;

        if (code == 0U)
        {
            // Unexpected 0x00 - corrupt packet or caller included frame delimiter.
            is_truncated = true;
            break;
        }
        src_idx++;

        run = (size_t)code - 1U;

        // Guard against truncated source or output buffer overflow.
        if ((run > (src_len - src_idx)) || (run > (dst_cap - dst_idx)))
        {
            is_truncated = true;
            break;
        }

        for (size_t i = 0U; i < run; i++)
        {
            dst[dst_idx] = src[src_idx];
            dst_idx++;
            src_idx++;
        }

        // Append the implicit zero between chunks, except after a full chunk
        // (code == 0xFF means 254 non-zero bytes with no implicit zero).
        if (code != (uint8_t)(COBS_MAX_CHUNK + 1U))
        {
            if (src_idx < src_len)
            {
                // More data follows - we need the implicit zero.
                if (dst_idx >= dst_cap)
                {
                    is_truncated = true; // no room for the inter-chunk zero
                    break;
                }
                dst[dst_idx] = 0U;
                dst_idx++;
            }
            // If src_idx == src_len: last chunk; no implicit zero needed.
        }
    }

    if (truncated != NULL)
    {
        *truncated = is_truncated;
    }
    return dst_idx;
}

// EOF /////////////////////////////////////////////////////////////////////////////
