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

// @file xlogic_sump.c
// @brief xLOGIC SUMP protocol parser and metadata builder.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xlogic_sump.h"
#include "xassert.h"
#include "xlogic_config.h"

#include "xlogic_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// Number of argument bytes consumed by every long command
#define LONG_CMD_ARG_BYTES 4U

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static bool is_long_command(uint8_t cmd);
static void process_short_command(xLOGIC_SUMP_Context_t *sump_ctx, uint8_t cmd, xLOGIC_SUMP_Event_t *event_out);
static void process_long_command(xLOGIC_SUMP_Context_t *sump_ctx, xLOGIC_SUMP_Event_t *event_out);
static uint32_t assemble_arg(const xLOGIC_SUMP_Context_t *sump_ctx);
static void append_byte(uint8_t *dest, uint32_t *offset, uint32_t max, uint8_t val);
static void append_u32_be(uint8_t *dest, uint32_t *offset, uint32_t max, uint32_t val);
static void append_string(uint8_t *dest, uint32_t *offset, uint32_t max, const char *str);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static bool is_long_command(uint8_t cmd)
{
    // In the OLS/SUMP protocol, long commands have bit 7 set (0x80-0xFF).
    return (cmd & 0x80U) != 0U;
}

static uint32_t assemble_arg(const xLOGIC_SUMP_Context_t *sump_ctx)
{
    // SUMP arguments are 4 bytes, little-endian (arg_bytes[0] = LSB)
    return ((uint32_t)sump_ctx->arg_bytes[3U] << 24U) | ((uint32_t)sump_ctx->arg_bytes[2U] << 16U) |
           ((uint32_t)sump_ctx->arg_bytes[1U] << 8U) | (uint32_t)sump_ctx->arg_bytes[0U];
}

static void process_short_command(xLOGIC_SUMP_Context_t *sump_ctx, uint8_t cmd, xLOGIC_SUMP_Event_t *event_out)
{
    switch (cmd)
    {
    case xLOGIC_SUMP_CMD_RESET:
        // Reset the parser and decoded config to defaults
        (void)xLOGIC_SUMP_Init(sump_ctx);
        *event_out = xLOGIC_SUMP_EVENT_RESET;
        break;

    case xLOGIC_SUMP_CMD_RUN:
        *event_out = xLOGIC_SUMP_EVENT_RUN;
        break;

    case xLOGIC_SUMP_CMD_ID_QUERY:
        *event_out = xLOGIC_SUMP_EVENT_QUERY_ID;
        break;

    case xLOGIC_SUMP_CMD_METADATA:
        *event_out = xLOGIC_SUMP_EVENT_METADATA;
        break;

    case xLOGIC_SUMP_CMD_XOFF: // XOFF: pause transmission - no core action needed in Phase 1
        /* fall through */
    default:
        *event_out = xLOGIC_SUMP_EVENT_NONE;
        break;
    }
}

static void process_long_command(xLOGIC_SUMP_Context_t *sump_ctx, xLOGIC_SUMP_Event_t *event_out)
{
    uint32_t arg = assemble_arg(sump_ctx);

    switch (sump_ctx->pending_cmd)
    {
    case xLOGIC_SUMP_CMD_SET_DIVIDER:
        // Only bits [23:0] are used for the divider
        sump_ctx->divider = arg & 0x00FFFFFFU;
        *event_out = xLOGIC_SUMP_EVENT_CONFIG_SET;
        break;

    case xLOGIC_SUMP_CMD_SET_COUNTS:
        // Bits [15:0]:  (read_count / 4) - 1
        // Bits [31:16]: (delay_count / 4) - 1
        sump_ctx->read_count = ((arg & 0x0000FFFFU) + 1U) * 4U;
        sump_ctx->delay_count = (((arg >> 16U) & 0x0000FFFFU) + 1U) * 4U;
        *event_out = xLOGIC_SUMP_EVENT_CONFIG_SET;
        break;

    case xLOGIC_SUMP_CMD_SET_FLAGS:
        sump_ctx->flags = arg;
        *event_out = xLOGIC_SUMP_EVENT_CONFIG_SET;
        break;

    case xLOGIC_SUMP_CMD_SET_TMASK0:
        sump_ctx->trigger_mask = arg;
        *event_out = xLOGIC_SUMP_EVENT_CONFIG_SET;
        break;

    case xLOGIC_SUMP_CMD_SET_TVALUE0:
        sump_ctx->trigger_value = arg;
        *event_out = xLOGIC_SUMP_EVENT_CONFIG_SET;
        break;

    case xLOGIC_SUMP_CMD_SET_TCONFIG0:
        // Bit 0: 0 = edge trigger, 1 = level trigger
        if ((arg & 0x00000001U) != 0U)
        {
            sump_ctx->trigger_mode = xLOGIC_TRIGGER_LEVEL;
        }
        else
        {
            sump_ctx->trigger_mode = xLOGIC_TRIGGER_EDGE;
        }
        *event_out = xLOGIC_SUMP_EVENT_CONFIG_SET;
        break;

    default:
        // Unknown long command - ignore silently
        *event_out = xLOGIC_SUMP_EVENT_NONE;
        break;
    }
}

// Safely append one byte to a bounded buffer.
static void append_byte(uint8_t *dest, uint32_t *offset, uint32_t max, uint8_t val)
{
    if (*offset < max)
    {
        dest[*offset] = val;
        *offset = *offset + 1U;
    }
}

