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

// @file xboot_port_fake.h
// @brief Fake SoC port declarations for host-based testing.
//

#ifndef XBOOT_PORT_FAKE_H
#define XBOOT_PORT_FAKE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
    #include <stdbool.h>
    #include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
    #include "xboot_handoff.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct
    {
        bool is_prepare_called;
        bool is_jump_called;
        bool is_reset_called;
        uint32_t recorded_entry_address;
        xRETURN_t prepare_fail_code;
    } xBOOT_Port_Fake_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    /**
     * @brief Retrieve the fake port operations table.
     * @return const xBOOT_Port_Ops_t* Operations table pointer
     */
    const xBOOT_Port_Ops_t *xBOOT_Port_Fake_Get_Ops(void);

    /**
     * @brief Reset the state of the fake port context.
     * @param ctx Fake port context
     */
    void xBOOT_Port_Fake_Reset_Context(xBOOT_Port_Fake_Context_t *ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_PORT_FAKE_H
// EOF /////////////////////////////////////////////////////////////////////////////
