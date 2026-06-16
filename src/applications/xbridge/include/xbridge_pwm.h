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

// @file xbridge_pwm.h
// @brief xBRIDGE PWM channel - WINUSB binary frame PWM frequency, duty-cycle, and enable control.
//

#ifndef XBRIDGE_PWM_H
#define XBRIDGE_PWM_H

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

    // PWM command opcodes (WINUSB binary frame cmd field)
#define xBRIDGE_PWM_CMD_SET_FREQUENCY 0x01U // set PWM frequency in Hz
#define xBRIDGE_PWM_CMD_SET_DUTY      0x02U // set duty cycle 0-10000 (= 0.00-100.00 %)
#define xBRIDGE_PWM_CMD_ENABLE        0x03U // start PWM output on a channel
#define xBRIDGE_PWM_CMD_DISABLE       0x04U // stop PWM output on a channel
#define xBRIDGE_PWM_CMD_SET_POLARITY  0x05U // set output polarity (normal / inverted)

    // PWM polarity values (used in SET_POLARITY payload)
#define xBRIDGE_PWM_POLARITY_NORMAL   0x00U
#define xBRIDGE_PWM_POLARITY_INVERTED 0x01U

    // Duty-cycle range: 0 = 0.00%, 10000 = 100.00%
#define xBRIDGE_PWM_DUTY_MIN 0U
#define xBRIDGE_PWM_DUTY_MAX 10000U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Hardware PWM peripheral ops table supplied by the port layer.
    typedef struct xBRIDGE_PWM_Peripheral_Ops_t
    {
        // Set PWM carrier frequency for a channel (Hz).
        xRETURN_t (*set_frequency)(void *pwm_ctx, uint32_t channel, uint32_t frequency_hz);

        // Set duty cycle for a channel (0-10000 = 0.00-100.00 %).
        xRETURN_t (*set_duty)(void *pwm_ctx, uint32_t channel, uint32_t duty_per_10k);

        // Enable PWM output on a channel.
        xRETURN_t (*enable)(void *pwm_ctx, uint32_t channel);

        // Disable PWM output on a channel (output goes to inactive level).
        xRETURN_t (*disable)(void *pwm_ctx, uint32_t channel);

        // Set output polarity (normal or inverted).
        xRETURN_t (*set_polarity)(void *pwm_ctx, uint32_t channel, uint8_t polarity);

    } xBRIDGE_PWM_Peripheral_Ops_t;

    // PWM bridge channel runtime context (caller-owned, zero-init before xBRIDGE_PWM_Init).
    typedef struct xBRIDGE_PWM_Context_t
    {
        const xBRIDGE_USB_Ops_t *usb_ops;
        void *usb_ctx;
        const xBRIDGE_PWM_Peripheral_Ops_t *pwm_ops;
        void *pwm_ctx;

        uint8_t cmd_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Cmd_t)];
        uint8_t resp_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Resp_t)];
        xBRIDGE_State_t state;

    } xBRIDGE_PWM_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the PWM bridge channel context and wire both ops tables.
    xRETURN_t xBRIDGE_PWM_Init(xBRIDGE_PWM_Context_t *ctx,
                               const xBRIDGE_USB_Ops_t *usb_ops,
                               void *usb_ctx,
                               const xBRIDGE_PWM_Peripheral_Ops_t *pwm_ops,
                               void *pwm_ctx);

    // Called from xUSBD WINUSB on_data_received for this channel's Bulk OUT endpoint.
    xRETURN_t xBRIDGE_PWM_On_USB_Receive(xBRIDGE_PWM_Context_t *ctx, const uint8_t *data, uint32_t length);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_PWM_H
// EOF /////////////////////////////////////////////////////////////////////////////
