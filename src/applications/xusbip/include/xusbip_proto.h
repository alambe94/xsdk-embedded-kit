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

// @file xusbip_proto.h
// @brief xUSBIP protocol codec - encode/decode all USB/IP wire messages.
// No I/O - only buffer transformations. Fully host-testable without hardware.
//

#ifndef XUSBIP_PROTO_H
#define XUSBIP_PROTO_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xusbip_defs.h"
#include "xusbip_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // Size of the op_common header on the wire (bytes)
#define xUSBIP_PROTO_OP_COMMON_SIZE 8U

    // Size of the control-phase import request busid field (bytes)
#define xUSBIP_PROTO_IMPORT_REQ_SIZE (xUSBIP_PROTO_OP_COMMON_SIZE + xUSBIP_BUSID_LEN)

    // Size of the full data-phase header (bytes)
#define xUSBIP_PROTO_DATA_HEADER_SIZE 48U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Decoded form of a USBIP_CMD_SUBMIT (host-byte-order)
    typedef struct xUSBIP_Cmd_Submit_t
    {
        uint32_t seqnum;
        uint32_t devid;
        uint32_t direction; // xUSBIP_DIR_IN or xUSBIP_DIR_OUT
        uint32_t ep;
        uint32_t transfer_flags;
        uint32_t transfer_buffer_length;
        uint32_t interval;
        uint8_t setup[8];
    } xUSBIP_Cmd_Submit_t;

    // Decoded form of a USBIP_CMD_UNLINK (host-byte-order)
    typedef struct xUSBIP_Cmd_Unlink_t
    {
        uint32_t seqnum;
        uint32_t devid;
        uint32_t direction;
        uint32_t ep;
        uint32_t unlink_seqnum;
    } xUSBIP_Cmd_Unlink_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Decode the op_common header from a raw buffer (big-endian -> host).
    // Returns xRETURN_xERR_xUSBIP_PROTO_VERSION if version != xUSBIP_PROTO_VERSION.
    xRETURN_t
    xUSBIP_Proto_Decode_Op_Common(const uint8_t *buf, uint32_t buf_len, uint16_t *version_out, uint16_t *code_out, uint32_t *status_out);

    // Encode OP_REP_DEVLIST into buf; sets *bytes_written_out on success.
    // Interface entries following each device are zero-filled.
    xRETURN_t xUSBIP_Proto_Encode_Devlist_Reply(uint8_t *buf,
                                                uint32_t buf_len,
                                                const xUSBIP_Op_Device_t *devices,
                                                uint32_t device_count,
                                                uint32_t *bytes_written_out);

    // Encode OP_REP_IMPORT into buf; sets *bytes_written_out on success.
    // If status != xUSBIP_OP_STATUS_OK, only the 8-byte op_common is written.
    xRETURN_t xUSBIP_Proto_Encode_Import_Reply(uint8_t *buf,
                                               uint32_t buf_len,
                                               const xUSBIP_Op_Device_t *device,
                                               uint32_t status,
                                               uint32_t *bytes_written_out);

    // Decode the basic header prefix to determine command type.
    // *command_out receives the host-order command code (e.g. xUSBIP_CMD_SUBMIT).
    xRETURN_t xUSBIP_Proto_Decode_Basic_Header(const uint8_t *buf, uint32_t buf_len, uint32_t *command_out);

    // Decode USBIP_CMD_SUBMIT from a raw buffer (big-endian -> host).
    xRETURN_t xUSBIP_Proto_Decode_Cmd_Submit(const uint8_t *buf, uint32_t buf_len, xUSBIP_Cmd_Submit_t *out);

    // Encode USBIP_RET_SUBMIT into buf; sets *bytes_written_out on success.
    // data/data_len carry the IN payload (NULL / 0 for OUT transfers).
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
                                             uint32_t *bytes_written_out);

    // Decode USBIP_CMD_UNLINK from a raw buffer (big-endian -> host).
    xRETURN_t xUSBIP_Proto_Decode_Cmd_Unlink(const uint8_t *buf, uint32_t buf_len, xUSBIP_Cmd_Unlink_t *out);

    // Encode USBIP_RET_UNLINK into buf; sets *bytes_written_out on success.
    xRETURN_t xUSBIP_Proto_Encode_Ret_Unlink(uint8_t *buf,
                                             uint32_t buf_len,
                                             uint32_t seqnum,
                                             uint32_t devid,
                                             uint32_t direction,
                                             uint32_t ep,
                                             uint32_t status,
                                             uint32_t *bytes_written_out);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUSBIP_PROTO_H
// EOF /////////////////////////////////////////////////////////////////////////////
