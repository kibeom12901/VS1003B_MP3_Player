#include "util_uart.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart2;

void uprintln(const char *fmt, ...)
{
    char buf[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
    const uint8_t crlf[2] = {'\r','\n'};
    HAL_UART_Transmit(&huart2, (uint8_t*)crlf, 2, HAL_MAX_DELAY);
}
