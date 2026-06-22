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

// @file xgpio_trace.h
// @brief xGPIO trace event IDs and macros.
//

#ifndef XGPIO_TRACE_H
#define XGPIO_TRACE_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// MODULE INCLUDES
#include "xgpio_config.h"
#include "xtrace_config.h"
#include "xtrace_registry.h"

#if xTRACE_ENABLE && xGPIO_TRACE_ENABLE
#include "xtrace.h"
#endif

    // MACROS //////////////////////////////////////////////////////////////////////////

    // xGPIO event code constants — offsets within xTRACE_BASE_xGPIO block (0x100 - 0x11F).

#define xGPIO_TRACE_CODE_INIT                                                                                                              \
    (xTRACE_BASE_xGPIO + 0x00U) /// @trace {"name": "GPIO_INIT", "type": "instant", "track": "xGPIO/Lifecycle", "args": ["unused"]}
#define xGPIO_TRACE_CODE_DEINIT                                                                                                            \
    (xTRACE_BASE_xGPIO + 0x01U) /// @trace {"name": "GPIO_DEINIT", "type": "instant", "track": "xGPIO/Lifecycle", "args": ["unused"]}
#define xGPIO_TRACE_CODE_CONFIGURE_PIN                                                                                                     \
    (xTRACE_BASE_xGPIO + 0x02U) /// @trace {"name": "GPIO_CONFIGURE_PIN", "type": "instant", "track": "xGPIO/Pin", "args": ["pin", "mode"]}
#define xGPIO_TRACE_CODE_PIN_WRITE                                                                                                         \
    (xTRACE_BASE_xGPIO + 0x03U) /// @trace {"name": "GPIO_PIN_WRITE", "type": "instant", "track": "xGPIO/Pin", "args": ["pin", "level"]}
#define xGPIO_TRACE_CODE_PIN_READ                                                                                                          \
    (xTRACE_BASE_xGPIO + 0x04U) /// @trace {"name": "GPIO_PIN_READ", "type": "instant", "track": "xGPIO/Pin", "args": ["pin", "level"]}
#define xGPIO_TRACE_CODE_PIN_TOGGLE                                                                                                        \
    (xTRACE_BASE_xGPIO + 0x05U) /// @trace {"name": "GPIO_PIN_TOGGLE", "type": "instant", "track": "xGPIO/Pin", "args": ["pin"]}
#define xGPIO_TRACE_CODE_CALLBACK                                                                                                          \
    (xTRACE_BASE_xGPIO + 0x06U) /// @trace {"name": "GPIO_CALLBACK", "type": "instant", "track": "xGPIO/Callback", "args": ["pin"]}

// Per-module emit macros — gated on both xTRACE_ENABLE and xGPIO_TRACE_ENABLE.
#if xTRACE_ENABLE && xGPIO_TRACE_ENABLE
    // clang-format off
#define xGPIO_TRACE_E0(g, id)          xTRACE_E0((g)->trace_ctx, (id))
#define xGPIO_TRACE_E1(g, id, a)       xTRACE_E1((g)->trace_ctx, (id), (uint32_t)(a))
#define xGPIO_TRACE_E2(g, id, a, b)    xTRACE_E2((g)->trace_ctx, (id), (uint32_t)(a), (uint32_t)(b))
// clang-format on
#else
#define xGPIO_TRACE_DISCARD_(v) ((void)(v))
#define xGPIO_TRACE_E0(g, id)                                                                                                              \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xGPIO_TRACE_DISCARD_(g);                                                                                                           \
        xGPIO_TRACE_DISCARD_(id);                                                                                                          \
    } while (0)
#define xGPIO_TRACE_E1(g, id, a)                                                                                                           \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xGPIO_TRACE_DISCARD_(g);                                                                                                           \
        xGPIO_TRACE_DISCARD_(id);                                                                                                          \
        xGPIO_TRACE_DISCARD_(a);                                                                                                           \
    } while (0)
#define xGPIO_TRACE_E2(g, id, a, b)                                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xGPIO_TRACE_DISCARD_(g);                                                                                                           \
        xGPIO_TRACE_DISCARD_(id);                                                                                                          \
        xGPIO_TRACE_DISCARD_(a);                                                                                                           \
        xGPIO_TRACE_DISCARD_(b);                                                                                                           \
    } while (0)
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XGPIO_TRACE_H
// EOF /////////////////////////////////////////////////////////////////////////////
