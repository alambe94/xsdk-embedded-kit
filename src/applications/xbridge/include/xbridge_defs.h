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

// @file xbridge_defs.h
// @brief xBRIDGE shared constants, frame structures, and bridge state definitions.
//

#ifndef XBRIDGE_DEFS_H
#define XBRIDGE_DEFS_H

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

    // MACROS //////////////////////////////////////////////////////////////////////

    // Channel ID constants
#define xBRIDGE_CHANNEL_UART 0x00U
#define xBRIDGE_CHANNEL_I2C  0x01U
#define xBRIDGE_CHANNEL_SPI  0x02U
#define xBRIDGE_CHANNEL_CAN  0x03U
#define xBRIDGE_CHANNEL_QSPI 0x04U
#define xBRIDGE_CHANNEL_DAP  0x05U
#define xBRIDGE_CHANNEL_GPIO 0x06U
#define xBRIDGE_CHANNEL_PWM  0x07U
#define xBRIDGE_CHANNEL_ADC  0x08U

    // Frame response status codes
#define xBRIDGE_STATUS_OK    0x00U
#define xBRIDGE_STATUS_ERROR 0x01U

    // Buffer and queue sizing
#define xBRIDGE_MAX_PAYLOAD_BYTES    (4096U)
#define xBRIDGE_UART_QUEUE_BYTES     (512U)
#define xBRIDGE_CAN_QUEUE_FRAMES     (32U)
#define xBRIDGE_DAP_PACKET_BYTES     (512U)
#define xBRIDGE_DAP_HID_PACKET_BYTES (64U)

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Bridge channel operating state
    typedef enum xBRIDGE_State_t
    {
        xBRIDGE_STATE_IDLE = 0U,
        xBRIDGE_STATE_ACTIVE = 1U,
        xBRIDGE_STATE_ERROR = 2U,
    } xBRIDGE_State_t;

    // WINUSB binary command frame (host -> device, Bulk OUT)
    typedef struct __attribute__((packed)) xBRIDGE_Frame_Cmd_t
    {
        uint8_t channel; // xBRIDGE_CHANNEL_*
        uint8_t cmd;     // channel-specific opcode
        uint16_t length; // payload byte count following this header
        uint32_t seq;    // sequence number echoed in response
        // Followed by `length` bytes of payload
    } xBRIDGE_Frame_Cmd_t;

    // WINUSB binary response frame (device -> host, Bulk IN)
    typedef struct __attribute__((packed)) xBRIDGE_Frame_Resp_t
    {
        uint8_t channel; // xBRIDGE_CHANNEL_*
        uint8_t status;  // xBRIDGE_STATUS_OK or error code
        uint16_t length; // response data byte count following this header
        uint32_t seq;    // echoes the request seq
        // Followed by `length` bytes of response data
    } xBRIDGE_Frame_Resp_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
