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

// @file xusbd_return.h
// @brief Status and error code definitions for the xUSB Device Stack.

#ifndef XUSBD_RETURN_H
#define XUSBD_RETURN_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xreturn.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum
    {
        // -------------------------------------------------------------------------
        // Errors [severity = xRETURN_SEVERITY_ERROR]
        // -------------------------------------------------------------------------

        // DCD errors [0x001 - 0x0FF]
        xRETURN_xERR_xUSBD_DCD_NULL_POINTER = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_ERROR, 0x001),
        xRETURN_xERR_xUSBD_DCD_INVALID_PORT,
        xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT,
        xRETURN_xERR_xUSBD_DCD_NO_BUFFER_AVA,
        xRETURN_xERR_xUSBD_DCD_SPEED_NOT_SUPPORTED,
        xRETURN_xERR_xUSBD_DCD_EP0_IN_SETUP_PHASE,

        // Generic USBD errors [0x100 - 0x1FF]
        xRETURN_xERR_xUSBD_NULL_POINTER = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_ERROR, 0x100),
        xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE,
        xRETURN_xERR_xUSBD_INDEX_OUT_OF_RANGE,
        xRETURN_xERR_xUSBD_ALREADY_INITIALIZED,

        // Class errors [0x200 - 0x2FF]
        xRETURN_xERR_xUSBD_REQ_GET_STATUS_INVALID_RECIPIENT = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_ERROR, 0x200),

        xRETURN_xERR_xUSBD_REQ_GET_DESC_NOT_SUPPORTED,
        xRETURN_xERR_xUSBD_REQ_SET_DESC_NOT_SUPPORTED,
        xRETURN_xERR_xUSBD_REQ_GET_CONFIG_NOT_ADDRESSED,
        xRETURN_xERR_xUSBD_REQ_SET_CONFIG_NOT_SUPPORTED,
        xRETURN_xERR_xUSBD_REQ_SET_CONFIG_NOT_ADDRESSED,
        xRETURN_xERR_xUSBD_INVALID_SETUP_PACKET,
        xRETURN_xERR_xUSBD_INVALID_CLASS_REQ,
        xRETURN_xERR_xUSBD_REQ_GET_DESC_CONFIG_OOR,
        xRETURN_xERR_xUSBD_REQ_CLEAR_EP_INVALID_REQ,
        xRETURN_xERR_xUSBD_REQ_CLEAR_DEVICE_INVALID_REQ,
        xRETURN_xERR_xUSBD_REQ_SET_FEATURE_EP_INVALID_REQ,
        xRETURN_xERR_xUSBD_REQ_SET_FEATURE_DEVICE_INVALID_REQ,
        xRETURN_xERR_xUSBD_REQ_SET_FEATURE_INVALID_RECIPIENT,
        xRETURN_xERR_xUSBD_REQ_SET_ADDRESS_INVALID_REQ,
        xRETURN_xERR_xUSBD_CLASS_NOT_INSTALLED,
        xRETURN_xERR_xUSBD_APP_NOT_INSTALLED,
        xRETURN_xERR_xUSBD_APP_FEATURE_NOT_SUPPORTED,
        xRETURN_xERR_xUSBD_ADDRESS_NOT_ALIGNED,
        xRETURN_xERR_xUSBD_CLASS_INSTANCE_NOT_AVAILABLE,
        xRETURN_xERR_xUSBD_NOT_INITIALIZED,
        xRETURN_xERR_xUSBD_INVALID_CONFIGURATION,
        xRETURN_xERR_xUSBD_RESOURCE_EXHAUSTED,
        xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST,

        // MSC errors [0x300 - 0x3FF]
        xRETURN_xERR_xUSBD_MSC_CBW_SIGNATURE = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_ERROR, 0x300),
        xRETURN_xERR_xUSBD_MSC_BOT_CMD_FAILED,
        xRETURN_xERR_xUSBD_MSC_CMD_UNKNOWN,

        // -------------------------------------------------------------------------
        // Warnings [severity = xRETURN_SEVERITY_WARNING]
        // -------------------------------------------------------------------------

        // DCD warnings [0x001 - 0x0FF]
        xRETURN_xWRN_xUSBD_DCD_EP_BUSY = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_WARNING, 0x001),

        // USBD warnings [0x100 - 0x1FF]
        xRETURN_xWRN_xUSBD_STRING_TRUNCATED = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_WARNING, 0x100),
        xRETURN_xWRN_xUSBD_BUFFER_MIGHT_OVERFLOW,

        // -------------------------------------------------------------------------
        // Messages [severity = xRETURN_SEVERITY_MESSAGE]
        // -------------------------------------------------------------------------

        xRETURN_xMSG_xUSBD_REQ_GET_STATUS_REMOTE_WAKE = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_MESSAGE, 0x001),
        xRETURN_xMSG_xUSBD_REQ_GET_STATUS_INTF,
        xRETURN_xMSG_xUSBD_REQ_GET_STATUS_EP,
        xRETURN_xMSG_xUSBD_REQ_CLEAR_EP_STALL,
        xRETURN_xMSG_xUSBD_REQ_CLEAR_REMOTE_WAKE,
        xRETURN_xMSG_xUSBD_REQ_CLEAR_TEST_MODE,
        xRETURN_xMSG_xUSBD_REQ_CLEAR_INVALID_RECIPIENT,
        xRETURN_xMSG_xUSBD_REQ_SET_FEATURE_EP_STALL,
        xRETURN_xMSG_xUSBD_REQ_SET_FEATURE_REMOTE_WAKE,
        xRETURN_xMSG_xUSBD_REQ_SET_FEATURE_TEST_MODE,
        xRETURN_xMSG_xUSBD_REQ_SET_ADDRESS,
        xRETURN_xMSG_xUSBD_REQ_GET_DESC_DEVICE,
        xRETURN_xMSG_xUSBD_REQ_GET_DESC_CONFIG,
        xRETURN_xMSG_xUSBD_REQ_GET_DESC_STRING,
        xRETURN_xMSG_xUSBD_REQ_GET_DESC_STRING_OOR,
        xRETURN_xMSG_xUSBD_REQ_GET_DESC_QUALIFIER,
        xRETURN_xMSG_xUSBD_REQ_GET_DESC_OTHER_SPEED,
        xRETURN_xMSG_xUSBD_REQ_GET_DESC_OTG,
        xRETURN_xMSG_xUSBD_REQ_GET_DESC_HID,
        xRETURN_xMSG_xUSBD_REQ_GET_DESC_REPORT,
        xRETURN_xMSG_xUSBD_REQ_GET_DESC_BOS,
        xRETURN_xMSG_xUSBD_REQ_GET_CONFIG_CONFIGURED,
        xRETURN_xMSG_xUSBD_REQ_GET_CONFIG_ADDRESSED,
        xRETURN_xMSG_xUSBD_REQ_SET_CONFIG_ZERO,
        xRETURN_xMSG_xUSBD_REQ_SET_CONFIG_ONE,
        xRETURN_xMSG_xUSBD_ENUMERATED_SPEED,

        // MSC messages [0x100 - 0x1FF]
        xRETURN_xMSG_xUSBD_MSC_REQ_GET_MAX_LUN = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_MESSAGE, 0x100),
        xRETURN_xMSG_xUSBD_MSC_REQ_RESET,
        xRETURN_xMSG_xUSBD_MSC_BOT_OUT,
        xRETURN_xMSG_xUSBD_MSC_CMD_TEST_UNIT_READY,
        xRETURN_xMSG_xUSBD_MSC_CMD_REWIND,
        xRETURN_xMSG_xUSBD_MSC_CMD_REQUEST_SENSE,
        xRETURN_xMSG_xUSBD_MSC_CMD_FORMAT,
        xRETURN_xMSG_xUSBD_MSC_CMD_INQUIRY,
        xRETURN_xMSG_xUSBD_MSC_CMD_MODE_SELECT6,
        xRETURN_xMSG_xUSBD_MSC_CMD_RELEASE6,
        xRETURN_xMSG_xUSBD_MSC_CMD_MODE_SENSE6,
        xRETURN_xMSG_xUSBD_MSC_CMD_START_STOP_UNIT,
        xRETURN_xMSG_xUSBD_MSC_CMD_SEND_DIAGNOSTIC,
        xRETURN_xMSG_xUSBD_MSC_CMD_PREVENT_REMOVAL,
        xRETURN_xMSG_xUSBD_MSC_CMD_READ_FORMAT_CAPACITIES,
        xRETURN_xMSG_xUSBD_MSC_CMD_READ_CAPACITY,
        xRETURN_xMSG_xUSBD_MSC_CMD_READ10,
        xRETURN_xMSG_xUSBD_MSC_CMD_WRITE10,
        xRETURN_xMSG_xUSBD_MSC_CMD_SEEK10,
        xRETURN_xMSG_xUSBD_MSC_CMD_WRITE_VERIFY10,
        xRETURN_xMSG_xUSBD_MSC_CMD_VERIFY10,
        xRETURN_xMSG_xUSBD_MSC_CMD_SYNCHRONIZE_CACHE,
        xRETURN_xMSG_xUSBD_MSC_CMD_MODE_SENSE10,
        xRETURN_xMSG_xUSBD_MSC_CMD_READ12,
        xRETURN_xMSG_xUSBD_MSC_CMD_WRITE12,
    } xRETURN_xUSBD_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // XUSBD_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
