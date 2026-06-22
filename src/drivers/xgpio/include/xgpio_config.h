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

// @file xgpio_config.h
// @brief Configuration structure definitions for xGPIO driver.
//

#ifndef XGPIO_CONFIG_H
#define XGPIO_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xgpio_defs.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct
    {
        xGPIO_Pin_Mode_t mode;
        xGPIO_Pin_Pull_t pull;
        xGPIO_Pin_Speed_t speed;
        uint32_t alternate_function;
    } xGPIO_Pin_Config_t;

#ifndef xGPIO_TRACE_ENABLE
#define xGPIO_TRACE_ENABLE 1U
#endif

    typedef struct
    {
        // Reserved for future target bank configuration (e.g. clock configurations)
        uint32_t reserved;
    } xGPIO_Config_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XGPIO_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