// Append a 4-byte big-endian uint32 to a bounded buffer.
static void append_u32_be(uint8_t *dest, uint32_t *offset, uint32_t max, uint32_t val)
{
    append_byte(dest, offset, max, (uint8_t)((val >> 24U) & 0xFFU));
    append_byte(dest, offset, max, (uint8_t)((val >> 16U) & 0xFFU));
    append_byte(dest, offset, max, (uint8_t)((val >> 8U) & 0xFFU));
    append_byte(dest, offset, max, (uint8_t)((val >> 0U) & 0xFFU));
}

// Append a null-terminated string (including the null terminator) to a bounded buffer.
static void append_string(uint8_t *dest, uint32_t *offset, uint32_t max, const char *str)
{
    uint32_t i = 0U;

    while (str[i] != '\0')
    {
        append_byte(dest, offset, max, (uint8_t)str[i]);
        i++;
    }

    // Null terminator
    append_byte(dest, offset, max, 0x00U);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xLOGIC_SUMP_Init(xLOGIC_SUMP_Context_t *sump_ctx)
{
    xASSERT(sump_ctx != NULL, "sump_ctx is NULL");

    if (sump_ctx == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    sump_ctx->parse_state = xLOGIC_SUMP_PARSE_STATE_CMD;
    sump_ctx->pending_cmd = 0U;
    sump_ctx->arg_bytes[0U] = 0U;
    sump_ctx->arg_bytes[1U] = 0U;
    sump_ctx->arg_bytes[2U] = 0U;
    sump_ctx->arg_bytes[3U] = 0U;
    sump_ctx->arg_count = 0U;
    sump_ctx->divider = 0U;
    sump_ctx->read_count = 0U;
    sump_ctx->delay_count = 0U;
    sump_ctx->flags = 0U;
    sump_ctx->trigger_mask = 0U;
    sump_ctx->trigger_value = 0U;
    sump_ctx->trigger_mode = xLOGIC_TRIGGER_NONE;

    return xRETURN_OK;
}

xRETURN_t xLOGIC_SUMP_Feed_Byte(xLOGIC_SUMP_Context_t *sump_ctx, uint8_t byte, xLOGIC_SUMP_Event_t *event_out)
{
    xASSERT(sump_ctx != NULL, "sump_ctx is NULL");
    xASSERT(event_out != NULL, "event_out is NULL");

    if (sump_ctx == NULL || event_out == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    *event_out = xLOGIC_SUMP_EVENT_NONE;

    if (sump_ctx->parse_state == xLOGIC_SUMP_PARSE_STATE_CMD)
    {
        if (is_long_command(byte))
        {
            sump_ctx->pending_cmd = byte;
            sump_ctx->arg_count = 0U;
            sump_ctx->parse_state = xLOGIC_SUMP_PARSE_STATE_ARG;
        }
        else
        {
            process_short_command(sump_ctx, byte, event_out);
        }
    }
    else
    {
        // Accumulate one argument byte
        sump_ctx->arg_bytes[sump_ctx->arg_count] = byte;
        sump_ctx->arg_count++;

        if (sump_ctx->arg_count == LONG_CMD_ARG_BYTES)
        {
            process_long_command(sump_ctx, event_out);
            sump_ctx->parse_state = xLOGIC_SUMP_PARSE_STATE_CMD;
        }
    }

    return xRETURN_OK;
}

xRETURN_t xLOGIC_SUMP_Build_Metadata(uint8_t *dest, uint32_t max_bytes, uint32_t *bytes_written_out)
{
    xASSERT(dest != NULL, "dest is NULL");
    xASSERT(bytes_written_out != NULL, "bytes_written_out is NULL");

    if (dest == NULL || bytes_written_out == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    if (max_bytes == 0U)
    {
        return xRETURN_xERR_xLOGIC_BUFFER_FULL;
    }

    uint32_t offset = 0U;

    // Device name - type 0x01 + null-terminated string
    append_byte(dest, &offset, max_bytes, xLOGIC_SUMP_META_DEVICE_NAME);
    append_string(dest, &offset, max_bytes, xLOGIC_CONFIG_SUMP_DEVICE_NAME);

    // Firmware version - type 0x02 + null-terminated string
    append_byte(dest, &offset, max_bytes, xLOGIC_SUMP_META_FW_VERSION);
    append_string(dest, &offset, max_bytes, xLOGIC_CONFIG_SUMP_FW_VERSION);

    // Sample memory in bytes - type 0x21 + uint32 BE
    append_byte(dest, &offset, max_bytes, xLOGIC_SUMP_META_SAMPLE_MEM);
    append_u32_be(dest, &offset, max_bytes, (uint32_t)xLOGIC_MAX_SAMPLE_BYTES);

    // Maximum sample rate in Hz - type 0x23 + uint32 BE
    append_byte(dest, &offset, max_bytes, xLOGIC_SUMP_META_MAX_RATE);
    append_u32_be(dest, &offset, max_bytes, xLOGIC_MAX_SAMPLE_RATE);

    // Number of probe channels - type 0x40 + uint8
    append_byte(dest, &offset, max_bytes, xLOGIC_SUMP_META_PROBE_COUNT);
    append_byte(dest, &offset, max_bytes, (uint8_t)xLOGIC_MAX_CHANNELS);

    // Protocol version - type 0x41 + uint8
    append_byte(dest, &offset, max_bytes, xLOGIC_SUMP_META_PROTO_VER);
    append_byte(dest, &offset, max_bytes, 2U);

    // End-of-metadata marker
    append_byte(dest, &offset, max_bytes, xLOGIC_SUMP_META_END);

    // If the buffer was too small, the append helpers silently stopped writing.
    // Report overflow if offset reached max_bytes without the end marker.
    if (offset == max_bytes && dest[offset - 1U] != xLOGIC_SUMP_META_END)
    {
        return xRETURN_xERR_xLOGIC_BUFFER_FULL;
    }

    *bytes_written_out = offset;

    return xRETURN_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
