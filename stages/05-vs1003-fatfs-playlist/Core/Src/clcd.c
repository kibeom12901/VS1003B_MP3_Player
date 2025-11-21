#include "clcd.h"

/* --- Local helpers --- */
static void CLCD_Write4(uint8_t nibble);
static void CLCD_Write(uint8_t byte, uint8_t rs);
static void clcd_delay_us(uint32_t us);

/* Conservative timing (safe on HD44780 clones) */
#define CLCD_DELAY_ENABLE_PULSE_US  2U
#define CLCD_DELAY_POST_WRITE_US    40U
#define CLCD_DELAY_CLEAR_MS         2U   // clear/home ~1.53ms max

void CLCD_GPIO_Init(void)
{
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitTypeDef gi = {0};
    gi.Pin = CLCD_RS_PIN | CLCD_E_PIN |
             CLCD_D4_PIN | CLCD_D5_PIN | CLCD_D6_PIN | CLCD_D7_PIN
#if CLCD_USE_RW
           | CLCD_RW_PIN
#endif
           ;
    gi.Mode  = GPIO_MODE_OUTPUT_PP;
    gi.Pull  = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(CLCD_GPIO_PORT, &gi);

    HAL_GPIO_WritePin(CLCD_GPIO_PORT,
                      CLCD_RS_PIN | CLCD_E_PIN |
                      CLCD_D4_PIN | CLCD_D5_PIN | CLCD_D6_PIN | CLCD_D7_PIN
#if CLCD_USE_RW
                    | CLCD_RW_PIN
#endif
                      , GPIO_PIN_RESET);
}

static void CLCD_Write4(uint8_t nibble)
{
    HAL_GPIO_WritePin(CLCD_GPIO_PORT, CLCD_D4_PIN, (nibble & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(CLCD_GPIO_PORT, CLCD_D5_PIN, (nibble & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(CLCD_GPIO_PORT, CLCD_D6_PIN, (nibble & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(CLCD_GPIO_PORT, CLCD_D7_PIN, (nibble & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(CLCD_GPIO_PORT, CLCD_E_PIN, GPIO_PIN_SET);
    clcd_delay_us(CLCD_DELAY_ENABLE_PULSE_US);
    HAL_GPIO_WritePin(CLCD_GPIO_PORT, CLCD_E_PIN, GPIO_PIN_RESET);
}

static void CLCD_Write(uint8_t byte, uint8_t rs)
{
    HAL_GPIO_WritePin(CLCD_GPIO_PORT, CLCD_RS_PIN, rs ? GPIO_PIN_SET : GPIO_PIN_RESET);
#if CLCD_USE_RW
    HAL_GPIO_WritePin(CLCD_GPIO_PORT, CLCD_RW_PIN, GPIO_PIN_RESET); // write mode
#endif
    CLCD_Write4((byte >> 4) & 0x0F);
    CLCD_Write4(byte & 0x0F);
    clcd_delay_us(CLCD_DELAY_POST_WRITE_US);
}

/* --- Public low-level --- */
void CLCD_Cmd(uint8_t cmd)    { CLCD_Write(cmd, 0); }
void CLCD_PutChar(uint8_t ch) { CLCD_Write(ch, 1);  }

void CLCD_GotoXY(uint8_t col, uint8_t row)
{
    static const uint8_t base[2] = {0x00, 0x40};   // 16x2
    if (row > 1) row = 1;
    CLCD_Cmd(0x80 | (base[row] + col));
}

void CLCD_Clear(void)
{
    CLCD_Cmd(0x01);
    HAL_Delay(CLCD_DELAY_CLEAR_MS);
}

void CLCD_ReturnHome(void)
{
    CLCD_Cmd(0x02);
    HAL_Delay(CLCD_DELAY_CLEAR_MS);
}

/* --- High-level --- */
void CLCD_Puts(uint8_t col, uint8_t row, const uint8_t *str)
{
    CLCD_GotoXY(col, row);
    while (*str) CLCD_PutChar(*str++);
}

void CLCD_DisplayOn(uint8_t cursor_on, uint8_t blink_on)
{
    uint8_t cmd = 0x08 | 0x04;
    if (cursor_on) cmd |= 0x02;
    if (blink_on)  cmd |= 0x01;
    CLCD_Cmd(cmd);
}

void CLCD_CreateChar(uint8_t location, const uint8_t map[8])
{
    location &= 0x07;
    CLCD_Cmd(0x40 | (location << 3));
    for (int i = 0; i < 8; ++i) CLCD_PutChar(map[i]);
}

/* --- Init sequence (4-bit) --- */
void CLCD_Init(void)
{
    HAL_Delay(50);  // >40ms after VDD up

    HAL_GPIO_WritePin(CLCD_GPIO_PORT, CLCD_RS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(CLCD_GPIO_PORT, CLCD_E_PIN,  GPIO_PIN_RESET);
#if CLCD_USE_RW
    HAL_GPIO_WritePin(CLCD_GPIO_PORT, CLCD_RW_PIN, GPIO_PIN_RESET);
#endif

    /* 8-bit wakeup (send 0x3 by nibble) x3 */
    CLCD_Write4(0x03); HAL_Delay(5);
    CLCD_Write4(0x03); clcd_delay_us(150);
    CLCD_Write4(0x03); clcd_delay_us(150);

    /* 4-bit select */
    CLCD_Write4(0x02); clcd_delay_us(150);

    /* Function set: 4-bit, 2 lines, 5x8 */
    CLCD_Cmd(0x28);

    /* Display off, clear, entry mode, display on */
    CLCD_Cmd(0x08);
    CLCD_Clear();
    CLCD_Cmd(0x06);
    CLCD_Cmd(0x0C);
}

/* --- µs delay (DWT-based) --- */
static void clcd_delay_us(uint32_t us)
{
#if (__CORTEX_M >= 0x03)
    if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk))
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk))
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    const uint32_t cycles = (SystemCoreClock / 1000000U) * us;
    const uint32_t start  = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles) { __NOP(); }
#else
    volatile uint32_t n = us * (SystemCoreClock / 1000000U / 5U);
    while (n--) { __NOP(); }
#endif
}
