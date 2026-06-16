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

// @file xboot_core.h
// @brief Core bootloader context, configurations, decision enums, and APIs.
//

#ifndef XBOOT_CORE_H
#define XBOOT_CORE_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_defs.h"
#include "xboot_config.h"
#include "xboot_return.h"

    // Forward declarations of future phase structures
    struct xBOOT_Storage_Ops_t;
    struct xBOOT_Port_Ops_t;

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef enum
    {
        xBOOT_DECISION_BOOT_PRIMARY = 0,
        xBOOT_DECISION_BOOT_SECONDARY,
        xBOOT_DECISION_ENTER_RECOVERY,
        xBOOT_DECISION_STAY_IN_BOOTLOADER,
        xBOOT_DECISION_NO_BOOTABLE_IMAGE
    } xBOOT_Decision_t;

    typedef enum
    {
        xBOOT_REASON_NORMAL = 0,
        xBOOT_REASON_UPDATE,
        xBOOT_REASON_ROLLBACK,
        xBOOT_REASON_RECOVERY
    } xBOOT_Reason_t;

    typedef struct
    {
        const struct xBOOT_Storage_Ops_t *storage_ops;
        void *storage_ctx;

        const struct xBOOT_Port_Ops_t *port_ops;
        void *port_ctx;

        bool force_recovery;
    } xBOOT_Config_t;

    typedef struct
    {
        const struct xBOOT_Storage_Ops_t *storage_ops;
        void *storage_ctx;

        const struct xBOOT_Port_Ops_t *port_ops;
        void *port_ctx;

        bool is_initialized;
        bool is_recovery_requested;
    } xBOOT_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xBOOT_Init(xBOOT_Context_t *boot_ctx, const xBOOT_Config_t *boot_config);
    xRETURN_t xBOOT_Run(xBOOT_Context_t *boot_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_CORE_H
// EOF /////////////////////////////////////////////////////////////////////////////
