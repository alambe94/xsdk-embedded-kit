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

// @file xgpio_defs.h
// @brief Public xGPIO data types.
//

#ifndef XGPIO_DEFS_H
#define XGPIO_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xgpio_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct xGPIO_Context_t xGPIO_Context_t;
    typedef struct xGPIO_Driver_Ops_t xGPIO_Driver_Ops_t;

    typedef struct
    {
        const xGPIO_Driver_Ops_t *ops;
        void *driver_ctx;
    } xGPIO_Instance_t;

    typedef enum
    {
        xGPIO_PIN_MODE_INPUT,
        xGPIO_PIN_MODE_OUTPUT_PUSH_PULL,
        xGPIO_PIN_MODE_OUTPUT_OPEN_DRAIN,
        xGPIO_PIN_MODE_ALTERNATE_PUSH_PULL,
        xGPIO_PIN_MODE_ALTERNATE_OPEN_DRAIN,
        xGPIO_PIN_MODE_ANALOG,
    } xGPIO_Pin_Mode_t;

    typedef enum
    {
        xGPIO_PIN_PULL_NONE,
        xGPIO_PIN_PULL_UP,
        xGPIO_PIN_PULL_DOWN,
    } xGPIO_Pin_Pull_t;

    typedef enum
    {
        xGPIO_PIN_SPEED_LOW,
        xGPIO_PIN_SPEED_MEDIUM,
        xGPIO_PIN_SPEED_HIGH,
        xGPIO_PIN_SPEED_VERY_HIGH,
    } xGPIO_Pin_Speed_t;

    typedef void (*xGPIO_Interrupt_Callback_t)(xGPIO_Context_t *gpio_ctx, uint32_t pin, void *user_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XGPIO_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
