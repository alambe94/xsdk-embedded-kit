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

// @file xrtos_task.h
// @brief xRTOS task context, configuration, states, and task lifecycle API.
//
// xRTOS_Task_Context_t is caller-owned storage for task runtime state.
// xRTOS_Task_Config_t is consumed once at creation and not retained by the kernel.
//
// The port layer owns the stack_top field; portable kernel code shall not
// read or write CPU register layout directly.
//

#ifndef XRTOS_TASK_H
#define XRTOS_TASK_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xrtos_defs.h"
#include "xrtos_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    struct xRTOS_Mutex_Context_t;

    typedef enum xRTOS_Task_State_t
    {
        xRTOS_TASK_STATE_READY = 0U,
        xRTOS_TASK_STATE_RUNNING = 1U,
        xRTOS_TASK_STATE_BLOCKED = 2U,
        xRTOS_TASK_STATE_SUSPENDED = 3U,
        xRTOS_TASK_STATE_TERMINATED = 4U,
    } xRTOS_Task_State_t;

    // Task entry function pointer type.
    typedef void (*xRTOS_Task_Entry_t)(void *arg);

    // Optional cleanup hook invoked by the tick timeout path before unblocking
    // a timed-out task. Used by primitives that maintain object-side wait state.
    typedef void (*xRTOS_Task_Block_Cleanup_t)(uint32_t task_id, void *arg);

    typedef struct xRTOS_Task_Block_Event_t
    {
        uint32_t wait_mask;
        uint32_t wait_options;

    } xRTOS_Task_Block_Event_t;

    typedef struct xRTOS_Task_Block_Notify_t
    {
        uint32_t value;
        uint32_t clear_on_exit;

    } xRTOS_Task_Block_Notify_t;

    // Primitive-owned payload stored while a task is blocked. Queue send/receive
    // and event/notify wait state use this instead of object-owned per-task side
    // tables or permanent per-task fields.
    typedef union xRTOS_Task_Block_Payload_t
    {
        const void *const_ptr;
        void *ptr;
        xRTOS_Task_Block_Event_t event;
        xRTOS_Task_Block_Notify_t notify;

    } xRTOS_Task_Block_Payload_t;

    // Caller-supplied task description. Read once at creation; not retained.
    typedef struct xRTOS_Task_Config_t
    {
        uint32_t task_id;
        uint32_t priority;
        xRTOS_Task_Entry_t entry;
        void *entry_arg;
        uint32_t *stack_mem;
        uint32_t stack_words;
        const char *name; // Optional display name (NULL = unnamed). Pointer must remain valid for kernel lifetime.

    } xRTOS_Task_Config_t;

    // Caller-owned task runtime state. All fields are initialized by
    // xRTOS_Task_Create; callers shall not modify them directly.
    typedef struct xRTOS_Task_Context_t
    {
        // Stack boundaries. port_ops->init_task_stack sets stack_top to the
        // initial CPU-ready frame. Portable code shall not read register layout.
        uint32_t *stack_top;
        uint32_t *stack_mem;

        // Timeout tracking: set when entering a timed wait; cleared on wake or expiry.
        xRTOS_Bitmap_t *wait_map_ptr; // Points to the blocking object's wait_map, or NULL.
        xRTOS_Task_Block_Cleanup_t block_cleanup;
        void *block_cleanup_arg;
        xRTOS_Task_Block_Payload_t block_payload;

        // Intrusive ready-list links owned by the scheduler. Tasks are linked
        // by effective_priority while in READY state.
        struct xRTOS_Task_Context_t *ready_prev;
        struct xRTOS_Task_Context_t *ready_next;

        // Mutex PI bookkeeping. A task can wait on at most one mutex at a time,
        // and can own multiple mutexes.
        struct xRTOS_Mutex_Context_t *held_mutex_head;
        struct xRTOS_Mutex_Context_t *mutex_waiting_on;
        struct xRTOS_Task_Context_t *mutex_wait_prev;
        struct xRTOS_Task_Context_t *mutex_wait_next;

        // Optional display name supplied at creation (NULL if unnamed).
        const char *name;

        // 32-bit fields and enums
        uint32_t task_id;
        uint32_t base_priority;
        uint32_t effective_priority;
        uint32_t stack_words;
        uint32_t wake_tick;
        xRETURN_t block_status; // Written by waker; read by blocked task on wake.

        // Task notification state.
        uint32_t notify_value;

        // 8-bit fields. Stored compactly because each task owns one context.
        uint8_t state;
        bool has_notify_pending;

#if xRTOS_CONFIG_CPU_STATS_ENABLE
        uint32_t ticks_running; // Ticks spent in RUNNING state since last xRTOS_CPU_Stats_Reset_All().
#endif

    } xRTOS_Task_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    static inline void xRTOS_Task_Block_Payload_Reset(xRTOS_Task_Block_Payload_t *payload)
    {
        payload->event.wait_mask = 0U;
        payload->event.wait_options = 0U;
    }

    static inline bool xRTOS_Task_Stack_Is_Valid(const xRTOS_Task_Context_t *task_ctx)
    {
        xASSERT(task_ctx != NULL, "task_ctx is NULL");
        return (task_ctx->stack_mem[0U] == xRTOS_STACK_CANARY) && (task_ctx->stack_top >= task_ctx->stack_mem) &&
               (task_ctx->stack_top <= (task_ctx->stack_mem + task_ctx->stack_words));
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xRTOS_Task_Create(xRTOS_Task_Context_t *task_ctx, const xRTOS_Task_Config_t *task_config);

    // Self-termination: transitions to TERMINATED and requests a reschedule.
    // The port layer shall set the initial stack return address to this function
    // so that a task returning from its entry function terminates cleanly.
    // This function does not return.
    void xRTOS_Task_Exit(void);

    // Cooperative yield: moves the calling task to the tail of its priority's
    // ready list and requests a context switch. The next task at the same
    // effective priority runs next. If no peer is ready the same task resumes.
    // Must not be called from ISR context. In debug builds this asserts; in
    // release builds it returns without requesting a port yield.
    void xRTOS_Task_Yield(void);

    // Change the base priority (and effective priority when not PI-boosted) of
    // any registered task. If the task is READY it is moved between ready lists.
    // new_priority must be a valid user priority (not IDLE_PRIORITY).
    // Returns xRETURN_xERR_xRTOS_INVALID_ARGUMENT for out-of-range priorities.
    xRETURN_t xRTOS_Task_Set_Priority(uint32_t task_id, uint32_t new_priority);

    // Return the number of stack words that still hold xRTOS_STACK_FILL_PATTERN,
    // i.e., words never written since task creation - the minimum free stack depth.
    // Requires xRTOS_CONFIG_STACK_WATERMARK_ENABLE=1; returns
    // xRETURN_xERR_xRTOS_INVALID_STATE and *words_free=0 when disabled.
    xRETURN_t xRTOS_Task_Get_Stack_Watermark(uint32_t task_id, uint32_t *words_free);

    // Snapshot of per-task CPU usage counters.
    typedef struct xRTOS_Task_CPU_Stats_t
    {
        uint32_t ticks_running; // Ticks the task spent RUNNING since last xRTOS_CPU_Stats_Reset_All().
        uint32_t total_ticks;   // Total ticks elapsed since last xRTOS_CPU_Stats_Reset_All().
    } xRTOS_Task_CPU_Stats_t;

    // Read CPU usage for one task into *out.
    // Requires xRTOS_CONFIG_CPU_STATS_ENABLE=1; returns
    // xRETURN_xERR_xRTOS_INVALID_STATE and zeroes *out when disabled.
    xRETURN_t xRTOS_Task_Get_CPU_Stats(uint32_t task_id, xRTOS_Task_CPU_Stats_t *out);

    // Zero every task's ticks_running counter and the global elapsed counter.
    // Requires xRTOS_CONFIG_CPU_STATS_ENABLE=1; no-op when disabled.
    void xRTOS_CPU_Stats_Reset_All(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_TASK_H
// EOF /////////////////////////////////////////////////////////////////////////////
