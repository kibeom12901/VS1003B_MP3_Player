#ifndef __VOLUME_H
#define __VOLUME_H
#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ADC tuning */
#define VOL_TASK_PERIOD_MS  80U
#define VOL_STEP_HYST       2
#define VOL_MAX_ATT         254

void VolumeTask(void);
int  ReadADC1_IN1_Filtered(void);

#ifdef __cplusplus
}
#endif
#endif /* __VOLUME_H */
