#ifndef __UTIL_UART_H
#define __UTIL_UART_H
#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdarg.h>

void uprintln(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
#endif /* __UTIL_UART_H */
