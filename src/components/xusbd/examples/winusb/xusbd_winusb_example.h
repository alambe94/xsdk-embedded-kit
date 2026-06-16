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

// @file xusbd_winusb_example.h
// @brief Application-level hooks and configuration for WinUSB.

#ifndef XUSBD_WINUSB_EXAMPLE_H
#define XUSBD_WINUSB_EXAMPLE_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_win.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum
    {
        xUSBD_WIN_APP_STATE_IDLE,
        xUSBD_WIN_APP_STATE_ECHO
    } xUSBD_WIN_App_State_t;

    typedef struct
    {
        volatile uint8_t reset_complete;
        volatile uint8_t rx_complete;
        volatile uint8_t tx_complete;
        volatile uint32_t rx_length;
        xUSBD_WIN_App_State_t state;
    } xUSBD_WIN_App_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////
    void xUSBD_WIN_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_WIN_App_Context_t *app_context);
    void xUSBD_WIN_App_Process(xUSBD_Class_Context_t *class_ctx);

#ifdef __cplusplus
}
#endif
#endif // XUSBD_WINUSB_EXAMPLE_H
// EOF /////////////////////////////////////////////////////////////////////////////
