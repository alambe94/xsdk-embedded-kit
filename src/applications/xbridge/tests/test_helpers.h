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

// @file test_helpers.h
// @brief xBRIDGE test helpers - fff mock definitions for USB and peripheral ops tables.
//

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

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
#include "fff.h"
#include "unity.h"
#include "xreturn.h"
#include "xbridge_core.h"
#include "xbridge_uart.h"
#include "xbridge_i2c.h"
#include "xbridge_spi.h"
#include "xbridge_can.h"
#include "xbridge_qspi.h"
#include "xbridge_dap.h"
#include "xbridge_gpio.h"
#include "xbridge_pwm.h"
#include "xbridge_adc.h"

    // MACROS //////////////////////////////////////////////////////////////////////

// fff macro expansions generate trailing ';' constructs that -Wpedantic rejects.
// Suppress for this section only; all other test code still compiles pedantically.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

    // FFF GLOBALS - one definition per executable (safe: each test is one .c file)
    DEFINE_FFF_GLOBALS;

    // Mock USB ops
    FAKE_VALUE_FUNC(xRETURN_t, mock_usb_send, void *, const uint8_t *, uint32_t);

    // Mock UART peripheral ops
    FAKE_VALUE_FUNC(xRETURN_t, mock_uart_set_line_coding, void *, uint32_t, uint8_t, uint8_t, uint8_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_uart_write, void *, const uint8_t *, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_uart_read, void *, uint8_t *, uint32_t, uint32_t *);
    FAKE_VALUE_FUNC(bool, mock_uart_is_rx_ready, void *);

    // Mock I2C peripheral ops
    FAKE_VALUE_FUNC(xRETURN_t, mock_i2c_set_speed, void *, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_i2c_write, void *, uint16_t, const uint8_t *, uint32_t, bool);
    FAKE_VALUE_FUNC(xRETURN_t, mock_i2c_read, void *, uint16_t, uint8_t *, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_i2c_write_read, void *, uint16_t, const uint8_t *, uint32_t, uint8_t *, uint32_t);

    // Mock SPI peripheral ops
    FAKE_VALUE_FUNC(xRETURN_t, mock_spi_set_mode, void *, uint8_t, uint8_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_spi_set_speed, void *, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_spi_cs_assert, void *, uint8_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_spi_cs_deassert, void *, uint8_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_spi_transfer, void *, const uint8_t *, uint8_t *, uint32_t);

    // Mock CAN peripheral ops
    FAKE_VALUE_FUNC(xRETURN_t, mock_can_set_bitrate, void *, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_can_open, void *);
    FAKE_VALUE_FUNC(xRETURN_t, mock_can_close, void *);
    FAKE_VALUE_FUNC(xRETURN_t, mock_can_transmit, void *, uint32_t, bool, bool, uint8_t, const uint8_t *);
    FAKE_VALUE_FUNC(bool, mock_can_rx_available, void *);
    FAKE_VALUE_FUNC(xRETURN_t, mock_can_receive, void *, uint32_t *, bool *, bool *, uint8_t *, uint8_t *);

    // Mock QSPI peripheral ops
    FAKE_VALUE_FUNC(xRETURN_t, mock_qspi_set_speed, void *, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_qspi_transfer, void *, uint8_t, uint8_t, uint32_t, uint32_t, const uint8_t *, uint8_t *, uint32_t);

    // Mock DAP peripheral ops
    FAKE_VALUE_FUNC(xRETURN_t, mock_dap_pin_write, void *, uint8_t, uint8_t);
    FAKE_VALUE_FUNC(uint8_t, mock_dap_pin_read, void *, uint8_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_dap_swj_clock, void *, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_dap_swj_sequence, void *, uint32_t, const uint8_t *);
    FAKE_VALUE_FUNC(xRETURN_t, mock_dap_swd_sequence, void *, uint32_t, const uint8_t *, uint8_t *);
    FAKE_VALUE_FUNC(xRETURN_t, mock_dap_jtag_sequence, void *, uint32_t, uint8_t, const uint8_t *, uint8_t *);
    FAKE_VALUE_FUNC(xRETURN_t, mock_dap_reset_target, void *);
    FAKE_VALUE_FUNC(xRETURN_t, mock_dap_delay_us, void *, uint32_t);

    // Mock GPIO peripheral ops
    FAKE_VALUE_FUNC(xRETURN_t, mock_gpio_set_direction, void *, uint32_t, uint32_t, uint8_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_gpio_write_pin, void *, uint32_t, uint32_t, uint8_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_gpio_write_port, void *, uint32_t, uint32_t, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_gpio_read_pin, void *, uint32_t, uint32_t, uint8_t *);
    FAKE_VALUE_FUNC(xRETURN_t, mock_gpio_read_port, void *, uint32_t, uint32_t *);
    FAKE_VALUE_FUNC(xRETURN_t, mock_gpio_set_pull, void *, uint32_t, uint32_t, uint8_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_gpio_toggle_pin, void *, uint32_t, uint32_t);

    // Mock PWM peripheral ops
    FAKE_VALUE_FUNC(xRETURN_t, mock_pwm_set_frequency, void *, uint32_t, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_pwm_set_duty, void *, uint32_t, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_pwm_enable, void *, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_pwm_disable, void *, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_pwm_set_polarity, void *, uint32_t, uint8_t);

    // Mock ADC peripheral ops
    FAKE_VALUE_FUNC(xRETURN_t, mock_adc_set_resolution, void *, uint8_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_adc_set_reference, void *, uint8_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_adc_set_sample_rate, void *, uint32_t);
    FAKE_VALUE_FUNC(xRETURN_t, mock_adc_read_single, void *, uint32_t, uint32_t *);
    FAKE_VALUE_FUNC(xRETURN_t, mock_adc_read_multi, void *, uint32_t, uint32_t *, uint32_t);

