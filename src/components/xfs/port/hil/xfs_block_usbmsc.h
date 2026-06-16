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

// @file xfs_block_usbmsc.h
// @brief USB MSC-backed xFS block-device adapter.

#ifndef XFS_BLOCK_USBMSC_H
#define XFS_BLOCK_USBMSC_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ///////////////////////////////////////////////////////////////////

#include <stdbool.h>
#include <stdint.h>

#include "xfs_block_device.h"
#include "xusbh_msc.h"

    // MACROS //////////////////////////////////////////////////////////////////////

#define xFS_USBMSC_DEFAULT_MAX_POLL_COUNT 1024U

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef xRETURN_t (*xFS_USBMSC_Poll_Callback_t)(void *poll_ctx);

    typedef struct xFS_USBMSC_Context_t
    {
        xUSBH_MSC_Context_t *msc_ctx;
        xFS_USBMSC_Poll_Callback_t poll;
        void *poll_ctx;
        uint32_t max_poll_count;
        uint32_t sector_count;
        uint32_t sector_size;
        uint8_t lun;
        bool is_initialized;
        bool is_media_present;
    } xFS_USBMSC_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    extern xFS_Block_Driver_t gxFS_USBMSC_Driver;

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xFS_USBMSC_Is_Ready(const xFS_USBMSC_Context_t *usbmsc_ctx, bool *is_ready);
    xRETURN_t xFS_USBMSC_Get_Capacity(const xFS_USBMSC_Context_t *usbmsc_ctx, xUSBH_MSC_Capacity_t *capacity);

#ifdef __cplusplus
}
#endif

#endif // XFS_BLOCK_USBMSC_H
// EOF /////////////////////////////////////////////////////////////////////////////
