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

// @file xfault.h
// @brief xFAULT fatal exception diagnostics public API.
//

#ifndef XFAULT_H
#define XFAULT_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xfault_config.h"
#include "xfault_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

#define xFAULT_VERSION_MAJOR  0U
#define xFAULT_VERSION_MINOR  2U
#define xFAULT_VERSION_PATCH  0U
#define xFAULT_VERSION_STRING "0.2.0"

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef uintptr_t xFAULT_Address_t;

    typedef enum xFAULT_Exception_Type_t
    {
        xFAULT_EXCEPTION_TYPE_UNKNOWN = 0U,
        xFAULT_EXCEPTION_TYPE_DATA_ABORT,
        xFAULT_EXCEPTION_TYPE_PREFETCH_ABORT,
        xFAULT_EXCEPTION_TYPE_UNDEFINED_INSTRUCTION,
    } xFAULT_Exception_Type_t;

    typedef void (*xFAULT_Halt_Callback_t)(void *halt_ctx);

    typedef struct xFAULT_Core_Registers_t
    {
        uint32_t r0;
        uint32_t r1;
        uint32_t r2;
        uint32_t r3;
        uint32_t r4;
        uint32_t r5;
        uint32_t r6;
        uint32_t r7;
        uint32_t r8;
        uint32_t r9;
        uint32_t r10;
        xFAULT_Address_t fp;
        uint32_t ip;
        xFAULT_Address_t sp;
        xFAULT_Address_t lr;
        xFAULT_Address_t pc;
        uint32_t cpsr;
        uint32_t spsr;
    } xFAULT_Core_Registers_t;

    typedef struct xFAULT_Exception_Frame_t
    {
        uint32_t r0;
        uint32_t r1;
        uint32_t r2;
        uint32_t r3;
        uint32_t r4;
        uint32_t r5;
        uint32_t r6;
        uint32_t r7;
        uint32_t r8;
        uint32_t r9;
        uint32_t r10;
        uint32_t fp;
        uint32_t ip;
        uint32_t sp;
        uint32_t lr;
        uint32_t pc;
        uint32_t cpsr;
        uint32_t spsr;
        uint32_t exception_type;
    } xFAULT_Exception_Frame_t;

    typedef struct xFAULT_CP15_Registers_t
    {
        uint32_t dfsr;
        uint32_t dfar;
        uint32_t ifsr;
        uint32_t ifar;
    } xFAULT_CP15_Registers_t;

    typedef struct xFAULT_Context_t
    {
        xFAULT_Core_Registers_t core;
        xFAULT_CP15_Registers_t cp15;
        xFAULT_Address_t backtrace[xFAULT_MAX_BACKTRACE_DEPTH];
        size_t backtrace_count;
        xFAULT_Exception_Type_t exception_type;
        bool is_valid;
    } xFAULT_Context_t;

    typedef struct xFAULT_Output_t
    {
        xRETURN_t (*write)(void *output_ctx, const uint8_t *buffer, size_t length, size_t *bytes_written);
        xRETURN_t (*flush)(void *output_ctx);
    } xFAULT_Output_t;

    typedef struct xFAULT_Config_t
    {
        const xFAULT_Output_t *output;
        void *output_ctx;
        xFAULT_Halt_Callback_t halt;
        void *halt_ctx;
    } xFAULT_Config_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xFAULT_Context_Init(xFAULT_Context_t *fault_ctx);

    xRETURN_t xFAULT_Backtrace_Capture(xFAULT_Context_t *fault_ctx, xFAULT_Address_t stack_base, xFAULT_Address_t stack_limit);

    xRETURN_t xFAULT_Dump_Text(const xFAULT_Context_t *fault_ctx, const xFAULT_Config_t *config);

    xRETURN_t xFAULT_Capture_CP15(xFAULT_CP15_Registers_t *cp15);

    xRETURN_t xFAULT_Context_From_Exception_Frame(xFAULT_Context_t *fault_ctx, const xFAULT_Exception_Frame_t *exception_frame);

    xRETURN_t xFAULT_Fatal_Config_Set(const xFAULT_Config_t *config, xFAULT_Address_t stack_base, xFAULT_Address_t stack_limit);

    xRETURN_t xFAULT_Fatal_Process(xFAULT_Context_t *fault_ctx);

    void xFAULT_Fatal_Entry(xFAULT_Context_t *fault_ctx);

    void xFAULT_Fatal_From_Exception_Frame(const xFAULT_Exception_Frame_t *exception_frame);

    void xFAULT_Halt_Default(void *halt_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XFAULT_H
// EOF /////////////////////////////////////////////////////////////////////////////
