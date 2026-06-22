// Copyright 2022 alambe94

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xspi_driver.h
// @brief xSPI hardware-port operations interface.
//

#ifndef XSPI_DRIVER_H
#define XSPI_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xspi_defs.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef void (*xSPI_Driver_Event_Callback_t)(void *callback_ctx, xSPI_Event_t event, const xSPI_Event_Info_t *event_info);

    struct xSPI_Driver_Ops_t
    {
        xRETURN_t (*init)(void *driver_ctx, const xSPI_Config_t *config);
        xRETURN_t (*deinit)(void *driver_ctx);
        xRETURN_t (*start)(void *driver_ctx);
        xRETURN_t (*stop)(void *driver_ctx);
        xRETURN_t (*set_event_callback)(void *driver_ctx, xSPI_Driver_Event_Callback_t callback, void *callback_ctx);
        xRETURN_t (*transfer)(void *driver_ctx, const xSPI_Device_t *device, const xSPI_Transaction_t *transaction);
    };

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSPI_DRIVER_H
// EOF /////////////////////////////////////////////////////////////////////////////
