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

// @file xrtos_trace.h
// @brief xRTOS trace event IDs (flat integers, LEB128 wire encoding).
//
// Call sites use xRTOS_TRACE_E* macros which add the per-module gate.
// Set xRTOS_TRACE_ENABLE=0 to strip all xRTOS trace calls module-wide.
// xRTOS event IDs are allocated from xTRACE_BASE_xRTOS in xtrace_registry.h.
// User events start at xTRACE_BASE_USER.
//
// Key events for Perfetto per-task timeline reconstruction:
//   TASK_SWITCH carries prev_task_id AND next_task_id so the decoder
//     can reconstruct RUNNING/READY/BLOCKED rows for every task.
//   MUTEX_HANDOFF carries prev_owner AND new_owner.
//   TASK_PRIO carries task_id AND new_priority (was one packed word).
//   TASK_CREATE carries task_id AND priority.
//

#ifndef XRTOS_TRACE_H
#define XRTOS_TRACE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xtrace_config.h"
#include "xtrace_registry.h"

#ifndef xRTOS_TRACE_ENABLE
#define xRTOS_TRACE_ENABLE xTRACE_ENABLE
#endif

#if xTRACE_ENABLE && xRTOS_TRACE_ENABLE
#include "xtrace.h"
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

#define xRTOS_TRACE_CODE_KERNEL_START                                                                                                      \
    (xTRACE_BASE_xRTOS + 0x00U) /// @trace {"type": "instant", "track": "xRTOS/Kernel", "args": ["first_task_id"]}
#define xRTOS_TRACE_CODE_TASK_CREATE                                                                                                       \
    (xTRACE_BASE_xRTOS + 0x01U) /// @trace {"type": "instant", "track": "xRTOS/Task", "args": ["task_id", "priority"]}
#define xRTOS_TRACE_CODE_TASK_SWITCH                                                                                                       \
    (xTRACE_BASE_xRTOS + 0x02U) /// @trace {"type": "instant", "track": "xRTOS/Task", "args": ["prev_task_id", "next_task_id"]}
#define xRTOS_TRACE_CODE_TASK_READY (xTRACE_BASE_xRTOS + 0x03U) /// @trace {"type": "instant", "track": "xRTOS/Task", "args": ["task_id"]}
#define xRTOS_TRACE_CODE_TASK_BLOCK                                                                                                        \
    (xTRACE_BASE_xRTOS + 0x04U) /// @trace {"type": "instant", "track": "xRTOS/Task", "args": ["task_id", "wait_object_ptr"]}
#define xRTOS_TRACE_CODE_TASK_EXIT    (xTRACE_BASE_xRTOS + 0x05U) /// @trace {"type": "instant", "track": "xRTOS/Task", "args": ["task_id"]}
#define xRTOS_TRACE_CODE_TASK_TIMEOUT (xTRACE_BASE_xRTOS + 0x06U) /// @trace {"type": "instant", "track": "xRTOS/Task", "args": ["task_id"]}
#define xRTOS_TRACE_CODE_TASK_PRIO                                                                                                         \
    (xTRACE_BASE_xRTOS + 0x07U) /// @trace {"type": "instant", "track": "xRTOS/Task", "args": ["task_id", "new_priority"]}
#define xRTOS_TRACE_CODE_TASK_NOTIFY                                                                                                       \
    (xTRACE_BASE_xRTOS + 0x08U) /// @trace {"type": "instant", "track": "xRTOS/Task", "args": ["target_task_id"]}
#define xRTOS_TRACE_CODE_TICK       (xTRACE_BASE_xRTOS + 0x09U) /// @trace {"type": "counter", "track": "xRTOS/Tick", "args": ["tick_count"]}
#define xRTOS_TRACE_CODE_TIMER_FIRE (xTRACE_BASE_xRTOS + 0x0AU) /// @trace {"type": "instant", "track": "xRTOS/Timer", "args": ["timer_id"]}

    // xRTOS event ID constants - synchronization primitives (base + 0x0B..0x15)
#define xRTOS_TRACE_CODE_SEM_GIVE (xTRACE_BASE_xRTOS + 0x0BU) /// @trace {"type": "counter", "track": "xRTOS/Sem", "args": ["count_after"]}
#define xRTOS_TRACE_CODE_SEM_TAKE (xTRACE_BASE_xRTOS + 0x0CU) /// @trace {"type": "counter", "track": "xRTOS/Sem", "args": ["count_after"]}
#define xRTOS_TRACE_CODE_MUTEX_LOCK                                                                                                        \
    (xTRACE_BASE_xRTOS + 0x0DU) /// @trace {"type": "begin", "track": "xRTOS/Mutex", "args": ["owner_task_id"]}
#define xRTOS_TRACE_CODE_MUTEX_UNLOCK                                                                                                      \
    (xTRACE_BASE_xRTOS + 0x0EU) /// @trace {"type": "end", "track": "xRTOS/Mutex", "args": ["releasing_task_id"]}
#define xRTOS_TRACE_CODE_MUTEX_HANDOFF                                                                                                     \
    (xTRACE_BASE_xRTOS + 0x0FU) /// @trace {"type": "instant", "track": "xRTOS/Mutex", "args": ["prev_owner", "new_owner"]}
#define xRTOS_TRACE_CODE_EVENT_SET                                                                                                         \
    (xTRACE_BASE_xRTOS + 0x10U) /// @trace {"type": "instant", "track": "xRTOS/Event", "args": ["flags_after_or"]}
