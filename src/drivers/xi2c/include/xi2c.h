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

// @file xi2c.h
// @brief Public xI2C controller API.
//

#ifndef XI2C_H
#define XI2C_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xi2c_config.h"
#include "xi2c_driver.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    struct xI2C_Context_t
    {
        const xI2C_Driver_Ops_t *ops;
        void *driver_ctx;
        xI2C_Callbacks_t callbacks;
        void *user_ctx;
        xI2C_Config_t config;
        bool is_initialized;
        bool is_started;
        bool is_busy;
        bool is_bus_acquired;
        xRETURN_t last_error;
    };

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

    xRETURN_t xI2C_Init(xI2C_Context_t *i2c_ctx, const xI2C_Instance_t *instance, const xI2C_Config_t *config);
    xRETURN_t xI2C_Deinit(xI2C_Context_t *i2c_ctx);
    xRETURN_t xI2C_Start(xI2C_Context_t *i2c_ctx);
    xRETURN_t xI2C_Stop(xI2C_Context_t *i2c_ctx);
    xRETURN_t xI2C_Get_Capabilities(const xI2C_Instance_t *instance, xI2C_Capabilities_t *capabilities);
    xRETURN_t xI2C_Get_Status(const xI2C_Context_t *i2c_ctx, xI2C_Status_t *status);
    xRETURN_t xI2C_Set_Callback(xI2C_Context_t *i2c_ctx, const xI2C_Callbacks_t *callbacks, void *user_ctx);

    xRETURN_t xI2C_Controller_Write(xI2C_Context_t *i2c_ctx,
                                    uint16_t device_address,
                                    const uint8_t *tx_buffer,
                                    uint32_t tx_length,
                                    uint32_t timeout_ms);

    xRETURN_t
    xI2C_Controller_Read(xI2C_Context_t *i2c_ctx, uint16_t device_address, uint8_t *rx_buffer, uint32_t rx_length, uint32_t timeout_ms);

    xRETURN_t xI2C_Controller_Write_Read(xI2C_Context_t *i2c_ctx,
                                         uint16_t device_address,
                                         const uint8_t *tx_buffer,
                                         uint32_t tx_length,
                                         uint8_t *rx_buffer,
                                         uint32_t rx_length,
                                         uint32_t timeout_ms);

    xRETURN_t xI2C_Controller_Transfer_Async(xI2C_Context_t *i2c_ctx, const xI2C_Transaction_t *transaction);
    xRETURN_t xI2C_Controller_Message_Sequence(xI2C_Context_t *i2c_ctx, const xI2C_Message_Sequence_t *sequence);
    xRETURN_t xI2C_Controller_Message_Sequence_Async(xI2C_Context_t *i2c_ctx, const xI2C_Message_Sequence_t *sequence);

    xRETURN_t xI2C_Acquire_Bus(xI2C_Context_t *i2c_ctx, uint32_t timeout_ms);
    xRETURN_t xI2C_Release_Bus(xI2C_Context_t *i2c_ctx);
    xRETURN_t xI2C_Abort(xI2C_Context_t *i2c_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XI2C_H
// EOF /////////////////////////////////////////////////////////////////////////////
