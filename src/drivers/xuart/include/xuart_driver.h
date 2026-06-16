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

// @file xuart_driver.h
// @brief xUART hardware-port operations interface (Ops dispatch table).
//

#ifndef XUART_DRIVER_H
#define XUART_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xuart_defs.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    // Port-to-core event sink: port fires this once per accepted async operation.
    typedef void (*xUART_Driver_Event_Callback_t)(void *callback_ctx, xUART_Event_t event, const xUART_Event_Info_t *event_info);

    struct xUART_Driver_Ops_t
    {
        xRETURN_t (*init)(void *driver_ctx, const xUART_Config_t *config);
        xRETURN_t (*deinit)(void *driver_ctx);
        xRETURN_t (*start)(void *driver_ctx);
        xRETURN_t (*stop)(void *driver_ctx);
        xRETURN_t (*set_event_callback)(void *driver_ctx, xUART_Driver_Event_Callback_t callback, void *callback_ctx);
        xRETURN_t (*transmit)(void *driver_ctx, const uint8_t *buffer, uint32_t length, uint32_t timeout_ms);
        xRETURN_t (*receive)(void *driver_ctx, uint8_t *buffer, uint32_t length, uint32_t timeout_ms);
        xRETURN_t (*transmit_async)(void *driver_ctx, const uint8_t *buffer, uint32_t length);
        xRETURN_t (*receive_async)(void *driver_ctx, uint8_t *buffer, uint32_t length);
        xRETURN_t (*abort_tx)(void *driver_ctx);
        xRETURN_t (*abort_rx)(void *driver_ctx);
    };

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUART_DRIVER_H
// EOF /////////////////////////////////////////////////////////////////////////////
