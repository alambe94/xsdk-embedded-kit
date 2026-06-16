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

// @file xspi_fake.h
// @brief Host-test fake port for the xSPI driver core.
//

#ifndef XSPI_FAKE_H
#define XSPI_FAKE_H

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
#include "xspi_driver.h"

// MACROS //////////////////////////////////////////////////////////////////////////

#define xSPI_FAKE_RX_FILL_BYTE 0xFFU

// TYPES ///////////////////////////////////////////////////////////////////////////

typedef struct
{
    xRETURN_t next_init_status;
    xRETURN_t next_deinit_status;
    xRETURN_t next_start_status;
    xRETURN_t next_stop_status;
    xRETURN_t next_transfer_status;

    const xSPI_Device_t *active_device;
    const xSPI_Transaction_t *active_transaction;

    const uint8_t *last_tx_buffer;
    uint8_t *last_rx_buffer;
    uint32_t last_chip_select;
    uint32_t last_length;

    uint32_t init_count;
    uint32_t deinit_count;
    uint32_t start_count;
    uint32_t stop_count;
    uint32_t transfer_count;

    bool is_initialized;
    bool is_started;
    bool is_busy;
} xSPI_Fake_Context_t;

// VARIABLES ///////////////////////////////////////////////////////////////////////

extern const xSPI_Driver_Ops_t xSPI_Fake_Driver_Ops;

// INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

void xSPI_Fake_Context_Init(xSPI_Fake_Context_t *fake_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSPI_FAKE_H
// EOF /////////////////////////////////////////////////////////////////////////////
