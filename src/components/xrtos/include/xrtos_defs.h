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

// @file xrtos_defs.h
// @brief xRTOS shared constants, bitmap type, and bitmap helper functions.
//
// All scheduler, wait, timeout, and timer bitmaps are accessed exclusively
// through the helpers defined here. Production code shall not open-code
// shifts such as (1U << priority) outside these helpers.
//
// xRTOS_BITMAP_WIDTH is the backing width for bitmap helpers. Task capacity,
// priority count, and timer capacity are configured independently; the bitmap
// width only needs to cover the largest of those domains.
//

#ifndef XRTOS_DEFS_H
#define XRTOS_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xassert.h"
#include "xrtos_config.h"
#include "xrtos_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // Module version
#define xRTOS_VERSION_MAJOR  0U
#define xRTOS_VERSION_MINOR  1U
#define xRTOS_VERSION_PATCH  0U
#define xRTOS_VERSION_STRING "0.1.0"

    // Bitmap geometry - xRTOS_BITMAP_WIDTH comes from xrtos_config.h.
#define xRTOS_BITMAP_WORD_BITS  32U
#define xRTOS_BITMAP_WORD_COUNT ((xRTOS_BITMAP_WIDTH + xRTOS_BITMAP_WORD_BITS - 1U) / xRTOS_BITMAP_WORD_BITS)

    // Task, priority, and timer limits.
#define xRTOS_MAX_TASKS      xRTOS_CONFIG_MAX_TASKS
#define xRTOS_MAX_PRIORITIES xRTOS_CONFIG_MAX_PRIORITIES
#define xRTOS_MAX_TIMERS     xRTOS_CONFIG_MAX_TIMERS

#if (xRTOS_MAX_TASKS == 0U)
#error "xRTOS_CONFIG_MAX_TASKS must be greater than zero"
#endif

#if (xRTOS_MAX_PRIORITIES < 2U)
#error "xRTOS_CONFIG_MAX_PRIORITIES must be at least 2"
#endif

#if (xRTOS_MAX_TIMERS == 0U)
#error "xRTOS_CONFIG_MAX_TIMERS must be greater than zero"
#endif

#if (xRTOS_BITMAP_WIDTH < xRTOS_MAX_TASKS)
#error "xRTOS_BITMAP_WIDTH must cover xRTOS_CONFIG_MAX_TASKS"
#endif

#if (xRTOS_BITMAP_WIDTH < xRTOS_MAX_PRIORITIES)
#error "xRTOS_BITMAP_WIDTH must cover xRTOS_CONFIG_MAX_PRIORITIES"
#endif

#if (xRTOS_BITMAP_WIDTH < xRTOS_MAX_TIMERS)
#error "xRTOS_BITMAP_WIDTH must cover xRTOS_CONFIG_MAX_TIMERS"
#endif

#if (xRTOS_BITMAP_WORD_BITS != 32U)
#error "xRTOS_BITMAP_WORD_BITS must be 32 - xRTOS targets 32-bit MCUs"
#endif

    // Timer execution methods
#define xRTOS_TIMER_METHOD_ISR  0U
#define xRTOS_TIMER_METHOD_TASK 1U

    // Compile-time validation for task count when Timer Task mode is enabled
#if (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_TASK) && (xRTOS_MAX_TASKS < 2U)
#error "xRTOS_CONFIG_MAX_TASKS must be at least 2 when Timer Task is enabled"
#endif

    // Task ID and priority sentinels. The idle task has its own task-id slot
    // and the lowest scheduling priority.
#define xRTOS_IDLE_TASK_ID         (xRTOS_MAX_TASKS - 1U)
#define xRTOS_TIMER_TASK_ID        (xRTOS_MAX_TASKS - 2U)
#define xRTOS_HIGHEST_PRIORITY     0U
#define xRTOS_IDLE_PRIORITY        (xRTOS_MAX_PRIORITIES - 1U)
#define xRTOS_LOWEST_USER_PRIORITY (xRTOS_IDLE_PRIORITY - 1U)

    // Sentinel values
    // xRTOS_INVALID_TASK_ID and xRTOS_WAIT_FOREVER share the same bit pattern
    // (0xFFFFFFFFU) but are semantically distinct - they are never compared
    // against each other.
