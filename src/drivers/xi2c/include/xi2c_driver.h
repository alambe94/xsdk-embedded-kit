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

// @file xi2c_driver.h
// @brief xI2C hardware-port operations interface.
//

#ifndef XI2C_DRIVER_H
#define XI2C_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xi2c_defs.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef void (*xI2C_Driver_Event_Callback_t)(void *callback_ctx, xI2C_Event_t event, const xI2C_Event_Info_t *event_info);

    struct xI2C_Driver_Ops_t
    {
        xRETURN_t (*init)(void *driver_ctx, const xI2C_Config_t *config);
        xRETURN_t (*deinit)(void *driver_ctx);
        xRETURN_t (*start)(void *driver_ctx);
        xRETURN_t (*stop)(void *driver_ctx);
        xRETURN_t (*get_capabilities)(const void *driver_ctx, xI2C_Capabilities_t *capabilities);
        xRETURN_t (*get_status)(void *driver_ctx, xI2C_Status_t *status);
        xRETURN_t (*set_event_callback)(void *driver_ctx, xI2C_Driver_Event_Callback_t callback, void *callback_ctx);
        xRETURN_t (*transfer)(void *driver_ctx, const xI2C_Transaction_t *transaction);
        xRETURN_t (*transfer_async)(void *driver_ctx, const xI2C_Transaction_t *transaction);
        xRETURN_t (*message_sequence)(void *driver_ctx, const xI2C_Message_Sequence_t *sequence);
        xRETURN_t (*message_sequence_async)(void *driver_ctx, const xI2C_Message_Sequence_t *sequence);
        xRETURN_t (*acquire_bus)(void *driver_ctx, uint32_t timeout_ms);
        xRETURN_t (*release_bus)(void *driver_ctx);
        xRETURN_t (*abort)(void *driver_ctx);
    };

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XI2C_DRIVER_H
// EOF /////////////////////////////////////////////////////////////////////////////
