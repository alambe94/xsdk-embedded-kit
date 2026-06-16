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

// @file xnet_timer.h
// @brief xNET passive timer and timeout processing helpers.
//

#ifndef XNET_TIMER_H
#define XNET_TIMER_H

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
#include "xnet_defs.h"
#include "xnet_core.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xNET_TIME_BEFORE(a, b) (((int32_t)((a) - (b))) < 0)
#define xNET_TIME_AFTER(a, b)  (((int32_t)((a) - (b))) > 0)

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    static inline bool xNET_Time_Before(uint32_t a, uint32_t b)
    {
        return ((int32_t)(a - b)) < 0;
    }

    static inline bool xNET_Time_After(uint32_t a, uint32_t b)
    {
        return ((int32_t)(a - b)) > 0;
    }

    static inline bool xNET_Time_Before_Or_Equal(uint32_t a, uint32_t b)
    {
        return ((int32_t)(a - b)) <= 0;
    }

    static inline bool xNET_Time_After_Or_Equal(uint32_t a, uint32_t b)
    {
        return ((int32_t)(a - b)) >= 0;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    /**
     * @brief Passive tick update for the xNET stack.
     *
     * Advances the internal system tick counter and triggers passive ticks
     * on all registered interfaces (e.g. ARP caches).
     *
     * @param net_ctx Pointer to the xNET context.
     * @param elapsed_ms Number of milliseconds elapsed since the last call.
     * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
     */
    xRETURN_t xNET_Tick(xNET_Context_t *net_ctx, uint32_t elapsed_ms);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_TIMER_H
// EOF /////////////////////////////////////////////////////////////////////////////
