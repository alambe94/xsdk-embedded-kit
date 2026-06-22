// Copyright 2026 alambe94
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

// @file xtimer_defs.h
// @brief Public xTIMER data types.
//

#ifndef XTIMER_DEFS_H
#define XTIMER_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xtimer_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct xTIMER_Context_t xTIMER_Context_t;
    typedef struct xTIMER_Driver_Ops_t xTIMER_Driver_Ops_t;

    typedef void (*xTIMER_Callback_t)(xTIMER_Context_t *timer_ctx, void *user_ctx);

    typedef struct
    {
        const xTIMER_Driver_Ops_t *ops;
        void *driver_ctx;
    } xTIMER_Instance_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XTIMER_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
