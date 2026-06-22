#ifndef UART_CONSOLE_H
#define UART_CONSOLE_H

#include <stdint.h>

void xSDK_Console_Init(uint32_t hclk_hz);
void xSDK_Console_PutChar(char character);
void xSDK_Console_Write(const char *text);
void xSDK_Console_PrintHex32(uint32_t val);

#endif /* UART_CONSOLE_H */
