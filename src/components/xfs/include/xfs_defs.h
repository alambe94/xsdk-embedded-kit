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

// @file xfs_defs.h
// @brief Shared FAT32 constants and endian helpers for xFS.

#ifndef XFS_DEFS_H
#define XFS_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ///////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "xfs_return.h"
#include "xbytes.h"

    // MACROS //////////////////////////////////////////////////////////////////////

#define xFS_VERSION_MAJOR  0U
#define xFS_VERSION_MINOR  4U
#define xFS_VERSION_PATCH  0U
#define xFS_VERSION_STRING "0.4.0"

#define XFS_SECTOR_SIZE 512U

#define FAT32_EOC_MIN      0x0FFFFFF8UL
#define FAT32_BAD_CLUSTER  0x0FFFFFF7UL
#define FAT32_FREE_CLUSTER 0x00000000UL

#define FAT32_ATTR_READ_ONLY 0x01U
#define FAT32_ATTR_HIDDEN    0x02U
#define FAT32_ATTR_SYSTEM    0x04U
#define FAT32_ATTR_VOLUME_ID 0x08U
#define FAT32_ATTR_DIRECTORY 0x10U
#define FAT32_ATTR_ARCHIVE   0x20U
#define FAT32_ATTR_LFN       0x0FU

#define xFS_IS_VALID_CLUSTER(cluster) ((cluster) >= 2U)

#define xFS_IS_EOC(cluster) ((cluster) >= FAT32_EOC_MIN)

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // XFS_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
