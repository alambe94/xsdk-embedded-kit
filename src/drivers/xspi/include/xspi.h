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

// @file xspi.h
// @brief Public xSPI controller API.
//

#ifndef XSPI_H
#define XSPI_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xspi_config.h"
#include "xspi_driver.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    struct xSPI_Context_t
    {
        const xSPI_Driver_Ops_t *ops;
        void *driver_ctx;
        bool is_initialized;
        bool is_started;
        bool is_busy;
        xRETURN_t last_error;
    };

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

    xRETURN_t xSPI_Init(xSPI_Context_t *context, const xSPI_Instance_t *instance, const xSPI_Config_t *config);
    xRETURN_t xSPI_Deinit(xSPI_Context_t *context);
    xRETURN_t xSPI_Start(xSPI_Context_t *context);
    xRETURN_t xSPI_Stop(xSPI_Context_t *context);
    xRETURN_t xSPI_Transfer(const xSPI_Device_t *device, const xSPI_Transaction_t *transaction);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSPI_H
// EOF /////////////////////////////////////////////////////////////////////////////
