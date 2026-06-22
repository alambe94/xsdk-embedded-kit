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

// @file xgpio_driver.h
// @brief xGPIO hardware-port operations interface.
//

#ifndef XGPIO_DRIVER_H
#define XGPIO_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xgpio_defs.h"
#include "xgpio_config.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef void (*xGPIO_Driver_Interrupt_Callback_t)(void *callback_ctx, uint32_t pin);

    struct xGPIO_Driver_Ops_t
    {
        xRETURN_t (*init)(void *driver_ctx, const xGPIO_Config_t *config);
        xRETURN_t (*deinit)(void *driver_ctx);
        xRETURN_t (*configure_pin)(void *driver_ctx, uint32_t pin, const xGPIO_Pin_Config_t *pin_config);
        xRETURN_t (*pin_write)(void *driver_ctx, uint32_t pin, bool level);
        xRETURN_t (*pin_read)(void *driver_ctx, uint32_t pin, bool *level);
        xRETURN_t (*pin_toggle)(void *driver_ctx, uint32_t pin);
        xRETURN_t (*set_interrupt_callback)(void *driver_ctx, uint32_t pin, xGPIO_Driver_Interrupt_Callback_t callback, void *callback_ctx);
    };

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XGPIO_DRIVER_H
// EOF /////////////////////////////////////////////////////////////////////////////
