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

// @file xrtos_config.h
// @brief xRTOS build-time configuration: kernel knobs and per-module log levels.
//
// Override any constant before including xrtos headers, or pass -D definitions
// from the build system.
//
// Kernel configuration:
//   xRTOS_CONFIG_MAX_TASKS       Maximum task_id slots (default 32).
//   xRTOS_CONFIG_MAX_PRIORITIES  Number of priority levels (default 32).
//   xRTOS_CONFIG_MAX_TIMERS      Maximum timer_id slots (default: max tasks).
//   xRTOS_BITMAP_WIDTH           Backing bitmap width. Defaults to the maximum
//                                of task slots, priority levels, and timer slots.
//                                Legacy builds may still override it directly.
//   xRTOS_STACK_CANARY       Sentinel written to stack_mem[0] on task creation
//                            and checked on every context switch in debug builds.
//   xRTOS_CONFIG_MIN_STACK_WORDS  Minimum task stack depth in 32-bit words.
//
// Log level values (see xlog.h):
//   0 - silent: no code generated.
//   1 - status code only: prints [XXXXXXXX].
//   2 - code + message:   prints [XXXXXXXX] <message>.
//

#ifndef XRTOS_CONFIG_H
#define XRTOS_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////

    // -------------------------------------------------------------------------
    // Kernel configuration knobs
    // -------------------------------------------------------------------------

    // Maximum task IDs. If legacy xRTOS_BITMAP_WIDTH is overridden, keep the
    // old behavior unless a more specific max is provided.
#ifndef xRTOS_CONFIG_MAX_TASKS
#ifdef xRTOS_BITMAP_WIDTH
#define xRTOS_CONFIG_MAX_TASKS xRTOS_BITMAP_WIDTH
#else
#define xRTOS_CONFIG_MAX_TASKS 32U
#endif
#endif

    // Number of priority levels. Priority 0 is highest; the last priority is idle.
#ifndef xRTOS_CONFIG_MAX_PRIORITIES
#ifdef xRTOS_BITMAP_WIDTH
#define xRTOS_CONFIG_MAX_PRIORITIES xRTOS_BITMAP_WIDTH
#else
#define xRTOS_CONFIG_MAX_PRIORITIES 32U
#endif
#endif

    // Maximum software timers. Defaults to task capacity to keep small builds small.
#ifndef xRTOS_CONFIG_MAX_TIMERS
#ifdef xRTOS_BITMAP_WIDTH
#define xRTOS_CONFIG_MAX_TIMERS xRTOS_BITMAP_WIDTH
#else
#define xRTOS_CONFIG_MAX_TIMERS xRTOS_CONFIG_MAX_TASKS
#endif
#endif

    // Timer execution method.
#ifndef xRTOS_CONFIG_TIMER_METHOD
#define xRTOS_CONFIG_TIMER_METHOD xRTOS_TIMER_METHOD_TASK
#endif

    // Timer task configuration. Only active when xRTOS_CONFIG_TIMER_METHOD == 1.
#ifndef xRTOS_CONFIG_TIMER_TASK_PRIORITY
#define xRTOS_CONFIG_TIMER_TASK_PRIORITY 1U
#endif

#ifndef xRTOS_CONFIG_TIMER_TASK_STACK_WORDS
#define xRTOS_CONFIG_TIMER_TASK_STACK_WORDS 128U
#endif

#ifndef xRTOS_CONFIG_MAX_U32
#define xRTOS_CONFIG_MAX_U32(a, b) (((a) > (b)) ? (a) : (b)) // NOLINT(bugprone-branch-clone)
#endif

    // Width of every bitmap-backed registry. It must cover all configured
    // task IDs, priorities, and timer IDs.
#ifndef xRTOS_BITMAP_WIDTH
#define xRTOS_BITMAP_WIDTH                                                                                                                 \
    xRTOS_CONFIG_MAX_U32(xRTOS_CONFIG_MAX_TASKS,                                                                                           \
                         xRTOS_CONFIG_MAX_U32(xRTOS_CONFIG_MAX_PRIORITIES, xRTOS_CONFIG_MAX_TIMERS)) // NOLINT(bugprone-branch-clone)
#endif

    // Sentinel written to stack_mem[0] on task creation; checked on every
    // context switch in debug builds to detect stack overflow.
#ifndef xRTOS_STACK_CANARY
#define xRTOS_STACK_CANARY 0xDEADBEEFU
#endif

    // Minimum task stack depth in 32-bit words required by xRTOS_Task_Create.
#ifndef xRTOS_CONFIG_MIN_STACK_WORDS
#define xRTOS_CONFIG_MIN_STACK_WORDS 16U
#endif

    // Fill the entire stack with xRTOS_STACK_FILL_PATTERN at task creation so
    // that the high-water mark (minimum free stack depth) can be read at runtime
    // via xRTOS_Task_Get_Stack_Watermark().  Disabled by default because the
    // fill loop adds O(stack_words) work at task creation time.
#ifndef xRTOS_CONFIG_STACK_WATERMARK_ENABLE
#define xRTOS_CONFIG_STACK_WATERMARK_ENABLE 0
#endif

    // Pattern written to every stack word when watermarking is enabled.
    // Must differ from xRTOS_STACK_CANARY so the canary check is unaffected.
#ifndef xRTOS_STACK_FILL_PATTERN
#define xRTOS_STACK_FILL_PATTERN 0xA5A5A5A5U
#endif

    // Enable per-task CPU tick counting.  Each tick the running task's
    // ticks_running counter is incremented; use xRTOS_Task_Get_CPU_Stats() to
    // read it and xRTOS_CPU_Stats_Reset_All() to reset all counters.
    // Disabled by default.
#ifndef xRTOS_CONFIG_CPU_STATS_ENABLE
#define xRTOS_CONFIG_CPU_STATS_ENABLE 0
#endif

    // Enable automatic round-robin time slicing among tasks that share the same
    // base priority.
    //
    // Multiple tasks at the same base priority are always permitted regardless
    // of this setting - the ready list handles them in FIFO order.
    //
    // When enabled:
    //   The tick handler requests a context switch each tick whenever another
    //   task is ready at the same effective priority, cycling them automatically.
    // When disabled (default):
    //   Scheduling among equal-priority tasks is cooperative. The application
    //   is responsible for yielding via xRTOS_Task_Delay(0) or blocking APIs
    //   to give peer tasks at the same priority a chance to run.
#ifndef xRTOS_CONFIG_ROUND_ROBIN_ENABLE
#define xRTOS_CONFIG_ROUND_ROBIN_ENABLE 0
#endif

    // -------------------------------------------------------------------------
    // Log level
    // -------------------------------------------------------------------------

#ifndef xRTOS_CONFIG_LOG_LEVEL
#define xRTOS_CONFIG_LOG_LEVEL 0
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
