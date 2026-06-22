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

// @file xi2c_trace.h
// @brief xI2C trace event IDs and macros.
//

#ifndef XI2C_TRACE_H
#define XI2C_TRACE_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// MODULE INCLUDES
#include "xi2c_config.h"
#include "xtrace_config.h"
#include "xtrace_registry.h"

#if xTRACE_ENABLE && xI2C_TRACE_ENABLE
#include "xtrace.h"
#endif

    // MACROS //////////////////////////////////////////////////////////////////////////

    // xI2C event code constants — offsets within xTRACE_BASE_xI2C block (0xC0 - 0xDF).

#define xI2C_TRACE_CODE_INIT                                                                                                               \
    (xTRACE_BASE_xI2C + 0x00U) /// @trace {"name": "I2C_INIT", "type": "instant", "track": "xI2C/Lifecycle", "args": ["bitrate_hz"]}
#define xI2C_TRACE_CODE_DEINIT                                                                                                             \
    (xTRACE_BASE_xI2C + 0x01U) /// @trace {"name": "I2C_DEINIT", "type": "instant", "track": "xI2C/Lifecycle", "args": ["unused"]}
#define xI2C_TRACE_CODE_START                                                                                                              \
    (xTRACE_BASE_xI2C + 0x02U) /// @trace {"name": "I2C_START", "type": "instant", "track": "xI2C/Lifecycle", "args": ["unused"]}
#define xI2C_TRACE_CODE_STOP                                                                                                               \
    (xTRACE_BASE_xI2C + 0x03U) /// @trace {"name": "I2C_STOP", "type": "instant", "track": "xI2C/Lifecycle", "args": ["unused"]}
#define xI2C_TRACE_CODE_WRITE_START                                                                                                        \
    (xTRACE_BASE_xI2C +                                                                                                                    \
     0x04U) /// @trace {"name": "I2C_WRITE_START", "type": "begin", "track": "xI2C/Transfer", "args": ["device_address", "tx_length"]}
#define xI2C_TRACE_CODE_WRITE_DONE                                                                                                         \
    (xTRACE_BASE_xI2C +                                                                                                                    \
     0x05U) /// @trace {"name": "I2C_WRITE_DONE", "type": "end", "track": "xI2C/Transfer", "args": ["device_address", "status"]}
#define xI2C_TRACE_CODE_READ_START                                                                                                         \
    (xTRACE_BASE_xI2C +                                                                                                                    \
     0x06U) /// @trace {"name": "I2C_READ_START", "type": "begin", "track": "xI2C/Transfer", "args": ["device_address", "rx_length"]}
#define xI2C_TRACE_CODE_READ_DONE                                                                                                          \
    (xTRACE_BASE_xI2C +                                                                                                                    \
     0x07U) /// @trace {"name": "I2C_READ_DONE", "type": "end", "track": "xI2C/Transfer", "args": ["device_address", "status"]}
#define xI2C_TRACE_CODE_WRITE_READ_START                                                                                                   \
    (xTRACE_BASE_xI2C +                                                                                                                    \
     0x08U) /// @trace {"name": "I2C_WRITE_READ_START", "type": "begin", "track": "xI2C/Transfer", "args": ["device_address", "tx_length"]}
#define xI2C_TRACE_CODE_WRITE_READ_DONE                                                                                                    \
    (xTRACE_BASE_xI2C +                                                                                                                    \
     0x09U) /// @trace {"name": "I2C_WRITE_READ_DONE", "type": "end", "track": "xI2C/Transfer", "args": ["device_address", "status"]}
#define xI2C_TRACE_CODE_ASYNC_START                                                                                                        \
    (xTRACE_BASE_xI2C +                                                                                                                    \
     0x0AU) /// @trace {"name": "I2C_ASYNC_START", "type": "begin", "track": "xI2C/Transfer", "args": ["device_address", "tx_length"]}
#define xI2C_TRACE_CODE_CALLBACK                                                                                                           \
    (xTRACE_BASE_xI2C + 0x0BU) /// @trace {"name": "I2C_CALLBACK", "type": "instant", "track": "xI2C/Callback", "args": ["event", "status"]}
#define xI2C_TRACE_CODE_ERROR                                                                                                              \
    (xTRACE_BASE_xI2C +                                                                                                                    \
     0x0CU) /// @trace {"name": "I2C_ERROR", "type": "error", "track": "xI2C/Error", "args": ["device_address", "status"]}

// Per-module emit macros — gated on both xTRACE_ENABLE and xI2C_TRACE_ENABLE.
#if xTRACE_ENABLE && xI2C_TRACE_ENABLE
    // clang-format off
#define xI2C_TRACE_E0(i, id)          xTRACE_E0((i)->trace_ctx, (id))
#define xI2C_TRACE_E1(i, id, a)       xTRACE_E1((i)->trace_ctx, (id), (uint32_t)(a))
#define xI2C_TRACE_E2(i, id, a, b)    xTRACE_E2((i)->trace_ctx, (id), (uint32_t)(a), (uint32_t)(b))
// clang-format on
#else
#define xI2C_TRACE_DISCARD_(v) ((void)(v))
#define xI2C_TRACE_E0(i, id)                                                                                                               \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xI2C_TRACE_DISCARD_(i);                                                                                                            \
        xI2C_TRACE_DISCARD_(id);                                                                                                           \
    } while (0)
#define xI2C_TRACE_E1(i, id, a)                                                                                                            \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xI2C_TRACE_DISCARD_(i);                                                                                                            \
        xI2C_TRACE_DISCARD_(id);                                                                                                           \
        xI2C_TRACE_DISCARD_(a);                                                                                                            \
    } while (0)
#define xI2C_TRACE_E2(i, id, a, b)                                                                                                         \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xI2C_TRACE_DISCARD_(i);                                                                                                            \
        xI2C_TRACE_DISCARD_(id);                                                                                                           \
        xI2C_TRACE_DISCARD_(a);                                                                                                            \
        xI2C_TRACE_DISCARD_(b);                                                                                                            \
    } while (0)
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XI2C_TRACE_H
// EOF /////////////////////////////////////////////////////////////////////////////
