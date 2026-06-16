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

// @file xrtos_scheduler.h
// @brief xRTOS scheduler context - priority-indexed ready lists plus task maps.
//
// Bitmap layout rule:
//   Bit T of ready_map, blocked_map, and suspended_map corresponds to task_id T.
//   Bit P of ready_priority_map corresponds to effective priority P and is set
//   when ready_head[P] is non-NULL.
//   Lower numeric effective_priority means higher scheduling urgency.
//

#ifndef XRTOS_SCHEDULER_H
#define XRTOS_SCHEDULER_H

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

    struct xRTOS_Task_Context_t;

    typedef struct xRTOS_Scheduler_Context_t
    {
        xRTOS_Bitmap_t ready_map;          // Bit T set: task_id T is ready to run.
        xRTOS_Bitmap_t blocked_map;        // Bit T set: task_id T is blocked.
        xRTOS_Bitmap_t suspended_map;      // Bit T set: task_id T is suspended.
        xRTOS_Bitmap_t ready_priority_map; // Bit P set: at least one ready task at effective priority P.

        struct xRTOS_Task_Context_t *ready_head[xRTOS_MAX_PRIORITIES]; // Indexed by effective priority.
        struct xRTOS_Task_Context_t *ready_tail[xRTOS_MAX_PRIORITIES]; // Indexed by effective priority.

        uint32_t current_task_id;  // Task ID of the currently running task.
        uint32_t next_task_id;     // Task ID selected for the next context switch.
        uint32_t current_priority; // Effective priority of current_task_id.
        uint32_t next_priority;    // Effective priority of next_task_id.

        bool is_started;          // True after xRTOS_Kernel_Start succeeds.
        bool is_schedule_pending; // True when a context switch has been requested.

    } xRTOS_Scheduler_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Select the highest-priority ready task in O(1) over priority words and
    // store its task ID in
    // scheduler.next_task_id. Returns xRETURN_xRTOS_OK on success or
    // xRETURN_xERR_xRTOS_NO_TASKS_READY when ready_priority_map is empty.
    xRETURN_t xRTOS_Scheduler_Select_Next(void);

    // Return the currently running task context, or NULL if no current task is
    // established yet.
    struct xRTOS_Task_Context_t *xRTOS_Scheduler_Current_Task(void);

    // Block the currently running task on wait_map_ptr. Clears the task's bit
    // from ready_map, sets it in blocked_map, stores wait_map_ptr in the task
    // context, and sets is_schedule_pending.
    // Pass NULL for wait_map_ptr to block without associating a wait object
    // (used for task notification wait).
    xRETURN_t xRTOS_Scheduler_Block_Current(xRTOS_Bitmap_t *wait_map_ptr);

    // Unblock the task at the given task_id with block_status written into
    // task_ctx->block_status. Clears the bit from blocked_map, sets it in
    // ready_map, and sets is_schedule_pending when the unblocked task has a
    // strictly higher priority (lower numeric value) than current_priority.
    xRETURN_t xRTOS_Scheduler_Unblock(uint32_t task_id, xRETURN_t block_status);

    // Commit the context switch selected by xRTOS_Scheduler_Select_Next.
    // Transitions the previous task to READY (unless TERMINATED/SUSPENDED),
    // transitions the next task to RUNNING, updates current_priority, and
    // clears is_schedule_pending. Triggers xASSERT on stack canary failure.
    void xRTOS_Scheduler_Switch(void);

    // Normal-wake helper for sync primitives (semaphore give, mutex unlock, etc.).
    // Selects the highest-priority waiter in wait_map, removes it from wait_map and
    // timeout_map, and unblocks it with block_status xRETURN_xRTOS_OK.
    // If unblocked_task_id is non-NULL it receives the task ID that was woken.
    // Returns xRETURN_xERR_xRTOS_NO_TASKS_READY when wait_map is empty.
    // Callers hold the scheduler lock (critical section) across this call.
    xRETURN_t xRTOS_Scheduler_Unblock_From_Wait_Map(xRTOS_Bitmap_t *wait_map, uint32_t *unblocked_task_id);

    // Find the task_id in task_map with the highest scheduling urgency
    // (lowest effective_priority). The map is task-id-indexed and is not modified.
    xRETURN_t xRTOS_Scheduler_Find_Highest_Task_In_Map(const xRTOS_Bitmap_t *task_map, uint32_t *task_id);

    // Update a task's effective priority. Used by mutex priority inheritance.
    xRETURN_t xRTOS_Scheduler_Set_Effective_Priority(uint32_t task_id, uint32_t effective_priority);

    // Disable interrupts via port_ops and return the saved state. Callers must
    // pair every Lock with exactly one Unlock.
    uint32_t xRTOS_Scheduler_Lock(void);

    // Restore interrupt state saved by xRTOS_Scheduler_Lock.
    void xRTOS_Scheduler_Unlock(uint32_t saved_state);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_SCHEDULER_H
// EOF /////////////////////////////////////////////////////////////////////////////