#define xRTOS_INVALID_TASK_ID 0xFFFFFFFFU
#define xRTOS_WAIT_FOREVER    0xFFFFFFFFU
#define xRTOS_NO_WAIT         0U

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct xRTOS_Bitmap_t
    {
        uint32_t words[xRTOS_BITMAP_WORD_COUNT];

    } xRTOS_Bitmap_t;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // CTZ (count trailing zeros) helper for a single non-zero 32-bit word.
    // MISRA Deviation Rule 1.2: __builtin_ctz is a GCC/Clang compiler extension.
    // Justification: maps to a single CLZ/RBIT hardware instruction on ARM targets.
    // The portable fallback branch is provided for non-GCC/Clang compilers.
    static inline uint32_t xrtos_bitmap_ctz32(uint32_t x)
    {
#if defined(__GNUC__) || defined(__clang__)
        return (uint32_t)__builtin_ctz(x);
#else
    uint32_t n = 0U;
    uint32_t v = x;
    while (((v & 1U) == 0U) && (n < (xRTOS_BITMAP_WORD_BITS - 1U)))
    {
        v >>= 1U;
        n++;
    }
    return n;
#endif
    }

    // Returns the valid-bit mask for a bitmap word. The final word may be only
    // partially used when xRTOS_BITMAP_WIDTH is not a multiple of 32.
    static inline uint32_t xrtos_bitmap_word_mask(uint32_t word_index)
    {
        uint32_t mask = 0xFFFFFFFFU;

#if ((xRTOS_BITMAP_WIDTH % xRTOS_BITMAP_WORD_BITS) != 0U)
        if (word_index == (xRTOS_BITMAP_WORD_COUNT - 1U))
        {
            uint32_t used_bits = xRTOS_BITMAP_WIDTH % xRTOS_BITMAP_WORD_BITS;
            mask = ((uint32_t)1U << used_bits) - 1U;
        }
#else
    (void)word_index;
#endif

        return mask;
    }

    static inline void xRTOS_Bitmap_Set(xRTOS_Bitmap_t *bitmap, uint32_t bit_index)
    {
        xASSERT(bitmap != NULL, "bitmap is NULL");
        xASSERT(bit_index < xRTOS_BITMAP_WIDTH, "bit_index out of range");

        uint32_t word = bit_index / xRTOS_BITMAP_WORD_BITS;
        uint32_t bit = bit_index % xRTOS_BITMAP_WORD_BITS;
        bitmap->words[word] |= (1U << bit);
    }

    static inline void xRTOS_Bitmap_Clear(xRTOS_Bitmap_t *bitmap, uint32_t bit_index)
    {
        xASSERT(bitmap != NULL, "bitmap is NULL");
        xASSERT(bit_index < xRTOS_BITMAP_WIDTH, "bit_index out of range");

        uint32_t word = bit_index / xRTOS_BITMAP_WORD_BITS;
        uint32_t bit = bit_index % xRTOS_BITMAP_WORD_BITS;
        bitmap->words[word] &= ~(1U << bit);
    }

    static inline bool xRTOS_Bitmap_Is_Set(const xRTOS_Bitmap_t *bitmap, uint32_t bit_index)
    {
        xASSERT(bitmap != NULL, "bitmap is NULL");
        xASSERT(bit_index < xRTOS_BITMAP_WIDTH, "bit_index out of range");

        uint32_t word = bit_index / xRTOS_BITMAP_WORD_BITS;
        uint32_t bit = bit_index % xRTOS_BITMAP_WORD_BITS;
        return (bitmap->words[word] & (1U << bit)) != 0U;
    }

    static inline bool xRTOS_Bitmap_Is_Empty(const xRTOS_Bitmap_t *bitmap)
    {
        xASSERT(bitmap != NULL, "bitmap is NULL");

        for (uint32_t w = 0U; w < xRTOS_BITMAP_WORD_COUNT; w++)
        {
            if ((bitmap->words[w] & xrtos_bitmap_word_mask(w)) != 0U)
            {
                return false;
            }
        }
        return true;
    }

    static inline void xRTOS_Bitmap_Reset(xRTOS_Bitmap_t *bitmap)
    {
        xASSERT(bitmap != NULL, "bitmap is NULL");

        for (uint32_t w = 0U; w < xRTOS_BITMAP_WORD_COUNT; w++)
        {
            bitmap->words[w] = 0U;
        }
    }

    // Finds the lowest set bit index in a bitmap. Scans from low to high word
    // index, then uses CTZ within the first nonzero word. Returns
    // xRETURN_xRTOS_OK and writes the bit index to *bit_index on success. Returns
    // xRETURN_xERR_xRTOS_INVALID_STATE if the bitmap is empty.
    static inline xRETURN_t xRTOS_Bitmap_Find_First_Set(const xRTOS_Bitmap_t *bitmap, uint32_t *bit_index)
    {
        xASSERT(bitmap != NULL, "bitmap is NULL");
        xASSERT(bit_index != NULL, "bit_index is NULL");

        for (uint32_t w = 0U; w < xRTOS_BITMAP_WORD_COUNT; w++)
        {
            uint32_t word_value = bitmap->words[w] & xrtos_bitmap_word_mask(w);

            if (word_value != 0U)
            {
                *bit_index = (w * xRTOS_BITMAP_WORD_BITS) + xrtos_bitmap_ctz32(word_value);
                return xRETURN_xRTOS_OK;
            }
        }
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
