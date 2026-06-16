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

// @file xfault_cortex_r5.c
// @brief xFAULT Cortex-R5 target helpers and host stubs.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xfault.h"

#include "xfault_log.h"

// MODULE MACROS ///////////////////////////////////////////////////////////////////

// MODULE TYPES ////////////////////////////////////////////////////////////////////

// MODULE VARIABLES ////////////////////////////////////////////////////////////////

static xFAULT_Context_t s_exception_context;

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// MODULE FUNCTION PROTOTYPES //////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xFAULT_Capture_CP15(xFAULT_CP15_Registers_t *cp15)
{
    if (cp15 == NULL)
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

#if defined(__arm__) && !defined(__thumb__)
    __asm volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(cp15->dfsr));
    __asm volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(cp15->dfar));
    __asm volatile("mrc p15, 0, %0, c5, c0, 1" : "=r"(cp15->ifsr));
    __asm volatile("mrc p15, 0, %0, c6, c0, 2" : "=r"(cp15->ifar));

    return xRETURN_OK;
#else
    cp15->dfsr = 0U;
    cp15->dfar = 0U;
    cp15->ifsr = 0U;
    cp15->ifar = 0U;

    return xRETURN_xERR_xFAULT_UNSUPPORTED_TARGET;
#endif
}

void xFAULT_Fatal_From_Exception_Frame(const xFAULT_Exception_Frame_t *exception_frame)
{
    if (exception_frame != NULL)
    {
        (void)xFAULT_Context_From_Exception_Frame(&s_exception_context, exception_frame);
        xFAULT_Fatal_Entry(&s_exception_context); // does not return - halts
    }

    // Reached only when exception_frame is NULL; halt without a context.
    xFAULT_Fatal_Entry(NULL);
}

// EOF /////////////////////////////////////////////////////////////////////////////
