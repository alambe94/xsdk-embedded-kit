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

// @file xspi_trace.h
// @brief xSPI trace event IDs and macros.
//

#ifndef XSPI_TRACE_H
#define XSPI_TRACE_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// MODULE INCLUDES
#include "xspi_config.h"
#include "xtrace_config.h"
#include "xtrace_registry.h"

#if xTRACE_ENABLE && xSPI_TRACE_ENABLE
#include "xtrace.h"
#endif

    // MACROS //////////////////////////////////////////////////////////////////////////

    // xSPI event code constants — offsets within xTRACE_BASE_xSPI block (0xE0 - 0xFF).

#define xSPI_TRACE_CODE_INIT                                                                                                               \
    (xTRACE_BASE_xSPI + 0x00U) /// @trace {"name": "SPI_INIT", "type": "instant", "track": "xSPI/Lifecycle", "args": ["default_clock_hz"]}
#define xSPI_TRACE_CODE_DEINIT                                                                                                             \
    (xTRACE_BASE_xSPI + 0x01U) /// @trace {"name": "SPI_DEINIT", "type": "instant", "track": "xSPI/Lifecycle", "args": ["unused"]}
#define xSPI_TRACE_CODE_START                                                                                                              \
    (xTRACE_BASE_xSPI + 0x02U) /// @trace {"name": "SPI_START", "type": "instant", "track": "xSPI/Lifecycle", "args": ["unused"]}
#define xSPI_TRACE_CODE_STOP                                                                                                               \
    (xTRACE_BASE_xSPI + 0x03U) /// @trace {"name": "SPI_STOP", "type": "instant", "track": "xSPI/Lifecycle", "args": ["unused"]}
#define xSPI_TRACE_CODE_TRANSFER_START                                                                                                     \
    (xTRACE_BASE_xSPI +                                                                                                                    \
     0x04U) /// @trace {"name": "SPI_TRANSFER_START", "type": "begin", "track": "xSPI/Transfer", "args": ["chip_select", "tx_length"]}
#define xSPI_TRACE_CODE_TRANSFER_DONE                                                                                                      \
    (xTRACE_BASE_xSPI +                                                                                                                    \
     0x05U) /// @trace {"name": "SPI_TRANSFER_DONE", "type": "end", "track": "xSPI/Transfer", "args": ["chip_select", "status"]}
#define xSPI_TRACE_CODE_CALLBACK                                                                                                           \
    (xTRACE_BASE_xSPI + 0x06U) /// @trace {"name": "SPI_CALLBACK", "type": "instant", "track": "xSPI/Callback", "args": ["event", "status"]}
#define xSPI_TRACE_CODE_ERROR                                                                                                              \
    (xTRACE_BASE_xSPI + 0x07U) /// @trace {"name": "SPI_ERROR", "type": "error", "track": "xSPI/Error", "args": ["chip_select", "status"]}

// Per-module emit macros — gated on both xTRACE_ENABLE and xSPI_TRACE_ENABLE.
#if xTRACE_ENABLE && xSPI_TRACE_ENABLE
    // clang-format off
#define xSPI_TRACE_E0(s, id)          xTRACE_E0((s)->trace_ctx, (id))
#define xSPI_TRACE_E1(s, id, a)       xTRACE_E1((s)->trace_ctx, (id), (uint32_t)(a))
#define xSPI_TRACE_E2(s, id, a, b)    xTRACE_E2((s)->trace_ctx, (id), (uint32_t)(a), (uint32_t)(b))
// clang-format on
#else
#define xSPI_TRACE_DISCARD_(v) ((void)(v))
#define xSPI_TRACE_E0(s, id)                                                                                                               \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xSPI_TRACE_DISCARD_(s);                                                                                                            \
        xSPI_TRACE_DISCARD_(id);                                                                                                           \
    } while (0)
#define xSPI_TRACE_E1(s, id, a)                                                                                                            \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xSPI_TRACE_DISCARD_(s);                                                                                                            \
        xSPI_TRACE_DISCARD_(id);                                                                                                           \
        xSPI_TRACE_DISCARD_(a);                                                                                                            \
    } while (0)
#define xSPI_TRACE_E2(s, id, a, b)                                                                                                         \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xSPI_TRACE_DISCARD_(s);                                                                                                            \
        xSPI_TRACE_DISCARD_(id);                                                                                                           \
        xSPI_TRACE_DISCARD_(a);                                                                                                            \
        xSPI_TRACE_DISCARD_(b);                                                                                                            \
    } while (0)
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSPI_TRACE_H
// EOF /////////////////////////////////////////////////////////////////////////////
