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

// @file xtrace.h
// @brief xTrace v5 - LEB128 variable-length stream recorder.
//
// Wire format per record:
//   [payload_len : LEB128] [event_id : LEB128] [delta_ts : LEB128] [param0..N : LEB128 each]
//
// delta_ts is us since the previous record.  The BOOT record (ID=0x01)
// carries an absolute timestamp so the decoder can anchor the session.
//
// Event ID Block Allocation Registry (ensures 1-byte LEB128 wire compression for core modules):
//   0x00 - 0x1F (0 - 31):    Core xTrace (GAP, BOOT, TIME_SYNC)
//   0x20 - 0x3F (32 - 63):   xRTOS
//   0x40 - 0x5F (64 - 95):   xFS
//   0x60 - 0x7F (96 - 127):  xUSB Device
//   0x80 - 0x9F (128 - 159): xUSB Host
//   0xA0+       (160+):      User Application / Drivers (2-byte LEB128 wire size)
//
// Call interface (compile-time gated via xTRACE_ENABLE):
//   xTRACE_E0(ctx, id)           - zero parameters
//   xTRACE_E1(ctx, id, a)        - one  parameter
//   xTRACE_E2(ctx, id, a, b)     - two  parameters
//   xTRACE_E3(ctx, id, a, b, c)  - three parameters
//
// All parameters are uint32_t on the wire.  The caller casts as needed.
//

#ifndef XTRACE_H
#define XTRACE_H

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
#include "xtrace_config.h"
#include "xtrace_return.h"
#include "xtrace_registry.h"

    // MACROS //////////////////////////////////////////////////////////////////////

// Module version
#define xTRACE_VERSION_MAJOR  0U
#define xTRACE_VERSION_MINOR  5U
#define xTRACE_VERSION_PATCH  0U
#define xTRACE_VERSION_STRING "0.5.0"

// Reserved event IDs - do not use in module or user code.
#define xTRACE_EV_GAP       0x00U // in-stream gap: dropped records since last flush
#define xTRACE_EV_BOOT      0x01U // session start: abs_timestamp, timestamp_hz
#define xTRACE_EV_TIME_SYNC 0x02U // timestamp overflow anchor: abs_us_lo, abs_us_hi

// Compile-time trace emission macros. Expand to no-ops when xTRACE_ENABLE is 0.
#if xTRACE_ENABLE
#define xTRACE_E0(ctx, id)          xTRACE_Emit0((ctx), (id))
#define xTRACE_E1(ctx, id, a)       xTRACE_Emit1((ctx), (id), (uint32_t)(a))
#define xTRACE_E2(ctx, id, a, b)    xTRACE_Emit2((ctx), (id), (uint32_t)(a), (uint32_t)(b))
#define xTRACE_E3(ctx, id, a, b, c) xTRACE_Emit3((ctx), (id), (uint32_t)(a), (uint32_t)(b), (uint32_t)(c))
#else
#define xTRACE_E0(ctx, id)                                                                                                                 \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(ctx);                                                                                                                       \
        (void)(id);                                                                                                                        \
    } while (0)
#define xTRACE_E1(ctx, id, a)                                                                                                              \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(ctx);                                                                                                                       \
        (void)(id);                                                                                                                        \
        (void)(a);                                                                                                                         \
    } while (0)
#define xTRACE_E2(ctx, id, a, b)                                                                                                           \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(ctx);                                                                                                                       \
        (void)(id);                                                                                                                        \
        (void)(a);                                                                                                                         \
        (void)(b);                                                                                                                         \
    } while (0)
