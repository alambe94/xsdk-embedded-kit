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

// @file xbridge_core.c
// @brief xBRIDGE core - shared binary frame parser and response builder for WINUSB channels.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xbridge_core.h"
#include "xassert.h"

#include "xbridge_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBRIDGE_Core_Parse_Frame(const uint8_t *buf, uint32_t buf_len, xBRIDGE_Frame_Cmd_t *cmd_out, const uint8_t **payload_out)
{
    xASSERT(buf != NULL, "buf is NULL");
    xASSERT(cmd_out != NULL, "cmd_out is NULL");
    xASSERT(payload_out != NULL, "payload_out is NULL");

    if (buf == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (cmd_out == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (payload_out == NULL)
    {
        return xRETURN_xERR_xBRIDGE_NULL_POINTER;
    }

    if (buf_len < sizeof(xBRIDGE_Frame_Cmd_t))
    {
        return xRETURN_xERR_xBRIDGE_PARSE_ERROR;
    }

    (void)memcpy(cmd_out, buf, sizeof(xBRIDGE_Frame_Cmd_t));

    if ((uint32_t)(sizeof(xBRIDGE_Frame_Cmd_t) + cmd_out->length) > buf_len)
    {
        return xRETURN_xERR_xBRIDGE_PARSE_ERROR;
    }

    if (cmd_out->length > xBRIDGE_MAX_PAYLOAD_BYTES)
    {
        return xRETURN_xERR_xBRIDGE_FRAME_TOO_LARGE;
    }

    *payload_out = buf + sizeof(xBRIDGE_Frame_Cmd_t);

    return xRETURN_OK;
}

uint32_t xBRIDGE_Core_Build_Response(uint8_t *resp_buf,
                                     uint32_t resp_buf_len,
                                     uint8_t channel,
                                     uint8_t status,
                                     uint32_t seq,
                                     const uint8_t *data,
                                     uint32_t data_len)
{
    xASSERT(resp_buf != NULL, "resp_buf is NULL");

    if (resp_buf == NULL)
    {
        return 0U;
    }

    uint32_t total = (uint32_t)sizeof(xBRIDGE_Frame_Resp_t) + data_len;

    if (total > resp_buf_len)
    {
        return 0U;
    }

    xBRIDGE_Frame_Resp_t hdr;
    hdr.channel = channel;
    hdr.status = status;
    hdr.length = (uint16_t)data_len;
    hdr.seq = seq;

    (void)memcpy(resp_buf, &hdr, sizeof(xBRIDGE_Frame_Resp_t));

    if ((data != NULL) && (data_len > 0U))
    {
        (void)memcpy(resp_buf + sizeof(xBRIDGE_Frame_Resp_t), data, data_len);
    }

    return total;
}

// EOF /////////////////////////////////////////////////////////////////////////////
