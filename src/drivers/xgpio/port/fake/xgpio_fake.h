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

// @file xgpio_fake.h
// @brief Fake xGPIO port header for host-side unit testing.
//

#ifndef XGPIO_FAKE_H
#define XGPIO_FAKE_H

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
#include "xgpio.h"

    // MACROS //////////////////////////////////////////////////////////////////////////
#define XGPIO_FAKE_MAX_PINS 32U

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct
    {
        bool pins_level[XGPIO_FAKE_MAX_PINS];
        xGPIO_Pin_Config_t pins_config[XGPIO_FAKE_MAX_PINS];
        xGPIO_Driver_Interrupt_Callback_t callbacks[XGPIO_FAKE_MAX_PINS];
        void *callback_contexts[XGPIO_FAKE_MAX_PINS];
        bool is_initialized;
    } xGPIO_Fake_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////////
    extern const xGPIO_Driver_Ops_t xGPIO_Fake_Driver_Ops;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
    void xGPIO_Fake_Trigger_Interrupt(xGPIO_Fake_Context_t *fake_ctx, uint32_t pin);
    void xGPIO_Fake_Set_Input_Level(xGPIO_Fake_Context_t *fake_ctx, uint32_t pin, bool level);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XGPIO_FAKE_H
// EOF /////////////////////////////////////////////////////////////////////////////
