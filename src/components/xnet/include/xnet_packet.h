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

// @file xnet_packet.h
// @brief Packet buffer allocation, release, and helper macros for the xNET module.
//

#ifndef XNET_PACKET_H
#define XNET_PACKET_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xnet_defs.h"
#include "xnet_return.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xNET_PACKET_BUFFER_ALIGNMENT 32U
#define xNET_PACKET_FRAME_SIZE       1536U /**< 1514 MTU+Header rounded up to next multiple of 32 for DMA alignment */

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xNET_Packet_Buffer_t xNET_Packet_Buffer_t;

    struct xNET_Packet_Buffer_t
    {
        uint8_t *buffer;      /**< Pointer to the start of the carved buffer */
        uint32_t capacity;    /**< Maximum buffer size (typically xNET_PACKET_FRAME_SIZE) */
        uint32_t data_offset; /**< Offset from start of buffer to active payload data */
        uint32_t data_length; /**< Length of the active payload data */
        bool is_in_use;       /**< Allocation status flag */
        uint32_t flags;       /**< RX/TX metadata flags */
    };

    // Forward declaration of xNET_Context_t to break circular dependency
    typedef struct xNET_Context_t xNET_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    /**
     * @brief Get pointer to the start of active payload data in the packet buffer.
     */
    static inline uint8_t *xNET_Packet_Get_Data(const xNET_Packet_Buffer_t *packet)
    {
        return (packet != NULL) ? (packet->buffer + packet->data_offset) : NULL;
    }

    /**
     * @brief Get length of the active payload data in the packet buffer.
     */
    static inline uint32_t xNET_Packet_Get_Length(const xNET_Packet_Buffer_t *packet)
    {
        return (packet != NULL) ? packet->data_length : 0U;
    }

    /**
     * @brief Get total capacity of the packet buffer.
     */
    static inline uint32_t xNET_Packet_Get_Capacity(const xNET_Packet_Buffer_t *packet)
    {
        return (packet != NULL) ? packet->capacity : 0U;
    }

    /**
     * @brief Explicitly set the data length of the packet buffer payload.
     */
    static inline xRETURN_t xNET_Packet_Set_Length(xNET_Packet_Buffer_t *packet, uint32_t length)
    {
        if (packet == NULL)
        {
            return xRETURN_xERR_xNET_NULL_POINTER;
        }
        if (packet->data_offset + length > packet->capacity)
        {
            return xRETURN_xERR_xNET_INVALID_LENGTH;
        }
        packet->data_length = length;
        return xRETURN_xNET_OK;
    }

    /**
     * @brief Push data offset backward to allocate headroom for protocol headers.
     */
    static inline xRETURN_t xNET_Packet_Push(xNET_Packet_Buffer_t *packet, uint32_t size)
    {
        if (packet == NULL)
        {
            return xRETURN_xERR_xNET_NULL_POINTER;
        }
        if (packet->data_offset < size)
        {
            return xRETURN_xERR_xNET_BUFFER_TOO_SMALL;
        }
        packet->data_offset -= size;
        packet->data_length += size;
        return xRETURN_xNET_OK;
    }

    /**
     * @brief Pull data offset forward to strip/consume protocol headers.
     */
    static inline xRETURN_t xNET_Packet_Pull(xNET_Packet_Buffer_t *packet, uint32_t size)
    {
        if (packet == NULL)
        {
            return xRETURN_xERR_xNET_NULL_POINTER;
        }
        if (packet->data_length < size)
        {
            return xRETURN_xERR_xNET_INVALID_LENGTH;
        }
        packet->data_offset += size;
        packet->data_length -= size;
        return xRETURN_xNET_OK;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xRETURN_t xNET_Packet_Alloc(xNET_Context_t *net_ctx, xNET_Packet_Buffer_t **packet_buf);
    xRETURN_t xNET_Packet_Release(xNET_Context_t *net_ctx, xNET_Packet_Buffer_t *packet_buf);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_PACKET_H
// EOF /////////////////////////////////////////////////////////////////////////////
