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

// @file xusbh_class.h
// @brief USB host class-driver registration and binding API.

#ifndef XUSBH_CLASS_H
#define XUSBH_CLASS_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbh_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    struct xUSBH_Class_Driver_t
    {
        xRETURN_t (*match)(const xUSBH_Interface_Context_t *interface_ctx, bool *is_match);
        xRETURN_t (*start)(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx);
        xRETURN_t (*stop)(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx);
        xRETURN_t (*transfer_complete)(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx, const xUSBH_Transfer_t *transfer);
    };

    typedef struct xUSBH_Class_Register_Config_t
    {
        const xUSBH_Class_Driver_t *driver;
        void *class_ctx;
    } xUSBH_Class_Register_Config_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xRETURN_t xUSBH_Register_Class(xUSBH_Context_t *host_ctx, const xUSBH_Class_Register_Config_t *config);
    xRETURN_t xUSBH_Class_Bind_Device(xUSBH_Context_t *host_ctx, xUSBH_Device_Context_t *device_ctx);
    xRETURN_t xUSBH_Class_Unbind_Device(xUSBH_Context_t *host_ctx, xUSBH_Device_Context_t *device_ctx);
    xRETURN_t xUSBH_Class_Transfer_Complete(xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer);

#ifdef __cplusplus
}
#endif

#endif // XUSBH_CLASS_H
// EOF /////////////////////////////////////////////////////////////////////////////
