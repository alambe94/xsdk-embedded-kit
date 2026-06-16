// Copyright 2022 alambe94

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xusbh_trace.h
// @brief xUSB host trace event IDs (flat integers, LEB128 wire encoding).

#ifndef XUSBH_TRACE_H
#define XUSBH_TRACE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // MODULE INCLUDES
#include "xtrace_config.h"
#include "xtrace_registry.h"

#ifndef xUSBH_TRACE_ENABLE
#define xUSBH_TRACE_ENABLE xTRACE_ENABLE
#endif

#if xTRACE_ENABLE && xUSBH_TRACE_ENABLE
#include "xtrace.h"
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

#define xUSBH_TRACE_CODE_PORT_CONNECT    (xTRACE_BASE_xUSBH + 0x00U) /// @trace {"type": "instant", "track": "xUSBH/Port", "args": ["port"]}
#define xUSBH_TRACE_CODE_PORT_DISCONNECT (xTRACE_BASE_xUSBH + 0x01U) /// @trace {"type": "instant", "track": "xUSBH/Port", "args": ["port"]}
#define xUSBH_TRACE_CODE_PORT_RESET      (xTRACE_BASE_xUSBH + 0x02U) /// @trace {"type": "instant", "track": "xUSBH/Port", "args": ["port"]}
#define xUSBH_TRACE_CODE_ENUM_STATE                                                                                                        \
    (xTRACE_BASE_xUSBH + 0x03U) /// @trace {"type": "instant", "track": "xUSBH/Enumeration", "args": ["port", "state"]}
#define xUSBH_TRACE_CODE_ENUM_ERROR                                                                                                        \
    (xTRACE_BASE_xUSBH + 0x04U) /// @trace {"type": "error", "track": "xUSBH/Enumeration", "args": ["state", "status"]}
#define xUSBH_TRACE_CODE_TRANSFER_SUBMIT                                                                                                   \
    (xTRACE_BASE_xUSBH + 0x05U) /// @trace {"type": "instant", "track": "xUSBH/Transfer", "args": ["endpoint", "length"]}
#define xUSBH_TRACE_CODE_TRANSFER_COMPLETE                                                                                                 \
    (xTRACE_BASE_xUSBH + 0x06U) /// @trace {"type": "instant", "track": "xUSBH/Transfer", "args": ["endpoint", "event", "actual_length"]}
#define xUSBH_TRACE_CODE_CLASS_BIND                                                                                                        \
    (xTRACE_BASE_xUSBH + 0x07U) /// @trace {"type": "instant", "track": "xUSBH/Class", "args": ["interface_number"]}
#define xUSBH_TRACE_CODE_HCD_ERROR (xTRACE_BASE_xUSBH + 0x08U) /// @trace {"type": "error", "track": "xUSBH/HCD", "args": ["status"]}

#if xTRACE_ENABLE && xUSBH_TRACE_ENABLE
#define xUSBH_TRACE_E1(host_ctx, code, arg)        xTRACE_E1((host_ctx)->trace_ctx, (code), (uint32_t)(arg))
#define xUSBH_TRACE_E2(host_ctx, code, arg0, arg1) xTRACE_E2((host_ctx)->trace_ctx, (code), (uint32_t)(arg0), (uint32_t)(arg1))
#define xUSBH_TRACE_E3(host_ctx, code, arg0, arg1, arg2)                                                                                   \
    xTRACE_E3((host_ctx)->trace_ctx, (code), (uint32_t)(arg0), (uint32_t)(arg1), (uint32_t)(arg2))
#else
#define xUSBH_TRACE_DISCARD_(value) ((void)sizeof(value))
#define xUSBH_TRACE_E1(host_ctx, code, arg)                                                                                                \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xUSBH_TRACE_DISCARD_(host_ctx);                                                                                                    \
        xUSBH_TRACE_DISCARD_(code);                                                                                                        \
        xUSBH_TRACE_DISCARD_(arg);                                                                                                         \
    } while (0)
#define xUSBH_TRACE_E2(host_ctx, code, arg0, arg1)                                                                                         \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xUSBH_TRACE_DISCARD_(host_ctx);                                                                                                    \
        xUSBH_TRACE_DISCARD_(code);                                                                                                        \
        xUSBH_TRACE_DISCARD_(arg0);                                                                                                        \
        xUSBH_TRACE_DISCARD_(arg1);                                                                                                        \
    } while (0)
#define xUSBH_TRACE_E3(host_ctx, code, arg0, arg1, arg2)                                                                                   \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xUSBH_TRACE_DISCARD_(host_ctx);                                                                                                    \
        xUSBH_TRACE_DISCARD_(code);                                                                                                        \
        xUSBH_TRACE_DISCARD_(arg0);                                                                                                        \
        xUSBH_TRACE_DISCARD_(arg1);                                                                                                        \
        xUSBH_TRACE_DISCARD_(arg2);                                                                                                        \
    } while (0)
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUSBH_TRACE_H
// EOF /////////////////////////////////////////////////////////////////////////////
