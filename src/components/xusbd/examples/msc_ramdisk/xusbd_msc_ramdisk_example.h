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

// @file xusbd_msc_ramdisk_example.h
// @brief Application-level hooks and configuration for USB MSC (mass storage).

#ifndef XUSBD_MSC_RAMDISK_EXAMPLE_H
#define XUSBD_MSC_RAMDISK_EXAMPLE_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_msc.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////
    void xUSBD_MSC_App_Init(xUSBD_Class_Context_t *class_ctx);
    void xUSBD_MSC_App_Process(xUSBD_Class_Context_t *class_ctx);

#ifdef __cplusplus
}
#endif
#endif // XUSBD_MSC_RAMDISK_EXAMPLE_H
// EOF /////////////////////////////////////////////////////////////////////////////
