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

// @file xusbip_proto.c
// @brief xUSBIP protocol codec - encode/decode all USB/IP wire messages.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbip_proto.h"
#include "xassert.h"

#include "xusbip_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static bool write_u8(uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint8_t val);
static bool write_u16_be(uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint16_t val);
static bool write_u32_be(uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint32_t val);
static bool write_bytes(uint8_t *buf, uint32_t buf_len, uint32_t *pos, const void *data, uint32_t len);
static bool write_zeros(uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint32_t len);
static bool read_u16_be(const uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint16_t *val);
static bool read_u32_be(const uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint32_t *val);
static bool read_bytes(const uint8_t *buf, uint32_t buf_len, uint32_t *pos, void *data, uint32_t len);
static bool encode_op_common(uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint16_t code, uint32_t status);
static bool encode_device_be(uint8_t *buf, uint32_t buf_len, uint32_t *pos, const xUSBIP_Op_Device_t *device);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static bool write_u8(uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint8_t val)
{
    if (*pos >= buf_len)
    {
        return false;
    }
    buf[*pos] = val;
    *pos = *pos + 1U;
    return true;
}

static bool write_u16_be(uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint16_t val)
{
    if ((*pos + 2U) > buf_len)
    {
        return false;
    }
    buf[*pos] = (uint8_t)((val >> 8U) & 0xFFU);
    buf[*pos + 1U] = (uint8_t)(val & 0xFFU);
    *pos = *pos + 2U;
    return true;
}

static bool write_u32_be(uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint32_t val)
{
    if ((*pos + 4U) > buf_len)
    {
        return false;
    }
    buf[*pos] = (uint8_t)((val >> 24U) & 0xFFU);
    buf[*pos + 1U] = (uint8_t)((val >> 16U) & 0xFFU);
    buf[*pos + 2U] = (uint8_t)((val >> 8U) & 0xFFU);
    buf[*pos + 3U] = (uint8_t)(val & 0xFFU);
    *pos = *pos + 4U;
    return true;
}

static bool write_bytes(uint8_t *buf, uint32_t buf_len, uint32_t *pos, const void *data, uint32_t len)
{
    if (len == 0U)
    {
        return true;
    }
    if ((*pos + len) > buf_len)
    {
        return false;
    }
    (void)memcpy(&buf[*pos], data, (size_t)len);
    *pos = *pos + len;
    return true;
}

static bool write_zeros(uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint32_t len)
{
    if (len == 0U)
    {
        return true;
    }
    if ((*pos + len) > buf_len)
    {
        return false;
    }
    (void)memset(&buf[*pos], 0, (size_t)len);
    *pos = *pos + len;
    return true;
}

static bool read_u16_be(const uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint16_t *val)
{
    if ((*pos + 2U) > buf_len)
    {
        return false;
    }
    *val = (uint16_t)(((uint16_t)buf[*pos] << 8U) | (uint16_t)buf[*pos + 1U]);
    *pos = *pos + 2U;
    return true;
}

static bool read_u32_be(const uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint32_t *val)
{
    if ((*pos + 4U) > buf_len)
    {
        return false;
    }
    *val = ((uint32_t)buf[*pos] << 24U) | ((uint32_t)buf[*pos + 1U] << 16U) | ((uint32_t)buf[*pos + 2U] << 8U) | (uint32_t)buf[*pos + 3U];
    *pos = *pos + 4U;
    return true;
}

static bool read_bytes(const uint8_t *buf, uint32_t buf_len, uint32_t *pos, void *data, uint32_t len)
{
    if (len == 0U)
    {
        return true;
    }
    if ((*pos + len) > buf_len)
    {
        return false;
    }
    (void)memcpy(data, &buf[*pos], (size_t)len);
    *pos = *pos + len;
    return true;
}

