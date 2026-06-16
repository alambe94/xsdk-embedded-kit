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

// @file xnet_config.h
// @brief Sizing and configuration parameters for the xNET module.
//

#ifndef XNET_CONFIG_H
#define XNET_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES

// MACROS //////////////////////////////////////////////////////////////////////
#ifndef xNET_CONFIG_LOG_LEVEL
#define xNET_CONFIG_LOG_LEVEL 3U // 0=None, 1=Error, 2=Warning, 3=Info, 4=Debug
#endif

#ifndef xNET_CONFIG_PACKET_POOL_SIZE
#define xNET_CONFIG_PACKET_POOL_SIZE 8U
#endif

#ifndef xNET_CONFIG_MAX_INTERFACES
#define xNET_CONFIG_MAX_INTERFACES 2U
#endif

#ifndef xNET_CONFIG_MAX_UDP_SOCKETS
#define xNET_CONFIG_MAX_UDP_SOCKETS 4U
#endif

#ifndef xNET_CONFIG_ARP_CACHE_SIZE
#define xNET_CONFIG_ARP_CACHE_SIZE 8U
#endif

#ifndef xNET_CONFIG_ARP_ENTRY_TIMEOUT_MS
#define xNET_CONFIG_ARP_ENTRY_TIMEOUT_MS 300000U // 5 minutes
#endif

#ifndef xNET_CONFIG_ARP_MAX_RETRIES
#define xNET_CONFIG_ARP_MAX_RETRIES 3U
#endif

#ifndef xNET_CONFIG_ARP_RETRY_TIMEOUT_MS
#define xNET_CONFIG_ARP_RETRY_TIMEOUT_MS 1000U // 1 second
#endif

#ifndef xNET_CONFIG_IPV4_DEFAULT_TTL
#define xNET_CONFIG_IPV4_DEFAULT_TTL 64U
#endif

#ifndef xNET_CONFIG_DNS_MAX_RETRIES
#define xNET_CONFIG_DNS_MAX_RETRIES 3U
#endif

#ifndef xNET_CONFIG_DNS_RETRY_TIMEOUT_MS
#define xNET_CONFIG_DNS_RETRY_TIMEOUT_MS 2000U
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
