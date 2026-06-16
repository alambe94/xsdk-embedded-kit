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

// @file xnet_interface_fake.h
// @brief Declarations for the fake network interface driver.
//

#ifndef XNET_INTERFACE_FAKE_H
#define XNET_INTERFACE_FAKE_H

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
#include "xnet_interface.h"
#include "xnet_return.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xNET_FAKE_TX_QUEUE_DEPTH 4U
#define xNET_FAKE_RX_QUEUE_DEPTH 4U

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct
    {
        uint8_t tx_queue[xNET_FAKE_TX_QUEUE_DEPTH][xNET_ETHERNET_MTU + xNET_ETHERNET_HEADER_SIZE];
        uint32_t tx_lengths[xNET_FAKE_TX_QUEUE_DEPTH];
        uint32_t tx_write_idx;
        uint32_t tx_read_idx;
        uint32_t tx_count;

        uint8_t rx_queue[xNET_FAKE_RX_QUEUE_DEPTH][xNET_ETHERNET_MTU + xNET_ETHERNET_HEADER_SIZE];
        uint32_t rx_lengths[xNET_FAKE_RX_QUEUE_DEPTH];
        uint32_t rx_write_idx;
        uint32_t rx_read_idx;
        uint32_t rx_count;

        bool is_link_up;
        bool inject_tx_fail;
        xNET_Interface_Context_t *interface_ctx; /**< Associated interface context */
    } xNET_Fake_Interface_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    const xNET_Interface_Ops_t *xNET_Fake_Interface_Get_Ops(void);
    xRETURN_t xNET_Fake_Interface_Init(xNET_Fake_Interface_Context_t *fake_ctx, xNET_Interface_Context_t *interface_ctx);
    xRETURN_t xNET_Fake_Interface_RX(xNET_Interface_Context_t *interface_ctx, const uint8_t *packet, uint32_t length);
    xRETURN_t xNET_Fake_Interface_TX_Pop(xNET_Fake_Interface_Context_t *fake_ctx, uint8_t *packet, uint32_t packet_size, uint32_t *length);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_INTERFACE_FAKE_H
// EOF /////////////////////////////////////////////////////////////////////////////
