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

// @file xusbip_urb.h
// @brief xUSBIP URB tracking layer - pending URB table, submit, complete, unlink, timeout.
//

#ifndef XUSBIP_URB_H
#define XUSBIP_URB_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xusbip_config.h"
#include "xusbip_proto.h"
#include "xusbip_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef enum xUSBIP_URB_State_t
    {
        xUSBIP_URB_STATE_FREE = 0U,           // slot is available
        xUSBIP_URB_STATE_SUBMITTED = 1U,      // sent to xUSBH, awaiting completion
        xUSBIP_URB_STATE_PENDING_UNLINK = 2U, // CMD_UNLINK received, cancel in progress
    } xUSBIP_URB_State_t;

    typedef struct xUSBIP_URB_Entry_t
    {
        xUSBIP_URB_State_t state;
        uint32_t seqnum;
        uint32_t devid;
        uint32_t direction;
        uint32_t ep;
        uint32_t transfer_flags;
        uint32_t requested_length;
        uint8_t setup[8];
        uint32_t timeout_ms;
        uint32_t submit_time_ms; // wall-clock stamp at submit
    } xUSBIP_URB_Entry_t;

    typedef struct xUSBIP_URB_Table_t
    {
        xUSBIP_URB_Entry_t entries[xUSBIP_CONFIG_MAX_PENDING_URBS];
    } xUSBIP_URB_Table_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xUSBIP_URB_Table_Init(xUSBIP_URB_Table_t *table);

    // Allocate a slot and record the submitted URB. Returns slot index in *slot_out.
    xRETURN_t xUSBIP_URB_Submit(xUSBIP_URB_Table_t *table, const xUSBIP_Cmd_Submit_t *submit, uint32_t submit_time_ms, uint32_t *slot_out);

    // Mark slot as completed (call after xUSBH completion fires).
    xRETURN_t xUSBIP_URB_Complete(xUSBIP_URB_Table_t *table, uint32_t slot);

    // Find a pending URB by sequence number. Returns slot index in *slot_out.
    xRETURN_t xUSBIP_URB_Find_By_Seqnum(const xUSBIP_URB_Table_t *table, uint32_t seqnum, uint32_t *slot_out);

    // Mark a URB as pending-unlink (CMD_UNLINK received).
    xRETURN_t xUSBIP_URB_Request_Unlink(xUSBIP_URB_Table_t *table, uint32_t seqnum);

    // Free a slot unconditionally (after RET_SUBMIT or RET_UNLINK is sent).
    xRETURN_t xUSBIP_URB_Free(xUSBIP_URB_Table_t *table, uint32_t slot);

    // Walk table and return slot of oldest expired entry in *slot_out.
    // Returns xRETURN_xERR_xUSBIP_URB_NOT_FOUND if no expired slots exist.
    xRETURN_t xUSBIP_URB_Find_Expired(const xUSBIP_URB_Table_t *table, uint32_t now_ms, uint32_t *slot_out);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUSBIP_URB_H
// EOF /////////////////////////////////////////////////////////////////////////////
