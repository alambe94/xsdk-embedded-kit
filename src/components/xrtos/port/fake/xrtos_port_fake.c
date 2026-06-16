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

// @file xrtos_port_fake.c
// @brief xRTOS host (PC) port - stub implementations of xRTOS_Port_Ops_t.
//
// All operations are no-ops or minimal stubs. The host port exists only to
// allow kernel unit tests to build and run on the development host without
// an ARM toolchain or target hardware.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xrtos_port_fake.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static void host_init_task_stack(xRTOS_Task_Context_t *task_ctx, xRTOS_Task_Entry_t entry, void *arg);
static void host_start_first_task(xRTOS_Task_Context_t *task_ctx);
static void host_yield(void);
static uint32_t host_disable_interrupts(void);
static void host_enable_interrupts(uint32_t saved_state);
static bool host_is_in_isr(void);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static void host_init_task_stack(xRTOS_Task_Context_t *task_ctx, xRTOS_Task_Entry_t entry, void *arg)
{
    // Host: no real stack frame is built. Point stack_top to the top of the
    // stack buffer so boundary checks in tests pass.
    (void)entry;
    (void)arg;
    task_ctx->stack_top = task_ctx->stack_mem + task_ctx->stack_words;
}

static void host_start_first_task(xRTOS_Task_Context_t *task_ctx)
{
    // Host: no real context restore. Returns immediately; the kernel's
    // for(;;) after this call handles the host case gracefully.
    (void)task_ctx;
}

static void host_yield(void)
{
    // Host: no real context switch. A real port would pend an interrupt here.
}

static uint32_t host_disable_interrupts(void)
{
    // Host: no real interrupt masking. Returns a dummy saved state.
    return 0U;
}

static void host_enable_interrupts(uint32_t saved_state)
{
    // Host: no real interrupt restoration.
    (void)saved_state;
}

static bool host_is_in_isr(void)
{
    // Host: no real interrupt context. Always returns false.
    return false;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

const xRTOS_Port_Ops_t xrtos_fake_port_ops = {
    .init_task_stack = host_init_task_stack,
    .start_first_task = host_start_first_task,
    .yield = host_yield,
    .disable_interrupts = host_disable_interrupts,
    .enable_interrupts = host_enable_interrupts,
    .is_in_isr = host_is_in_isr,
};

// EOF /////////////////////////////////////////////////////////////////////////////
