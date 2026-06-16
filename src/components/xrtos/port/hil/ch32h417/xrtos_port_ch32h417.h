// Copyright 2022 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xrtos_port_ch32h417.h
// @brief xRTOS CH32H417 QingKeV5F port declarations.

#ifndef XRTOS_PORT_CH32H417_H
#define XRTOS_PORT_CH32H417_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "xrtos_port.h"

#define XRTOS_PORT_CH32H417_STACK_ALIGN_BYTES 16U
#define XRTOS_PORT_CH32H417_MIN_STACK_WORDS   96U

typedef struct xRTOS_Port_CH32H417_Frame_t
{
    uint32_t mepc;
    uint32_t ra;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t s0;
    uint32_t s1;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t mstatus;
} xRTOS_Port_CH32H417_Frame_t;

extern const xRTOS_Port_Ops_t xrtos_ch32h417_port_ops;

void xRTOS_Port_CH32H417_Tick_ISR(void);
void xRTOS_Port_CH32H417_Switch_Context(void);
void xRTOS_Port_CH32H417_Start_First_Task(xRTOS_Task_Context_t *task_ctx);
void xRTOS_Port_CH32H417_Timer_Init(uint32_t hclk_hz, uint32_t tick_hz);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_PORT_CH32H417_H
// EOF /////////////////////////////////////////////////////////////////////////////
