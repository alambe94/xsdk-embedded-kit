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

// @file xtrace.c
// @brief xTrace v5 - LEB128 variable-length stream recorder.
//
// Wire format per record:
//   [payload_len : LEB128] [event_id : LEB128] [delta_ts : LEB128] [param0..N : LEB128 each]
//
// The length prefix makes the stream self-describing: the host decoder can skip
// unknown event IDs without needing a schema to determine parameter count.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xtrace.h"

#include "xtrace_log.h"

// MODULE MACROS ///////////////////////////////////////////////////////////////////

#define xTRACE_MIN_CAPACITY_BYTES 16U

// Bits in word 0 that must never be cleared: GAP (0), BOOT (1), TIME_SYNC (2).
#define xTRACE_CORE_PROTECTED_MASK ((1U << xTRACE_EV_GAP) | (1U << xTRACE_EV_BOOT) | (1U << xTRACE_EV_TIME_SYNC))

// MODULE TYPES ////////////////////////////////////////////////////////////////////

// MODULE VARIABLES ////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// MODULE FUNCTION PROTOTYPES //////////////////////////////////////////////////////

static uint32_t leb128_size(uint32_t v);
static size_t ring_available(const xTRACE_Context_t *ctx);
static void ring_put(xTRACE_Context_t *ctx, uint8_t byte);
static void ring_put_leb128(xTRACE_Context_t *ctx, uint32_t value);
static uint8_t ring_peek_byte(const xTRACE_Context_t *ctx, size_t pos);
static uint32_t ring_record_count(const xTRACE_Context_t *ctx);
static void ring_put_boot(xTRACE_Context_t *ctx, uint32_t timestamp);
static void ring_put_gap(xTRACE_Context_t *ctx, uint32_t delta, uint32_t dropped_count);
static bool event_is_enabled(const xTRACE_Context_t *ctx, uint32_t event_id);
static bool emit_start(xTRACE_Context_t *ctx, uint32_t event_id, uint32_t extra_bytes);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// Number of bytes needed to LEB128-encode v.
static uint32_t leb128_size(uint32_t v)
{
    uint32_t n = 1U;
    uint32_t w = v;
    while (w > 127U)
    {
        w >>= 7;
        n++;
    }
    return n;
}

// Bytes currently buffered (not yet flushed).
static size_t ring_available(const xTRACE_Context_t *ctx)
{
    if (ctx->write_pos >= ctx->read_pos)
    {
        return ctx->capacity_bytes - (ctx->write_pos - ctx->read_pos) - 1U;
    }
    return ctx->read_pos - ctx->write_pos - 1U;
}

// Write one byte to the ring (caller must ensure space is available).
static void ring_put(xTRACE_Context_t *ctx, uint8_t byte)
{
    ctx->buffer[ctx->write_pos] = byte;
    ctx->write_pos = (ctx->write_pos + 1U) % ctx->capacity_bytes;
}

// Write a LEB128-encoded value (caller must ensure space is available).
static void ring_put_leb128(xTRACE_Context_t *ctx, uint32_t value)
{
    uint32_t v = value;
    do
    {
        uint8_t byte = (uint8_t)(v & 0x7FU);
        v >>= 7;
        if (v != 0U)
        {
            byte = (uint8_t)((uint32_t)byte | 0x80U);
        }
        ring_put(ctx, byte);
    } while (v != 0U);
}

// Read one byte from the ring without modifying any index (handles wrap).
static uint8_t ring_peek_byte(const xTRACE_Context_t *ctx, size_t pos)
{
    return ctx->buffer[pos % ctx->capacity_bytes];
}

// Count complete records currently retained in the ring.
static uint32_t ring_record_count(const xTRACE_Context_t *ctx)
{
    size_t pos = ctx->read_pos;
    size_t traversed = 0U;
    uint32_t count = 0U;

    while ((pos != ctx->write_pos) && (traversed < ctx->capacity_bytes))
    {
        uint32_t payload_len = 0U;
        uint32_t shift = 0U;
        uint32_t prefix_bytes = 0U;
        uint8_t b;

        do
        {
            b = ring_peek_byte(ctx, pos);
            pos = (pos + 1U) % ctx->capacity_bytes;
            prefix_bytes++;
            payload_len |= ((uint32_t)(b & 0x7FU)) << shift;
            shift += 7U;
        } while (((b & 0x80U) != 0U) && (shift < 35U));

        pos = (pos + payload_len) % ctx->capacity_bytes;
        traversed += prefix_bytes + payload_len;
        count++;
    }

    return count;
}

