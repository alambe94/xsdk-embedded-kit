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

// @file bench_xtrace.c
// @brief Overhead benchmark for xTrace v2 LEB128 recorder.
//
// Measures three call paths:
//   fast path  - ring buffer has space; LEB128 record is stored.
//   drop path  - ring buffer is full; dropped_count incremented.
//   flush path - bytes drained through a null-sink transport.
//
// Build (host build, not a CTest test):
//   xsdk.bat
//
// Run manually:
//   build/host/src/components/xutil/xtrace/tests/bench_xtrace.exe
//

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xtrace.h"

// -- Platform timer ------------------------------------------------------------

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef LARGE_INTEGER bench_clock_t;
static LARGE_INTEGER s_freq;

static void bench_clock_init(void)
{
    QueryPerformanceFrequency(&s_freq);
}
static void bench_clock_now(bench_clock_t *t)
{
    QueryPerformanceCounter(t);
}
static double bench_clock_ns(const bench_clock_t *a, const bench_clock_t *b)
{
    return (double)(b->QuadPart - a->QuadPart) * 1e9 / (double)s_freq.QuadPart;
}
#else
#include <time.h>
typedef struct timespec bench_clock_t;
static void bench_clock_init(void)
{
}
static void bench_clock_now(bench_clock_t *t)
{
    clock_gettime(CLOCK_MONOTONIC, t);
}
static double bench_clock_ns(const bench_clock_t *a, const bench_clock_t *b)
{
    return (double)((b->tv_sec - a->tv_sec) * (long long)1000000000 + (b->tv_nsec - a->tv_nsec));
}
#endif

// -- Null-sink transport -------------------------------------------------------

static uint32_t s_sink_total_bytes;

static xRETURN_t sink_write(void *ctx, const uint8_t *buffer, size_t length, size_t *bytes_written)
{
    (void)ctx;
    (void)buffer;
    s_sink_total_bytes += length;
    *bytes_written = length;
    return xRETURN_OK;
}

static const xTRACE_Transport_t s_sink_transport = {.write = sink_write};

// -- Benchmark parameters ------------------------------------------------------

#define BENCH_ITERATIONS 100000U
#define BENCH_WARMUP     1000U

// LEB128 records with event_id<128, delta<128, one param<128 -> 3 bytes each.
// Fast path buffer must hold warmup + measurement iterations without overflow.
// 3 bytes/record * (100000 + 1000) + BOOT(~5) ~ 303 KB -> round up.
#define BENCH_FAST_BYTES ((BENCH_ITERATIONS + BENCH_WARMUP) * 4U + 32U)

// Drop path: small buffer, pre-filled so every subsequent call is a drop.
#define BENCH_DROP_BYTES 32U

// Flush path: 8 KB buffer, refilled each cycle.
#define BENCH_FLUSH_BYTES  (8U * 1024U)
#define BENCH_FLUSH_CYCLES 100U

static uint8_t s_fast_buf[BENCH_FAST_BYTES];
static uint8_t s_drop_buf[BENCH_DROP_BYTES];
static uint8_t s_flush_buf[BENCH_FLUSH_BYTES];
static xTRACE_Context_t s_fast_ctx;
static xTRACE_Context_t s_drop_ctx;
static xTRACE_Context_t s_flush_ctx;

// -- Fixtures ------------------------------------------------------------------

static xTRACE_Time_t bench_timestamp(void *ctx)
{
    (void)ctx;
    return 0U;
}

static void bench_init_fast(void)
{
    const xTRACE_Config_t cfg = {
        .buffer = s_fast_buf,
        .capacity_bytes = BENCH_FAST_BYTES,
        .timestamp_fn = bench_timestamp,
        .timestamp_ctx = NULL,
        .timestamp_hz = 1U,
        .is_enabled = true,
    };
    memset(s_fast_buf, 0, sizeof(s_fast_buf));
    xTRACE_Init(&s_fast_ctx, &cfg, NULL, NULL);
}

static void bench_init_drop(void)
{
    const xTRACE_Config_t cfg = {
        .buffer = s_drop_buf,
        .capacity_bytes = BENCH_DROP_BYTES,
        .timestamp_fn = bench_timestamp,
        .timestamp_ctx = NULL,
        .timestamp_hz = 1U,
        .is_enabled = true,
    };
    memset(s_drop_buf, 0, sizeof(s_drop_buf));
    xTRACE_Init(&s_drop_ctx, &cfg, NULL, NULL);
    // Fill buffer so every subsequent call is a drop
    while (s_drop_ctx.dropped_count == 0U)
    {
        xTRACE_Emit1(&s_drop_ctx, 0x20U, 0U);
    }
    s_drop_ctx.dropped_count = 0U;
}

