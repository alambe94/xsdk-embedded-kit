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

// @file xfs_format.c
// @brief xFS FAT32 volume formatter.

// INCLUDES ///////////////////////////////////////////////////////////////////
#include <string.h>

#include "xfs_format.h"
#include "xfs_defs.h"
#include "xfs_fat32.h"
#include "xfs_fat32_bpb.h"
#include "xfs_config.h"

#include "xfs_log.h"

// MACROS //////////////////////////////////////////////////////////////////////

#define FAT32_BOOT_JUMP_SIZE       3U
#define FAT32_OEM_NAME_OFFSET      3U
#define FAT32_OEM_NAME_SIZE        8U
#define FAT32_ROOT_ENTRY_COUNT     0U
#define FAT32_TOTAL_SECTORS_16     0U
#define FAT32_MEDIA_DESCRIPTOR     0xF8U
#define FAT32_SECTORS_PER_TRACK    63U
#define FAT32_HEAD_COUNT           255U
#define FAT32_EXTENDED_BOOT_SIG    0x29U
#define FAT32_DRIVE_NUMBER         0x80U
#define FAT32_FS_INFO_DISABLED     0xFFFFU
#define FAT32_BACKUP_BOOT_DISABLED 0xFFFFU
#define FAT32_RESERVED_ENTRY_ZERO  0x0FFFFFF8UL
#define FAT32_RESERVED_ENTRY_ONE   0x0FFFFFFFUL
#define FORMAT_MAX_FAT_SIZE_PASSES 8U

#define FAT32_ROOT_ENTRY_COUNT_OFFSET    17U
#define FAT32_TOTAL_SECTORS_16_OFFSET    19U
#define FAT32_MEDIA_OFFSET               21U
#define FAT32_FAT_SIZE_16_OFFSET         22U
#define FAT32_SECTORS_PER_TRACK_OFFSET   24U
#define FAT32_HEAD_COUNT_OFFSET          26U
#define FAT32_HIDDEN_SECTOR_COUNT_OFFSET 28U
#define FAT32_EXT_FLAGS_OFFSET           40U
#define FAT32_FS_VERSION_OFFSET          42U
#define FAT32_FS_INFO_OFFSET             48U
#define FAT32_BACKUP_BOOT_OFFSET         50U
#define FAT32_DRIVE_NUMBER_OFFSET        64U
#define FAT32_EXTENDED_BOOT_SIG_OFFSET   66U
#define FAT32_VOLUME_ID_OFFSET           67U
#define FAT32_VOLUME_LABEL_OFFSET        71U
#define FAT32_FS_TYPE_OFFSET             82U

// TYPES ///////////////////////////////////////////////////////////////////////

typedef struct
{
    uint32_t fat_size;
    uint32_t cluster_heap_start;
    uint32_t total_clusters;

} xFS_Format_Layout_t;

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

static uint32_t format_div_round_up(uint32_t value, uint32_t divisor);

static bool format_validate_config(const xFS_Format_Config_t *config);

static xRETURN_t format_resolve_layout(const xFS_Format_Config_t *config, xFS_Format_Layout_t *layout);

static void format_zero_sector(uint8_t *sector);

static void format_copy_label(uint8_t *sector, const xFS_Format_Config_t *config);

static void format_write_boot_sector(uint8_t *sector, const xFS_Format_Config_t *config, const xFS_Format_Layout_t *layout);

static void format_write_fat_entry(uint8_t *sector, uint32_t sector_index, uint32_t bytes_per_sector, uint32_t cluster, uint32_t value);

static xRETURN_t format_write_fat_tables(xFS_Block_Driver_t *driver,
                                         void *driver_ctx,
                                         const xFS_Format_Config_t *config,
                                         const xFS_Format_Layout_t *layout,
                                         uint8_t *sector);

static xRETURN_t format_write_root_directory(xFS_Block_Driver_t *driver,
                                             void *driver_ctx,
                                             const xFS_Format_Config_t *config,
                                             const xFS_Format_Layout_t *layout,
                                             uint8_t *sector);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

