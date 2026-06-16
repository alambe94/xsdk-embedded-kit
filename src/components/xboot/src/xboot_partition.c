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

// @file xboot_partition.c
// @brief Partition table management implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_partition.h"
#include "xboot_verify.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t validate_header(const uint8_t *buffer, uint32_t magic, uint32_t version, uint32_t num_entries);
static xRETURN_t validate_entry(const xBOOT_Partition_Entry_t *entry, uint32_t storage_size, uint32_t sector_size);
static bool check_overlap(const xBOOT_Partition_Entry_t *entry, const xBOOT_Partition_Table_t *table, uint32_t current_idx);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t validate_header(const uint8_t *buffer, uint32_t magic, uint32_t version, uint32_t num_entries)
{
    if (magic != xBOOT_PARTITION_MAGIC)
    {
        return xRETURN_xERR_xBOOT_INVALID_PARTITION;
    }

    if (version != xBOOT_PARTITION_VERSION_1)
    {
        return xRETURN_xERR_xBOOT_INVALID_PARTITION;
    }

    if (num_entries > xBOOT_MAX_PARTITIONS)
    {
        return xRETURN_xERR_xBOOT_INVALID_PARTITION;
    }

    // Check partition table CRC32
    uint32_t expected_crc;
    (void)memcpy(&expected_crc, &buffer[offsetof(xBOOT_Partition_Table_t, crc32)], sizeof(uint32_t));
    uint32_t computed_crc = xBOOT_CRC32_Calculate(buffer, offsetof(xBOOT_Partition_Table_t, crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    if (computed_crc != expected_crc)
    {
        return xRETURN_xERR_xBOOT_CRC_MISMATCH;
    }

    return xRETURN_xBOOT_OK;
}

static xRETURN_t validate_entry(const xBOOT_Partition_Entry_t *entry, uint32_t storage_size, uint32_t sector_size)
{
    // 1. Rejects invalid partition types
    if (entry->type == xBOOT_PARTITION_TYPE_INVALID)
    {
        return xRETURN_xERR_xBOOT_INVALID_PARTITION;
    }

    // 2. Alignment checks
    if ((!xBOOT_IS_ALIGNED(entry->offset, sector_size)) || (!xBOOT_IS_ALIGNED(entry->size, sector_size)))
    {
        return xRETURN_xERR_xBOOT_INVALID_PARTITION;
    }

    // 3. Reject empty partitions
    if (entry->size == 0U)
    {
        return xRETURN_xERR_xBOOT_INVALID_PARTITION;
    }

    // 4. Storage boundary checks
    if ((entry->offset > storage_size) || (entry->size > storage_size) || ((entry->offset + entry->size) > storage_size))
    {
        return xRETURN_xERR_xBOOT_INVALID_PARTITION;
    }

    return xRETURN_xBOOT_OK;
}

static bool check_overlap(const xBOOT_Partition_Entry_t *entry, const xBOOT_Partition_Table_t *table, uint32_t current_idx)
{
    for (uint32_t j = 0U; j < current_idx; j++)
    {
        const xBOOT_Partition_Entry_t *other = &table->entries[j];
        if ((entry->offset < (other->offset + other->size)) && (other->offset < (entry->offset + entry->size)))
        {
            return true;
        }
    }
    return false;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBOOT_Partition_Table_Parse(const uint8_t *buffer,
                                      uint32_t buffer_size,
                                      uint32_t storage_size,
                                      uint32_t sector_size,
                                      xBOOT_Partition_Table_t *out_table)
{
    if ((buffer == NULL) || (out_table == NULL))
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (buffer_size < sizeof(xBOOT_Partition_Table_t))
    {
        return xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    }

    if ((storage_size == 0U) || (sector_size == 0U))
    {
        return xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    }

    // Extract table header fields safely
    uint32_t magic;
    uint32_t version;
    uint32_t num_entries;
    (void)memcpy(&magic, &buffer[offsetof(xBOOT_Partition_Table_t, magic)], sizeof(uint32_t));
    (void)memcpy(&version, &buffer[offsetof(xBOOT_Partition_Table_t, version)], sizeof(uint32_t));
    (void)memcpy(&num_entries, &buffer[offsetof(xBOOT_Partition_Table_t, num_entries)], sizeof(uint32_t));

    xRETURN_t status = validate_header(buffer, magic, version, num_entries);
    if (status != xRETURN_xBOOT_OK)
    {
        return status;
    }

    uint32_t expected_crc;
    (void)memcpy(&expected_crc, &buffer[offsetof(xBOOT_Partition_Table_t, crc32)], sizeof(uint32_t));

    // Initialize out_table
    out_table->magic = magic;
    out_table->version = version;
    out_table->num_entries = num_entries;
    out_table->reserved = 0U;
    out_table->crc32 = expected_crc;

    // Parse entries
    for (uint32_t i = 0U; i < num_entries; i++)
    {
        uint32_t entry_offset = offsetof(xBOOT_Partition_Table_t, entries) + (i * sizeof(xBOOT_Partition_Entry_t));

        (void)memcpy(&out_table->entries[i].type, &buffer[entry_offset + offsetof(xBOOT_Partition_Entry_t, type)], sizeof(uint32_t));
        (void)memcpy(&out_table->entries[i].offset, &buffer[entry_offset + offsetof(xBOOT_Partition_Entry_t, offset)], sizeof(uint32_t));
        (void)memcpy(&out_table->entries[i].size, &buffer[entry_offset + offsetof(xBOOT_Partition_Entry_t, size)], sizeof(uint32_t));
        (void)memcpy(&out_table->entries[i].flags, &buffer[entry_offset + offsetof(xBOOT_Partition_Entry_t, flags)], sizeof(uint32_t));

        xBOOT_Partition_Entry_t *entry = &out_table->entries[i];

        status = validate_entry(entry, storage_size, sector_size);
        if (status != xRETURN_xBOOT_OK)
        {
            return status;
        }

        if (check_overlap(entry, out_table, i))
        {
            return xRETURN_xERR_xBOOT_INVALID_PARTITION;
        }
    }

    return xRETURN_xBOOT_OK;
}

xRETURN_t xBOOT_Partition_Lookup_Type(const xBOOT_Partition_Table_t *table, uint32_t type, xBOOT_Partition_Entry_t *out_entry)
{
    if ((table == NULL) || (out_entry == NULL))
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    for (uint32_t i = 0U; i < table->num_entries; i++)
    {
        if (table->entries[i].type == type)
        {
            (void)memcpy(out_entry, &table->entries[i], sizeof(xBOOT_Partition_Entry_t));
            return xRETURN_xBOOT_OK;
        }
    }

    return xRETURN_xERR_xBOOT_INVALID_PARTITION;
}

xRETURN_t xBOOT_Partition_Lookup_Slot(const xBOOT_Partition_Table_t *table, uint32_t slot_id, xBOOT_Partition_Entry_t *out_entry)
{
    if ((table == NULL) || (out_entry == NULL))
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    uint32_t type;
    if (slot_id == 0U)
    {
        type = xBOOT_PARTITION_TYPE_PRIMARY_APP;
    }
    else if (slot_id == 1U)
    {
        type = xBOOT_PARTITION_TYPE_SECONDARY_APP;
    }
    else
    {
        return xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    }

    return xBOOT_Partition_Lookup_Type(table, type, out_entry);
}

// EOF /////////////////////////////////////////////////////////////////////////////
