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

// @file xbridge_core.h
// @brief xBRIDGE core framework - USB ops table and shared binary frame parser/builder.
//

#ifndef XBRIDGE_CORE_H
#define XBRIDGE_CORE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xbridge_defs.h"
#include "xbridge_return.h"
#include "xreturn.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // USB-side ops: how xBRIDGE sends data back to the USB host.
    // Each channel context holds a pointer to one of these tables plus a usb_ctx.
    typedef struct xBRIDGE_USB_Ops_t
    {
        // Write data to the active USB IN endpoint (CDC TX, WINUSB bulk IN, or HID IN).
        xRETURN_t (*send)(void *usb_ctx, const uint8_t *data, uint32_t length);

    } xBRIDGE_USB_Ops_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Parse a complete xBRIDGE_Frame_Cmd_t from a raw Bulk OUT buffer.
    // On success sets *cmd_out to point into buf and *payload_out to the payload bytes.
    xRETURN_t xBRIDGE_Core_Parse_Frame(const uint8_t *buf, uint32_t buf_len, xBRIDGE_Frame_Cmd_t *cmd_out, const uint8_t **payload_out);

    // Serialize a xBRIDGE_Frame_Resp_t plus optional data into resp_buf.
    // Returns the total byte count written (header + data_len), or 0 on overflow.
    uint32_t xBRIDGE_Core_Build_Response(uint8_t *resp_buf,
                                         uint32_t resp_buf_len,
                                         uint8_t channel,
                                         uint8_t status,
                                         uint32_t seq,
                                         const uint8_t *data,
                                         uint32_t data_len);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_CORE_H
// EOF /////////////////////////////////////////////////////////////////////////////
