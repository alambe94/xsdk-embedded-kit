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

// @file xi2c_defs.h
// @brief Public xI2C data types shared by the core, ports, and bus clients.
//

#ifndef XI2C_DEFS_H
#define XI2C_DEFS_H

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
#include "xi2c_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct xI2C_Context_t xI2C_Context_t;
    typedef struct xI2C_Driver_Ops_t xI2C_Driver_Ops_t;

    typedef struct xI2C_Instance_t
    {
        const xI2C_Driver_Ops_t *ops;
        void *driver_ctx;
    } xI2C_Instance_t;

    typedef struct xI2C_Device_t
    {
        xI2C_Context_t *bus_ctx;
        uint16_t device_address;
        uint32_t flags;
    } xI2C_Device_t;

    typedef enum xI2C_Address_Mode_t
    {
        xI2C_ADDRESS_MODE_7_BIT,
        xI2C_ADDRESS_MODE_10_BIT,
    } xI2C_Address_Mode_t;

    typedef struct xI2C_Config_t
    {
        uint32_t bitrate_hz;
        xI2C_Address_Mode_t address_mode;
        bool has_own_address;
        uint16_t own_address;
    } xI2C_Config_t;

    typedef struct xI2C_Capabilities_t
    {
        bool can_async;
        bool can_dma;
        bool can_target_mode;
        bool can_ten_bit_address;
        bool can_bus_recovery;
        bool can_message_sequence;
        bool can_acquire_bus;
        uint32_t max_clock_hz;
        uint32_t max_messages;
    } xI2C_Capabilities_t;

    typedef struct xI2C_Status_t
    {
        bool is_initialized;
        bool is_started;
        bool is_busy;
        bool is_bus_acquired;
        bool has_bus_error;
        bool has_arbitration_lost;
        xRETURN_t last_error;
    } xI2C_Status_t;

    typedef enum xI2C_Transaction_Flags_t
    {
        xI2C_TRANSACTION_FLAGS_NONE = 0x00000000U,
        xI2C_TRANSACTION_FLAGS_REPEATED_START = 0x00000001U,
    } xI2C_Transaction_Flags_t;

    typedef struct xI2C_Transaction_t
    {
        uint16_t device_address;
        const uint8_t *tx_buffer;
        uint32_t tx_length;
        uint8_t *rx_buffer;
        uint32_t rx_length;
        uint32_t flags;
        uint32_t timeout_ms;
    } xI2C_Transaction_t;

    typedef enum xI2C_Message_Direction_t
    {
        xI2C_MESSAGE_DIRECTION_WRITE,
        xI2C_MESSAGE_DIRECTION_READ,
    } xI2C_Message_Direction_t;

    typedef enum xI2C_Message_Flags_t
    {
        xI2C_MESSAGE_FLAGS_NONE = 0x00000000U,
        xI2C_MESSAGE_FLAGS_RESTART = 0x00000001U,
        xI2C_MESSAGE_FLAGS_STOP = 0x00000002U,
        xI2C_MESSAGE_FLAGS_TEN_BIT_ADDR = 0x00000004U,
    } xI2C_Message_Flags_t;

    typedef struct xI2C_Message_t
    {
        uint16_t device_address;
        xI2C_Message_Direction_t direction;
        const uint8_t *tx_buffer;
        uint8_t *rx_buffer;
        uint32_t length;
        uint32_t flags;
    } xI2C_Message_t;

    typedef struct xI2C_Message_Sequence_t
    {
        const xI2C_Message_t *messages;
        uint32_t message_count;
        uint32_t timeout_ms;
    } xI2C_Message_Sequence_t;

    typedef enum xI2C_Event_t
    {
        xI2C_EVENT_TRANSFER_COMPLETE,
        xI2C_EVENT_NACK,
        xI2C_EVENT_ARBITRATION_LOST,
        xI2C_EVENT_BUS_ERROR,
        xI2C_EVENT_TIMEOUT,
        xI2C_EVENT_ABORTED,
    } xI2C_Event_t;

    typedef struct xI2C_Event_Info_t
    {
        const xI2C_Transaction_t *transaction;
        const xI2C_Message_Sequence_t *message_sequence;
        uint32_t messages_completed;
        uint32_t bytes_transferred;
        xRETURN_t error_code;
    } xI2C_Event_Info_t;

    typedef struct xI2C_Callbacks_t
    {
        void (*on_event)(xI2C_Context_t *i2c_ctx, xI2C_Event_t event, const xI2C_Event_Info_t *event_info, void *user_ctx);
    } xI2C_Callbacks_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XI2C_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