#pragma GCC diagnostic pop

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // Pre-wired ops tables for each channel
    static const xBRIDGE_USB_Ops_t g_mock_usb_ops = {
        .send = mock_usb_send,
    };

    static const xBRIDGE_UART_Peripheral_Ops_t g_mock_uart_ops = {
        .set_line_coding = mock_uart_set_line_coding,
        .write = mock_uart_write,
        .read = mock_uart_read,
        .is_rx_ready = mock_uart_is_rx_ready,
    };

    static const xBRIDGE_I2C_Peripheral_Ops_t g_mock_i2c_ops = {
        .set_speed = mock_i2c_set_speed,
        .write = mock_i2c_write,
        .read = mock_i2c_read,
        .write_read = mock_i2c_write_read,
    };

    static const xBRIDGE_SPI_Peripheral_Ops_t g_mock_spi_ops = {
        .set_mode = mock_spi_set_mode,
        .set_speed = mock_spi_set_speed,
        .cs_assert = mock_spi_cs_assert,
        .cs_deassert = mock_spi_cs_deassert,
        .transfer = mock_spi_transfer,
    };

    static const xBRIDGE_CAN_Peripheral_Ops_t g_mock_can_ops = {
        .set_bitrate = mock_can_set_bitrate,
        .open = mock_can_open,
        .close = mock_can_close,
        .transmit = mock_can_transmit,
        .rx_available = mock_can_rx_available,
        .receive = mock_can_receive,
    };

    static const xBRIDGE_QSPI_Peripheral_Ops_t g_mock_qspi_ops = {
        .set_speed = mock_qspi_set_speed,
        .transfer = mock_qspi_transfer,
    };

    static const xBRIDGE_DAP_Peripheral_Ops_t g_mock_dap_ops = {
        .pin_write = mock_dap_pin_write,
        .pin_read = mock_dap_pin_read,
        .swj_clock = mock_dap_swj_clock,
        .swj_sequence = mock_dap_swj_sequence,
        .swd_sequence = mock_dap_swd_sequence,
        .jtag_sequence = mock_dap_jtag_sequence,
        .reset_target = mock_dap_reset_target,
        .delay_us = mock_dap_delay_us,
    };

    static const xBRIDGE_GPIO_Peripheral_Ops_t g_mock_gpio_ops = {
        .set_direction = mock_gpio_set_direction,
        .write_pin = mock_gpio_write_pin,
        .write_port = mock_gpio_write_port,
        .read_pin = mock_gpio_read_pin,
        .read_port = mock_gpio_read_port,
        .set_pull = mock_gpio_set_pull,
        .toggle_pin = mock_gpio_toggle_pin,
    };

    static const xBRIDGE_PWM_Peripheral_Ops_t g_mock_pwm_ops = {
        .set_frequency = mock_pwm_set_frequency,
        .set_duty = mock_pwm_set_duty,
        .enable = mock_pwm_enable,
        .disable = mock_pwm_disable,
        .set_polarity = mock_pwm_set_polarity,
    };

    static const xBRIDGE_ADC_Peripheral_Ops_t g_mock_adc_ops = {
        .set_resolution = mock_adc_set_resolution,
        .set_reference = mock_adc_set_reference,
        .set_sample_rate = mock_adc_set_sample_rate,
        .read_single = mock_adc_read_single,
        .read_multi = mock_adc_read_multi,
    };

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    static inline void reset_all_mocks(void)
    {
        RESET_FAKE(mock_usb_send);

        RESET_FAKE(mock_uart_set_line_coding);
        RESET_FAKE(mock_uart_write);
        RESET_FAKE(mock_uart_read);
        RESET_FAKE(mock_uart_is_rx_ready);

        RESET_FAKE(mock_i2c_set_speed);
        RESET_FAKE(mock_i2c_write);
        RESET_FAKE(mock_i2c_read);
        RESET_FAKE(mock_i2c_write_read);

        RESET_FAKE(mock_spi_set_mode);
        RESET_FAKE(mock_spi_set_speed);
        RESET_FAKE(mock_spi_cs_assert);
        RESET_FAKE(mock_spi_cs_deassert);
        RESET_FAKE(mock_spi_transfer);

        RESET_FAKE(mock_can_set_bitrate);
        RESET_FAKE(mock_can_open);
        RESET_FAKE(mock_can_close);
        RESET_FAKE(mock_can_transmit);
        RESET_FAKE(mock_can_rx_available);
        RESET_FAKE(mock_can_receive);

        RESET_FAKE(mock_qspi_set_speed);
        RESET_FAKE(mock_qspi_transfer);

        RESET_FAKE(mock_dap_pin_write);
        RESET_FAKE(mock_dap_pin_read);
        RESET_FAKE(mock_dap_swj_clock);
        RESET_FAKE(mock_dap_swj_sequence);
        RESET_FAKE(mock_dap_swd_sequence);
        RESET_FAKE(mock_dap_jtag_sequence);
        RESET_FAKE(mock_dap_reset_target);
        RESET_FAKE(mock_dap_delay_us);

        RESET_FAKE(mock_gpio_set_direction);
        RESET_FAKE(mock_gpio_write_pin);
        RESET_FAKE(mock_gpio_write_port);
        RESET_FAKE(mock_gpio_read_pin);
        RESET_FAKE(mock_gpio_read_port);
        RESET_FAKE(mock_gpio_set_pull);
        RESET_FAKE(mock_gpio_toggle_pin);

        RESET_FAKE(mock_pwm_set_frequency);
        RESET_FAKE(mock_pwm_set_duty);
        RESET_FAKE(mock_pwm_enable);
        RESET_FAKE(mock_pwm_disable);
        RESET_FAKE(mock_pwm_set_polarity);

        RESET_FAKE(mock_adc_set_resolution);
        RESET_FAKE(mock_adc_set_reference);
        RESET_FAKE(mock_adc_set_sample_rate);
        RESET_FAKE(mock_adc_read_single);
        RESET_FAKE(mock_adc_read_multi);

        FFF_RESET_HISTORY();
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // TEST_HELPERS_H
// EOF /////////////////////////////////////////////////////////////////////////////
