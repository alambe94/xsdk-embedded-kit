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

// @file xusbd_drv.h
// @brief Fake USB Device Controller Driver port for host-side verification.

#ifndef XUSBD_DRV_H
#define XUSBD_DRV_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_dcd.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////
#define xUSBD_FAKE_DCD_ENDPOINT_SLOT_COUNT  32U
#define xUSBD_FAKE_DCD_TX_QUEUE_DEPTH       8U
#define xUSBD_FAKE_DCD_TRANSFER_QUEUE_DEPTH 8U
#define xUSBD_FAKE_DCD_PACKET_SIZE          1024U
#define xUSBD_FAKE_DCD_DEFAULT_FRAME_NUMBER 0U

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xUSBD_Fake_DCD_Packet_t
    {
        uint8_t ep_addr;
        uint8_t data[xUSBD_FAKE_DCD_PACKET_SIZE];
        uint32_t length;
        bool is_zlp_required;
    } xUSBD_Fake_DCD_Packet_t;

    typedef struct xUSBD_Fake_DCD_Endpoint_t
    {
        uint8_t ep_type;
        uint16_t mps;
        uint8_t *receive_buffer;
        uint32_t receive_length;
        bool is_initialized;
        bool is_stalled;
        bool is_receive_armed;
    } xUSBD_Fake_DCD_Endpoint_t;

    typedef struct xUSBD_Fake_DCD_Context_t
    {
        void *device_ctx;
        xUSBD_DCD_Event_Callback_t event_callback;
        USB_Speed_t speed;
        uint32_t frame_number;
        uint8_t address;
        uint8_t test_mode;

        xUSBD_Fake_DCD_Endpoint_t endpoints[xUSBD_FAKE_DCD_ENDPOINT_SLOT_COUNT];
        xUSBD_Fake_DCD_Packet_t tx_queue[xUSBD_FAKE_DCD_TX_QUEUE_DEPTH];
        const xUSBD_DCD_Transfer_t *transfer_queue[xUSBD_FAKE_DCD_TRANSFER_QUEUE_DEPTH];

        uint32_t tx_read_idx;
        uint32_t tx_write_idx;
        uint32_t tx_count;
        uint32_t transfer_read_idx;
        uint32_t transfer_write_idx;
        uint32_t transfer_count;

        uint32_t init_count;
        uint32_t deinit_count;
        uint32_t set_event_callback_count;
        uint32_t connect_count;
        uint32_t disconnect_count;
        uint32_t enable_interrupts_count;
        uint32_t disable_interrupts_count;
        uint32_t set_address_count;
        uint32_t set_remote_wakeup_count;
        uint32_t set_test_mode_count;
        uint32_t get_frame_number_count;
        uint32_t get_speed_count;
        uint32_t ep_init_count;
        uint32_t ep_deinit_count;
        uint32_t ep_receive_count;
        uint32_t ep_send_count;
        uint32_t ep_transfer_queue_count;
        uint32_t ep_stall_count;
        uint32_t ep_clear_stall_count;
        uint32_t ep_is_stalled_count;

        uint8_t last_ep_addr;

        xRETURN_t init_return;
        xRETURN_t deinit_return;
        xRETURN_t set_event_callback_return;
        xRETURN_t connect_return;
        xRETURN_t disconnect_return;
        xRETURN_t enable_interrupts_return;
        xRETURN_t disable_interrupts_return;
        xRETURN_t set_address_return;
        xRETURN_t set_remote_wakeup_return;
        xRETURN_t set_test_mode_return;
        xRETURN_t ep_init_return;
        xRETURN_t ep_deinit_return;
        xRETURN_t ep_receive_return;
        xRETURN_t ep_send_return;
        xRETURN_t ep_transfer_queue_return;
        xRETURN_t ep_stall_return;
        xRETURN_t ep_clear_stall_return;

        bool is_initialized;
        bool is_connected;
        bool are_interrupts_enabled;
        bool has_remote_wakeup;
    } xUSBD_Fake_DCD_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////
    extern xUSBD_DCD_Ops_t xUSBD_Fake_DCD_Ops;
    extern xUSBD_Fake_DCD_Context_t xUSBD_Fake_DCD_Context;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xRETURN_t xUSBD_Fake_DCD_Init(xUSBD_Fake_DCD_Context_t *fake_ctx);
    xRETURN_t
    xUSBD_Fake_DCD_Fire_Event(xUSBD_Fake_DCD_Context_t *fake_ctx, USB_DCD_Event_t event, uint8_t ep_addr, uint8_t *data, uint32_t length);
    xRETURN_t xUSBD_Fake_DCD_RX(xUSBD_Fake_DCD_Context_t *fake_ctx, uint8_t ep_addr, const uint8_t *data, uint32_t length);
    xRETURN_t xUSBD_Fake_DCD_TX_Pop(xUSBD_Fake_DCD_Context_t *fake_ctx,
                                    uint8_t *ep_addr,
                                    uint8_t *data,
                                    uint32_t data_size,
                                    uint32_t *length,
                                    bool *is_zlp_required);
    xRETURN_t xUSBD_Fake_DCD_Transfer_Pop(xUSBD_Fake_DCD_Context_t *fake_ctx, const xUSBD_DCD_Transfer_t **transfer);
    xRETURN_t xUSBD_Fake_DCD_Complete_Transfer(xUSBD_Fake_DCD_Context_t *fake_ctx, xRETURN_t status, uint32_t actual_length);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////
