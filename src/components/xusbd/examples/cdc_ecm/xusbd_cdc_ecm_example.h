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

// @file xusbd_cdc_ecm_example.h
// @brief Application-level hooks and configuration for USB CDC ECM.

#ifndef XUSBD_CDC_ECM_EXAMPLE_H
#define XUSBD_CDC_ECM_EXAMPLE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xusbd_cdc.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum
    {
        xUSBD_CDC_ECM_STATE_INIT,
        xUSBD_CDC_ECM_STATE_READY,
        xUSBD_CDC_ECM_STATE_TRANSMITTING
    } xUSBD_CDC_ECM_State_t;

    typedef struct
    {
        uint8_t reset_complete;
        uint8_t tx_complete;
        uint8_t rx_complete;
        uint32_t rx_length;
        xUSBD_CDC_ECM_State_t state;
        uint8_t rx_buffer[2048];
    } xUSBD_CDC_ECM_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    void xUSBD_CDC_ECM_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_CDC_ECM_Context_t *app_context);
    void xUSBD_CDC_ECM_Process(xUSBD_Class_Context_t *class_ctx);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_CDC_ECM_EXAMPLE_H
// EOF /////////////////////////////////////////////////////////////////////////////
