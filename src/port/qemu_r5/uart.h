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

// @file uart.h
// @brief Minimal UART output helpers for ARM R5 QEMU smoke tests.
//
// Define BOARD_REALVIEW_PB_A8 or BOARD_ZYNQ_A9 before including (or via the
// compiler command line). Defaults to BOARD_ZYNQ_A9 when neither is defined.
//
// All functions are static inline so this header can be included from
// multiple translation units without link errors.

#ifndef XSDK_PORT_ARM_R5_QEMU_UART_H
#define XSDK_PORT_ARM_R5_QEMU_UART_H

#include <stdint.h>

#if !defined(BOARD_ZYNQ_A9) && !defined(BOARD_REALVIEW_PB_A8)
#define BOARD_ZYNQ_A9
#endif

// ---- Zynq-7000 / ZynqMP UART (Cadence UART) --------------------------------
#ifdef BOARD_ZYNQ_A9

#define XSDK_UART_CR_OFFSET   0x00U
#define XSDK_UART_SR_OFFSET   0x2CU
#define XSDK_UART_FIFO_OFFSET 0x30U
#define XSDK_ZYNQMP_UART_BASE 0xFF000000U
#define XSDK_ZYNQ_UART_BASE   0xE0000000U

static inline void uart_init(void)
{
    *(volatile uint32_t *)(XSDK_ZYNQMP_UART_BASE + XSDK_UART_CR_OFFSET) = 0x14U;
    *(volatile uint32_t *)(XSDK_ZYNQ_UART_BASE   + XSDK_UART_CR_OFFSET) = 0x14U;
}

static inline void uart_putc(char c)
{
    volatile uint32_t *sr;
    volatile uint32_t *fifo;
    uint32_t t;

    sr   = (volatile uint32_t *)(XSDK_ZYNQMP_UART_BASE + XSDK_UART_SR_OFFSET);
    fifo = (volatile uint32_t *)(XSDK_ZYNQMP_UART_BASE + XSDK_UART_FIFO_OFFSET);
    t = 1000000U;
    while (((*sr & 0x10U) != 0U) && (t > 0U)) { t--; }
    if (t > 0U) { *fifo = (uint32_t)c; }

    sr   = (volatile uint32_t *)(XSDK_ZYNQ_UART_BASE + XSDK_UART_SR_OFFSET);
    fifo = (volatile uint32_t *)(XSDK_ZYNQ_UART_BASE + XSDK_UART_FIFO_OFFSET);
    t = 1000000U;
    while (((*sr & 0x10U) != 0U) && (t > 0U)) { t--; }
    if (t > 0U) { *fifo = (uint32_t)c; }
}

// ---- RealView PB-A8 PL011 UART ---------------------------------------------
#elif defined(BOARD_REALVIEW_PB_A8)

// UART0 (0x10009000) - text output -> QEMU -serial mon:stdio
#define XSDK_PL011_BASE    0x10009000U
#define XSDK_UART_DR       (*(volatile uint32_t *)(XSDK_PL011_BASE + 0x00U))
#define XSDK_UART_FR       (*(volatile uint32_t *)(XSDK_PL011_BASE + 0x18U))
#define XSDK_UART_FR_TXFF  (1U << 5U)

// UART1 (0x1000A000) - binary xTRACE records -> QEMU -serial file:trace.bin
#define XSDK_PL011_TRACE_BASE   0x1000A000U
#define XSDK_UART_TRACE_DR      (*(volatile uint32_t *)(XSDK_PL011_TRACE_BASE + 0x00U))
#define XSDK_UART_TRACE_FR      (*(volatile uint32_t *)(XSDK_PL011_TRACE_BASE + 0x18U))

static inline void uart_init(void)
{
    // QEMU PL011 is pre-initialized; nothing to do.
}

static inline void uart_putc(char c)
{
    uint32_t t = 1000000U;
    while (((XSDK_UART_FR & XSDK_UART_FR_TXFF) != 0U) && (t > 0U)) { t--; }
    XSDK_UART_DR = (uint32_t)c;
}

// Write one raw byte to UART1 (xTRACE binary stream).
static inline void uart_trace_putb(uint8_t b)
{
    uint32_t t = 1000000U;
    while (((XSDK_UART_TRACE_FR & XSDK_UART_FR_TXFF) != 0U) && (t > 0U)) { t--; }
    XSDK_UART_TRACE_DR = (uint32_t)b;
}
#define XSDK_UART1_AVAILABLE 1

#endif // board selection

// Stub for boards that have no second UART - trace bytes are silently dropped.
#ifndef XSDK_UART1_AVAILABLE
static inline void uart_trace_putb(uint8_t b) { (void)b; }
#endif

// ---- Board-agnostic helpers ------------------------------------------------

static inline void uart_puts(const char *s)
{
    while (*s != '\0')
    {
        if (*s == '\n') { uart_putc('\r'); }
        uart_putc(*s);
        s++;
    }
}

static inline void uart_puti(uint32_t v)
{
    char buf[12];
    int  i = 11;
    buf[i] = '\0';
    if (v == 0U)
    {
        buf[--i] = '0';
    }
    else
    {
        while (v > 0U) { buf[--i] = (char)('0' + (v % 10U)); v /= 10U; }
    }
    uart_puts(&buf[i]);
}

#endif // XSDK_PORT_ARM_R5_QEMU_UART_H
