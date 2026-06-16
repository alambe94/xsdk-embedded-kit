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

// @file xusbd_trace.h
// @brief xUSB device trace event IDs (flat integers, LEB128 wire encoding).
//
// xUSBD event IDs are allocated from xTRACE_BASE_xUSB in xtrace_registry.h.
// This header defines the dictionary-visible event IDs only. Runtime emission
// hooks are added separately so the ID registry can be reviewed first.

#ifndef XUSBD_TRACE_H
#define XUSBD_TRACE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
// MODULE INCLUDES
#include "xtrace_config.h"
#include "xtrace_registry.h"

#ifndef xUSBD_TRACE_ENABLE
#define xUSBD_TRACE_ENABLE xTRACE_ENABLE
#endif

#if xTRACE_ENABLE && xUSBD_TRACE_ENABLE
#include "xtrace.h"
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

#define xUSBD_TRACE_CODE_BUS_RESET (xTRACE_BASE_xUSB + 0x00U) /// @trace {"type": "instant", "track": "xUSBD/Bus", "args": ["active_speed"]}
#define xUSBD_TRACE_CODE_BUS_CONNECT                                                                                                       \
    (xTRACE_BASE_xUSB + 0x01U) /// @trace {"type": "instant", "track": "xUSBD/Bus", "args": ["active_speed"]}
#define xUSBD_TRACE_CODE_BUS_DISCONNECT (xTRACE_BASE_xUSB + 0x02U) /// @trace {"type": "instant", "track": "xUSBD/Bus", "args": ["unused"]}
#define xUSBD_TRACE_CODE_SET_ADDRESS                                                                                                       \
    (xTRACE_BASE_xUSB + 0x03U) /// @trace {"type": "instant", "track": "xUSBD/Control", "args": ["address"]}
#define xUSBD_TRACE_CODE_SET_CONFIGURATION                                                                                                 \
    (xTRACE_BASE_xUSB + 0x04U) /// @trace {"type": "instant", "track": "xUSBD/Control", "args": ["configuration_value"]}
#define xUSBD_TRACE_CODE_CONTROL_REQUEST                                                                                                   \
    (xTRACE_BASE_xUSB + 0x05U) /// @trace {"type": "instant", "track": "xUSBD/Control", "args": ["packed_request"]}
#define xUSBD_TRACE_CODE_EP_STALL                                                                                                          \
    (xTRACE_BASE_xUSB + 0x06U) /// @trace {"type": "instant", "track": "xUSBD/Endpoint", "args": ["endpoint_address"]}
#define xUSBD_TRACE_CODE_EP_CLEAR_STALL                                                                                                    \
    (xTRACE_BASE_xUSB + 0x07U) /// @trace {"type": "instant", "track": "xUSBD/Endpoint", "args": ["endpoint_address"]}
#define xUSBD_TRACE_CODE_EP_IN                                                                                                             \
    (xTRACE_BASE_xUSB + 0x08U) /// @trace {"type": "counter", "track": "xUSBD/Endpoint", "args": ["transfer_length"]}
#define xUSBD_TRACE_CODE_EP_OUT                                                                                                            \
    (xTRACE_BASE_xUSB + 0x09U) /// @trace {"type": "counter", "track": "xUSBD/Endpoint", "args": ["transfer_length"]}
#define xUSBD_TRACE_CODE_CLASS_REQUEST                                                                                                     \
    (xTRACE_BASE_xUSB + 0x0AU)                                /// @trace {"type": "instant", "track": "xUSBD/Class", "args": ["owner_id"]}
#define xUSBD_TRACE_CODE_DCD_ERROR (xTRACE_BASE_xUSB + 0x0BU) /// @trace {"type": "error", "track": "xUSBD/DCD", "args": ["status"]}

#if xTRACE_ENABLE && xUSBD_TRACE_ENABLE
#define xUSBD_TRACE_E1(device_ctx, code, arg) xTRACE_E1((device_ctx)->trace_ctx, (code), (uint32_t)(arg))
#else
#define xUSBD_TRACE_DISCARD_(value) ((void)sizeof(value))
#define xUSBD_TRACE_E1(device_ctx, code, arg)                                                                                              \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xUSBD_TRACE_DISCARD_(device_ctx);                                                                                                  \
        xUSBD_TRACE_DISCARD_(code);                                                                                                        \
        xUSBD_TRACE_DISCARD_(arg);                                                                                                         \
    } while (0)
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUSBD_TRACE_H
// EOF /////////////////////////////////////////////////////////////////////////////
