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

// @file xfs_path.c
// @brief FAT32 SFN path parsing and directory walking.

// INCLUDES ///////////////////////////////////////////////////////////////////

#include <string.h>

#include "xfs_path.h"
#include "xfs_defs.h"
#include "xfs_trace.h"
#include "xfs_config.h"

#include "xfs_log.h"

// MACROS //////////////////////////////////////////////////////////////////////

#define XFS_SFN_NAME_LENGTH 8U
#define XFS_SFN_EXT_LENGTH  3U

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

static char path_ascii_to_upper(char value)
{
    if ((value >= 'a') && (value <= 'z'))
    {
        return (char)(value - ('a' - 'A'));
    }

    return value;
}

static bool path_is_valid_sfn_char(char value)
{
    if (((value >= 'A') && (value <= 'Z')) || ((value >= '0') && (value <= '9')))
    {
        return true;
    }

    switch (value)
    {
    case '$':
    case '%':
    case '\'':
    case '-':
    case '_':
    case '@':
    case '~':
    case '`':
    case '!':
    case '(':
    case ')':
    case '{':
    case '}':
    case '^':
    case '#':
    case '&':
        return true;

    default:
        return false;
    }
}

static bool path_has_more_components(const char *path)
{
    const char *cursor;

    if (path == NULL)
    {
        return false;
    }

    cursor = path;

    while (path_is_separator(*cursor))
    {
        cursor++;
    }

    return (*cursor != '\0');
}

static bool path_is_root(const char *path)
{
    const char *cursor;

    if ((path == NULL) || (*path == '\0'))
    {
        return false;
    }

    cursor = path;

    while (path_is_separator(*cursor))
    {
        cursor++;
    }

    return (*cursor == '\0');
}

static xRETURN_t
path_extract_component(const char **cursor_ptr, uint8_t sfn[FAT32_DIRECTORY_NAME_LENGTH], bool *is_last, bool *had_trailing_sep)
{
    xRETURN_t status;
    const char *start;
    const char *cursor;
    uint32_t length;
    bool consumed_sep;

    cursor = *cursor_ptr;
    start = cursor;

    while ((*cursor != '\0') && !path_is_separator(*cursor))
    {
        cursor++;
    }

    length = (uint32_t)(cursor - start);

    status = xFS_Path_To_SFN(start, length, sfn);

    if (status != xRETURN_OK)
    {
        return status;
    }

    consumed_sep = false;

    while (path_is_separator(*cursor))
    {
        consumed_sep = true;
        cursor++;
    }

    *cursor_ptr = cursor;
    *is_last = (*cursor == '\0');

    if (had_trailing_sep != NULL)
    {
        *had_trailing_sep = consumed_sep;
    }

    return xRETURN_OK;
}

