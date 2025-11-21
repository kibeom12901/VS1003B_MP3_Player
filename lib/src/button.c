#include "button.h"
#include "playlist.h"
#include "util_uart.h"

volatile uint8_t g_next_requested  = 0;
volatile uint8_t g_prev_requested  = 0;
volatile uint8_t g_shuffle_enabled = 0;

/* Internal state */
static uint8_t  g_btn_prev        = 0;
static uint32_t g_btn_press_t     = 0;
static uint32_t g_btn_last_up     = 0;
static uint8_t  g_btn_clicks      = 0;
static uint8_t  g_btn_long_fired  = 0;

void ButtonTask(void)
{
    uint8_t now = (HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) == GPIO_PIN_SET);
    uint32_t t  = HAL_GetTick();

    if (!g_btn_prev && now) {                // pressed
        g_btn_press_t = t;
        g_btn_long_fired = 0;
        if (t - g_btn_last_up <= BTN_DOUBLE_MS)
            g_btn_clicks = 2;
        else
            g_btn_clicks = 1;
    }

    if (now && !g_btn_long_fired) {          // long press
        if (t - g_btn_press_t >= BTN_LONG_MS) {
            g_btn_long_fired = 1;
            g_shuffle_enabled ^= 1;
            uprintln("Shuffle %s", g_shuffle_enabled ? "ON" : "OFF");
            if (g_shuffle_enabled) ShuffleTracks();
            g_btn_clicks = 0;                // cancel single/double
        }
    }

    if (g_btn_prev && !now) {                // released
        g_btn_last_up = t;
        if (!g_btn_long_fired && g_btn_clicks == 2) {
            g_prev_requested = 1;            // double → previous
            g_btn_clicks = 0;
        }
    }

    if (g_btn_clicks == 1 && (t - g_btn_last_up) > BTN_DOUBLE_MS && !now) {
        g_next_requested = 1;                // single → next
        g_btn_clicks = 0;
    }

    g_btn_prev = now;
}
