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

// @file xtimer_config.h
// @brief Configuration structure definitions for xTIMER driver.
//

#ifndef XTIMER_CONFIG_H
#define XTIMER_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xtimer_defs.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

#ifndef xTIMER_TRACE_ENABLE
#define xTIMER_TRACE_ENABLE 1U
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct
    {
        uint32_t period_us;
        uint32_t module_clk_hz;
    } xTIMER_Config_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XTIMER_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
