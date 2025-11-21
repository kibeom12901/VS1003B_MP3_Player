#include "volume.h"
#include "adc.h"
#include "vs1003.h"
#include "util_uart.h"

extern ADC_HandleTypeDef hadc1;

static int g_vol_att      = 0x20;
static int g_vol_att_last = -999;

static int ReadADC1_IN1_Filtered_Internal(void)
{
    static int filt = -1;
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    int raw = (int)HAL_ADC_GetValue(&hadc1);   // 0..4095
    HAL_ADC_Stop(&hadc1);
    if (filt < 0) filt = raw;
    filt = (filt * 3 + raw) / 4;               // simple IIR filter
    return filt;
}

int ReadADC1_IN1_Filtered(void)
{
    return ReadADC1_IN1_Filtered_Internal();
}

/* Periodic call (≈80 ms) */
void VolumeTask(void)
{
    static uint32_t last_ms = 0;
    uint32_t now = HAL_GetTick();
    if ((now - last_ms) < VOL_TASK_PERIOD_MS) return;
    last_ms = now;

    int raw = ReadADC1_IN1_Filtered_Internal();    // 0..4095
    int pct = (raw * 100 + 2047) / 4095;           // %
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    /* Map pot → attenuation (0 loudest .. 254 quietest) */
    int att = (int)((100 - pct) * VOL_MAX_ATT / 100);

    if (g_vol_att_last < 0 ||
        att >= g_vol_att_last + VOL_STEP_HYST ||
        att <= g_vol_att_last - VOL_STEP_HYST) {

        g_vol_att = att;
        VS_SetVolume((uint8_t)g_vol_att, (uint8_t)g_vol_att);
        g_vol_att_last = g_vol_att;

        static uint8_t print_div = 0;
        if ((print_div++ & 0x07) == 0)
            uprintln("VOL: pot=%4d  ~%3d%%  att=%d (0=loud)", raw, pct, g_vol_att);
    }
}
