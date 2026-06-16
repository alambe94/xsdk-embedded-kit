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

// @file xfs_return.h
// @brief xFS module return codes.
//

#ifndef XFS_RETURN_H
#define XFS_RETURN_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xreturn.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef enum
    {
        // Success.
        xRETURN_xFS_OK = 0,

        // A required pointer argument was NULL.
        xRETURN_xERR_xFS_NULL_POINTER = xRETURN_MAKE(xRETURN_xFS_MODULE, xRETURN_SEVERITY_ERROR, 0x001),

        // An argument value was syntactically invalid for the requested operation.
        xRETURN_xERR_xFS_INVALID_ARGUMENT,

        // The filesystem context is not mounted.
        xRETURN_xERR_xFS_NOT_MOUNTED,

        // The block driver reported an input/output failure.
        xRETURN_xERR_xFS_IO,

        // FAT32 boot-sector or volume geometry validation failed.
        xRETURN_xERR_xFS_INVALID_VOLUME,

        // A requested path, directory entry, or cluster was not found.
        xRETURN_xERR_xFS_NOT_FOUND,

        // A requested path resolves to a file where a directory was required.
        xRETURN_xERR_xFS_NOT_DIRECTORY,

        // A requested path resolves to a directory where a file was required.
        xRETURN_xERR_xFS_NOT_FILE,

        // No free cluster is available on the volume.
        xRETURN_xERR_xFS_DISK_FULL,

        // The directory cannot accept another entry.
        xRETURN_xERR_xFS_DIR_FULL,

        // On-disk metadata or a FAT cluster chain is internally inconsistent.
        xRETURN_xERR_xFS_CORRUPT,

        // A create operation targets an entry that already exists.
        xRETURN_xERR_xFS_ALREADY_EXISTS,

        // A sector, cluster, entry index, or file offset is outside the valid range.
        xRETURN_xERR_xFS_OUT_OF_RANGE,

        // Runtime state is inconsistent with the requested operation.
        xRETURN_xERR_xFS_INVALID_STATE,

        // A delete operation targets a non-empty directory.
        xRETURN_xERR_xFS_NOT_EMPTY,
    } xRETURN_xFS_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XFS_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
