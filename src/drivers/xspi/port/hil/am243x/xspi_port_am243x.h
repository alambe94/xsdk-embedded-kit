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

// @file xspi_port_am243x.h
// @brief TI AM243x hardware port header for the xSPI driver core.
//

#ifndef XSPI_PORT_AM243X_H
#define XSPI_PORT_AM243X_H

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
    #include "xspi_driver.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        uint32_t base_addr;
        uint32_t input_clock_hz;
        xRETURN_t last_error;
        bool is_initialized;
        bool is_started;
        bool is_busy;
    } xSPI_AM243x_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    extern const xSPI_Driver_Ops_t xSPI_AM243x_Driver_Ops;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSPI_PORT_AM243X_H
// EOF /////////////////////////////////////////////////////////////////////////////
