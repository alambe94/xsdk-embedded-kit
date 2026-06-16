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

// @file xsdk_soc_mmr.h
// @brief AM243x MAIN_CTRL_MMR partition lock/unlock.
//
// MAIN_CTRL_MMR0 base: 0x43000000 (CSL_CTRL_MMR0_CFG0_BASE)
// Partition n KICK0 offset: 0x1008 + 0x4000*n
// Partition n KICK1 offset: 0x100C + 0x4000*n
//

#ifndef XSDK_SOC_MMR_H
#define XSDK_SOC_MMR_H

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
    #define XSDK_AM243X_MAIN_CTRL_MMR_BASE  (0x43000000UL)

    #define XSDK_AM243X_MMR_KICK0_OFF(n)    (0x1008UL + (0x4000UL * (uint32_t)(n)))
    #define XSDK_AM243X_MMR_KICK1_OFF(n)    (0x100CUL + (0x4000UL * (uint32_t)(n)))

    #define XSDK_AM243X_MMR_KICK0_UNLOCK    (0x68EF3490UL)
    #define XSDK_AM243X_MMR_KICK1_UNLOCK    (0xD172BC5AUL)
    #define XSDK_AM243X_MMR_KICK_LOCK       (0x00000000UL)

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    static inline void xsdk_soc_mmr_unlock_main(uint32_t partition)
    {
        volatile uint32_t *k0 = (volatile uint32_t *)(XSDK_AM243X_MAIN_CTRL_MMR_BASE +
                                                       XSDK_AM243X_MMR_KICK0_OFF(partition));
        volatile uint32_t *k1 = (volatile uint32_t *)(XSDK_AM243X_MAIN_CTRL_MMR_BASE +
                                                       XSDK_AM243X_MMR_KICK1_OFF(partition));
        *k0 = XSDK_AM243X_MMR_KICK0_UNLOCK;
        *k1 = XSDK_AM243X_MMR_KICK1_UNLOCK;
    }

    static inline void xsdk_soc_mmr_lock_main(uint32_t partition)
    {
        volatile uint32_t *k0 = (volatile uint32_t *)(XSDK_AM243X_MAIN_CTRL_MMR_BASE +
                                                       XSDK_AM243X_MMR_KICK0_OFF(partition));
        *k0 = XSDK_AM243X_MMR_KICK_LOCK;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSDK_SOC_MMR_H
// EOF /////////////////////////////////////////////////////////////////////////////
