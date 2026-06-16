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

// @file xboot_partition.h
// @brief Partition table management interface.
//

#ifndef XBOOT_PARTITION_H
#define XBOOT_PARTITION_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stdbool.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_return.h"
#include "xboot_defs.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xBOOT_PARTITION_TYPE_INVALID       0U
#define xBOOT_PARTITION_TYPE_BOOTLOADER    1U
#define xBOOT_PARTITION_TYPE_PRIMARY_APP   2U
#define xBOOT_PARTITION_TYPE_SECONDARY_APP 3U
#define xBOOT_PARTITION_TYPE_SCRATCH       4U
#define xBOOT_PARTITION_TYPE_RECOVERY      5U
#define xBOOT_PARTITION_TYPE_METADATA      6U
#define xBOOT_PARTITION_TYPE_FACTORY       7U

#define xBOOT_PARTITION_VERSION_1 1U

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xBOOT_Partition_Entry_t
    {
        uint32_t type;   // Partition type ID
        uint32_t offset; // Flash start offset in bytes
        uint32_t size;   // Size of the partition in bytes
        uint32_t flags;  // Partition flags
    } xBOOT_Partition_Entry_t;

    typedef struct xBOOT_Partition_Table_t
    {
        uint32_t magic;       // xBOOT_PARTITION_MAGIC (0x58425054U)
        uint32_t version;     // Format version (e.g. 1)
        uint32_t num_entries; // Number of active entries
        uint32_t reserved;    // Padding/alignment field
        xBOOT_Partition_Entry_t entries[xBOOT_MAX_PARTITIONS];
        uint32_t crc32; // CRC32 of preceding fields
    } xBOOT_Partition_Table_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    /**
     * @brief Parse and validate the partition table from raw buffer.
     * @param buffer Raw buffer containing the partition table data
     * @param buffer_size Size of the raw buffer in bytes
     * @param storage_size Size of the flash storage in bytes
     * @param sector_size Align size for offset and size (usually sector erase size)
     * @param out_table Pointer to output structure
     * @return xRETURN_t xRETURN_xBOOT_OK on success, error code otherwise
     */
    xRETURN_t xBOOT_Partition_Table_Parse(const uint8_t *buffer,
                                          uint32_t buffer_size,
                                          uint32_t storage_size,
                                          uint32_t sector_size,
                                          xBOOT_Partition_Table_t *out_table);

    /**
     * @brief Looks up a partition entry by type.
     * @param table Parsed partition table
     * @param type Partition type to lookup
     * @param out_entry Output partition entry
     * @return xRETURN_t xRETURN_xBOOT_OK on success, xRETURN_xERR_xBOOT_NOT_FOUND if not found
     */
    xRETURN_t xBOOT_Partition_Lookup_Type(const xBOOT_Partition_Table_t *table, uint32_t type, xBOOT_Partition_Entry_t *out_entry);

    /**
     * @brief Looks up a partition entry by Slot ID (0 -> Primary App, 1 -> Secondary App).
     * @param table Parsed partition table
     * @param slot_id Slot ID to lookup
     * @param out_entry Output partition entry
     * @return xRETURN_t xRETURN_xBOOT_OK on success, xRETURN_xERR_xBOOT_NOT_FOUND/xRETURN_xERR_xBOOT_INVALID_ARGUMENT
     */
    xRETURN_t xBOOT_Partition_Lookup_Slot(const xBOOT_Partition_Table_t *table, uint32_t slot_id, xBOOT_Partition_Entry_t *out_entry);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_PARTITION_H
// EOF /////////////////////////////////////////////////////////////////////////////
