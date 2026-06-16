// Copyright 2022 alambe94
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

// @file xspi.c
// @brief Portable xSPI controller core (simplified).
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xassert.h"
#include "xspi.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xSPI_Init(xSPI_Context_t *context, const xSPI_Instance_t *instance, const xSPI_Config_t *config)
{
    xASSERT(context != NULL, "xSPI context is NULL");
    xASSERT(instance != NULL, "xSPI instance is NULL");
    xASSERT(config != NULL, "xSPI config is NULL");

    if ((context == NULL) || (instance == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    xASSERT(instance->ops != NULL, "xSPI ops table is NULL");
    xASSERT(instance->driver_ctx != NULL, "xSPI driver context is NULL");
    xASSERT(instance->ops->init != NULL, "xSPI init op is NULL");
    xASSERT(instance->ops->deinit != NULL, "xSPI deinit op is NULL");
    xASSERT(instance->ops->start != NULL, "xSPI start op is NULL");
    xASSERT(instance->ops->stop != NULL, "xSPI stop op is NULL");
    xASSERT(instance->ops->transfer != NULL, "xSPI transfer op is NULL");

    if ((instance->ops == NULL) || (instance->driver_ctx == NULL) || (instance->ops->init == NULL) || (instance->ops->deinit == NULL) ||
        (instance->ops->start == NULL) || (instance->ops->stop == NULL) || (instance->ops->transfer == NULL))
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    (void)memset(context, 0, sizeof(*context));

    xRETURN_t status = instance->ops->init(instance->driver_ctx, config);
    if (status != xRETURN_OK)
    {
        return status;
    }

    context->ops = instance->ops;
    context->driver_ctx = instance->driver_ctx;
    context->is_initialized = true;
    context->is_started = false;
    context->is_busy = false;
    context->last_error = xRETURN_OK;

    return xRETURN_OK;
}

xRETURN_t xSPI_Deinit(xSPI_Context_t *context)
{
    xASSERT(context != NULL, "xSPI context is NULL");

    if (context == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    if (context->is_initialized == false)
    {
        return xRETURN_OK;
    }

    if (context->is_busy == true)
    {
        return xRETURN_xERR_xSPI_BUSY;
    }

    xRETURN_t status = context->ops->deinit(context->driver_ctx);
    if (status != xRETURN_OK)
    {
        context->last_error = status;
        return status;
    }

    (void)memset(context, 0, sizeof(*context));

    return xRETURN_OK;
}

xRETURN_t xSPI_Start(xSPI_Context_t *context)
{
    xASSERT(context != NULL, "xSPI context is NULL");

    if (context == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    if (context->is_initialized == false)
    {
        return xRETURN_xERR_xSPI_NOT_INITIALIZED;
    }

    if (context->is_started == true)
    {
        return xRETURN_OK;
    }

    xRETURN_t status = context->ops->start(context->driver_ctx);
    context->last_error = status;

    if (status == xRETURN_OK)
    {
        context->is_started = true;
    }

    return status;
}

xRETURN_t xSPI_Stop(xSPI_Context_t *context)
{
    xASSERT(context != NULL, "xSPI context is NULL");

    if (context == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    if (context->is_initialized == false)
    {
        return xRETURN_xERR_xSPI_NOT_INITIALIZED;
    }

    if (context->is_busy == true)
    {
        return xRETURN_xERR_xSPI_BUSY;
    }

    if (context->is_started == false)
    {
        return xRETURN_OK;
    }

    xRETURN_t status = context->ops->stop(context->driver_ctx);
    context->last_error = status;

    if (status == xRETURN_OK)
    {
        context->is_started = false;
    }

    return status;
}

xRETURN_t xSPI_Transfer(const xSPI_Device_t *device, const xSPI_Transaction_t *transaction)
{
    xASSERT(device != NULL, "xSPI device is NULL");
    xASSERT(transaction != NULL, "xSPI transaction is NULL");

    if ((device == NULL) || (transaction == NULL))
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    xASSERT(device->bus_ctx != NULL, "xSPI bus context is NULL");

    if (device->bus_ctx == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    xSPI_Context_t *context = device->bus_ctx;

    if (context->is_initialized == false)
    {
        return xRETURN_xERR_xSPI_NOT_INITIALIZED;
    }

    if (context->is_started == false)
    {
        return xRETURN_xERR_xSPI_NOT_STARTED;
    }

    if (context->is_busy == true)
    {
        return xRETURN_xERR_xSPI_BUSY;
    }

    context->is_busy = true;
    context->last_error = xRETURN_OK;

    xRETURN_t status = context->ops->transfer(context->driver_ctx, device, transaction);

    context->is_busy = false;
    context->last_error = status;

    return status;
}

// EOF /////////////////////////////////////////////////////////////////////////////
