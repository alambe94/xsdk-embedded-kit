// Copyright 2022 alambe94
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

// @file xsdk_port_am243x.c
// @brief AM243x SoC port integration and initialization.
//
// Implements early initialization hooks (such as MPU and cache setup)
// called by the startup assembly code.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xrtos_port_am243x.h"
#include "xsdk_soc_cache.h"
#include "xsdk_soc_mpu.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xRTOS_Port_AM243x_Early_Init(void)
{
    // Initialize MPU regions.
    xsdk_soc_mpu_init();

    // Enable Instruction and Data caches.
    xsdk_soc_cache_enable();
}

// EOF /////////////////////////////////////////////////////////////////////////////