// Write 8-byte op_common header with specified code and status
static bool encode_op_common(uint8_t *buf, uint32_t buf_len, uint32_t *pos, uint16_t code, uint32_t status)
{
    if (!write_u16_be(buf, buf_len, pos, (uint16_t)xUSBIP_PROTO_VERSION))
    {
        return false;
    }
    if (!write_u16_be(buf, buf_len, pos, code))
    {
        return false;
    }
    return write_u32_be(buf, buf_len, pos, status);
}

// Encode a device entry with all multi-byte fields in big-endian order
static bool encode_device_be(uint8_t *buf, uint32_t buf_len, uint32_t *pos, const xUSBIP_Op_Device_t *device)
{
    if (!write_bytes(buf, buf_len, pos, device->path, xUSBIP_PATH_LEN))
    {
        return false;
    }
    if (!write_bytes(buf, buf_len, pos, device->busid, xUSBIP_BUSID_LEN))
    {
        return false;
    }
    if (!write_u32_be(buf, buf_len, pos, device->busnum))
    {
        return false;
    }
    if (!write_u32_be(buf, buf_len, pos, device->devnum))
    {
        return false;
    }
    if (!write_u32_be(buf, buf_len, pos, device->speed))
    {
        return false;
    }
    if (!write_u16_be(buf, buf_len, pos, device->idVendor))
    {
        return false;
    }
    if (!write_u16_be(buf, buf_len, pos, device->idProduct))
    {
        return false;
    }
    if (!write_u16_be(buf, buf_len, pos, device->bcdDevice))
    {
        return false;
    }
    if (!write_u8(buf, buf_len, pos, device->bDeviceClass))
    {
        return false;
    }
    if (!write_u8(buf, buf_len, pos, device->bDeviceSubClass))
    {
        return false;
    }
    if (!write_u8(buf, buf_len, pos, device->bDeviceProtocol))
    {
        return false;
    }
    if (!write_u8(buf, buf_len, pos, device->bConfigurationValue))
    {
        return false;
    }
    if (!write_u8(buf, buf_len, pos, device->bNumConfigurations))
    {
        return false;
    }
    return write_u8(buf, buf_len, pos, device->bNumInterfaces);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t
xUSBIP_Proto_Decode_Op_Common(const uint8_t *buf, uint32_t buf_len, uint16_t *version_out, uint16_t *code_out, uint32_t *status_out)
{
    xASSERT(buf != NULL, "buf is NULL");
    xASSERT(version_out != NULL, "version_out is NULL");
    xASSERT(code_out != NULL, "code_out is NULL");
    xASSERT(status_out != NULL, "status_out is NULL");

    if ((buf == NULL) || (version_out == NULL) || (code_out == NULL) || (status_out == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    if (buf_len < xUSBIP_PROTO_OP_COMMON_SIZE)
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    uint32_t pos = 0U;
    uint16_t ver = 0U;
    uint16_t code = 0U;
    uint32_t status = 0U;

    if (!read_u16_be(buf, buf_len, &pos, &ver))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_u16_be(buf, buf_len, &pos, &code))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_u32_be(buf, buf_len, &pos, &status))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    *version_out = ver;
    *code_out = code;
    *status_out = status;

    if (ver != (uint16_t)xUSBIP_PROTO_VERSION)
    {
        return xRETURN_xERR_xUSBIP_PROTO_VERSION;
    }

    return xRETURN_OK;
}

xRETURN_t xUSBIP_Proto_Encode_Devlist_Reply(uint8_t *buf,
                                            uint32_t buf_len,
                                            const xUSBIP_Op_Device_t *devices,
                                            uint32_t device_count,
                                            uint32_t *bytes_written_out)
{
    xASSERT(buf != NULL, "buf is NULL");
    xASSERT(bytes_written_out != NULL, "bytes_written_out is NULL");

    if ((buf == NULL) || (bytes_written_out == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    if ((device_count > 0U) && (devices == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    uint32_t pos = 0U;

    if (!encode_op_common(buf, buf_len, &pos, (uint16_t)xUSBIP_OP_REP_DEVLIST, xUSBIP_OP_STATUS_OK))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }

    if (!write_u32_be(buf, buf_len, &pos, device_count))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }

    for (uint32_t i = 0U; i < device_count; i++)
    {
        if (!encode_device_be(buf, buf_len, &pos, &devices[i]))
        {
            return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
        }

        // Append zero-padded interface entries (bNumInterfaces of them)
        uint32_t iface_bytes = (uint32_t)devices[i].bNumInterfaces * (uint32_t)sizeof(xUSBIP_Op_Interface_t);
        if (!write_zeros(buf, buf_len, &pos, iface_bytes))
        {
            return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
        }
    }

    *bytes_written_out = pos;
    return xRETURN_OK;
}

xRETURN_t xUSBIP_Proto_Encode_Import_Reply(uint8_t *buf,
                                           uint32_t buf_len,
                                           const xUSBIP_Op_Device_t *device,
                                           uint32_t status,
                                           uint32_t *bytes_written_out)
{
    xASSERT(buf != NULL, "buf is NULL");
    xASSERT(bytes_written_out != NULL, "bytes_written_out is NULL");

    if ((buf == NULL) || (bytes_written_out == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    uint32_t pos = 0U;

    if (!encode_op_common(buf, buf_len, &pos, (uint16_t)xUSBIP_OP_REP_IMPORT, status))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }

    if (status == xUSBIP_OP_STATUS_OK)
    {
        if (device == NULL)
        {
            return xRETURN_xERR_xUSBIP_NULL_POINTER;
        }
        if (!encode_device_be(buf, buf_len, &pos, device))
        {
            return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
        }
    }

    *bytes_written_out = pos;
    return xRETURN_OK;
}

xRETURN_t xUSBIP_Proto_Decode_Basic_Header(const uint8_t *buf, uint32_t buf_len, uint32_t *command_out)
{
    xASSERT(buf != NULL, "buf is NULL");
    xASSERT(command_out != NULL, "command_out is NULL");

    if ((buf == NULL) || (command_out == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    if (buf_len < xUSBIP_PROTO_DATA_HEADER_SIZE)
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    uint32_t pos = 0U;
    uint32_t cmd = 0U;

    if (!read_u32_be(buf, buf_len, &pos, &cmd))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    if ((cmd != xUSBIP_CMD_SUBMIT) && (cmd != xUSBIP_CMD_UNLINK))
    {
        return xRETURN_xERR_xUSBIP_PROTO_OPCODE;
    }

    *command_out = cmd;
    return xRETURN_OK;
}

xRETURN_t xUSBIP_Proto_Decode_Cmd_Submit(const uint8_t *buf, uint32_t buf_len, xUSBIP_Cmd_Submit_t *out)
{
    xASSERT(buf != NULL, "buf is NULL");
    xASSERT(out != NULL, "out is NULL");

    if ((buf == NULL) || (out == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    if (buf_len < xUSBIP_PROTO_DATA_HEADER_SIZE)
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    uint32_t pos = 0U;
    uint32_t command = 0U;

    if (!read_u32_be(buf, buf_len, &pos, &command))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    if (command != xUSBIP_CMD_SUBMIT)
    {
        return xRETURN_xERR_xUSBIP_PROTO_OPCODE;
    }

    if (!read_u32_be(buf, buf_len, &pos, &out->seqnum))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_u32_be(buf, buf_len, &pos, &out->devid))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_u32_be(buf, buf_len, &pos, &out->direction))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_u32_be(buf, buf_len, &pos, &out->ep))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_u32_be(buf, buf_len, &pos, &out->transfer_flags))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_u32_be(buf, buf_len, &pos, &out->transfer_buffer_length))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    // skip start_frame and number_of_packets (not used for non-isochronous)
    pos = pos + 8U;
    if (pos > buf_len)
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    if (!read_u32_be(buf, buf_len, &pos, &out->interval))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_bytes(buf, buf_len, &pos, out->setup, 8U))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    return xRETURN_OK;
}

xRETURN_t xUSBIP_Proto_Encode_Ret_Submit(uint8_t *buf,
                                         uint32_t buf_len,
                                         uint32_t seqnum,
                                         uint32_t devid,
                                         uint32_t direction,
                                         uint32_t ep,
                                         uint32_t status,
                                         uint32_t actual_length,
                                         const uint8_t *data,
                                         uint32_t data_len,
                                         uint32_t *bytes_written_out)
{
    xASSERT(buf != NULL, "buf is NULL");
    xASSERT(bytes_written_out != NULL, "bytes_written_out is NULL");

    if ((buf == NULL) || (bytes_written_out == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    if ((data_len > 0U) && (data == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    uint32_t pos = 0U;

    if (!write_u32_be(buf, buf_len, &pos, xUSBIP_RET_SUBMIT))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    if (!write_u32_be(buf, buf_len, &pos, seqnum))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    if (!write_u32_be(buf, buf_len, &pos, devid))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    if (!write_u32_be(buf, buf_len, &pos, direction))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    if (!write_u32_be(buf, buf_len, &pos, ep))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    if (!write_u32_be(buf, buf_len, &pos, status))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    if (!write_u32_be(buf, buf_len, &pos, actual_length))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    // start_frame, number_of_packets, error_count (zeros for non-ISO)
    if (!write_zeros(buf, buf_len, &pos, 12U))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    // setup[8] (zeros in reply)
    if (!write_zeros(buf, buf_len, &pos, 8U))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }

    // Append IN data payload
    if ((data != NULL) && (data_len > 0U))
    {
        if (!write_bytes(buf, buf_len, &pos, data, data_len))
        {
            return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
        }
    }

    *bytes_written_out = pos;
    return xRETURN_OK;
}

xRETURN_t xUSBIP_Proto_Decode_Cmd_Unlink(const uint8_t *buf, uint32_t buf_len, xUSBIP_Cmd_Unlink_t *out)
{
    xASSERT(buf != NULL, "buf is NULL");
    xASSERT(out != NULL, "out is NULL");

    if ((buf == NULL) || (out == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    if (buf_len < xUSBIP_PROTO_DATA_HEADER_SIZE)
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    uint32_t pos = 0U;
    uint32_t command = 0U;

    if (!read_u32_be(buf, buf_len, &pos, &command))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    if (command != xUSBIP_CMD_UNLINK)
    {
        return xRETURN_xERR_xUSBIP_PROTO_OPCODE;
    }

    if (!read_u32_be(buf, buf_len, &pos, &out->seqnum))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_u32_be(buf, buf_len, &pos, &out->devid))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_u32_be(buf, buf_len, &pos, &out->direction))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_u32_be(buf, buf_len, &pos, &out->ep))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }
    if (!read_u32_be(buf, buf_len, &pos, &out->unlink_seqnum))
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    return xRETURN_OK;
}

xRETURN_t xUSBIP_Proto_Encode_Ret_Unlink(uint8_t *buf,
                                         uint32_t buf_len,
                                         uint32_t seqnum,
                                         uint32_t devid,
                                         uint32_t direction,
                                         uint32_t ep,
                                         uint32_t status,
                                         uint32_t *bytes_written_out)
{
    xASSERT(buf != NULL, "buf is NULL");
    xASSERT(bytes_written_out != NULL, "bytes_written_out is NULL");

    if ((buf == NULL) || (bytes_written_out == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    uint32_t pos = 0U;

    if (!write_u32_be(buf, buf_len, &pos, xUSBIP_RET_UNLINK))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    if (!write_u32_be(buf, buf_len, &pos, seqnum))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    if (!write_u32_be(buf, buf_len, &pos, devid))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    if (!write_u32_be(buf, buf_len, &pos, direction))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    if (!write_u32_be(buf, buf_len, &pos, ep))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    if (!write_u32_be(buf, buf_len, &pos, status))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }
    // padding[24]
    if (!write_zeros(buf, buf_len, &pos, 24U))
    {
        return xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL;
    }

    *bytes_written_out = pos;
    return xRETURN_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
