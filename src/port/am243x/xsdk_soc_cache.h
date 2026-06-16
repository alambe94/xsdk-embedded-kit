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

// @file xsdk_soc_cache.h
// @brief AM243x Cortex-R5 L1 Cache configuration helper.
//
// Provides functions to enable Instruction and Data caches on R5F cores.
//

#ifndef XSDK_SOC_CACHE_H
#define XSDK_SOC_CACHE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
    #include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    static inline void xsdk_soc_cache_enable(void)
    {
        uint32_t r;
        __asm volatile(
            // Step 1: disable I+D caches before touching them
            "mrc p15, 0, %0, c1, c0, 0\n"
            "bic %0, %0, #0x1000\n" // clear I bit
            "bic %0, %0, #0x0004\n" // clear C bit
            "mcr p15, 0, %0, c1, c0, 0\n"
            "dsb\n"
            "isb\n"

            // Step 2: enable ECC on L1 cache (Cortex-R5 ACTLR bits 3 and 5)
            "mrc p15, 0, %0, c1, c0, 1\n" // read ACTLR
            "orr %0, %0, #(1 << 3)\n"
            "orr %0, %0, #(1 << 5)\n"
            "mcr p15, 0, %0, c1, c0, 1\n" // write ACTLR

            // Step 3: invalidate D-cache (Cortex-R5: c15 c5 0 = flush entire D-cache)
            "mcr p15, 0, %0, c15, c5, 0\n"
            // Step 4: invalidate I-cache (ICIALLU: c7 c5 0)
            "mcr p15, 0, %0, c7,  c5, 0\n"
            "dsb\n"
            "isb\n"

            // Step 5: re-enable I+D caches
            "mrc p15, 0, %0, c1, c0, 0\n"
            "orr %0, %0, #0x1000\n" // set I bit
            "orr %0, %0, #0x0004\n" // set C bit
            "mcr p15, 0, %0, c1, c0, 0\n"
            "dsb\n"
            "isb\n"
            : "=r"(r)
            :
            : "memory");
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSDK_SOC_CACHE_H
// EOF /////////////////////////////////////////////////////////////////////////////