static void ring_put_boot(xTRACE_Context_t *ctx, uint32_t timestamp)
{
    uint32_t payload = 1U + leb128_size(timestamp) + leb128_size(ctx->timestamp_hz);
    ring_put_leb128(ctx, payload);
    ring_put(ctx, (uint8_t)xTRACE_EV_BOOT);
    ring_put_leb128(ctx, timestamp);
    ring_put_leb128(ctx, ctx->timestamp_hz);
}

static void ring_put_gap(xTRACE_Context_t *ctx, uint32_t delta, uint32_t dropped_count)
{
    uint32_t payload = 1U + leb128_size(delta) + leb128_size(dropped_count);
    ring_put_leb128(ctx, payload);
    ring_put(ctx, (uint8_t)xTRACE_EV_GAP);
    ring_put_leb128(ctx, delta);
    ring_put_leb128(ctx, dropped_count);
}

// Return true when event_id is currently enabled in the filter mask.
// Event IDs >= 256 are not covered by the mask and are always allowed.
static bool event_is_enabled(const xTRACE_Context_t *ctx, uint32_t event_id)
{
    if (event_id >= 256U)
    {
        return true;
    }
    uint32_t word_idx = event_id / 32U;
    uint32_t bit_mask = 1U << (event_id % 32U);
    return (ctx->enabled_events_mask[word_idx] & bit_mask) != 0U;
}

