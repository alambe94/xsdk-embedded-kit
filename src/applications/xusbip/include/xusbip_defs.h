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

// @file xusbip_defs.h
// @brief xUSBIP protocol constants, packed on-wire structures, and endian helpers.
// All multi-byte wire fields are big-endian; all host-facing values are host-byte-order.
// USB/IP wire field names (seqnum, busid, devid, ep) are preserved from the specification.
//

#ifndef XUSBIP_DEFS_H
#define XUSBIP_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////

    // Protocol version
#define xUSBIP_PROTO_VERSION 0x0111U

    // TCP port
#define xUSBIP_TCP_PORT 3240U

    // OP codes (control phase, big-endian on wire)
#define xUSBIP_OP_REQ_DEVLIST   0x8005U
#define xUSBIP_OP_REP_DEVLIST   0x0005U
#define xUSBIP_OP_REQ_IMPORT    0x8003U
#define xUSBIP_OP_REP_IMPORT    0x0003U
#define xUSBIP_OP_STATUS_OK     0x00000000U
#define xUSBIP_OP_STATUS_FAILED 0x00000001U

    // URB command codes (data phase, big-endian on wire)
#define xUSBIP_CMD_SUBMIT 0x00000001U
#define xUSBIP_RET_SUBMIT 0x00000003U
#define xUSBIP_CMD_UNLINK 0x00000002U
#define xUSBIP_RET_UNLINK 0x00000004U

    // Direction
#define xUSBIP_DIR_OUT 0x00000000U
#define xUSBIP_DIR_IN  0x00000001U

    // Transfer flags (subset of Linux URB flags used by USB/IP)
#define xUSBIP_URB_SHORT_NOT_OK 0x00000001U
#define xUSBIP_URB_ISO_ASAP     0x00000002U
#define xUSBIP_URB_ZERO_PACKET  0x00000040U

    // Speed values used in OP_REP_DEVLIST / OP_REP_IMPORT
#define xUSBIP_SPEED_UNKNOWN  0x00000000U
#define xUSBIP_SPEED_LOW      0x00000001U
#define xUSBIP_SPEED_FULL     0x00000002U
#define xUSBIP_SPEED_HIGH     0x00000003U
#define xUSBIP_SPEED_WIRELESS 0x00000004U
#define xUSBIP_SPEED_SUPER    0x00000005U

    // Wire string field sizes (bytes, including null terminator)
#define xUSBIP_PATH_LEN  256U
#define xUSBIP_BUSID_LEN 32U

    // Linux errno equivalents used in RET_SUBMIT status field
