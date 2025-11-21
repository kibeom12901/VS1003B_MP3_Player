#include "main.h"
#include "adc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "fatfs.h"
#include "vs1003.h"
#include "playlist.h"
#include "player.h"
#include "button.h"
#include "volume.h"
#include "util_uart.h"
#include "MP3Sample.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_FATFS_Init();

    uprintln("VS1003 + SD/FatFS MP3 player starting...");
    SD_SPI2_Check();

    if (!TryMount()) {
        uprintln("Mount failed — fallback sample");
        VS_InitSimple();
        VolumeTask();
        VS_SendMusic(sampleMp3, sampleMp3_len);
        VS_SendZeros(2052);
        while (1) { VolumeTask(); HAL_Delay(50); }
    }

    VS_InitSimple();
    for (int i=0;i<6;i++){ VolumeTask(); HAL_Delay(30); }

    BuildPlaylist("/music");
    if (g_track_count == 0) {
        uprintln("No MP3s found in /music");
        VS_SendMusic(sampleMp3, sampleMp3_len);
        VS_SendZeros(2052);
        while (1) { VolumeTask(); HAL_Delay(100); }
    }

    uint32_t idx = 0;
    while (1) {
        uprintln("♪ Now Playing: %s", g_tracks[idx].path);
        PlayTrack(&g_tracks[idx]);

        if (g_prev_requested) {
            g_prev_requested = 0;
            idx = (idx == 0) ? g_track_count - 1 : (idx - 1);
        } else {
            idx = (idx + 1) % g_track_count;
            g_next_requested = 0;
        }
    }
}

/* ------------------------------------------------------------------------- */
/*                             Support Functions                              */
/* ------------------------------------------------------------------------- */
static void MX_NVIC_Init(void)
{
  HAL_NVIC_SetPriority(TIM7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM7_IRQn);
  HAL_NVIC_SetPriority(EXTI0_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/* System Clock Configuration: HSE + PLL @ 168 MHz */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM       = 8;
  RCC_OscInitStruct.PLL.PLLN       = 336;
  RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ       = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1| RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file; (void)line;
}
#endif

