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

// @file xusbip_return.h
// @brief xUSBIP module-specific xRETURN_t error, warning, and message codes.
//

#ifndef XUSBIP_RETURN_H
#define XUSBIP_RETURN_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xreturn.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef enum
    {
        // -------------------------------------------------------------------------
        // Errors [severity = xRETURN_SEVERITY_ERROR]
        // -------------------------------------------------------------------------
        xRETURN_xERR_xUSBIP_NULL_POINTER = xRETURN_MAKE(xRETURN_xUSBIP_MODULE, xRETURN_SEVERITY_ERROR, 0x001U),
        xRETURN_xERR_xUSBIP_INVALID_ARGUMENT,
        xRETURN_xERR_xUSBIP_INVALID_STATE,
        xRETURN_xERR_xUSBIP_NO_DEVICE_SLOT,     // no free export slot
        xRETURN_xERR_xUSBIP_NO_URB_SLOT,        // pending URB table full
        xRETURN_xERR_xUSBIP_DEVICE_NOT_FOUND,   // busid not in export table
        xRETURN_xERR_xUSBIP_PROTO_VERSION,      // unsupported protocol version
        xRETURN_xERR_xUSBIP_PROTO_OPCODE,       // unknown or out-of-order op-code
        xRETURN_xERR_xUSBIP_TCP_SEND,           // TCP send failure
        xRETURN_xERR_xUSBIP_TCP_RECV,           // TCP receive failure
        xRETURN_xERR_xUSBIP_TRANSFER_TOO_LARGE, // URB larger than config max
        xRETURN_xERR_xUSBIP_URB_NOT_FOUND,      // unlink seqnum not pending
        xRETURN_xERR_xUSBIP_USBH_SUBMIT,        // xUSBH submit returned error
        xRETURN_xERR_xUSBIP_USBH_CANCEL,        // xUSBH cancel returned error
        xRETURN_xERR_xUSBIP_BUFFER_TOO_SMALL,   // output buffer insufficient

        // -------------------------------------------------------------------------
        // Warnings [severity = xRETURN_SEVERITY_WARNING]
        // -------------------------------------------------------------------------
        xRETURN_xWRN_xUSBIP_URB_TIMEOUT = xRETURN_MAKE(xRETURN_xUSBIP_MODULE, xRETURN_SEVERITY_WARNING, 0x001U),
        xRETURN_xWRN_xUSBIP_CLIENT_DISCONNECT, // remote client closed connection
        xRETURN_xWRN_xUSBIP_DEVICE_RESET,      // USB device reset during session

        // -------------------------------------------------------------------------
        // Messages [severity = xRETURN_SEVERITY_MESSAGE]
        // -------------------------------------------------------------------------
        xRETURN_xMSG_xUSBIP_DEVLIST_SENT = xRETURN_MAKE(xRETURN_xUSBIP_MODULE, xRETURN_SEVERITY_MESSAGE, 0x001U),
        xRETURN_xMSG_xUSBIP_DEVICE_ATTACHED, // client successfully imported device
        xRETURN_xMSG_xUSBIP_DEVICE_DETACHED, // client disconnected; device released
        xRETURN_xMSG_xUSBIP_URB_COMPLETE,    // URB returned to client
    } xRETURN_xUSBIP_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUSBIP_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
