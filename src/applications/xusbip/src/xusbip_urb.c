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

// @file xusbip_urb.c
// @brief xUSBIP URB tracking layer - pending URB table, submit, complete, unlink, timeout.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbip_urb.h"
#include "xassert.h"

#include "xusbip_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xUSBIP_URB_Table_Init(xUSBIP_URB_Table_t *table)
{
    xASSERT(table != NULL, "table is NULL");

    if (table == NULL)
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    (void)memset(table, 0, sizeof(xUSBIP_URB_Table_t));
    return xRETURN_OK;
}

xRETURN_t xUSBIP_URB_Submit(xUSBIP_URB_Table_t *table, const xUSBIP_Cmd_Submit_t *submit, uint32_t submit_time_ms, uint32_t *slot_out)
{
    xASSERT(table != NULL, "table is NULL");
    xASSERT(submit != NULL, "submit is NULL");
    xASSERT(slot_out != NULL, "slot_out is NULL");

    if ((table == NULL) || (submit == NULL) || (slot_out == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    for (uint32_t i = 0U; i < xUSBIP_CONFIG_MAX_PENDING_URBS; i++)
    {
        if (table->entries[i].state == xUSBIP_URB_STATE_FREE)
        {
            table->entries[i].state = xUSBIP_URB_STATE_SUBMITTED;
            table->entries[i].seqnum = submit->seqnum;
            table->entries[i].devid = submit->devid;
            table->entries[i].direction = submit->direction;
            table->entries[i].ep = submit->ep;
            table->entries[i].transfer_flags = submit->transfer_flags;
            table->entries[i].requested_length = submit->transfer_buffer_length;
            table->entries[i].timeout_ms = xUSBIP_CONFIG_URB_TIMEOUT_MS;
            table->entries[i].submit_time_ms = submit_time_ms;
            (void)memcpy(table->entries[i].setup, submit->setup, 8U);
            *slot_out = i;
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xUSBIP_NO_URB_SLOT;
}

xRETURN_t xUSBIP_URB_Complete(xUSBIP_URB_Table_t *table, uint32_t slot)
{
    xASSERT(table != NULL, "table is NULL");

    if (table == NULL)
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    if (slot >= xUSBIP_CONFIG_MAX_PENDING_URBS)
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    if (table->entries[slot].state == xUSBIP_URB_STATE_FREE)
    {
        return xRETURN_xERR_xUSBIP_INVALID_STATE;
    }

    table->entries[slot].state = xUSBIP_URB_STATE_FREE;
    return xRETURN_OK;
}

xRETURN_t xUSBIP_URB_Find_By_Seqnum(const xUSBIP_URB_Table_t *table, uint32_t seqnum, uint32_t *slot_out)
{
    xASSERT(table != NULL, "table is NULL");
    xASSERT(slot_out != NULL, "slot_out is NULL");

    if ((table == NULL) || (slot_out == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    for (uint32_t i = 0U; i < xUSBIP_CONFIG_MAX_PENDING_URBS; i++)
    {
        if ((table->entries[i].state != xUSBIP_URB_STATE_FREE) && (table->entries[i].seqnum == seqnum))
        {
            *slot_out = i;
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xUSBIP_URB_NOT_FOUND;
}

xRETURN_t xUSBIP_URB_Request_Unlink(xUSBIP_URB_Table_t *table, uint32_t seqnum)
{
    xASSERT(table != NULL, "table is NULL");

    if (table == NULL)
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    for (uint32_t i = 0U; i < xUSBIP_CONFIG_MAX_PENDING_URBS; i++)
    {
        if ((table->entries[i].state == xUSBIP_URB_STATE_SUBMITTED) && (table->entries[i].seqnum == seqnum))
        {
            table->entries[i].state = xUSBIP_URB_STATE_PENDING_UNLINK;
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xUSBIP_URB_NOT_FOUND;
}

xRETURN_t xUSBIP_URB_Free(xUSBIP_URB_Table_t *table, uint32_t slot)
{
    xASSERT(table != NULL, "table is NULL");

    if (table == NULL)
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    if (slot >= xUSBIP_CONFIG_MAX_PENDING_URBS)
    {
        return xRETURN_xERR_xUSBIP_INVALID_ARGUMENT;
    }

    (void)memset(&table->entries[slot], 0, sizeof(xUSBIP_URB_Entry_t));
    return xRETURN_OK;
}

xRETURN_t xUSBIP_URB_Find_Expired(const xUSBIP_URB_Table_t *table, uint32_t now_ms, uint32_t *slot_out)
{
    xASSERT(table != NULL, "table is NULL");
    xASSERT(slot_out != NULL, "slot_out is NULL");

    if ((table == NULL) || (slot_out == NULL))
    {
        return xRETURN_xERR_xUSBIP_NULL_POINTER;
    }

    uint32_t oldest_slot = 0U;
    uint32_t oldest_age = 0U;
    bool found_expired = false;

    for (uint32_t i = 0U; i < xUSBIP_CONFIG_MAX_PENDING_URBS; i++)
    {
        if (table->entries[i].state == xUSBIP_URB_STATE_FREE)
        {
            continue;
        }

        if (table->entries[i].timeout_ms == 0U)
        {
            continue;
        }

        uint32_t age = now_ms - table->entries[i].submit_time_ms;
        if (age >= table->entries[i].timeout_ms)
        {
            if (!found_expired || (age > oldest_age))
            {
                oldest_slot = i;
                oldest_age = age;
                found_expired = true;
            }
        }
    }

    if (!found_expired)
    {
        return xRETURN_xERR_xUSBIP_URB_NOT_FOUND;
    }

    *slot_out = oldest_slot;
    return xRETURN_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
