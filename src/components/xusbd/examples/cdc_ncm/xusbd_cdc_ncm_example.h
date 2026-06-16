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

// @file xusbd_cdc_ncm_example.h
// @brief Application-level hooks and configuration for USB CDC NCM.

#ifndef XUSBD_CDC_NCM_EXAMPLE_H
#define XUSBD_CDC_NCM_EXAMPLE_H

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
        xUSBD_CDC_NCM_STATE_INIT,
        xUSBD_CDC_NCM_STATE_READY,
        xUSBD_CDC_NCM_STATE_TRANSMITTING
    } xUSBD_CDC_NCM_State_t;

    typedef struct
    {
        uint8_t reset_complete;
        uint8_t tx_complete;
        uint8_t rx_complete;
        uint32_t rx_length;
        xUSBD_CDC_NCM_State_t state;
        uint32_t tx_length;
        uint16_t sequence;
        uint8_t rx_data[2048];
        uint8_t tx_data[2048];
    } xUSBD_CDC_NCM_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    void xUSBD_CDC_NCM_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_CDC_NCM_Context_t *app_context);
    void xUSBD_CDC_NCM_Process(xUSBD_Class_Context_t *class_ctx);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_CDC_NCM_EXAMPLE_H
// EOF /////////////////////////////////////////////////////////////////////////////
