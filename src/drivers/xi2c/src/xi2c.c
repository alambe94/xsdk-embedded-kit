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

// @file xi2c.c
// @brief Portable xI2C controller core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xassert.h"
#include "xi2c.h"
#include "xi2c_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xI2C_Init(xI2C_Context_t *i2c_ctx, const xI2C_Instance_t *instance, const xI2C_Config_t *config)
{
    xASSERT(i2c_ctx != NULL, "xI2C context is NULL");
    xASSERT(instance != NULL, "xI2C instance is NULL");
    xASSERT(config != NULL, "xI2C config is NULL");

    if ((i2c_ctx == NULL) || (instance == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    xASSERT(instance->ops != NULL, "xI2C ops is NULL");
    xASSERT(instance->driver_ctx != NULL, "xI2C driver context is NULL");
    xASSERT(instance->ops->init != NULL, "xI2C init op is NULL");
    xASSERT(instance->ops->deinit != NULL, "xI2C deinit op is NULL");
    xASSERT(instance->ops->start != NULL, "xI2C start op is NULL");
    xASSERT(instance->ops->stop != NULL, "xI2C stop op is NULL");
    xASSERT(instance->ops->transfer != NULL, "xI2C transfer op is NULL");

    if ((instance->ops == NULL) || (instance->driver_ctx == NULL) || (instance->ops->init == NULL) || (instance->ops->deinit == NULL) ||
        (instance->ops->start == NULL) || (instance->ops->stop == NULL) || (instance->ops->transfer == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    (void)memset(i2c_ctx, 0, sizeof(*i2c_ctx));

    xRETURN_t status = instance->ops->init(instance->driver_ctx, config);
    if (status != xRETURN_OK)
    {
        return status;
    }

    i2c_ctx->ops = instance->ops;
    i2c_ctx->driver_ctx = instance->driver_ctx;
    i2c_ctx->config = *config;
    i2c_ctx->is_initialized = true;

    return xRETURN_OK;
}

xRETURN_t xI2C_Deinit(xI2C_Context_t *i2c_ctx)
{
    if (i2c_ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_OK;
    }

    if (i2c_ctx->is_busy)
    {
        return xRETURN_xERR_xI2C_BUSY;
    }

    xRETURN_t status = i2c_ctx->ops->deinit(i2c_ctx->driver_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    (void)memset(i2c_ctx, 0, sizeof(*i2c_ctx));

    return xRETURN_OK;
}

xRETURN_t xI2C_Start(xI2C_Context_t *i2c_ctx)
{
    if (i2c_ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (i2c_ctx->is_started)
    {
        return xRETURN_OK;
    }

    xRETURN_t status = i2c_ctx->ops->start(i2c_ctx->driver_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    i2c_ctx->is_started = true;

    return xRETURN_OK;
}

xRETURN_t xI2C_Stop(xI2C_Context_t *i2c_ctx)
{
    if (i2c_ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (i2c_ctx->is_busy)
    {
        return xRETURN_xERR_xI2C_BUSY;
    }

    if (!i2c_ctx->is_started)
    {
        return xRETURN_OK;
    }

    xRETURN_t status = i2c_ctx->ops->stop(i2c_ctx->driver_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    i2c_ctx->is_started = false;

    return xRETURN_OK;
}

xRETURN_t xI2C_Get_Capabilities(const xI2C_Instance_t *instance, xI2C_Capabilities_t *capabilities)
{
    if ((instance == NULL) || (capabilities == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if ((instance->ops == NULL) || (instance->ops->get_capabilities == NULL))
    {
        return xRETURN_xERR_xI2C_UNSUPPORTED;
    }

    return instance->ops->get_capabilities(instance->driver_ctx, capabilities);
}

xRETURN_t xI2C_Get_Status(const xI2C_Context_t *i2c_ctx, xI2C_Status_t *status)
{
    if ((i2c_ctx == NULL) || (status == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (i2c_ctx->ops != NULL && i2c_ctx->ops->get_status != NULL)
    {
        return i2c_ctx->ops->get_status(i2c_ctx->driver_ctx, status);
    }

    status->is_initialized = i2c_ctx->is_initialized;
    status->is_started = i2c_ctx->is_started;
    status->is_busy = i2c_ctx->is_busy;
    status->is_bus_acquired = i2c_ctx->is_bus_acquired;
    status->has_bus_error = false;
    status->has_arbitration_lost = false;
    status->last_error = i2c_ctx->last_error;

    return xRETURN_OK;
}

xRETURN_t xI2C_Set_Callback(xI2C_Context_t *i2c_ctx, const xI2C_Callbacks_t *callbacks, void *user_ctx)
{
    if (i2c_ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (i2c_ctx->is_busy)
    {
        return xRETURN_xERR_xI2C_BUSY;
    }

    if (callbacks != NULL)
    {
        i2c_ctx->callbacks = *callbacks;
    }
    else
    {
        (void)memset(&i2c_ctx->callbacks, 0, sizeof(i2c_ctx->callbacks));
    }
    i2c_ctx->user_ctx = user_ctx;

    return xRETURN_OK;
}

xRETURN_t
xI2C_Controller_Write(xI2C_Context_t *i2c_ctx, uint16_t device_address, const uint8_t *tx_buffer, uint32_t tx_length, uint32_t timeout_ms)
{
    if ((i2c_ctx == NULL) || (tx_buffer == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (tx_length == 0U)
    {
        return xRETURN_xERR_xI2C_INVALID_ARG;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (!i2c_ctx->is_started)
    {
        return xRETURN_xERR_xI2C_NOT_STARTED;
    }

    if (i2c_ctx->is_busy)
    {
        return xRETURN_xERR_xI2C_BUSY;
    }

    xI2C_Transaction_t transaction = {.device_address = device_address,
                                      .tx_buffer = tx_buffer,
                                      .tx_length = tx_length,
                                      .rx_buffer = NULL,
                                      .rx_length = 0U,
                                      .flags = xI2C_TRANSACTION_FLAGS_NONE,
                                      .timeout_ms = timeout_ms};

    i2c_ctx->is_busy = true;
    xRETURN_t status = i2c_ctx->ops->transfer(i2c_ctx->driver_ctx, &transaction);
    i2c_ctx->is_busy = false;
    i2c_ctx->last_error = status;

    return status;
}

xRETURN_t
xI2C_Controller_Read(xI2C_Context_t *i2c_ctx, uint16_t device_address, uint8_t *rx_buffer, uint32_t rx_length, uint32_t timeout_ms)
{
    if ((i2c_ctx == NULL) || (rx_buffer == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (rx_length == 0U)
    {
        return xRETURN_xERR_xI2C_INVALID_ARG;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (!i2c_ctx->is_started)
    {
        return xRETURN_xERR_xI2C_NOT_STARTED;
    }

    if (i2c_ctx->is_busy)
    {
        return xRETURN_xERR_xI2C_BUSY;
    }

    xI2C_Transaction_t transaction = {.device_address = device_address,
                                      .tx_buffer = NULL,
                                      .tx_length = 0U,
                                      .rx_buffer = rx_buffer,
                                      .rx_length = rx_length,
                                      .flags = xI2C_TRANSACTION_FLAGS_NONE,
                                      .timeout_ms = timeout_ms};

    i2c_ctx->is_busy = true;
    xRETURN_t status = i2c_ctx->ops->transfer(i2c_ctx->driver_ctx, &transaction);
    i2c_ctx->is_busy = false;
    i2c_ctx->last_error = status;

    return status;
}

xRETURN_t xI2C_Controller_Write_Read(xI2C_Context_t *i2c_ctx,
                                     uint16_t device_address,
                                     const uint8_t *tx_buffer,
                                     uint32_t tx_length,
                                     uint8_t *rx_buffer,
                                     uint32_t rx_length,
                                     uint32_t timeout_ms)
{
    if ((i2c_ctx == NULL) || (tx_buffer == NULL) || (rx_buffer == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if ((tx_length == 0U) || (rx_length == 0U))
    {
        return xRETURN_xERR_xI2C_INVALID_ARG;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (!i2c_ctx->is_started)
    {
        return xRETURN_xERR_xI2C_NOT_STARTED;
    }

    if (i2c_ctx->is_busy)
    {
        return xRETURN_xERR_xI2C_BUSY;
    }

    xI2C_Transaction_t transaction = {.device_address = device_address,
                                      .tx_buffer = tx_buffer,
                                      .tx_length = tx_length,
                                      .rx_buffer = rx_buffer,
                                      .rx_length = rx_length,
                                      .flags = xI2C_TRANSACTION_FLAGS_REPEATED_START,
                                      .timeout_ms = timeout_ms};

    i2c_ctx->is_busy = true;
    xRETURN_t status = i2c_ctx->ops->transfer(i2c_ctx->driver_ctx, &transaction);
    i2c_ctx->is_busy = false;
    i2c_ctx->last_error = status;

    return status;
}

xRETURN_t xI2C_Controller_Transfer_Async(xI2C_Context_t *i2c_ctx, const xI2C_Transaction_t *transaction)
{
    if ((i2c_ctx == NULL) || (transaction == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (!i2c_ctx->is_started)
    {
        return xRETURN_xERR_xI2C_NOT_STARTED;
    }

    if (i2c_ctx->is_busy)
    {
        return xRETURN_xERR_xI2C_BUSY;
    }

    if (i2c_ctx->ops->transfer_async == NULL)
    {
        return xRETURN_xERR_xI2C_UNSUPPORTED;
    }

    i2c_ctx->is_busy = true;
    xRETURN_t status = i2c_ctx->ops->transfer_async(i2c_ctx->driver_ctx, transaction);
    if (status != xRETURN_OK)
    {
        i2c_ctx->is_busy = false;
    }
    i2c_ctx->last_error = status;

    return status;
}

xRETURN_t xI2C_Controller_Message_Sequence(xI2C_Context_t *i2c_ctx, const xI2C_Message_Sequence_t *sequence)
{
    if ((i2c_ctx == NULL) || (sequence == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (!i2c_ctx->is_started)
    {
        return xRETURN_xERR_xI2C_NOT_STARTED;
    }

    if (i2c_ctx->is_busy)
    {
        return xRETURN_xERR_xI2C_BUSY;
    }

    if (i2c_ctx->ops->message_sequence == NULL)
    {
        return xRETURN_xERR_xI2C_UNSUPPORTED;
    }

    i2c_ctx->is_busy = true;
    xRETURN_t status = i2c_ctx->ops->message_sequence(i2c_ctx->driver_ctx, sequence);
    i2c_ctx->is_busy = false;
    i2c_ctx->last_error = status;

    return status;
}

xRETURN_t xI2C_Controller_Message_Sequence_Async(xI2C_Context_t *i2c_ctx, const xI2C_Message_Sequence_t *sequence)
{
    if ((i2c_ctx == NULL) || (sequence == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (!i2c_ctx->is_started)
    {
        return xRETURN_xERR_xI2C_NOT_STARTED;
    }

    if (i2c_ctx->is_busy)
    {
        return xRETURN_xERR_xI2C_BUSY;
    }

    if (i2c_ctx->ops->message_sequence_async == NULL)
    {
        return xRETURN_xERR_xI2C_UNSUPPORTED;
    }

    i2c_ctx->is_busy = true;
    xRETURN_t status = i2c_ctx->ops->message_sequence_async(i2c_ctx->driver_ctx, sequence);
    if (status != xRETURN_OK)
    {
        i2c_ctx->is_busy = false;
    }
    i2c_ctx->last_error = status;

    return status;
}

xRETURN_t xI2C_Acquire_Bus(xI2C_Context_t *i2c_ctx, uint32_t timeout_ms)
{
    if (i2c_ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (!i2c_ctx->is_started)
    {
        return xRETURN_xERR_xI2C_NOT_STARTED;
    }

    if (i2c_ctx->is_bus_acquired)
    {
        return xRETURN_OK;
    }

    if (i2c_ctx->ops->acquire_bus == NULL)
    {
        return xRETURN_xERR_xI2C_UNSUPPORTED;
    }

    xRETURN_t status = i2c_ctx->ops->acquire_bus(i2c_ctx->driver_ctx, timeout_ms);
    if (status == xRETURN_OK)
    {
        i2c_ctx->is_bus_acquired = true;
    }

    return status;
}

xRETURN_t xI2C_Release_Bus(xI2C_Context_t *i2c_ctx)
{
    if (i2c_ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (!i2c_ctx->is_started)
    {
        return xRETURN_xERR_xI2C_NOT_STARTED;
    }

    if (!i2c_ctx->is_bus_acquired)
    {
        return xRETURN_OK;
    }

    if (i2c_ctx->ops->release_bus == NULL)
    {
        return xRETURN_xERR_xI2C_UNSUPPORTED;
    }

    xRETURN_t status = i2c_ctx->ops->release_bus(i2c_ctx->driver_ctx);
    if (status == xRETURN_OK)
    {
        i2c_ctx->is_bus_acquired = false;
    }

    return status;
}

xRETURN_t xI2C_Abort(xI2C_Context_t *i2c_ctx)
{
    if (i2c_ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (!i2c_ctx->is_initialized)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (!i2c_ctx->is_started)
    {
        return xRETURN_xERR_xI2C_NOT_STARTED;
    }

    if (!i2c_ctx->is_busy)
    {
        return xRETURN_OK;
    }

    if (i2c_ctx->ops->abort == NULL)
    {
        return xRETURN_xERR_xI2C_UNSUPPORTED;
    }

    return i2c_ctx->ops->abort(i2c_ctx->driver_ctx);
}

// EOF /////////////////////////////////////////////////////////////////////////////
