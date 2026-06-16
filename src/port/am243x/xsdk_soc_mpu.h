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

// @file xsdk_soc_mpu.h
// @brief AM243x Cortex-R5 MPU configuration helper.
//
// MPU configuration is critical to define correct caching, buffering, and
// execute permissions for memory zones (TCM, SRAM, Flash, Peripherals).
//

#ifndef XSDK_SOC_MPU_H
#define XSDK_SOC_MPU_H

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

    static inline void xsdk_soc_mpu_set_region(uint32_t region, uint32_t base_addr, uint32_t size_encoding, uint32_t access_control)
    {
        __asm volatile(
            "mcr p15, 0, %0, c6, c2, 0\n" // Write Region Number (RGNR)
            "mcr p15, 0, %1, c6, c1, 0\n" // Write Base Address (DRBAR)
            "mcr p15, 0, %2, c6, c1, 2\n" // Write Size and Enable (DRSR)
            "mcr p15, 0, %3, c6, c1, 4\n" // Write Access Control (DRACR)
            :
            : "r"(region), "r"(base_addr), "r"(size_encoding), "r"(access_control)
            : "memory");
    }

    static inline void xsdk_soc_mpu_disable(void)
    {
        uint32_t sctlr;
        __asm volatile(
            "mrc p15, 0, %0, c1, c0, 0\n"
            "bic %0, %0, #1\n" // Clear M bit (disable MPU)
            "mcr p15, 0, %0, c1, c0, 0\n"
            "dsb\n"
            "isb\n"
            : "=r"(sctlr)
            :
            : "memory");
    }

    static inline void xsdk_soc_mpu_enable(void)
    {
        uint32_t sctlr;
        __asm volatile(
            "mrc p15, 0, %0, c1, c0, 0\n"
            "orr %0, %0, #1\n" // Set M bit (enable MPU)
            "mcr p15, 0, %0, c1, c0, 0\n"
            "dsb\n"
            "isb\n"
            : "=r"(sctlr)
            :
            : "memory");
    }

    static inline void xsdk_soc_mpu_init(void)
    {
        // ARM TRM: modifying MPU regions while MPU is enabled is UNPREDICTABLE.
        // Disable first, program all regions, then re-enable.
        xsdk_soc_mpu_disable();

        // MPU regions configured according to AM243x standard layout:
        //
        // Region 0: 2GB Guard, Base 0x00000000
        // Size: 2GB (30U << 1 | 1U)
        // Access: Supervisor RW / User Read (AP=2), Sharable (S=1), Execute-Never (XN=1)
        // Value: 0x1204U (XN=1, AP=2, S=1)
        //
        xsdk_soc_mpu_set_region(0, 0x00000000U, (30U << 1) | 1U, 0x1204U);

        // Region 1: 32KB TCMA (vectors/cacheable code), Base 0x00000000
        // Size: 32KB (14U << 1 | 1U)
        // Access: Supervisor RW / User Read (AP=2), Normal Cacheable (TEX=1, C=1, B=1)
        // Value: 0x020BU (AP=2, TEX=1, C=1, B=1)
        //
        xsdk_soc_mpu_set_region(1, 0x00000000U, (14U << 1) | 1U, 0x020BU);

        // Region 2: 32KB TCMB0 (stack/cacheable data), Base 0x41010000
        // Size: 32KB (14U << 1 | 1U)
        // Access: Same as Region 1
        //
        xsdk_soc_mpu_set_region(2, 0x41010000U, (14U << 1) | 1U, 0x020BU);

        // Region 3: 2MB MSRAM (program and data), Base 0x70000000
        // Size: 2MB (20U << 1 | 1U)
        // Access: Same as Region 1
        //
        xsdk_soc_mpu_set_region(3, 0x70000000U, (20U << 1) | 1U, 0x020BU);

        // Region 4: 256MB OSPI Flash (execution in place), Base 0x60000000
        // Size: 256MB (27U << 1 | 1U)
        // Access: All Read-Only (AP=6), Normal Cacheable (TEX=1, C=1, B=1)
        // Value: 0x060BU (AP=6, TEX=1, C=1, B=1)
        //
        xsdk_soc_mpu_set_region(4, 0x60000000U, (27U << 1) | 1U, 0x060BU);

        // Region 5: 32MB MAIN peripherals (UART0@0x02800000, TIMER8@0x02480000), Base 0x02000000
        // Size: 32MB (24U << 1 | 1U)
        // Access: Full RW (AP=3), Device non-shareable (TEX=2, C=0, B=1), Execute-Never (XN=1)
        // Value: 0x1311U - prevents cached MMIO reads (LSR polling) returning stale data.
        // Region 0 (guard) would otherwise cover these with Normal Strongly-Ordered which is
        // correct for ordering but still wrong for AP - this region overrides it with AP=3.
        xsdk_soc_mpu_set_region(5, 0x02000000U, (24U << 1) | 1U, 0x1311U);

        // Region 6: 32KB VIM (R5FSS0-0 VIM registers), Base 0x2FFF0000
        // Size: 32KB (14U << 1 | 1U)
        // Access: Same as Region 5 (Device non-shareable, full RW, XN)
        // Covers VIM register space up to offset 0x2800 (INT_VEC for 256 interrupts).
        xsdk_soc_mpu_set_region(6, 0x2FFF0000U, (14U << 1) | 1U, 0x1311U);

        xsdk_soc_mpu_enable();
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSDK_SOC_MPU_H
// EOF /////////////////////////////////////////////////////////////////////////////
