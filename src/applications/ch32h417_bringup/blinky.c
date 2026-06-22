#include "blinky.h"
#include "xgpio.h"
#include "xsdk_port_ch32h417.h"

#define LED_GREEN_PIN 2U
#define LED_BLUE_PIN  3U

void xSDK_Blinky_Init(void)
{
    xGPIO_Pin_Config_t led_pin_cfg = {
        .mode = xGPIO_PIN_MODE_OUTPUT_PUSH_PULL, .speed = xGPIO_PIN_SPEED_VERY_HIGH, .pull = xGPIO_PIN_PULL_NONE, .alternate_function = 0U};

    (void)xGPIO_Configure_Pin(&g_gpio_c_ctx, LED_GREEN_PIN, &led_pin_cfg);
    (void)xGPIO_Configure_Pin(&g_gpio_c_ctx, LED_BLUE_PIN, &led_pin_cfg);

    // Turn both LEDs OFF initially (active low, so write true)
    (void)xGPIO_Pin_Write(&g_gpio_c_ctx, LED_GREEN_PIN, true);
    (void)xGPIO_Pin_Write(&g_gpio_c_ctx, LED_BLUE_PIN, true);
}

void xSDK_Blinky_Set_Blue(bool on)
{
    // Active low: false = ON, true = OFF
    (void)xGPIO_Pin_Write(&g_gpio_c_ctx, LED_BLUE_PIN, !on);
}

void xSDK_Blinky_Set_Green(bool on)
{
    // Active low: false = ON, true = OFF
    (void)xGPIO_Pin_Write(&g_gpio_c_ctx, LED_GREEN_PIN, !on);
}
