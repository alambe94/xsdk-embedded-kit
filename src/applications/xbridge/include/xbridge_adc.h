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

// @file xbridge_adc.h
// @brief xBRIDGE ADC channel - WINUSB binary frame ADC single/multi-channel sampling bridge.
//

#ifndef XBRIDGE_ADC_H
#define XBRIDGE_ADC_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xbridge_core.h"
#include "xbridge_defs.h"
#include "xbridge_return.h"
#include "xreturn.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // ADC command opcodes (WINUSB binary frame cmd field)
#define xBRIDGE_ADC_CMD_READ_SINGLE     0x01U // one-shot read of a single channel
#define xBRIDGE_ADC_CMD_READ_MULTI      0x02U // one-shot read of multiple channels (bitmask)
#define xBRIDGE_ADC_CMD_SET_RESOLUTION  0x03U // set ADC resolution in bits (8, 10, 12, 16)
#define xBRIDGE_ADC_CMD_SET_REFERENCE   0x04U // set reference voltage source
#define xBRIDGE_ADC_CMD_SET_SAMPLE_RATE 0x05U // set sample rate in samples/second

    // ADC reference voltage source values (used in SET_REFERENCE payload)
#define xBRIDGE_ADC_REF_VDD      0x00U // internal VDD rail
#define xBRIDGE_ADC_REF_INTERNAL 0x01U // internal bandgap reference
#define xBRIDGE_ADC_REF_EXTERNAL 0x02U // external VREF pin

    // Maximum number of ADC channels readable in a single READ_MULTI command
#define xBRIDGE_ADC_MAX_CHANNELS 32U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Hardware ADC peripheral ops table supplied by the port layer.
    typedef struct xBRIDGE_ADC_Peripheral_Ops_t
    {
        // Set ADC resolution in bits (8, 10, 12, or 16).
        xRETURN_t (*set_resolution)(void *adc_ctx, uint8_t bits);

        // Set ADC reference voltage source (xBRIDGE_ADC_REF_*).
        xRETURN_t (*set_reference)(void *adc_ctx, uint8_t reference);

        // Set ADC sample rate in samples per second.
        xRETURN_t (*set_sample_rate)(void *adc_ctx, uint32_t samples_per_sec);

        // Perform a single blocking conversion on one channel.
        // result is written as a raw ADC count (up to 16 bits).
        xRETURN_t (*read_single)(void *adc_ctx, uint32_t channel, uint32_t *result);

        // Perform a blocking conversion on all channels indicated by channel_mask.
        // results[] must be large enough for popcount(channel_mask) entries.
        // channels are stored in ascending channel index order.
        xRETURN_t (*read_multi)(void *adc_ctx, uint32_t channel_mask, uint32_t *results, uint32_t result_count);

    } xBRIDGE_ADC_Peripheral_Ops_t;

    // ADC bridge channel runtime context (caller-owned, zero-init before xBRIDGE_ADC_Init).
    typedef struct xBRIDGE_ADC_Context_t
    {
        const xBRIDGE_USB_Ops_t *usb_ops;
        void *usb_ctx;
        const xBRIDGE_ADC_Peripheral_Ops_t *adc_ops;
        void *adc_ctx;

        uint8_t cmd_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Cmd_t)];
        uint8_t resp_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Resp_t)];
        xBRIDGE_State_t state;

        uint8_t resolution_bits; // current ADC resolution (8/10/12/16)

    } xBRIDGE_ADC_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the ADC bridge channel context and wire both ops tables.
    xRETURN_t xBRIDGE_ADC_Init(xBRIDGE_ADC_Context_t *ctx,
                               const xBRIDGE_USB_Ops_t *usb_ops,
                               void *usb_ctx,
                               const xBRIDGE_ADC_Peripheral_Ops_t *adc_ops,
                               void *adc_ctx);

    // Called from xUSBD WINUSB on_data_received for this channel's Bulk OUT endpoint.
    xRETURN_t xBRIDGE_ADC_On_USB_Receive(xBRIDGE_ADC_Context_t *ctx, const uint8_t *data, uint32_t length);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_ADC_H
// EOF /////////////////////////////////////////////////////////////////////////////
