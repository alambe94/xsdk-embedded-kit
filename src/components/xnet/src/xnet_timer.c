// Copyright 2026 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xnet_timer.c
// @brief Passive timer and timeout layer implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xnet_timer.h"
#include "xnet_arp.h"
#include "xnet_dhcp.h"
#include "xassert.h"

#include "xnet_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xNET_Tick(xNET_Context_t *net_ctx, uint32_t elapsed_ms)
{
    xASSERT(net_ctx != NULL, "net_ctx is NULL");
    if (net_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xASSERT(net_ctx->is_initialized == true, "net_ctx not initialized");
    if (net_ctx->is_initialized == false)
    {
        return xRETURN_xERR_xNET_INVALID_STATE;
    }

    net_ctx->system_ticks += elapsed_ms;

    // Loop through all registered interfaces and tick them
    xNET_Interface_Context_t *curr = net_ctx->interface_list;
    for (uint32_t i = 0U; (curr != NULL) && (i < xNET_CONFIG_MAX_INTERFACES); i++)
    {
        xNET_ARP_Tick(curr, elapsed_ms);
        xNET_DHCP_Tick(curr, elapsed_ms);
        curr = curr->next;
    }

    return xRETURN_xNET_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