static uint32_t format_div_round_up(uint32_t value, uint32_t divisor)
{
    return (value + divisor - 1U) / divisor;
}

static bool format_validate_config(const xFS_Format_Config_t *config)
{
    if (config == NULL)
    {
        return false;
    }

    if ((config->bytes_per_sector != XFS_FORMAT_DEFAULT_SECTOR_SIZE) || (config->sector_count == 0U) ||
        (config->sectors_per_cluster == 0U) || (config->sectors_per_cluster > 0xFFU) || (config->reserved_sector_count == 0U) ||
        (config->reserved_sector_count > 0xFFFFU) || (config->fat_count == 0U) || (config->fat_count > 0xFFU) ||
        !xFS_IS_VALID_CLUSTER(config->root_dir_cluster))
    {
        return false;
    }

    return true;
}

static xRETURN_t format_resolve_layout(const xFS_Format_Config_t *config, xFS_Format_Layout_t *layout)
{
    uint32_t pass;
    uint32_t fat_size;
    uint32_t data_sectors;
    uint32_t needed_fat_size;

    if ((config == NULL) || (layout == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    fat_size = config->fat_size;

    if (fat_size == 0U)
    {
        fat_size = 1U;
    }

    for (pass = 0U; pass < FORMAT_MAX_FAT_SIZE_PASSES; pass++)
    {
        if (config->sector_count <= (config->reserved_sector_count + (config->fat_count * fat_size)))
        {
            return xRETURN_xERR_xFS_INVALID_ARGUMENT;
        }

        data_sectors = config->sector_count - config->reserved_sector_count - (config->fat_count * fat_size);
        layout->total_clusters = data_sectors / config->sectors_per_cluster;

        if (layout->total_clusters == 0U)
        {
            return xRETURN_xERR_xFS_INVALID_ARGUMENT;
        }

        needed_fat_size = format_div_round_up((layout->total_clusters + FAT32_CLUSTER_MIN) * FAT32_ENTRY_SIZE, config->bytes_per_sector);

        if ((config->fat_size != 0U) && (needed_fat_size > fat_size))
        {
            return xRETURN_xERR_xFS_INVALID_ARGUMENT;
        }

        if ((config->fat_size != 0U) || (needed_fat_size == fat_size))
        {
            layout->fat_size = fat_size;
            layout->cluster_heap_start = config->reserved_sector_count + (config->fat_count * fat_size);
            break;
        }

        fat_size = needed_fat_size;
    }

    if (pass == FORMAT_MAX_FAT_SIZE_PASSES)
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    if (config->root_dir_cluster >= (FAT32_CLUSTER_MIN + layout->total_clusters))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    return xRETURN_OK;
}

static void format_zero_sector(uint8_t *sector)
{
    uint32_t index;

    for (index = 0U; index < XFS_FORMAT_DEFAULT_SECTOR_SIZE; index++)
    {
        sector[index] = 0U;
    }
}

static void format_copy_label(uint8_t *sector, const xFS_Format_Config_t *config)
{
    static const uint8_t default_label[XFS_FORMAT_VOLUME_LABEL_LENGTH] = {'X', 'F', 'S', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    uint32_t index;
    bool has_label;

    has_label = false;

    for (index = 0U; index < XFS_FORMAT_VOLUME_LABEL_LENGTH; index++)
    {
        if (config->volume_label[index] != 0U)
        {
            has_label = true;
        }
    }

    if (has_label)
    {
        (void)memcpy(&sector[FAT32_VOLUME_LABEL_OFFSET], config->volume_label, XFS_FORMAT_VOLUME_LABEL_LENGTH);
    }
    else
    {
        (void)memcpy(&sector[FAT32_VOLUME_LABEL_OFFSET], default_label, XFS_FORMAT_VOLUME_LABEL_LENGTH);
    }
}

static void format_write_boot_sector(uint8_t *sector, const xFS_Format_Config_t *config, const xFS_Format_Layout_t *layout)
{
    static const uint8_t boot_jump[FAT32_BOOT_JUMP_SIZE] = {0xEBU, 0x58U, 0x90U};
    static const uint8_t oem_name[FAT32_OEM_NAME_SIZE] = {'X', 'E', 'S', 'D', 'K', ' ', ' ', ' '};
    static const uint8_t fs_type[FAT32_OEM_NAME_SIZE] = {'F', 'A', 'T', '3', '2', ' ', ' ', ' '};

    format_zero_sector(sector);

    (void)memcpy(&sector[0U], boot_jump, FAT32_BOOT_JUMP_SIZE);
    (void)memcpy(&sector[FAT32_OEM_NAME_OFFSET], oem_name, FAT32_OEM_NAME_SIZE);

    xWrite_LE16(&sector[FAT32_BYTES_PER_SECTOR_OFFSET], (uint16_t)config->bytes_per_sector);
    sector[FAT32_SECTORS_PER_CLUSTER_OFFSET] = (uint8_t)config->sectors_per_cluster;
    xWrite_LE16(&sector[FAT32_RESERVED_SECTOR_COUNT_OFFSET], (uint16_t)config->reserved_sector_count);
    sector[FAT32_NUMBER_OF_FATS_OFFSET] = (uint8_t)config->fat_count;
    xWrite_LE16(&sector[FAT32_ROOT_ENTRY_COUNT_OFFSET], FAT32_ROOT_ENTRY_COUNT);
    xWrite_LE16(&sector[FAT32_TOTAL_SECTORS_16_OFFSET], FAT32_TOTAL_SECTORS_16);
    sector[FAT32_MEDIA_OFFSET] = FAT32_MEDIA_DESCRIPTOR;
    xWrite_LE16(&sector[FAT32_FAT_SIZE_16_OFFSET], 0U);
    xWrite_LE16(&sector[FAT32_SECTORS_PER_TRACK_OFFSET], FAT32_SECTORS_PER_TRACK);
    xWrite_LE16(&sector[FAT32_HEAD_COUNT_OFFSET], FAT32_HEAD_COUNT);
    xWrite_LE32(&sector[FAT32_HIDDEN_SECTOR_COUNT_OFFSET], 0U);
    xWrite_LE32(&sector[FAT32_TOTAL_SECTORS_32_OFFSET], config->sector_count);
    xWrite_LE32(&sector[FAT32_SIZE_32_OFFSET], layout->fat_size);
    xWrite_LE16(&sector[FAT32_EXT_FLAGS_OFFSET], 0U);
    xWrite_LE16(&sector[FAT32_FS_VERSION_OFFSET], 0U);
    xWrite_LE32(&sector[FAT32_ROOT_CLUSTER_OFFSET], config->root_dir_cluster);
    xWrite_LE16(&sector[FAT32_FS_INFO_OFFSET], FAT32_FS_INFO_DISABLED);
    xWrite_LE16(&sector[FAT32_BACKUP_BOOT_OFFSET], FAT32_BACKUP_BOOT_DISABLED);
    sector[FAT32_DRIVE_NUMBER_OFFSET] = FAT32_DRIVE_NUMBER;
    sector[FAT32_EXTENDED_BOOT_SIG_OFFSET] = FAT32_EXTENDED_BOOT_SIG;
    xWrite_LE32(&sector[FAT32_VOLUME_ID_OFFSET], config->volume_id);
    format_copy_label(sector, config);
    (void)memcpy(&sector[FAT32_FS_TYPE_OFFSET], fs_type, FAT32_OEM_NAME_SIZE);
    xWrite_LE16(&sector[FAT32_BOOT_SIGNATURE_OFFSET], FAT32_BOOT_SIGNATURE_VALUE);
}

static void format_write_fat_entry(uint8_t *sector, uint32_t sector_index, uint32_t bytes_per_sector, uint32_t cluster, uint32_t value)
{
    uint32_t fat_offset;
    uint32_t entry_sector;
    uint32_t entry_offset;

    fat_offset = cluster * FAT32_ENTRY_SIZE;
    entry_sector = fat_offset / bytes_per_sector;

    if (entry_sector == sector_index)
    {
        entry_offset = fat_offset % bytes_per_sector;
        xWrite_LE32(&sector[entry_offset], value);
    }
}

static xRETURN_t format_write_fat_tables(xFS_Block_Driver_t *driver,
                                         void *driver_ctx,
                                         const xFS_Format_Config_t *config,
                                         const xFS_Format_Layout_t *layout,
                                         uint8_t *sector)
{
    xRETURN_t status;
    uint32_t fat_index;
    uint32_t fat_sector;
    uint32_t lba;

    for (fat_index = 0U; fat_index < config->fat_count; fat_index++)
    {
        for (fat_sector = 0U; fat_sector < layout->fat_size; fat_sector++)
        {
            format_zero_sector(sector);
            format_write_fat_entry(sector, fat_sector, config->bytes_per_sector, 0U, FAT32_RESERVED_ENTRY_ZERO);
            format_write_fat_entry(sector, fat_sector, config->bytes_per_sector, 1U, FAT32_RESERVED_ENTRY_ONE);
            format_write_fat_entry(sector, fat_sector, config->bytes_per_sector, config->root_dir_cluster, FAT32_EOC_MIN);

            lba = config->reserved_sector_count + (fat_index * layout->fat_size) + fat_sector;
            status = driver->write_sector(driver_ctx, lba, sector, 1U);

            if (status != xRETURN_OK)
            {
                return status;
            }
        }
    }

    return xRETURN_OK;
}

static xRETURN_t format_write_root_directory(xFS_Block_Driver_t *driver,
                                             void *driver_ctx,
                                             const xFS_Format_Config_t *config,
                                             const xFS_Format_Layout_t *layout,
                                             uint8_t *sector)
{
    xRETURN_t status;
    uint32_t sector_index;
    uint32_t lba;

    format_zero_sector(sector);

    for (sector_index = 0U; sector_index < config->sectors_per_cluster; sector_index++)
    {
        lba = layout->cluster_heap_start + ((config->root_dir_cluster - FAT32_CLUSTER_MIN) * config->sectors_per_cluster) + sector_index;
        status = driver->write_sector(driver_ctx, lba, sector, 1U);

        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

xRETURN_t xFS_Format_Config_Default(xFS_Format_Config_t *config, uint32_t sector_count)
{
    if (config == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    (void)memset(config, 0, sizeof(*config));

    config->bytes_per_sector = XFS_FORMAT_DEFAULT_SECTOR_SIZE;
    config->sector_count = sector_count;
    config->sectors_per_cluster = 1U;
    config->reserved_sector_count = 1U;
    config->fat_count = 1U;
    config->fat_size = 0U;
    config->root_dir_cluster = FAT32_CLUSTER_MIN;
    config->volume_id = XFS_FORMAT_DEFAULT_VOLUME_ID;

    return xRETURN_OK;
}

xRETURN_t xFS_Format_FAT32(xFS_Block_Driver_t *driver, void *driver_ctx, const xFS_Format_Config_t *config)
{
    xRETURN_t status;
    xFS_Format_Layout_t layout;
    uint8_t sector[XFS_FORMAT_DEFAULT_SECTOR_SIZE];

    if ((driver == NULL) || (driver->init == NULL) || (driver->write_sector == NULL) || (driver->flush == NULL))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    if (!format_validate_config(config))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    status = format_resolve_layout(config, &layout);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = driver->init(driver_ctx);

    if (status != xRETURN_OK)
    {
        return status;
    }

    format_write_boot_sector(sector, config, &layout);
    status = driver->write_sector(driver_ctx, 0U, sector, 1U);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = format_write_fat_tables(driver, driver_ctx, config, &layout, sector);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = format_write_root_directory(driver, driver_ctx, config, &layout, sector);

    if (status != xRETURN_OK)
    {
        return status;
    }

    return driver->flush(driver_ctx);
}
// EOF /////////////////////////////////////////////////////////////////////////////