static xRETURN_t path_enter_directory(const xFS_FAT32_Directory_Entry_t *entry, uint32_t *cluster)
{
    uint32_t next_cluster;

    if ((entry->attributes & FAT32_ATTR_DIRECTORY) == 0U)
    {
        return xRETURN_xERR_xFS_NOT_DIRECTORY;
    }

    next_cluster = fat32_entry_first_cluster(entry);

    if (!xFS_IS_VALID_CLUSTER(next_cluster))
    {
        xFS_LOG(xRETURN_xERR_xFS_CORRUPT, "directory entry has invalid first cluster");
        return xRETURN_xERR_xFS_CORRUPT;
    }

    *cluster = next_cluster;

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

xRETURN_t xFS_Path_To_SFN(const char *path_component, uint32_t component_length, uint8_t sfn[FAT32_DIRECTORY_NAME_LENGTH])
{
    uint32_t index;
    uint32_t name_index;
    uint32_t ext_index;
    bool in_extension;
    char value;

    if ((path_component == NULL) || (sfn == NULL) || (component_length == 0U))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    for (index = 0U; index < FAT32_DIRECTORY_NAME_LENGTH; index++)
    {
        sfn[index] = (uint8_t)' ';
    }

    name_index = 0U;
    ext_index = 0U;
    in_extension = false;

    for (index = 0U; index < component_length; index++)
    {
        value = path_ascii_to_upper(path_component[index]);

        if (value == '.')
        {
            if ((in_extension == true) || (name_index == 0U))
            {
                return xRETURN_xERR_xFS_INVALID_ARGUMENT;
            }

            in_extension = true;
            continue;
        }

        if (!path_is_valid_sfn_char(value))
        {
            return xRETURN_xERR_xFS_INVALID_ARGUMENT;
        }

        if (in_extension == false)
        {
            if (name_index >= XFS_SFN_NAME_LENGTH)
            {
                return xRETURN_xERR_xFS_INVALID_ARGUMENT;
            }

            sfn[name_index] = (uint8_t)value;
            name_index++;
        }
        else
        {
            if (ext_index >= XFS_SFN_EXT_LENGTH)
            {
                return xRETURN_xERR_xFS_INVALID_ARGUMENT;
            }

            sfn[XFS_SFN_NAME_LENGTH + ext_index] = (uint8_t)value;
            ext_index++;
        }
    }

    if (name_index == 0U)
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    return xRETURN_OK;
}

xRETURN_t xFS_Path_Walk(xFS_Context_t *fs_ctx,
                        xFS_Cache_Entry_t *cache,
                        const char *path,
                        xFS_FAT32_Directory_Entry_t *entry,
                        uint32_t *entry_cluster,
                        uint32_t *entry_index)
{
    xRETURN_t status;
    const char *cursor;
    uint32_t current_dir_cluster;
    uint32_t found_cluster;
    uint32_t found_index;
    bool is_last;
    uint8_t sfn[FAT32_DIRECTORY_NAME_LENGTH];
    xFS_FAT32_Directory_Entry_t found_entry;

    if ((fs_ctx == NULL) || (cache == NULL) || (path == NULL) || (entry == NULL))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    if (!path_has_more_components(path))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    cursor = path;
    current_dir_cluster = fs_ctx->root_dir_cluster;

    while (path_is_separator(*cursor))
    {
        cursor++;
    }

    while (*cursor != '\0')
    {
        status = path_extract_component(&cursor, sfn, &is_last, NULL);

        if (status != xRETURN_OK)
        {
            return status;
        }

        status = xFS_FAT32_Directory_Find_Entry(fs_ctx, cache, current_dir_cluster, sfn, &found_entry, &found_cluster, &found_index);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (is_last)
        {
            *entry = found_entry;

            if (entry_cluster != NULL)
            {
                *entry_cluster = found_cluster;
            }

            if (entry_index != NULL)
            {
                *entry_index = found_index;
            }

            return xRETURN_OK;
        }

        status = path_enter_directory(&found_entry, &current_dir_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    return xRETURN_xERR_xFS_NOT_FOUND; // unreachable - loop always returns before exiting
}

xRETURN_t xFS_Path_Resolve_Parent(xFS_Context_t *fs_ctx,
                                  xFS_Cache_Entry_t *cache,
                                  const char *path,
                                  uint32_t *parent_cluster,
                                  uint8_t child_name[FAT32_DIRECTORY_NAME_LENGTH])
{
    xRETURN_t status;
    const char *cursor;
    uint32_t current_dir_cluster;
    bool is_last;
    bool had_trailing_sep;
    uint8_t sfn[FAT32_DIRECTORY_NAME_LENGTH];
    xFS_FAT32_Directory_Entry_t found_entry;

    if ((fs_ctx == NULL) || (cache == NULL) || (path == NULL) || (parent_cluster == NULL) || (child_name == NULL))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    if (!path_has_more_components(path))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    cursor = path;
    current_dir_cluster = fs_ctx->root_dir_cluster;

    while (path_is_separator(*cursor))
    {
        cursor++;
    }

    while (*cursor != '\0')
    {
        status = path_extract_component(&cursor, sfn, &is_last, &had_trailing_sep);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (is_last)
        {
            if (had_trailing_sep == true)
            {
                return xRETURN_xERR_xFS_INVALID_ARGUMENT;
            }

            (void)memcpy(child_name, sfn, FAT32_DIRECTORY_NAME_LENGTH);
            *parent_cluster = current_dir_cluster;
            return xRETURN_OK;
        }

        status = xFS_FAT32_Directory_Find_Entry(fs_ctx, cache, current_dir_cluster, sfn, &found_entry, NULL, NULL);

        if (status != xRETURN_OK)
        {
            return status;
        }

        status = path_enter_directory(&found_entry, &current_dir_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    return xRETURN_xERR_xFS_NOT_FOUND; // unreachable - loop always returns before exiting
}

xRETURN_t xFS_Path_Exists(xFS_Context_t *fs_ctx, const char *path, bool *exists)
{
    xRETURN_t status;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;

    if ((fs_ctx == NULL) || (path == NULL) || (exists == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    if (path_is_root(path))
    {
        *exists = true;
        return xRETURN_OK;
    }

    status = xFS_Cache_Init(&cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_Path_Walk(fs_ctx, &cache, path, &entry, NULL, NULL);

    (void)xFS_Cache_Invalidate(&cache);

    if (status == xRETURN_xERR_xFS_NOT_FOUND)
    {
        *exists = false;
        return xRETURN_OK;
    }

    if (status == xRETURN_OK)
    {
        *exists = true;
    }

    return status;
}

xRETURN_t xFS_Path_Stat(xFS_Context_t *fs_ctx, const char *path, xFS_Stat_t *stat)
{
    xRETURN_t status;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;

    if ((fs_ctx == NULL) || (path == NULL) || (stat == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    if (path_is_root(path))
    {
        stat->size = 0U;
        stat->attributes = FAT32_ATTR_DIRECTORY;
        stat->first_cluster = fs_ctx->root_dir_cluster;
        stat->is_directory = true;
        return xRETURN_OK;
    }

    status = xFS_Cache_Init(&cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_Path_Walk(fs_ctx, &cache, path, &entry, NULL, NULL);

    (void)xFS_Cache_Invalidate(&cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    stat->size = xRead_LE32((const uint8_t *)&entry.file_size);
    stat->attributes = entry.attributes;
    stat->first_cluster = fat32_entry_first_cluster(&entry);
    stat->is_directory = ((entry.attributes & FAT32_ATTR_DIRECTORY) != 0U);

    return xRETURN_OK;
}

static xRETURN_t path_rename_cross_directory(xFS_Context_t *fs_ctx,
                                             xFS_Cache_Entry_t *cache,
                                             xFS_FAT32_Directory_Entry_t *old_entry,
                                             uint32_t old_cluster,
                                             uint32_t old_index,
                                             uint32_t new_parent,
                                             const uint8_t new_sfn[FAT32_DIRECTORY_NAME_LENGTH])
{
    xRETURN_t status;
    xFS_FAT32_Directory_Entry_t new_entry;
    uint32_t free_cluster;
    uint32_t free_index;

    if ((old_entry->attributes & FAT32_ATTR_DIRECTORY) != 0U)
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    status = xFS_FAT32_Directory_Find_Free_Entry(fs_ctx, cache, new_parent, &free_cluster, &free_index);

    if (status != xRETURN_OK)
    {
        return status;
    }

    new_entry = *old_entry;
    (void)memcpy(new_entry.name, new_sfn, FAT32_DIRECTORY_NAME_LENGTH);

    status = xFS_FAT32_Directory_Write_Entry(fs_ctx, cache, free_cluster, free_index, &new_entry);

    if (status != xRETURN_OK)
    {
        return status;
    }

    old_entry->name[0U] = FAT32_DIRECTORY_ENTRY_FREE;

    return xFS_FAT32_Directory_Write_Entry(fs_ctx, cache, old_cluster, old_index, old_entry);
}

xRETURN_t xFS_Path_Rename(xFS_Context_t *fs_ctx, const char *old_path, const char *new_path)
{
    xRETURN_t status;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    xFS_FAT32_Directory_Entry_t check_entry;
    uint32_t old_parent;
    uint32_t new_parent;
    uint32_t entry_cluster;
    uint32_t entry_index;
    uint8_t old_sfn[FAT32_DIRECTORY_NAME_LENGTH];
    uint8_t new_sfn[FAT32_DIRECTORY_NAME_LENGTH];

    if ((fs_ctx == NULL) || (old_path == NULL) || (new_path == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    status = xFS_Cache_Init(&cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_Path_Resolve_Parent(fs_ctx, &cache, old_path, &old_parent, old_sfn);

    if (status != xRETURN_OK)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    status = xFS_Path_Resolve_Parent(fs_ctx, &cache, new_path, &new_parent, new_sfn);

    if (status != xRETURN_OK)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    if ((old_parent == new_parent) && (memcmp(old_sfn, new_sfn, FAT32_DIRECTORY_NAME_LENGTH) == 0))
    {
        (void)xFS_Cache_Invalidate(&cache);
        return xRETURN_OK;
    }

    status = xFS_FAT32_Directory_Find_Entry(fs_ctx, &cache, old_parent, old_sfn, &entry, &entry_cluster, &entry_index);

    if (status != xRETURN_OK)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    status = xFS_FAT32_Directory_Find_Entry(fs_ctx, &cache, new_parent, new_sfn, &check_entry, NULL, NULL);

    if (status == xRETURN_OK)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return xRETURN_xERR_xFS_ALREADY_EXISTS;
    }

    if (status != xRETURN_xERR_xFS_NOT_FOUND)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    if (old_parent == new_parent)
    {
        (void)memcpy(entry.name, new_sfn, FAT32_DIRECTORY_NAME_LENGTH);
        status = xFS_FAT32_Directory_Write_Entry(fs_ctx, &cache, entry_cluster, entry_index, &entry);
    }
    else
    {
        status = path_rename_cross_directory(fs_ctx, &cache, &entry, entry_cluster, entry_index, new_parent, new_sfn);
    }

    if (status == xRETURN_OK)
    {
        xFS_TRACE_E1(fs_ctx, xFS_TRACE_CODE_FILE_RENAME, new_parent);
    }

    (void)xFS_Cache_Invalidate(&cache);
    return status;
}
// EOF /////////////////////////////////////////////////////////////////////////////