#define xRTOS_TRACE_CODE_EVENT_WAIT                                                                                                        \
    (xTRACE_BASE_xRTOS + 0x11U) /// @trace {"type": "instant", "track": "xRTOS/Event", "args": ["matched_flags"]}
#define xRTOS_TRACE_CODE_QUEUE_SEND                                                                                                        \
    (xTRACE_BASE_xRTOS + 0x12U) /// @trace {"type": "counter", "track": "xRTOS/Queue", "args": ["used_count"]}
#define xRTOS_TRACE_CODE_QUEUE_RECV                                                                                                        \
    (xTRACE_BASE_xRTOS + 0x13U) /// @trace {"type": "counter", "track": "xRTOS/Queue", "args": ["used_count"]}
#define xRTOS_TRACE_CODE_TIMER_START   (xTRACE_BASE_xRTOS + 0x14U) /// @trace {"type": "begin", "track": "xRTOS/Timer", "args": ["timer_id"]}
#define xRTOS_TRACE_CODE_TIMER_STOP    (xTRACE_BASE_xRTOS + 0x15U) /// @trace {"type": "end", "track": "xRTOS/Timer", "args": ["timer_id"]}
#define xRTOS_TRACE_CODE_ISR_ENTER     (xTRACE_BASE_xRTOS + 0x16U) /// @trace {"type": "begin", "track": "xRTOS/ISR", "args": ["vector_num"]}
#define xRTOS_TRACE_CODE_ISR_EXIT      (xTRACE_BASE_xRTOS + 0x17U) /// @trace {"type": "end", "track": "xRTOS/ISR", "args": ["vector_num"]}
#define xRTOS_TRACE_CODE_STACK_CORRUPT (xTRACE_BASE_xRTOS + 0x18U) /// @trace {"type": "error", "track": "xRTOS/Task", "args": ["task_id"]}
#define xRTOS_TRACE_CODE_QUEUE_FULL                                                                                                        \
    (xTRACE_BASE_xRTOS + 0x19U)                               /// @trace {"type": "instant", "track": "xRTOS/Queue", "args": ["queue_ptr"]}
#define xRTOS_TRACE_CODE_SEM_FULL (xTRACE_BASE_xRTOS + 0x1AU) /// @trace {"type": "instant", "track": "xRTOS/Sem", "args": ["sem_ptr"]}
#define xRTOS_TRACE_CODE_OBJECT_NAME                                                                                                       \
    (xTRACE_BASE_xRTOS + 0x1BU) /// @trace {"type": "instant", "track": "xRTOS/Names", "args": ["obj_type", "obj_id"]}

    // Object type codes for xRTOS_TRACE_CODE_OBJECT_NAME records.
    typedef enum
    {
        xRTOS_TRACE_OBJ_TASK = 0U,
        xRTOS_TRACE_OBJ_SEM = 1U,
        xRTOS_TRACE_OBJ_MUTEX = 2U,
        xRTOS_TRACE_OBJ_QUEUE = 3U,
        xRTOS_TRACE_OBJ_EVENT = 4U,
        xRTOS_TRACE_OBJ_TIMER = 5U,
    } xRTOS_TRACE_ObjType_t;

// Per-module emit macros - gated on xRTOS_TRACE_ENABLE.
#if xTRACE_ENABLE && xRTOS_TRACE_ENABLE
    // clang-format off
#define xRTOS_TRACE_E0(k, id)             xTRACE_E0((k)->trace_ctx, (id))
#define xRTOS_TRACE_E1(k, id, a)          xTRACE_E1((k)->trace_ctx, (id), (uint32_t)(a))
#define xRTOS_TRACE_E2(k, id, a, b)       xTRACE_E2((k)->trace_ctx, (id), (uint32_t)(a), (uint32_t)(b))
#define xRTOS_TRACE_E3(k, id, a, b, c)    xTRACE_E3((k)->trace_ctx, (id), (uint32_t)(a), (uint32_t)(b), (uint32_t)(c))
#define xRTOS_TRACE_NAME(k, obj_type, obj_id, name) \
    do { if ((k) != NULL) { xTRACE_EmitName((k)->trace_ctx, xRTOS_TRACE_CODE_OBJECT_NAME, (uint32_t)(obj_type), (uint32_t)(obj_id), (name)); } } while (0)
// clang-format on
#else
#define xRTOS_TRACE_E0(k, id)                                                                                                              \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(k);                                                                                                                         \
        (void)(id);                                                                                                                        \
    } while (0)
#define xRTOS_TRACE_E1(k, id, a)                                                                                                           \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(k);                                                                                                                         \
        (void)(id);                                                                                                                        \
        (void)(a);                                                                                                                         \
    } while (0)
#define xRTOS_TRACE_E2(k, id, a, b)                                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(k);                                                                                                                         \
        (void)(id);                                                                                                                        \
        (void)(a);                                                                                                                         \
        (void)(b);                                                                                                                         \
    } while (0)
#define xRTOS_TRACE_E3(k, id, a, b, c)                                                                                                     \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(k);                                                                                                                         \
        (void)(id);                                                                                                                        \
        (void)(a);                                                                                                                         \
        (void)(b);                                                                                                                         \
        (void)(c);                                                                                                                         \
    } while (0)
#define xRTOS_TRACE_NAME(k, obj_type, obj_id, name)                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(k);                                                                                                                         \
        (void)(obj_type);                                                                                                                  \
        (void)(obj_id);                                                                                                                    \
        (void)(name);                                                                                                                      \
    } while (0)
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_TRACE_H
// EOF /////////////////////////////////////////////////////////////////////////////
