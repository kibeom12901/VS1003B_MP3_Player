/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : VS10xx (VS1003/VS1053) bring-up + MP3 playback from flash
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "MP3Sample.h"    // provides: sampleMp3[], sampleMp3_len
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* -------- VS10xx pin map (match CubeMX setup) -------- */
#define VS_XCS_PORT   GPIOA
#define VS_XCS_PIN    GPIO_PIN_4   // SCI CS
#define VS_XDCS_PORT  GPIOE
#define VS_XDCS_PIN   GPIO_PIN_3   // SDI CS
#define VS_RST_PORT   GPIOE
#define VS_RST_PIN    GPIO_PIN_2   // Reset
#define VS_DREQ_PORT  GPIOB
#define VS_DREQ_PIN   GPIO_PIN_0   // Data request (input)

/* SCI register addresses */
#define SCI_MODE      0x00
#define SCI_STATUS    0x01
#define SCI_CLOCKF    0x03
#define SCI_VOL       0x0B

/* SCI opcodes */
#define SCI_WRITE_OP  0x02
#define SCI_READ_OP   0x03

/* ---- VS10xx SCI MODE bits ---- */
#define SM_DIFF            (1<<0)
#define SM_LAYER12         (1<<1)
#define SM_RESET           (1<<2)
#define SM_CANCEL          (1<<3)
#define SM_EARSPEAKER_LO   (1<<4)
#define SM_TESTS           (1<<5)
#define SM_STREAM          (1<<6)
#define SM_EARSPEAKER_HI   (1<<7)
#define SM_DACT            (1<<8)
#define SM_SDIORD          (1<<9)
#define SM_SDISHARE        (1<<10)
#define SM_SDINEW          (1<<11)   // MUST be 1 on VS10xx
#define SM_ADPCM           (1<<12)
#define SM_LINE1           (1<<14)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define VS_XCS_L()        HAL_GPIO_WritePin(VS_XCS_PORT,  VS_XCS_PIN,  GPIO_PIN_RESET)
#define VS_XCS_H()        HAL_GPIO_WritePin(VS_XCS_PORT,  VS_XCS_PIN,  GPIO_PIN_SET)
#define VS_XDCS_L()       HAL_GPIO_WritePin(VS_XDCS_PORT, VS_XDCS_PIN, GPIO_PIN_RESET)
#define VS_XDCS_H()       HAL_GPIO_WritePin(VS_XDCS_PORT, VS_XDCS_PIN, GPIO_PIN_SET)
#define VS_RST_L()        HAL_GPIO_WritePin(VS_RST_PORT,  VS_RST_PIN,  GPIO_PIN_RESET)
#define VS_RST_H()        HAL_GPIO_WritePin(VS_RST_PORT,  VS_RST_PIN,  GPIO_PIN_SET)
#define VS_DREQ_IS_H()   (HAL_GPIO_ReadPin(VS_DREQ_PORT, VS_DREQ_PIN) == GPIO_PIN_SET)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart2;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */
static void uprintln(const char *fmt, ...);
static HAL_StatusTypeDef VS_SCI_Write(uint8_t reg, uint16_t val);
static HAL_StatusTypeDef VS_SCI_Read(uint8_t reg, uint16_t *out);
static void VS_HardReset(void);
static void VS_SDI_SendChunk(const uint8_t *p, uint16_t n);
static void VS_SendMusic(const uint8_t *buf, uint32_t len);
static void VS_SendZeros(uint16_t n);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void uprintln(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, (uint16_t)strlen(buf), HAL_MAX_DELAY);
    const char crlf[2] = {'\r','\n'};
    HAL_UART_Transmit(&huart2, (uint8_t*)crlf, 2, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef VS_SCI_Write(uint8_t reg, uint16_t val)
{
    uint8_t tx[4] = {SCI_WRITE_OP, reg, (uint8_t)(val >> 8), (uint8_t)val};
    while (!VS_DREQ_IS_H());
    VS_XCS_L();
    HAL_StatusTypeDef s = HAL_SPI_Transmit(&hspi1, tx, sizeof(tx), HAL_MAX_DELAY);
    VS_XCS_H();
    return s;
}

static HAL_StatusTypeDef VS_SCI_Read(uint8_t reg, uint16_t *out)
{
    uint8_t tx[2] = {SCI_READ_OP, reg};
    uint8_t rx[2] = {0};
    while (!VS_DREQ_IS_H());
    VS_XCS_L();
    HAL_StatusTypeDef s = HAL_SPI_Transmit(&hspi1, tx, 2, HAL_MAX_DELAY);
    s |= HAL_SPI_Receive(&hspi1, rx, 2, HAL_MAX_DELAY);
    VS_XCS_H();
    *out = ((uint16_t)rx[0] << 8) | rx[1];
    return s;
}

static void VS_HardReset(void)
{
    VS_XCS_H();
    VS_XDCS_H();
    VS_RST_L();
    HAL_Delay(5);
    VS_RST_H();
    uint32_t t0 = HAL_GetTick();
    while (!VS_DREQ_IS_H()) {
        if (HAL_GetTick() - t0 > 300) {
            uprintln("ERR: DREQ timeout");
            break;
        }
    }
}

/* send ≤32 bytes when DREQ is high */
static void VS_SDI_SendChunk(const uint8_t *p, uint16_t n)
{
    while (n) {
        while (!VS_DREQ_IS_H()); // ready for another burst
        uint16_t chunk = (n > 32) ? 32 : n;
        VS_XDCS_L();
        HAL_SPI_Transmit(&hspi1, (uint8_t*)p, chunk, HAL_MAX_DELAY);
        VS_XDCS_H();
        p += chunk;
        n -= chunk;
    }
}

/* stream whole buffer */
static void VS_SendMusic(const uint8_t *buf, uint32_t len)
{
    uint32_t i = 0;
    while (i < len) {
        uint16_t chunk = (len - i > 512) ? 512 : (uint16_t)(len - i); // coarse slice
        VS_SDI_SendChunk(&buf[i], chunk);
        i += chunk;
    }
}

/* a small end-fill to flush decoder (no SM_CANCEL here) */
static void VS_SendZeros(uint16_t n)
{
    uint8_t z[32] = {0};
    while (n) {
        uint16_t c = (n > sizeof(z)) ? sizeof(z) : n;
        VS_SDI_SendChunk(z, c);
        n -= c;
    }
}
/* USER CODE END 0 */

/**
  * @brief The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM7_Init();
  MX_TIM4_Init();
  MX_TIM10_Init();
  MX_TIM2_Init();
  MX_TIM5_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_ADC2_Init();
  MX_SPI1_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();

  /* USER CODE BEGIN 2 */
  uprintln("NEWEST VS10xx bring-up starting...");

  VS_HardReset();

  /* REQUIRED: enable SDI NEW mode early (per VLSI) */
  VS_SCI_Write(SCI_MODE, SM_SDINEW);
  HAL_Delay(2);

  uint16_t mode=0, status=0, clockf=0;
  if (VS_SCI_Read(SCI_MODE, &mode)    != HAL_OK) uprintln("SCI_MODE read HAL error");
  if (VS_SCI_Read(SCI_STATUS, &status)!= HAL_OK) uprintln("SCI_STATUS read HAL error");
  if (VS_SCI_Read(SCI_CLOCKF, &clockf)!= HAL_OK) uprintln("SCI_CLOCKF read HAL error");

  uprintln("SCI_MODE = 0x%04X", mode);
  uprintln("SCI_STATUS= 0x%04X", status);
  uprintln("SCI_CLOCKF= 0x%04X", clockf);

  if ((mode==0x0000 || mode==0xFFFF) || (status==0x0000 || status==0xFFFF)) {
      uprintln("=> Likely wiring/SPI mode/CS/reset issue (saw 0x0000/0xFFFF).");
  } else {
      uprintln("=> SCI reads look sane.");
  }

  /* bump internal clock (then you may raise SPI baud later) & set volume */
  uprintln("Setting CLOCKF and VOL...");
  VS_SCI_Write(SCI_CLOCKF, 0x8800);  // common safe value after reset
  VS_SCI_Write(SCI_VOL,     0x2020); // attenuation; smaller = louder
  HAL_Delay(50);

  /* Play embedded MP3 */
  uprintln("Sending MP3Sample data (%u bytes)...", sampleMp3_len);
  VS_SendMusic(sampleMp3, sampleMp3_len);

  /* Small end-fill so DAC stops cleanly */
  VS_SendZeros(2052);
  uprintln("Playback complete.");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    HAL_Delay(1000);
  }
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
}
/* USER CODE END 3 */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  /* Keep YOUR CubeMX-generated version here as before. (This copy matches what you posted and avoids duplicate defs elsewhere.) */
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
    Error_Handler();
  }
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* TIM7_IRQn interrupt configuration (left from your template; unused here) */
  HAL_NVIC_SetPriority(TIM7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM7_IRQn);
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}

#ifdef USE_FULL_ASSERT
/**
  * @brief Reports assert_param error location.
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif /* USE_FULL_ASSERT */
