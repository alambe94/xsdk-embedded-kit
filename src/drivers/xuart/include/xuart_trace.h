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

// @file xuart_trace.h
// @brief xUART trace event IDs (flat integers, LEB128 wire encoding).
//
// xUART event IDs are allocated from xTRACE_BASE_xUART in xtrace_registry.h.
// Per-call-site macros add the per-module enable gate; set xUART_TRACE_ENABLE=0
// in the build to strip all xUART trace calls at compile time.
//
// Lifecycle events (INIT, DEINIT, START, STOP) carry a single arg.
// Transfer events (TX_START, TX_DONE, RX_START, RX_DONE) form begin/end pairs
// for both blocking and async paths: async TX_DONE / RX_DONE fires from
// core_event_sink via the EVENT record when TX_COMPLETE / RX_COMPLETE arrives.
//

#ifndef XUART_TRACE_H
#define XUART_TRACE_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// MODULE INCLUDES
#include "xuart_config.h"
#include "xtrace_config.h"
#include "xtrace_registry.h"

#if xTRACE_ENABLE && xUART_TRACE_ENABLE
#include "xtrace.h"
#endif

    // MACROS //////////////////////////////////////////////////////////////////////////

    // xUART event code constants — offsets within xTRACE_BASE_xUART block (0xA0 - 0xBF).

#define xUART_TRACE_CODE_INIT     (xTRACE_BASE_xUART + 0x00U) /// @trace {"type": "instant", "track": "xUART/Lifecycle", "args": ["baud_rate"]}
#define xUART_TRACE_CODE_DEINIT   (xTRACE_BASE_xUART + 0x01U) /// @trace {"type": "instant", "track": "xUART/Lifecycle", "args": ["port"]}
#define xUART_TRACE_CODE_START    (xTRACE_BASE_xUART + 0x02U) /// @trace {"type": "instant", "track": "xUART/Lifecycle", "args": ["port"]}
#define xUART_TRACE_CODE_STOP     (xTRACE_BASE_xUART + 0x03U) /// @trace {"type": "instant", "track": "xUART/Lifecycle", "args": ["port"]}
#define xUART_TRACE_CODE_TX_START (xTRACE_BASE_xUART + 0x04U) /// @trace {"type": "begin", "track": "xUART/TX", "args": ["port", "length"]}
#define xUART_TRACE_CODE_TX_DONE  (xTRACE_BASE_xUART + 0x05U) /// @trace {"type": "end", "track": "xUART/TX", "args": ["port", "status"]}
#define xUART_TRACE_CODE_RX_START (xTRACE_BASE_xUART + 0x06U) /// @trace {"type": "begin", "track": "xUART/RX", "args": ["port", "length"]}
#define xUART_TRACE_CODE_RX_DONE  (xTRACE_BASE_xUART + 0x07U) /// @trace {"type": "end", "track": "xUART/RX", "args": ["port", "status"]}
#define xUART_TRACE_CODE_TX_COMPLETE                                                                                                       \
    (xTRACE_BASE_xUART + 0x08U) /// @trace {"type": "end", "track": "xUART/TX", "args": ["port", "bytes_transferred"]}
#define xUART_TRACE_CODE_TX_ABORTED                                                                                                        \
    (xTRACE_BASE_xUART + 0x09U) /// @trace {"type": "end", "track": "xUART/TX", "args": ["port", "bytes_transferred"]}
#define xUART_TRACE_CODE_TX_TIMEOUT                                                                                                        \
    (xTRACE_BASE_xUART + 0x0AU) /// @trace {"type": "end", "track": "xUART/TX", "args": ["port", "bytes_transferred"]}
#define xUART_TRACE_CODE_RX_COMPLETE                                                                                                       \
    (xTRACE_BASE_xUART + 0x0BU) /// @trace {"type": "end", "track": "xUART/RX", "args": ["port", "bytes_transferred"]}
#define xUART_TRACE_CODE_RX_ABORTED                                                                                                        \
    (xTRACE_BASE_xUART + 0x0CU) /// @trace {"type": "end", "track": "xUART/RX", "args": ["port", "bytes_transferred"]}
#define xUART_TRACE_CODE_RX_TIMEOUT                                                                                                        \
    (xTRACE_BASE_xUART + 0x0DU) /// @trace {"type": "end", "track": "xUART/RX", "args": ["port", "bytes_transferred"]}
#define xUART_TRACE_CODE_RX_OVERRUN (xTRACE_BASE_xUART + 0x0EU) /// @trace {"type": "error", "track": "xUART/RX", "args": ["port"]}
#define xUART_TRACE_CODE_RX_FRAMING (xTRACE_BASE_xUART + 0x0FU) /// @trace {"type": "error", "track": "xUART/RX", "args": ["port"]}
#define xUART_TRACE_CODE_RX_PARITY  (xTRACE_BASE_xUART + 0x10U) /// @trace {"type": "error", "track": "xUART/RX", "args": ["port"]}
#define xUART_TRACE_CODE_ABORT_TX   (xTRACE_BASE_xUART + 0x11U) /// @trace {"type": "instant", "track": "xUART/TX", "args": ["port"]}
#define xUART_TRACE_CODE_ABORT_RX   (xTRACE_BASE_xUART + 0x12U) /// @trace {"type": "instant", "track": "xUART/RX", "args": ["port"]}
#define xUART_TRACE_CODE_ERROR      (xTRACE_BASE_xUART + 0x13U) /// @trace {"type": "error", "track": "xUART/Error", "args": ["port", "status"]}

// Per-module emit macros — gated on both xTRACE_ENABLE and xUART_TRACE_ENABLE.
#if xTRACE_ENABLE && xUART_TRACE_ENABLE
    // clang-format off
#define xUART_TRACE_E0(u, id)          xTRACE_E0((u)->trace_ctx, (id))
#define xUART_TRACE_E1(u, id, a)       xTRACE_E1((u)->trace_ctx, (id), (uint32_t)(a))
#define xUART_TRACE_E2(u, id, a, b)    xTRACE_E2((u)->trace_ctx, (id), (uint32_t)(a), (uint32_t)(b))
// clang-format on
#else
#define xUART_TRACE_DISCARD_(v) ((void)sizeof(v))
#define xUART_TRACE_E0(u, id)                                                                                                              \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xUART_TRACE_DISCARD_(u);                                                                                                           \
        xUART_TRACE_DISCARD_(id);                                                                                                          \
    } while (0)
#define xUART_TRACE_E1(u, id, a)                                                                                                           \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xUART_TRACE_DISCARD_(u);                                                                                                           \
        xUART_TRACE_DISCARD_(id);                                                                                                          \
        xUART_TRACE_DISCARD_(a);                                                                                                           \
    } while (0)
#define xUART_TRACE_E2(u, id, a, b)                                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xUART_TRACE_DISCARD_(u);                                                                                                           \
        xUART_TRACE_DISCARD_(id);                                                                                                          \
        xUART_TRACE_DISCARD_(a);                                                                                                           \
        xUART_TRACE_DISCARD_(b);                                                                                                           \
    } while (0)
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUART_TRACE_H
// EOF /////////////////////////////////////////////////////////////////////////////
