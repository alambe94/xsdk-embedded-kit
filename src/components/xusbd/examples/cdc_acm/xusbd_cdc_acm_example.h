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

// @file xusbd_cdc_acm_example.h
// @brief Application-level hooks and configuration for USB CDC.

#ifndef XUSBD_CDC_ACM_EXAMPLE_H
#define XUSBD_CDC_ACM_EXAMPLE_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_cdc.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum
    {
        xUSBD_CDC_APP_STATE_INIT,
        xUSBD_CDC_APP_STATE_WAIT_LINE_CODING,
        xUSBD_CDC_APP_STATE_SEND_FIRST_MESSAGE,
        xUSBD_CDC_APP_STATE_ECHO,
    } xUSBD_CDC_App_State_t;

    typedef struct
    {
        uint8_t reset_complete;
        uint8_t line_coding_received;
        uint8_t tx_complete;
        uint8_t rx_complete;
        uint32_t rx_length;
        xUSBD_CDC_App_State_t state;
        USB_CDC_Line_Code_t line_coding;
    } xUSBD_CDC_App_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////
    void xUSBD_CDC_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_CDC_App_Context_t *app_info);
    void xUSBD_CDC_App_Process(xUSBD_Class_Context_t *class_ctx);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_CDC_ACM_EXAMPLE_H
// EOF /////////////////////////////////////////////////////////////////////////////
