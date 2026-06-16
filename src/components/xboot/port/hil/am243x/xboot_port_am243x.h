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

// @file xboot_port_am243x.h
// @brief AM243x port interface and initialization function declarations.
//

#ifndef XBOOT_PORT_AM243X_H
#define XBOOT_PORT_AM243X_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_handoff.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    /**
     * @brief Retrieve the port operations implementation table for AM243x.
     * @return const xBOOT_Port_Ops_t* Pointer to operations structure
     */
    const xBOOT_Port_Ops_t *xBOOT_Port_AM243x_Get_Ops(void);

    /**
     * @brief Initialize AM243x SoC (clocks, sciclient, UART console).
     * @return xRETURN_t xRETURN_xBOOT_OK on success, error code otherwise
     */
    xRETURN_t xBOOT_Port_AM243x_Init(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_PORT_AM243X_H
// EOF /////////////////////////////////////////////////////////////////////////////
