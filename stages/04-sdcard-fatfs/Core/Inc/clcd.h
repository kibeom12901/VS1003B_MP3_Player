#ifndef __CLCD_H__
#define __CLCD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* --- Pin map (your wiring) --- */
#define CLCD_GPIO_PORT   GPIOE
#define CLCD_RS_PIN      GPIO_PIN_0     // PE0
#define CLCD_E_PIN       GPIO_PIN_1     // PE1

/* If RW is tied to GND, leave 0. If wired to PE2, set to 1. */
#define CLCD_USE_RW      0
#define CLCD_RW_PIN      GPIO_PIN_2     // PE2 (only if CLCD_USE_RW=1)

#define CLCD_D4_PIN      GPIO_PIN_4     // PE4
#define CLCD_D5_PIN      GPIO_PIN_5     // PE5
#define CLCD_D6_PIN      GPIO_PIN_6     // PE6
#define CLCD_D7_PIN      GPIO_PIN_7     // PE7

/* --- API --- */
void CLCD_GPIO_Init(void);
void CLCD_Init(void);

void CLCD_Clear(void);
void CLCD_ReturnHome(void);

void CLCD_Cmd(uint8_t cmd);
void CLCD_PutChar(uint8_t ch);
void CLCD_Puts(uint8_t col, uint8_t row, const uint8_t *str);
void CLCD_GotoXY(uint8_t col, uint8_t row);

void CLCD_DisplayOn(uint8_t cursor_on, uint8_t blink_on);
void CLCD_CreateChar(uint8_t location, const uint8_t map[8]);

#ifdef __cplusplus
}
#endif

#endif /* __CLCD_H__ */
