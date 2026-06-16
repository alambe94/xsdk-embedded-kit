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

// @file xfs_trace.h
// @brief xFS trace event IDs (flat integers, LEB128 wire encoding).
//
// All xFS events carry exactly one uint32 parameter (arg).  Call sites use
// xFS_TRACE_E1(fs_ctx, code, arg) which expands to xTRACE_E1 and is a no-op
// when xTRACE_ENABLE or xFS_TRACE_ENABLE is 0.
//

#ifndef XFS_TRACE_H
#define XFS_TRACE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ///////////////////////////////////////////////////////////////////
#include "xtrace_config.h"
#include "xtrace_registry.h"

#ifndef xFS_TRACE_ENABLE
#define xFS_TRACE_ENABLE xTRACE_ENABLE
#endif

#if xTRACE_ENABLE && xFS_TRACE_ENABLE
#include "xtrace.h"
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

#define xFS_TRACE_CODE_MOUNT                                                                                                               \
    (xTRACE_BASE_xFS + 0x00U) /// @trace {"name": "FS_MOUNT", "type": "instant", "track": "xFS/Core", "args": ["root_dir_cluster"]}
#define xFS_TRACE_CODE_UNMOUNT                                                                                                             \
    (xTRACE_BASE_xFS + 0x01U) /// @trace {"name": "FS_UNMOUNT", "type": "instant", "track": "xFS/Core", "args": ["unused"]}
#define xFS_TRACE_CODE_FILE_OPEN   (xTRACE_BASE_xFS + 0x02U) /// @trace {"type": "begin", "track": "xFS/File", "args": ["start_cluster"]}
#define xFS_TRACE_CODE_FILE_CREATE (xTRACE_BASE_xFS + 0x03U) /// @trace {"type": "begin", "track": "xFS/File", "args": ["entry_cluster"]}
#define xFS_TRACE_CODE_FILE_CLOSE  (xTRACE_BASE_xFS + 0x04U) /// @trace {"type": "end", "track": "xFS/File", "args": ["start_cluster"]}
#define xFS_TRACE_CODE_FILE_DELETE (xTRACE_BASE_xFS + 0x05U) /// @trace {"type": "instant", "track": "xFS/File", "args": ["first_cluster"]}
#define xFS_TRACE_CODE_FILE_SEEK   (xTRACE_BASE_xFS + 0x06U) /// @trace {"type": "instant", "track": "xFS/File", "args": ["position"]}
#define xFS_TRACE_CODE_DIR_OPEN    (xTRACE_BASE_xFS + 0x07U) /// @trace {"type": "begin", "track": "xFS/Dir", "args": ["start_cluster"]}
#define xFS_TRACE_CODE_DIR_CLOSE   (xTRACE_BASE_xFS + 0x08U) /// @trace {"type": "end", "track": "xFS/Dir", "args": ["current_cluster"]}
#define xFS_TRACE_CODE_DIR_CREATE  (xTRACE_BASE_xFS + 0x09U) /// @trace {"type": "instant", "track": "xFS/Dir", "args": ["dir_cluster"]}
#define xFS_TRACE_CODE_DIR_DELETE  (xTRACE_BASE_xFS + 0x0AU) /// @trace {"type": "instant", "track": "xFS/Dir", "args": ["unused"]}
#define xFS_TRACE_CODE_FILE_RENAME                                                                                                         \
    (xTRACE_BASE_xFS + 0x0BU) /// @trace {"type": "instant", "track": "xFS/File", "args": ["new_parent_cluster"]}
#define xFS_TRACE_CODE_SYNC                                                                                                                \
    (xTRACE_BASE_xFS + 0x0CU) /// @trace {"name": "FS_SYNC", "type": "instant", "track": "xFS/Core", "args": ["unused"]}
#define xFS_TRACE_CODE_FILE_WRITE (xTRACE_BASE_xFS + 0x0DU) /// @trace {"type": "counter", "track": "xFS/IO", "args": ["bytes_written"]}
#define xFS_TRACE_CODE_FILE_READ  (xTRACE_BASE_xFS + 0x0EU) /// @trace {"type": "counter", "track": "xFS/IO", "args": ["bytes_read"]}

// Emit a trace event through the volume's trace handle.
// fs_ctx must be a non-NULL xFS_Context_t *; trace_ctx within it may be NULL
// (xTRACE_Emit1 guards against both NULL and uninitialised contexts).
#if xTRACE_ENABLE && xFS_TRACE_ENABLE
#define xFS_TRACE_E1(fs_ctx, code, arg) xTRACE_E1((fs_ctx)->trace_ctx, (code), (uint32_t)(arg))
#else
#define xFS_TRACE_E1(fs_ctx, code, arg)                                                                                                    \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(fs_ctx);                                                                                                                    \
        (void)(code);                                                                                                                      \
        (void)(arg);                                                                                                                       \
    } while (0)
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // XFS_TRACE_H
// EOF /////////////////////////////////////////////////////////////////////////////
