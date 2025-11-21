#ifndef __BUTTON_H
#define __BUTTON_H
#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Pin configuration (Discovery default) */
#define BTN_PORT  GPIOA
#define BTN_PIN   GPIO_PIN_0

/* Gesture timing (ms) */
#define BTN_LONG_MS    700U
#define BTN_DOUBLE_MS  350U

/* External flags (used by main/player) */
extern volatile uint8_t g_next_requested;
extern volatile uint8_t g_prev_requested;
extern volatile uint8_t g_shuffle_enabled;

/* Functions */
void ButtonTask(void);

#ifdef __cplusplus
}
#endif
#endif /* __BUTTON_H */