// Common prologue for every Emit call.
//   1. Guard check (NULL, not-initialized, not-enabled).
//   2. Event filter check.
//   3. Compute delta timestamp.
//   4. Pre-check available space for GAP (if pending) + this event.
//   5. Emit GAP record if pending.
//   6. Write length prefix + event_id + delta_ts to the ring.
// Returns true when the caller should write its parameters; false on failure.
// extra_bytes: bytes needed for parameters AFTER event_id+delta (not counting those).
static bool emit_start(xTRACE_Context_t *ctx, uint32_t event_id, uint32_t extra_bytes)
{
    if ((ctx == NULL) || (!ctx->is_initialized) || (!ctx->is_enabled))
    {
        return false;
    }
    if (!event_is_enabled(ctx, event_id))
    {
        return false;
    }

    uint32_t ts = ctx->timestamp_fn(ctx->timestamp_ctx);
    uint32_t delta = ts - ctx->last_timestamp;

    // Space needed for an in-stream GAP record, if one is pending.
    uint32_t gap_size = 0U;
    if (ctx->is_gap_pending)
    {
        // GAP payload: [0x00](1B) + [delta: LEB128] + [dropped_count: LEB128]
        uint32_t gap_payload = 1U + leb128_size(delta) + leb128_size(ctx->dropped_count);
        gap_size = leb128_size(gap_payload) + gap_payload;
    }

    // Space needed for this event: [prefix: LEB128] + [event_id: LEB128] + [delta: LEB128] + extra.
    uint32_t evt_payload = leb128_size(event_id) + leb128_size(delta) + extra_bytes;
    uint32_t evt_size = leb128_size(evt_payload) + evt_payload;
    uint32_t needed = gap_size + evt_size;

    if (ring_available(ctx) < needed)
    {
        if (ctx->overrun_policy == xTRACE_OVERRUN_OVERWRITE)
        {
            uint32_t overwritten = ring_record_count(ctx);
            uint32_t total_dropped = ctx->dropped_count + overwritten;
            uint32_t boot_payload = 1U + leb128_size(ts) + leb128_size(ctx->timestamp_hz);
            uint32_t boot_size = leb128_size(boot_payload) + boot_payload;
            uint32_t reset_gap_payload = 1U + leb128_size(0U) + leb128_size(total_dropped);
            uint32_t reset_gap_size = leb128_size(reset_gap_payload) + reset_gap_payload;
            uint32_t reset_evt_payload = leb128_size(event_id) + leb128_size(0U) + extra_bytes;
            uint32_t reset_evt_size = leb128_size(reset_evt_payload) + reset_evt_payload;

            if ((boot_size + reset_gap_size + reset_evt_size) >= ctx->capacity_bytes)
            {
                ctx->dropped_count++;
                ctx->is_gap_pending = true;
                return false;
            }

            ctx->read_pos = ctx->write_pos;
            ring_put_boot(ctx, ts);
            ring_put_gap(ctx, 0U, total_dropped);
            ctx->dropped_count = 0U;
            ctx->is_gap_pending = false;
            ring_put_leb128(ctx, reset_evt_payload);
            ring_put_leb128(ctx, event_id);
            ring_put_leb128(ctx, 0U);
            ctx->last_timestamp = ts;
            return true;
        }

        ctx->dropped_count++;
        ctx->is_gap_pending = true;
        return false;
    }

    // Emit pending GAP record first.
    if (ctx->is_gap_pending)
    {
        ring_put_gap(ctx, delta, ctx->dropped_count);
        ctx->dropped_count = 0U;
        ctx->is_gap_pending = false;
        // After emitting GAP at ts, the event immediately following also happened at ts -> delta = 0.
        ctx->last_timestamp = ts;
        delta = 0U;
    }

    // Recompute payload with the (possibly updated) delta before writing the prefix.
    uint32_t final_payload = leb128_size(event_id) + leb128_size(delta) + extra_bytes;
    ring_put_leb128(ctx, final_payload);
    ring_put_leb128(ctx, event_id);
    ring_put_leb128(ctx, delta);
    ctx->last_timestamp = ts;
    return true;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xTRACE_Init(xTRACE_Context_t *trace_ctx, const xTRACE_Config_t *config, const xTRACE_Transport_t *transport, void *transport_ctx)
{
    if (trace_ctx == NULL)
    {
        return xRETURN_xERR_xTRACE_NULL_POINTER;
    }
    if (config == NULL)
    {
        return xRETURN_xERR_xTRACE_NULL_POINTER;
    }
    if (config->buffer == NULL)
    {
        return xRETURN_xERR_xTRACE_NULL_POINTER;
    }
    if (config->timestamp_fn == NULL)
    {
        return xRETURN_xERR_xTRACE_NULL_POINTER;
    }
    if ((transport != NULL) && (transport->write == NULL))
    {
        return xRETURN_xERR_xTRACE_NULL_POINTER;
    }
    if (config->capacity_bytes < xTRACE_MIN_CAPACITY_BYTES)
    {
        return xRETURN_xERR_xTRACE_INVALID_ARG;
    }
    if (config->timestamp_hz == 0U)
    {
        return xRETURN_xERR_xTRACE_INVALID_ARG;
    }

    uint32_t abs_ts = config->timestamp_fn(config->timestamp_ctx);

    trace_ctx->buffer = config->buffer;
    trace_ctx->capacity_bytes = config->capacity_bytes;
    trace_ctx->write_pos = 0U;
    trace_ctx->read_pos = 0U;
    trace_ctx->dropped_count = 0U;
    trace_ctx->last_timestamp = abs_ts;
    trace_ctx->is_gap_pending = false;
    trace_ctx->overrun_policy = config->overrun_policy;
    trace_ctx->timestamp_fn = config->timestamp_fn;
    trace_ctx->timestamp_ctx = config->timestamp_ctx;
    trace_ctx->timestamp_hz = config->timestamp_hz;
    trace_ctx->transport = transport;
    trace_ctx->transport_ctx = transport_ctx;
    trace_ctx->is_enabled = config->is_enabled;
    trace_ctx->is_initialized = true;

    // All events enabled by default.
    (void)memset(trace_ctx->enabled_events_mask, 0xFFU, sizeof(trace_ctx->enabled_events_mask));

    // Emit BOOT record: [prefix][0x01][abs_ts: LEB128][timestamp_hz: LEB128]
    if (trace_ctx->is_enabled)
    {
        uint32_t boot_payload = 1U + leb128_size(abs_ts) + leb128_size(config->timestamp_hz);
        uint32_t boot_total = leb128_size(boot_payload) + boot_payload;
        if (ring_available(trace_ctx) >= boot_total)
        {
            ring_put_boot(trace_ctx, abs_ts);
        }
        else
        {
            // Buffer too small to hold the BOOT record; count the loss.
            trace_ctx->dropped_count++;
        }
    }

    return xRETURN_OK;
}

xRETURN_t xTRACE_Deinit(xTRACE_Context_t *trace_ctx)
{
    if (trace_ctx == NULL)
    {
        return xRETURN_xERR_xTRACE_NULL_POINTER;
    }
    if (!trace_ctx->is_initialized)
    {
        return xRETURN_xERR_xTRACE_NOT_INITIALIZED;
    }

    (void)memset(trace_ctx, 0, sizeof(*trace_ctx));

    return xRETURN_OK;
}

void xTRACE_Emit0(xTRACE_Context_t *trace_ctx, uint32_t event_id)
{
    if (trace_ctx == NULL)
    {
        return;
    }
    xTRACE_LOCK_DEFINE();
    xTRACE_LOCK(trace_ctx);
    (void)emit_start(trace_ctx, event_id, 0U);
    xTRACE_UNLOCK(trace_ctx);
}

void xTRACE_Emit1(xTRACE_Context_t *trace_ctx, uint32_t event_id, uint32_t a)
{
    if (trace_ctx == NULL)
    {
        return;
    }
    xTRACE_LOCK_DEFINE();
    xTRACE_LOCK(trace_ctx);
    if (emit_start(trace_ctx, event_id, leb128_size(a)))
    {
        ring_put_leb128(trace_ctx, a);
    }
    xTRACE_UNLOCK(trace_ctx);
}

void xTRACE_Emit2(xTRACE_Context_t *trace_ctx, uint32_t event_id, uint32_t a, uint32_t b)
{
    if (trace_ctx == NULL)
    {
        return;
    }
    xTRACE_LOCK_DEFINE();
    xTRACE_LOCK(trace_ctx);
    if (emit_start(trace_ctx, event_id, leb128_size(a) + leb128_size(b)))
    {
        ring_put_leb128(trace_ctx, a);
        ring_put_leb128(trace_ctx, b);
    }
    xTRACE_UNLOCK(trace_ctx);
}

void xTRACE_Emit3(xTRACE_Context_t *trace_ctx, uint32_t event_id, uint32_t a, uint32_t b, uint32_t c)
{
    if (trace_ctx == NULL)
    {
        return;
    }
    xTRACE_LOCK_DEFINE();
    xTRACE_LOCK(trace_ctx);
    if (emit_start(trace_ctx, event_id, leb128_size(a) + leb128_size(b) + leb128_size(c)))
    {
        ring_put_leb128(trace_ctx, a);
        ring_put_leb128(trace_ctx, b);
        ring_put_leb128(trace_ctx, c);
    }
    xTRACE_UNLOCK(trace_ctx);
}

void xTRACE_EmitName(xTRACE_Context_t *trace_ctx, uint32_t event_id, uint32_t obj_type, uint32_t obj_id, const char *name)
{
    if (name == NULL)
    {
        return;
    }

    uint32_t str_len = 0U;
    while (name[str_len] != '\0')
    {
        uint8_t byte = (uint8_t)name[str_len];
        if ((byte < 0x20U) || (byte > 0x7EU))
        {
            return;
        }
        str_len++;
    }

    uint32_t extra = leb128_size(obj_type) + leb128_size(obj_id) + leb128_size(str_len) + str_len;

    if (trace_ctx == NULL)
    {
        return;
    }
    xTRACE_LOCK_DEFINE();
    xTRACE_LOCK(trace_ctx);
    if (emit_start(trace_ctx, event_id, extra))
    {
        ring_put_leb128(trace_ctx, obj_type);
        ring_put_leb128(trace_ctx, obj_id);
        ring_put_leb128(trace_ctx, str_len);
        for (uint32_t i = 0U; i < str_len; i++)
        {
            ring_put(trace_ctx, (uint8_t)name[i]);
        }
    }
    xTRACE_UNLOCK(trace_ctx);
}

xRETURN_t xTRACE_Flush(xTRACE_Context_t *trace_ctx)
{
    if (trace_ctx == NULL)
    {
        return xRETURN_xERR_xTRACE_NULL_POINTER;
    }
    if (!trace_ctx->is_initialized)
    {
        return xRETURN_xERR_xTRACE_NOT_INITIALIZED;
    }
    if (trace_ctx->transport == NULL)
    {
        return xRETURN_OK;
    }

    while (trace_ctx->read_pos != trace_ctx->write_pos)
    {
        size_t contiguous;

        if (trace_ctx->write_pos > trace_ctx->read_pos)
        {
            contiguous = trace_ctx->write_pos - trace_ctx->read_pos;
        }
        else
        {
            // Data wraps: drain from read_pos to end of buffer first.
            contiguous = trace_ctx->capacity_bytes - trace_ctx->read_pos;
        }

        size_t written = 0U;
        xRETURN_t ret =
            trace_ctx->transport->write(trace_ctx->transport_ctx, &trace_ctx->buffer[trace_ctx->read_pos], contiguous, &written);

        if (ret != xRETURN_OK)
        {
            return xRETURN_xERR_xTRACE_TRANSPORT;
        }
        if (written > contiguous)
        {
            return xRETURN_xERR_xTRACE_TRANSPORT;
        }
        if (written == 0U)
        {
            break;
        }

        trace_ctx->read_pos = (trace_ctx->read_pos + written) % trace_ctx->capacity_bytes;
    }

    return xRETURN_OK;
}

xRETURN_t xTRACE_Get_Status(const xTRACE_Context_t *trace_ctx, xTRACE_Status_t *status)
{
    if (trace_ctx == NULL)
    {
        return xRETURN_xERR_xTRACE_NULL_POINTER;
    }
    if (status == NULL)
    {
        return xRETURN_xERR_xTRACE_NULL_POINTER;
    }
    if (!trace_ctx->is_initialized)
    {
        return xRETURN_xERR_xTRACE_NOT_INITIALIZED;
    }

    size_t used;
    if (trace_ctx->write_pos >= trace_ctx->read_pos)
    {
        used = trace_ctx->write_pos - trace_ctx->read_pos;
    }
    else
    {
        used = trace_ctx->capacity_bytes - (trace_ctx->read_pos - trace_ctx->write_pos);
    }

    status->capacity_bytes = trace_ctx->capacity_bytes;
    status->used_bytes = used;
    status->dropped_count = trace_ctx->dropped_count;
    status->overrun_policy = trace_ctx->overrun_policy;
    status->is_initialized = trace_ctx->is_initialized;
    status->is_enabled = trace_ctx->is_enabled;

    return xRETURN_OK;
}

void xTRACE_Filter_Enable_Event(xTRACE_Context_t *trace_ctx, uint32_t event_id)
{
    if ((trace_ctx == NULL) || (event_id >= 256U))
    {
        return;
    }
    uint32_t word_idx = event_id / 32U;
    trace_ctx->enabled_events_mask[word_idx] |= (1U << (event_id % 32U));
}

void xTRACE_Filter_Disable_Event(xTRACE_Context_t *trace_ctx, uint32_t event_id)
{
    if ((trace_ctx == NULL) || (event_id >= 256U))
    {
        return;
    }
    uint32_t word_idx = event_id / 32U;
    trace_ctx->enabled_events_mask[word_idx] &= ~(1U << (event_id % 32U));
}

void xTRACE_Filter_Enable_All(xTRACE_Context_t *trace_ctx)
{
    if (trace_ctx == NULL)
    {
        return;
    }
    (void)memset(trace_ctx->enabled_events_mask, 0xFFU, sizeof(trace_ctx->enabled_events_mask));
}

void xTRACE_Filter_Disable_All(xTRACE_Context_t *trace_ctx)
{
    if (trace_ctx == NULL)
    {
        return;
    }
    (void)memset(trace_ctx->enabled_events_mask, 0x00U, sizeof(trace_ctx->enabled_events_mask));
    // Preserve protected core events even after a full disable.
    trace_ctx->enabled_events_mask[0] = xTRACE_CORE_PROTECTED_MASK;
}

void xTRACE_Filter_Enable_Module(xTRACE_Context_t *trace_ctx, uint32_t module_base)
{
    if ((trace_ctx == NULL) || (module_base >= 256U))
    {
        return;
    }
    uint32_t word_idx = module_base / 32U;
    trace_ctx->enabled_events_mask[word_idx] = 0xFFFFFFFFU;
}

void xTRACE_Filter_Disable_Module(xTRACE_Context_t *trace_ctx, uint32_t module_base)
{
    if ((trace_ctx == NULL) || (module_base >= 256U))
    {
        return;
    }
    uint32_t word_idx = module_base / 32U;
    if (word_idx == 0U)
    {
        // Core block: GAP/BOOT/TIME_SYNC remain enabled.
        trace_ctx->enabled_events_mask[0] = xTRACE_CORE_PROTECTED_MASK;
    }
    else
    {
        trace_ctx->enabled_events_mask[word_idx] = 0x00000000U;
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
