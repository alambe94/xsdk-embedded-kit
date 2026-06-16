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

// @file xrtos_port_fake.h
// @brief xRTOS host (PC) port - stub operations for unit testing.
//
// Provides a concrete xRTOS_Port_Ops_t instance, xrtos_fake_port_ops, that
// compiles and runs on a host PC. No real context switching or interrupt
// masking occurs. This port exists solely so the portable kernel logic can
// be exercised by CTest without an ARM target.
//
// Do NOT use this port in production firmware.
//

#ifndef XRTOS_PORT_FAKE_H
#define XRTOS_PORT_FAKE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xrtos_port.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // Concrete port ops instance. Pass a pointer to this in xRTOS_Kernel_Init.
    extern const xRTOS_Port_Ops_t xrtos_fake_port_ops;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_PORT_FAKE_H
// EOF /////////////////////////////////////////////////////////////////////////////
