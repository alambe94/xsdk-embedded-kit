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

// @file xlogic_sump.h
// @brief xLOGIC SUMP protocol parser - byte-at-a-time state machine and metadata builder.

#ifndef XLOGIC_SUMP_H
#define XLOGIC_SUMP_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>
#include <stdbool.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xlogic_defs.h"
#include "xlogic_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // SUMP short command opcodes
#define xLOGIC_SUMP_CMD_RESET    0x00U // abort capture, return to IDLE
#define xLOGIC_SUMP_CMD_RUN      0x01U // arm capture
#define xLOGIC_SUMP_CMD_ID_QUERY 0x02U // device replies "1ALS"
#define xLOGIC_SUMP_CMD_METADATA 0x04U // device replies with metadata block
#define xLOGIC_SUMP_CMD_XOFF     0x05U // pause sample transmission

    // SUMP long command opcodes (1 opcode byte + 4 argument bytes, LE)
#define xLOGIC_SUMP_CMD_SET_DIVIDER  0x80U // bits [23:0] = divider value
#define xLOGIC_SUMP_CMD_SET_COUNTS   0x81U // bits [15:0]=(read_count/4)-1, [31:16]=(delay/4)-1
#define xLOGIC_SUMP_CMD_SET_FLAGS    0x82U // channel group enables, RLE, noise filter
#define xLOGIC_SUMP_CMD_SET_TMASK0   0xC0U // trigger mask stage 0
#define xLOGIC_SUMP_CMD_SET_TVALUE0  0xC1U // trigger value stage 0
#define xLOGIC_SUMP_CMD_SET_TCONFIG0 0xC4U // trigger config stage 0

    // SUMP metadata field type codes (as per OLS / extended SUMP spec)
#define xLOGIC_SUMP_META_DEVICE_NAME 0x01U // null-terminated string: device name
#define xLOGIC_SUMP_META_FW_VERSION  0x02U // null-terminated string: firmware version
#define xLOGIC_SUMP_META_SAMPLE_MEM  0x21U // uint32 BE: sample memory in bytes
#define xLOGIC_SUMP_META_MAX_RATE    0x23U // uint32 BE: max sample rate in Hz
#define xLOGIC_SUMP_META_PROBE_COUNT 0x40U // uint8: number of probe channels
#define xLOGIC_SUMP_META_PROTO_VER   0x41U // uint8: protocol version (2)
#define xLOGIC_SUMP_META_END         0x00U // end-of-metadata marker

    // Length of the "1ALS" ID string (no null terminator on wire)
#define xLOGIC_SUMP_ID_BYTES 4U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Parser internal state
    typedef enum xLOGIC_SUMP_Parse_State_t
    {
        xLOGIC_SUMP_PARSE_STATE_CMD = 0U, // waiting for opcode byte
        xLOGIC_SUMP_PARSE_STATE_ARG = 1U, // consuming 4-byte LE argument
    } xLOGIC_SUMP_Parse_State_t;

    // Event produced by xLOGIC_SUMP_Feed_Byte for each complete command
    typedef enum xLOGIC_SUMP_Event_t
    {
        xLOGIC_SUMP_EVENT_NONE = 0U,       // no complete command yet
        xLOGIC_SUMP_EVENT_RESET = 1U,      // 0x00 received
        xLOGIC_SUMP_EVENT_RUN = 2U,        // 0x01 received - arm capture
        xLOGIC_SUMP_EVENT_QUERY_ID = 3U,   // 0x02 received - reply with "1ALS"
        xLOGIC_SUMP_EVENT_METADATA = 4U,   // 0x04 received - reply with metadata block
        xLOGIC_SUMP_EVENT_CONFIG_SET = 5U, // any long command fully received
    } xLOGIC_SUMP_Event_t;

    // Parser context - holds complete decoded capture configuration.
    typedef struct xLOGIC_SUMP_Context_t
    {
        // Parser state machine
        xLOGIC_SUMP_Parse_State_t parse_state;
        uint8_t pending_cmd;   // opcode awaiting argument bytes
        uint8_t arg_bytes[4U]; // accumulated argument bytes (LE)
        uint32_t arg_count;    // bytes received so far (0-4)

        // Decoded capture configuration (updated by long commands)
        uint32_t divider;                   // clock divider (sample_rate = ref / (div+1))
        uint32_t read_count;                // total samples to capture
        uint32_t delay_count;               // post-trigger samples
        uint32_t flags;                     // channel group enables, RLE, noise filter
        uint32_t trigger_mask;              // channel bitmask for trigger
        uint32_t trigger_value;             // expected pattern under trigger_mask
        xLOGIC_Trigger_Mode_t trigger_mode; // decoded from 0xC4 config argument

    } xLOGIC_SUMP_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // Compute the sample rate from a SUMP divider value.
    static inline uint32_t xLOGIC_SUMP_Divider_To_Rate(uint32_t divider)
    {
        return xLOGIC_SUMP_CLOCK_HZ / (divider + 1U);
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the SUMP parser context to its default (reset) state.
    xRETURN_t xLOGIC_SUMP_Init(xLOGIC_SUMP_Context_t *sump_ctx);

    // Feed one received byte into the parser.
    // event_out is set to the event produced (NONE if the command is not yet complete).
    // Returns xRETURN_OK; never returns an error for a valid byte stream.
    xRETURN_t xLOGIC_SUMP_Feed_Byte(xLOGIC_SUMP_Context_t *sump_ctx, uint8_t byte, xLOGIC_SUMP_Event_t *event_out);

    // Build the SUMP v2 metadata response into dest.
    // bytes_written_out receives the number of bytes written (including the 0x00 end marker).
    // Returns xRETURN_xERR_xLOGIC_BUFFER_FULL if dest is too small.
    xRETURN_t xLOGIC_SUMP_Build_Metadata(uint8_t *dest, uint32_t max_bytes, uint32_t *bytes_written_out);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XLOGIC_SUMP_H
// EOF /////////////////////////////////////////////////////////////////////////////
