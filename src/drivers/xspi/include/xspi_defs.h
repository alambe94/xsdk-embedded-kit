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

// @file xspi_defs.h
// @brief Public xSPI data types shared by the core, ports, and bus clients.
//

#ifndef XSPI_DEFS_H
#define XSPI_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xspi_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

#define xSPI_MODE_CPOL_HIGH        (1UL << 0U)
#define xSPI_MODE_CPHA_SECOND_EDGE (1UL << 1U)
#define xSPI_MODE_CS_ACTIVE_HIGH   (1UL << 2U)
#define xSPI_MODE_LOOPBACK         (1UL << 3U)

#define xSPI_MODE_0 0UL
#define xSPI_MODE_1 xSPI_MODE_CPHA_SECOND_EDGE
#define xSPI_MODE_2 xSPI_MODE_CPOL_HIGH
#define xSPI_MODE_3 (xSPI_MODE_CPOL_HIGH | xSPI_MODE_CPHA_SECOND_EDGE)

#define xSPI_MODE_FLAGS_MASK (xSPI_MODE_CPOL_HIGH | xSPI_MODE_CPHA_SECOND_EDGE | xSPI_MODE_CS_ACTIVE_HIGH | xSPI_MODE_LOOPBACK)

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct xSPI_Context_t xSPI_Context_t;
    typedef struct xSPI_Driver_Ops_t xSPI_Driver_Ops_t;

    typedef enum
    {
        xSPI_BIT_ORDER_MSB_FIRST = 0U,
        xSPI_BIT_ORDER_LSB_FIRST = 1U,
    } xSPI_Bit_Order_t;

    typedef struct
    {
        uint32_t default_clock_hz;
        uint32_t default_mode_flags;
        uint8_t bits_per_word;
        xSPI_Bit_Order_t bit_order;
    } xSPI_Config_t;

    typedef struct
    {
        const xSPI_Driver_Ops_t *ops;
        void *driver_ctx;
    } xSPI_Instance_t;

    typedef struct
    {
        xSPI_Context_t *bus_ctx;
        uint32_t chip_select;
        uint32_t mode_flags;
        uint32_t max_clock_hz;
    } xSPI_Device_t;

    typedef struct
    {
        const uint8_t *tx_buffer;
        uint8_t *rx_buffer;
        uint32_t length;
        uint32_t clock_hz;
        uint32_t timeout_ms;
        uint8_t bits_per_word;
    } xSPI_Transaction_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSPI_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