static void bench_refill_flush(void)
{
    const xTRACE_Config_t cfg = {
        .buffer = s_flush_buf,
        .capacity_bytes = BENCH_FLUSH_BYTES,
        .timestamp_fn = bench_timestamp,
        .timestamp_ctx = NULL,
        .timestamp_hz = 1U,
        .is_enabled = true,
    };
    memset(s_flush_buf, 0, sizeof(s_flush_buf));
    s_sink_total_bytes = 0U;
    xTRACE_Init(&s_flush_ctx, &cfg, &s_sink_transport, NULL);
    while (s_flush_ctx.dropped_count == 0U)
    {
        xTRACE_Emit1(&s_flush_ctx, 0x20U, 0x01U);
    }
    s_flush_ctx.dropped_count = 0U;
    s_flush_ctx.is_gap_pending = false;
}

// -- Runners -------------------------------------------------------------------

static void bench_flush_throughput(void)
{
    bench_clock_t t0;
    bench_clock_t t1;
    double total_ns = 0.0;
    uint64_t total_bytes = 0U;

    // Warmup
    for (uint32_t cycle = 0U; cycle < 5U; cycle++)
    {
        bench_refill_flush();
        xTRACE_Flush(&s_flush_ctx);
    }

    for (uint32_t cycle = 0U; cycle < BENCH_FLUSH_CYCLES; cycle++)
    {
        bench_refill_flush();
        uint32_t bytes_to_flush = (s_flush_ctx.write_pos >= s_flush_ctx.read_pos)
                                      ? (s_flush_ctx.write_pos - s_flush_ctx.read_pos)
                                      : (s_flush_ctx.capacity_bytes - s_flush_ctx.read_pos + s_flush_ctx.write_pos);

        bench_clock_now(&t0);
        xTRACE_Flush(&s_flush_ctx);
        bench_clock_now(&t1);
        total_ns += bench_clock_ns(&t0, &t1);
        total_bytes += (uint64_t)bytes_to_flush;
    }

    double mb_per_s = (double)total_bytes / (total_ns / 1e3);
    printf("  %-36s  %8.2f MB/s\n", "flush throughput  (null sink)", mb_per_s);
}

static void bench_run(const char *label, xTRACE_Context_t *ctx)
{
    bench_clock_t t0;
    bench_clock_t t1;

    for (uint32_t i = 0U; i < BENCH_WARMUP; i++)
    {
        xTRACE_Emit1(ctx, 0x20U, i);
    }

    bench_clock_now(&t0);
    for (uint32_t i = 0U; i < BENCH_ITERATIONS; i++)
    {
        xTRACE_Emit1(ctx, 0x20U, i);
    }
    bench_clock_now(&t1);

    double total_ns = bench_clock_ns(&t0, &t1);
    double ns_per_call = total_ns / (double)BENCH_ITERATIONS;
    double mcalls_per_s = (double)BENCH_ITERATIONS / (total_ns / 1e3);

    printf("  %-36s  %8.2f ns/call   %6.2f M calls/s\n", label, ns_per_call, mcalls_per_s);
}

// -- Main ---------------------------------------------------------------------

int main(void)
{
    xTRACE_Status_t status;
    memset(&status, 0, sizeof(status));

    bench_clock_init();

    printf("xTrace v2 LEB128 call-path overhead  (%u iterations, %u warmup)\n", BENCH_ITERATIONS, BENCH_WARMUP);
    printf("--------------------------------------------------------------------\n");

    bench_init_fast();
    bench_run("fast path  (buffer has space)", &s_fast_ctx);

    bench_init_drop();
    bench_run("drop path  (buffer full)", &s_drop_ctx);

    printf("\n");
    printf("xTrace v2 flush throughput  (%u cycles, %u-byte buffer)\n", BENCH_FLUSH_CYCLES, BENCH_FLUSH_BYTES);
    printf("--------------------------------------------------------------------\n");

    bench_flush_throughput();

    printf("--------------------------------------------------------------------\n");
    printf("  Context size : %u bytes\n", (unsigned)sizeof(xTRACE_Context_t));
    printf("  Buffer type  : uint8_t byte stream (LEB128 variable-length records)\n");
    printf("  Flush buffer : %u bytes\n", BENCH_FLUSH_BYTES);

    xTRACE_Get_Status(&s_drop_ctx, &status);
    printf("  Drop path confirms %u drops recorded\n", (unsigned)status.dropped_count);

    printf("\n  Disabled path (xTRACE_ENABLE=0): macros compile to (void)(ctx)/(void)(id)/...\n");
    printf("  -- zero overhead, no measurement needed.\n");

    return 0;
}

// EOF /////////////////////////////////////////////////////////////////////////////
