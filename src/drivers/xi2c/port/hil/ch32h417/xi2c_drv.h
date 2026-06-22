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

// @file xi2c_drv.h
// @brief CH32H417 I2C hardware port header for the xI2C driver core.
//

#ifndef XI2C_DRV_H
#define XI2C_DRV_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

// MODULE INCLUDES
#include "xi2c_driver.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct
    {
        I2C_TypeDef             *i2c;
        uint32_t                 pclk_hz;

        bool                     is_initialized;
        bool                     is_started;
        bool                     is_busy;
    } xI2C_CH32H417_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    extern const xI2C_Driver_Ops_t xI2C_CH32H417_Driver_Ops;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XI2C_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////
