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

// @file xrtos_timer.h
// @brief xRTOS lightweight software timers executed in the tick ISR.
//
// Timer callbacks run directly in the tick ISR context.  They must be
// extremely short, non-blocking, and must never call blocking APIs
// (semaphore take, mutex lock, task delay, etc.).
//
// Slot assignment is static: the caller picks a timer_id in [0, xRTOS_MAX_TIMERS)
// and passes it in xRTOS_Timer_Config_t.  xRTOS_Timer_Init rejects
// timer_ids that are already occupied in the kernel timer_table.
//
// Usage:
//
//   static xRTOS_Timer_Context_t my_timer;
//
//   static void on_tick(void *arg)
//   {
//       (void)arg;
//       // fast ISR-context work only
//   }
//
//   xRTOS_Timer_Config_t cfg = {
//       .timer_id     = 0U,
//       .callback     = on_tick,
//       .callback_arg = NULL,
//       .period_ticks = 100U,
//       .is_periodic  = true,
//   };
//   xRTOS_Timer_Init(&my_timer, &cfg);
//   xRTOS_Timer_Start(&my_timer);
//

#ifndef XRTOS_TIMER_H
#define XRTOS_TIMER_H

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

    typedef void (*xRTOS_Timer_Callback_t)(void *arg);

    typedef struct xRTOS_Timer_Config_t
    {
        uint32_t timer_id;               // Caller-assigned slot index: 0 to xRTOS_MAX_TIMERS-1.
        xRTOS_Timer_Callback_t callback; // Function invoked on expiry (must not be NULL).
        void *callback_arg;              // Passed verbatim to callback; may be NULL.
        uint32_t period_ticks;           // Ticks between Start and first fire (and between reloads).
        bool is_periodic;                // true: auto-reload; false: one-shot.
        const char *name;                // Optional display name (NULL = unnamed). Pointer must remain valid for kernel lifetime.

    } xRTOS_Timer_Config_t;

    typedef struct xRTOS_Timer_Context_t
    {
        xRTOS_Timer_Callback_t callback;
        void *callback_arg;
        uint32_t period_ticks;
        uint32_t expiry_tick; // tick_count value at which the timer fires.
        bool is_periodic;
        uint32_t timer_id;
        const char *name; // Optional display name supplied at creation (NULL if unnamed).

    } xRTOS_Timer_Context_t;

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Populate timer_ctx from config and register it in the kernel timer_table.
    // Returns xRETURN_xERR_xRTOS_NULL_POINTER  if timer_ctx, config, or
    //   config->callback is NULL.
    // Returns xRETURN_xERR_xRTOS_INVALID_ARGUMENT if timer_id >= xRTOS_MAX_TIMERS,
    //   period_ticks is zero, or the slot timer_table[timer_id] is already occupied.
    // Returns xRETURN_xERR_xRTOS_INVALID_STATE if the kernel is not initialized.
    // Timer is left inactive after Init; call Start to arm it.
    xRETURN_t xRTOS_Timer_Init(xRTOS_Timer_Context_t *timer_ctx, const xRTOS_Timer_Config_t *config);

    // Arm the timer: compute expiry_tick = tick_count + period_ticks and set the
    // bit in active_timers_map.
    // Returns xRETURN_xERR_xRTOS_NULL_POINTER if timer_ctx is NULL.
    // Returns xRETURN_xERR_xRTOS_INVALID_STATE if the kernel is not initialized.
    // Returns xRETURN_xERR_xRTOS_INVALID_ARGUMENT if timer_ctx is not registered.
    // Calling Start on an already-active timer resets its deadline.
    xRETURN_t xRTOS_Timer_Start(xRTOS_Timer_Context_t *timer_ctx);

    // Disarm the timer: clear the bit from active_timers_map.
    // The callback will not fire until Start is called again.
    // Returns xRETURN_xERR_xRTOS_NULL_POINTER if timer_ctx is NULL.
    // Returns xRETURN_xERR_xRTOS_INVALID_STATE if the kernel is not initialized.
    // Returns xRETURN_xERR_xRTOS_INVALID_ARGUMENT if timer_ctx is not registered.
    xRETURN_t xRTOS_Timer_Stop(xRTOS_Timer_Context_t *timer_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_TIMER_H
// EOF /////////////////////////////////////////////////////////////////////////////
