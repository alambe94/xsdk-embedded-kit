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

// @file xspi_fake.c
// @brief Host-test fake port for the xSPI driver core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xspi_fake.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xSPI_Fake_Context_t *as_fake_context(void *driver_ctx);
static void fake_record_transfer(xSPI_Fake_Context_t *fake_ctx, const xSPI_Device_t *device, const xSPI_Transaction_t *transaction);
static void fake_copy_transfer_data(const xSPI_Transaction_t *transaction);
static xRETURN_t fake_init(void *driver_ctx, const xSPI_Config_t *config);
static xRETURN_t fake_deinit(void *driver_ctx);
static xRETURN_t fake_start(void *driver_ctx);
static xRETURN_t fake_stop(void *driver_ctx);
static xRETURN_t fake_set_event_callback(void *driver_ctx, xSPI_Driver_Event_Callback_t callback, void *callback_ctx);
static xRETURN_t fake_transfer(void *driver_ctx, const xSPI_Device_t *device, const xSPI_Transaction_t *transaction);

const xSPI_Driver_Ops_t xSPI_Fake_Driver_Ops = {
    fake_init,
    fake_deinit,
    fake_start,
    fake_stop,
    fake_set_event_callback,
    fake_transfer,
};

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xSPI_Fake_Context_t *as_fake_context(void *driver_ctx)
{
    return (xSPI_Fake_Context_t *)driver_ctx;
}

static void fake_record_transfer(xSPI_Fake_Context_t *fake_ctx, const xSPI_Device_t *device, const xSPI_Transaction_t *transaction)
{
    fake_ctx->last_tx_buffer = transaction->tx_buffer;
    fake_ctx->last_rx_buffer = transaction->rx_buffer;
    fake_ctx->last_chip_select = device->chip_select;
    fake_ctx->last_length = transaction->length;
}

static void fake_copy_transfer_data(const xSPI_Transaction_t *transaction)
{
    uint32_t index;

    if (transaction->rx_buffer != NULL)
    {
        for (index = 0U; index < transaction->length; index++)
        {
            if (transaction->tx_buffer != NULL)
            {
                transaction->rx_buffer[index] = transaction->tx_buffer[index];
            }
            else
            {
                transaction->rx_buffer[index] = (uint8_t)xSPI_FAKE_RX_FILL_BYTE;
            }
        }
    }
}

static xRETURN_t fake_init(void *driver_ctx, const xSPI_Config_t *config)
{
    xSPI_Fake_Context_t *fake_ctx;

    if ((driver_ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->init_count++;

    if (fake_ctx->next_init_status != xRETURN_OK)
    {
        return fake_ctx->next_init_status;
    }

    fake_ctx->is_initialized = true;

    return xRETURN_OK;
}

static xRETURN_t fake_deinit(void *driver_ctx)
{
    xSPI_Fake_Context_t *fake_ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->deinit_count++;

    if (fake_ctx->next_deinit_status != xRETURN_OK)
    {
        return fake_ctx->next_deinit_status;
    }

    fake_ctx->is_initialized = false;
    fake_ctx->is_started = false;
    fake_ctx->is_busy = false;
    fake_ctx->active_device = NULL;
    fake_ctx->active_transaction = NULL;

    return xRETURN_OK;
}

static xRETURN_t fake_start(void *driver_ctx)
{
    xSPI_Fake_Context_t *fake_ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->start_count++;

    if (fake_ctx->next_start_status != xRETURN_OK)
    {
        return fake_ctx->next_start_status;
    }

    fake_ctx->is_started = true;

    return xRETURN_OK;
}

static xRETURN_t fake_stop(void *driver_ctx)
{
    xSPI_Fake_Context_t *fake_ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->stop_count++;

    if (fake_ctx->next_stop_status != xRETURN_OK)
    {
        return fake_ctx->next_stop_status;
    }

    fake_ctx->is_started = false;

    return xRETURN_OK;
}

static xRETURN_t fake_transfer(void *driver_ctx, const xSPI_Device_t *device, const xSPI_Transaction_t *transaction)
{
    xSPI_Fake_Context_t *fake_ctx;

    if ((driver_ctx == NULL) || (device == NULL) || (transaction == NULL))
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->transfer_count++;

    if (fake_ctx->next_transfer_status != xRETURN_OK)
    {
        return fake_ctx->next_transfer_status;
    }

    fake_record_transfer(fake_ctx, device, transaction);
    fake_copy_transfer_data(transaction);

    return xRETURN_OK;
}

static xRETURN_t fake_set_event_callback(void *driver_ctx, xSPI_Driver_Event_Callback_t callback, void *callback_ctx)
{
    xSPI_Fake_Context_t *fake_ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    fake_ctx = as_fake_context(driver_ctx);
    fake_ctx->set_event_callback_count++;

    if (fake_ctx->next_set_event_callback_status != xRETURN_OK)
    {
        return fake_ctx->next_set_event_callback_status;
    }

    fake_ctx->registered_callback = callback;
    fake_ctx->registered_callback_ctx = callback_ctx;

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xSPI_Fake_Context_Init(xSPI_Fake_Context_t *fake_ctx)
{
    if (fake_ctx != NULL)
    {
        (void)memset(fake_ctx, 0, sizeof(*fake_ctx));
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