#define xTRACE_E3(ctx, id, a, b, c)                                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(ctx);                                                                                                                       \
        (void)(id);                                                                                                                        \
        (void)(a);                                                                                                                         \
        (void)(b);                                                                                                                         \
        (void)(c);                                                                                                                         \
    } while (0)
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef uint32_t xTRACE_Time_t;

    // Overrun behavior policy when the ring buffer fills.
    typedef enum
    {
        xTRACE_OVERRUN_DROP = 0,      // Drop new records when buffer is full (default)
        xTRACE_OVERRUN_OVERWRITE = 1, // Overwrite oldest records when buffer is full
    } xTRACE_Overrun_Policy_t;

    // Caller-supplied timestamp function. Must be callable from ISR context.
    typedef xTRACE_Time_t (*xTRACE_Timestamp_Fn_t)(void *timestamp_ctx);

    // Transport interface: xTRACE_Flush calls write; Emit functions never do.
    typedef struct xTRACE_Transport_t
    {
        xRETURN_t (*write)(void *transport_ctx, const uint8_t *buffer, size_t length, size_t *bytes_written);
    } xTRACE_Transport_t;

    // Caller-owned configuration. Passed once to xTRACE_Init.
    typedef struct xTRACE_Config_t
    {
        uint8_t *buffer;       // byte array (caller-owned); minimum 16 bytes
        size_t capacity_bytes; // total bytes in buffer; must be >= 16

        xTRACE_Timestamp_Fn_t timestamp_fn;
        void *timestamp_ctx;
        uint32_t timestamp_hz; // Hz of timestamp source; must be non-zero

        xTRACE_Overrun_Policy_t overrun_policy; // Overrun handling policy

        bool is_enabled;
    } xTRACE_Config_t;

    // Caller-owned context. Must remain valid for the lifetime of the trace session.
    typedef struct xTRACE_Context_t
    {
        uint8_t *buffer;
        size_t capacity_bytes;
        size_t write_pos; // next write byte offset (circular)
        size_t read_pos;  // next read byte offset (circular, advanced by Flush)
        uint32_t dropped_count;
        uint32_t last_timestamp; // last emitted absolute timestamp (us)
        bool is_gap_pending;     // true when overflow occurred - emit GAP next

        xTRACE_Overrun_Policy_t overrun_policy; // Policy copied from config
        uint32_t enabled_events_mask[8];        // Bitmask of enabled event IDs (256 bits)

        xTRACE_Timestamp_Fn_t timestamp_fn;
        void *timestamp_ctx;
        uint32_t timestamp_hz;

        const xTRACE_Transport_t *transport;
        void *transport_ctx;

        bool is_initialized;
        bool is_enabled;
    } xTRACE_Context_t;

    // Status snapshot returned by xTRACE_Get_Status.
    typedef struct xTRACE_Status_t
    {
        size_t capacity_bytes;
        size_t used_bytes;
        uint32_t dropped_count;
        xTRACE_Overrun_Policy_t overrun_policy;
        bool is_initialized;
        bool is_enabled;
    } xTRACE_Status_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t
    xTRACE_Init(xTRACE_Context_t *trace_ctx, const xTRACE_Config_t *config, const xTRACE_Transport_t *transport, void *transport_ctx);

    xRETURN_t xTRACE_Deinit(xTRACE_Context_t *trace_ctx);

    // Emit functions - return void so trace calls never affect caller control flow.
    void xTRACE_Emit0(xTRACE_Context_t *trace_ctx, uint32_t event_id);
    void xTRACE_Emit1(xTRACE_Context_t *trace_ctx, uint32_t event_id, uint32_t a);
    void xTRACE_Emit2(xTRACE_Context_t *trace_ctx, uint32_t event_id, uint32_t a, uint32_t b);
    void xTRACE_Emit3(xTRACE_Context_t *trace_ctx, uint32_t event_id, uint32_t a, uint32_t b, uint32_t c);

    // Variable-length printable-ASCII name record:
    // [event_id][delta_ts][obj_type][obj_id][str_len][bytes...]
    // Silently drops if name is NULL, contains bytes outside 0x20..0x7E, or
    // tracing is not active.
    void xTRACE_EmitName(xTRACE_Context_t *trace_ctx, uint32_t event_id, uint32_t obj_type, uint32_t obj_id, const char *name);

    // Drain buffered bytes to the transport. May be called from task context.
    xRETURN_t xTRACE_Flush(xTRACE_Context_t *trace_ctx);

    xRETURN_t xTRACE_Get_Status(const xTRACE_Context_t *trace_ctx, xTRACE_Status_t *status);

    // Dynamic runtime filtering functions
    void xTRACE_Filter_Enable_Event(xTRACE_Context_t *trace_ctx, uint32_t event_id);
    void xTRACE_Filter_Disable_Event(xTRACE_Context_t *trace_ctx, uint32_t event_id);
    void xTRACE_Filter_Enable_All(xTRACE_Context_t *trace_ctx);
    void xTRACE_Filter_Disable_All(xTRACE_Context_t *trace_ctx);

    // Convenience wrappers: enable/disable all 32 event IDs in a module's block.
    // module_base must be one of the xTRACE_BASE_* constants from xtrace_registry.h.
    // xTRACE_Filter_Disable_Module protects GAP/BOOT/TIME_SYNC: the core block
    // (xTRACE_BASE_CORE) retains those three IDs enabled regardless.
    void xTRACE_Filter_Enable_Module(xTRACE_Context_t *trace_ctx, uint32_t module_base);
    void xTRACE_Filter_Disable_Module(xTRACE_Context_t *trace_ctx, uint32_t module_base);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XTRACE_H
// EOF /////////////////////////////////////////////////////////////////////////////
