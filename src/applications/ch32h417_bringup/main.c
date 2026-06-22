#include <stdint.h>
#include <stdbool.h>

#ifndef asm
#define asm __asm__
#endif

#include "ch32h417.h"
#include "xsdk_port_ch32h417.h"
#include "uart_console.h"
#include "blinky.h"
#include "usb_dev.h"
#include "test_peripherals.h"
#include "rtos_tasks.h"

int main(void)
{
    uint32_t actual_hclk = xRCC_Get_HCLK_Freq();

    xSDK_Port_Init();
    xSDK_Port_USART1_Pinmux_Init();

    // Initialize UART Console
    xSDK_Console_Init(actual_hclk);
    xSDK_Console_Write("xSDK CH32H417 boot\r\n");

    // Initialize LEDs
    xSDK_Blinky_Init();

    // Initialize Peripheral Test Suite
    // xSDK_I2C_Test_Init(actual_hclk);
    // xSDK_SPI_Test_Init(actual_hclk);
    // xSDK_Timer_Test_Init(actual_hclk);

    // Initialize USB Device Stack
    xSDK_USB_Init();

    // USBSS RAM bring-up: report link status changes without entering the RTOS scheduler.
    for (;;)
    {
        xSDK_USB_Service();
        xSDK_USB_Dump_Status("main");
    }
}
