#ifndef TEST_PERIPHERALS_H
#define TEST_PERIPHERALS_H

#include <stdint.h>

void xSDK_I2C_Test_Init(uint32_t hclk_hz);
void xSDK_SPI_Test_Init(uint32_t hclk_hz);
void xSDK_Timer_Test_Init(uint32_t hclk_hz);

void xSDK_Timer_Test_Run(void);
void xSDK_I2C_Test_Run(void);
void xSDK_SPI_Test_Run(void);

#endif /* TEST_PERIPHERALS_H */
