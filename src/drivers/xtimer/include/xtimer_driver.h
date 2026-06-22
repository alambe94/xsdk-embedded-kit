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

// @file xtimer_driver.h
// @brief xTIMER hardware-port operations interface.
//

#ifndef XTIMER_DRIVER_H
#define XTIMER_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xtimer_defs.h"
#include "xtimer_config.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef void (*xTIMER_Driver_Event_Callback_t)(void *callback_ctx);

    struct xTIMER_Driver_Ops_t
    {
        xRETURN_t (*init)(void *driver_ctx, const xTIMER_Config_t *config);
        xRETURN_t (*deinit)(void *driver_ctx);
        xRETURN_t (*start)(void *driver_ctx);
        xRETURN_t (*stop)(void *driver_ctx);
        xRETURN_t (*get_count)(void *driver_ctx, uint32_t *count);
        xRETURN_t (*clear_irq)(void *driver_ctx);
        xRETURN_t (*set_event_callback)(void *driver_ctx, xTIMER_Driver_Event_Callback_t callback, void *callback_ctx);
    };

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XTIMER_DRIVER_H
// EOF /////////////////////////////////////////////////////////////////////////////
