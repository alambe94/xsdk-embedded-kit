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

// @file xtimer_fake.h
// @brief Host-based fake timer hardware port header.
//

#ifndef XTIMER_FAKE_H
#define XTIMER_FAKE_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xtimer_driver.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        uint32_t count;
        bool is_initialized;
        bool is_started;
        xTIMER_Driver_Event_Callback_t callback;
        void *callback_ctx;
    } xTIMER_Fake_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    extern const xTIMER_Driver_Ops_t xTIMER_Fake_Driver_Ops;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    void xTIMER_Fake_Trigger_Interrupt(xTIMER_Fake_Context_t *ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XTIMER_FAKE_H
// EOF /////////////////////////////////////////////////////////////////////////////