#define xUSBIP_ERRNO_SUCCESS 0U
#define xUSBIP_ERRNO_EPIPE   32U // stall / pipe error
#define xUSBIP_ERRNO_ENODEV  19U // device gone

    // TYPES ///////////////////////////////////////////////////////////////////////

    // op_common header - version, op-code, and status (8 bytes)
    typedef struct __attribute__((packed)) xUSBIP_Op_Common_t
    {
        uint16_t version;
        uint16_t code;
        uint32_t status;
    } xUSBIP_Op_Common_t;

    // Per-device descriptor in OP_REP_DEVLIST and OP_REP_IMPORT (312 bytes).
    // USB/IP specification field names are preserved verbatim.
    typedef struct __attribute__((packed)) xUSBIP_Op_Device_t
    {
        char path[xUSBIP_PATH_LEN];
        char busid[xUSBIP_BUSID_LEN];
        uint32_t busnum;
        uint32_t devnum;
        uint32_t speed;
        uint16_t idVendor;
        uint16_t idProduct;
        uint16_t bcdDevice;
        uint8_t bDeviceClass;
        uint8_t bDeviceSubClass;
        uint8_t bDeviceProtocol;
        uint8_t bConfigurationValue;
        uint8_t bNumConfigurations;
        uint8_t bNumInterfaces;
    } xUSBIP_Op_Device_t;

    // Per-interface entry appended after each device in OP_REP_DEVLIST (4 bytes)
    typedef struct __attribute__((packed)) xUSBIP_Op_Interface_t
    {
        uint8_t bInterfaceClass;
        uint8_t bInterfaceSubClass;
        uint8_t bInterfaceProtocol;
        uint8_t padding;
    } xUSBIP_Op_Interface_t;

    // Basic URB header common to all data-phase commands (20 bytes)
    typedef struct __attribute__((packed)) xUSBIP_Header_Basic_t
    {
        uint32_t command;
        uint32_t seqnum;
        uint32_t devid;
        uint32_t direction;
        uint32_t ep;
    } xUSBIP_Header_Basic_t;

    // USBIP_CMD_SUBMIT - client URB submission (48 bytes total)
    typedef struct __attribute__((packed)) xUSBIP_Header_Cmd_Submit_t
    {
        xUSBIP_Header_Basic_t basic;
        uint32_t transfer_flags;
        uint32_t transfer_buffer_length;
        uint32_t start_frame;
        uint32_t number_of_packets;
        uint32_t interval;
        uint8_t setup[8];
    } xUSBIP_Header_Cmd_Submit_t;

    // USBIP_RET_SUBMIT - server URB reply (48 bytes total + optional data payload)
    typedef struct __attribute__((packed)) xUSBIP_Header_Ret_Submit_t
    {
        xUSBIP_Header_Basic_t basic;
        uint32_t status;
        uint32_t actual_length;
        uint32_t start_frame;
        uint32_t number_of_packets;
        uint32_t error_count;
        uint8_t setup[8];
    } xUSBIP_Header_Ret_Submit_t;

    // USBIP_CMD_UNLINK - client cancel request (48 bytes total)
    typedef struct __attribute__((packed)) xUSBIP_Header_Cmd_Unlink_t
    {
        xUSBIP_Header_Basic_t basic;
        uint32_t unlink_seqnum;
        uint8_t padding[24];
    } xUSBIP_Header_Cmd_Unlink_t;

    // USBIP_RET_UNLINK - server cancel reply (48 bytes total)
    typedef struct __attribute__((packed)) xUSBIP_Header_Ret_Unlink_t
    {
        xUSBIP_Header_Basic_t basic;
        uint32_t status;
        uint8_t padding[24];
    } xUSBIP_Header_Ret_Unlink_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // Big-endian <-> host byte-order conversion helpers.
    // Swapping is symmetric, so the same function encodes and decodes.
    static inline uint16_t xusbip_be16(uint16_t val)
    {
        return (uint16_t)(((val & 0x00FFU) << 8U) | ((val & 0xFF00U) >> 8U));
    }

    static inline uint32_t xusbip_be32(uint32_t val)
    {
        return ((val & 0x000000FFU) << 24U) | ((val & 0x0000FF00U) << 8U) | ((val & 0x00FF0000U) >> 8U) | ((val & 0xFF000000U) >> 24U);
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

// Wire struct size assertions - verified at compile time
_Static_assert(sizeof(xUSBIP_Op_Common_t) == 8U, "xUSBIP_Op_Common_t size mismatch");
_Static_assert(sizeof(xUSBIP_Op_Device_t) == 312U, "xUSBIP_Op_Device_t size mismatch");
_Static_assert(sizeof(xUSBIP_Op_Interface_t) == 4U, "xUSBIP_Op_Interface_t size mismatch");
_Static_assert(sizeof(xUSBIP_Header_Basic_t) == 20U, "xUSBIP_Header_Basic_t size mismatch");
_Static_assert(sizeof(xUSBIP_Header_Cmd_Submit_t) == 48U, "xUSBIP_Header_Cmd_Submit_t size mismatch");
_Static_assert(sizeof(xUSBIP_Header_Ret_Submit_t) == 48U, "xUSBIP_Header_Ret_Submit_t size mismatch");
_Static_assert(sizeof(xUSBIP_Header_Cmd_Unlink_t) == 48U, "xUSBIP_Header_Cmd_Unlink_t size mismatch");
_Static_assert(sizeof(xUSBIP_Header_Ret_Unlink_t) == 48U, "xUSBIP_Header_Ret_Unlink_t size mismatch");

#endif // XUSBIP_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
