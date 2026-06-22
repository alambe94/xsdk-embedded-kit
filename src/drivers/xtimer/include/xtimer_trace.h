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

// @file xtimer_trace.h
// @brief xTIMER trace event IDs and macros.
//

#ifndef XTIMER_TRACE_H
#define XTIMER_TRACE_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// MODULE INCLUDES
#include "xtimer_config.h"
#include "xtrace_config.h"
#include "xtrace_registry.h"

#if xTRACE_ENABLE && xTIMER_TRACE_ENABLE
#include "xtrace.h"
#endif

    // MACROS //////////////////////////////////////////////////////////////////////////

    // xTIMER event code constants — offsets within xTRACE_BASE_xTIMER block (0x120 - 0x13F).

#define xTIMER_TRACE_CODE_INIT                                                                                                             \
    (xTRACE_BASE_xTIMER +                                                                                                                  \
     0x00U) /// @trace {"name": "TIMER_INIT", "type": "instant", "track": "xTIMER/Lifecycle", "args": ["period_us", "module_clk_hz"]}
#define xTIMER_TRACE_CODE_DEINIT                                                                                                           \
    (xTRACE_BASE_xTIMER + 0x01U) /// @trace {"name": "TIMER_DEINIT", "type": "instant", "track": "xTIMER/Lifecycle", "args": ["unused"]}
#define xTIMER_TRACE_CODE_START                                                                                                            \
    (xTRACE_BASE_xTIMER + 0x02U) /// @trace {"name": "TIMER_START", "type": "instant", "track": "xTIMER/Lifecycle", "args": ["unused"]}
#define xTIMER_TRACE_CODE_STOP                                                                                                             \
    (xTRACE_BASE_xTIMER + 0x03U) /// @trace {"name": "TIMER_STOP", "type": "instant", "track": "xTIMER/Lifecycle", "args": ["unused"]}
#define xTIMER_TRACE_CODE_GET_COUNT                                                                                                        \
    (xTRACE_BASE_xTIMER + 0x04U) /// @trace {"name": "TIMER_GET_COUNT", "type": "instant", "track": "xTIMER/Lifecycle", "args": ["count"]}
#define xTIMER_TRACE_CODE_CLEAR_IRQ                                                                                                        \
    (xTRACE_BASE_xTIMER + 0x05U) /// @trace {"name": "TIMER_CLEAR_IRQ", "type": "instant", "track": "xTIMER/Lifecycle", "args": ["unused"]}
#define xTIMER_TRACE_CODE_CALLBACK                                                                                                         \
    (xTRACE_BASE_xTIMER + 0x06U) /// @trace {"name": "TIMER_CALLBACK", "type": "instant", "track": "xTIMER/Callback", "args": ["unused"]}

// Per-module emit macros — gated on both xTRACE_ENABLE and xTIMER_TRACE_ENABLE.
#if xTRACE_ENABLE && xTIMER_TRACE_ENABLE
    // clang-format off
#define xTIMER_TRACE_E0(t, id)          xTRACE_E0((t)->trace_ctx, (id))
#define xTIMER_TRACE_E1(t, id, a)       xTRACE_E1((t)->trace_ctx, (id), (uint32_t)(a))
#define xTIMER_TRACE_E2(t, id, a, b)    xTRACE_E2((t)->trace_ctx, (id), (uint32_t)(a), (uint32_t)(b))
// clang-format on
#else
#define xTIMER_TRACE_DISCARD_(v) ((void)(v))
#define xTIMER_TRACE_E0(t, id)                                                                                                             \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xTIMER_TRACE_DISCARD_(t);                                                                                                          \
        xTIMER_TRACE_DISCARD_(id);                                                                                                         \
    } while (0)
#define xTIMER_TRACE_E1(t, id, a)                                                                                                          \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xTIMER_TRACE_DISCARD_(t);                                                                                                          \
        xTIMER_TRACE_DISCARD_(id);                                                                                                         \
        xTIMER_TRACE_DISCARD_(a);                                                                                                          \
    } while (0)
#define xTIMER_TRACE_E2(t, id, a, b)                                                                                                       \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xTIMER_TRACE_DISCARD_(t);                                                                                                          \
        xTIMER_TRACE_DISCARD_(id);                                                                                                         \
        xTIMER_TRACE_DISCARD_(a);                                                                                                          \
        xTIMER_TRACE_DISCARD_(b);                                                                                                          \
    } while (0)
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XTIMER_TRACE_H
// EOF /////////////////////////////////////////////////////////////////////////////
