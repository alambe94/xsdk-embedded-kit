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

// @file xrtos_blocking.h
// @brief Private helpers for common task blocking bookkeeping.
//
// Ownership of the wait_map bit:
//   xRTOS_Scheduler_Block_Current sets the task's bit in wait_map_ptr.
//   xRTOS_Scheduler_Unblock clears it via task_ctx->wait_map_ptr.
//   These helpers only own the timeout_map bit and cleanup fields.

#ifndef XRTOS_BLOCKING_H
#define XRTOS_BLOCKING_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_task.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    static inline void xrtos_blocking_payload_cleanup(uint32_t task_id, void *arg)
    {
        (void)task_id;

        xRTOS_Task_Context_t *task_ctx = (xRTOS_Task_Context_t *)arg;
        xASSERT(task_ctx != NULL, "task_ctx is NULL");

        xRTOS_Task_Block_Payload_Reset(&task_ctx->block_payload);
    }

    // Prepare the current task for a blocking wait. Arms the timeout and stores
    // the cleanup hook. Caller must hold the scheduler lock and must call
    // xRTOS_Scheduler_Block_Current after this setup succeeds.
    static inline void xrtos_blocking_prepare(xRTOS_Kernel_Context_t *kernel,
                                              xRTOS_Task_Context_t *task_ctx,
                                              uint32_t timeout_ticks,
                                              xRTOS_Task_Block_Cleanup_t cleanup,
                                              void *cleanup_arg)
    {
        xASSERT(kernel != NULL, "kernel is NULL");
        xASSERT(task_ctx != NULL, "task_ctx is NULL");
        xASSERT(task_ctx->task_id < xRTOS_MAX_TASKS, "task_id out of range");

        task_ctx->block_status = xRETURN_xRTOS_OK;
        task_ctx->block_cleanup = cleanup;
        task_ctx->block_cleanup_arg = cleanup_arg;
        xRTOS_Task_Block_Payload_Reset(&task_ctx->block_payload);

        if (timeout_ticks != xRTOS_WAIT_FOREVER)
        {
            task_ctx->wake_tick = kernel->tick_count + timeout_ticks;
            xRTOS_Bitmap_Set(&kernel->timeout_map, task_ctx->task_id);
        }
    }

    // Roll back the setup above after xRTOS_Scheduler_Block_Current fails.
    // The wait_map bit is never set at this point (Block_Current fails before
    // setting it), so only the timeout and cleanup state need reverting.
    static inline void xrtos_blocking_cancel(xRTOS_Kernel_Context_t *kernel, xRTOS_Task_Context_t *task_ctx)
    {
        xASSERT(kernel != NULL, "kernel is NULL");
        xASSERT(task_ctx != NULL, "task_ctx is NULL");
        xASSERT(task_ctx->task_id < xRTOS_MAX_TASKS, "task_id out of range");

        xRTOS_Bitmap_Clear(&kernel->timeout_map, task_ctx->task_id);
        task_ctx->block_cleanup = NULL;
        task_ctx->block_cleanup_arg = NULL;
        xRTOS_Task_Block_Payload_Reset(&task_ctx->block_payload);
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_BLOCKING_H
// EOF /////////////////////////////////////////////////////////////////////////////
