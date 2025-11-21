/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : VS10xx (VS1003/VS1053) SPI bring-up: SCI read test
  ******************************************************************************
  * Wiring (match CubeMX):
  *   VS_XCS  -> PA4   (GPIO out, idle HIGH)   // SCI chip select
  *   VS_XDCS -> PE3   (GPIO out, idle HIGH)   // SDI chip select (unused here)
  *   VS_RST  -> PE2   (GPIO out)              // Reset (active LOW)
  *   VS_DREQ -> PB0   (GPIO in, pull-down/up as needed)
  *   SPI1    -> SCK/PA5, MISO/PA6, MOSI/PA7   // Mode 0
  *   UART2   -> PA2 (TX)                      // 115200 for logs
  *
  * Expected after successful bring-up:
  *   SCI_MODE   ~ 0x0800
  *   SCI_STATUS ~ non-zero (e.g., 0x0038)
  *   SCI_CLOCKF = 0x0000 or 0x0008 depending on chip/reset
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
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
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define VS_XCS_L()     HAL_GPIO_WritePin(VS_XCS_PORT,  VS_XCS_PIN, GPIO_PIN_RESET)
#define VS_XCS_H()     HAL_GPIO_WritePin(VS_XCS_PORT,  VS_XCS_PIN, GPIO_PIN_SET)
#define VS_XDCS_L()    HAL_GPIO_WritePin(VS_XDCS_PORT, VS_XDCS_PIN, GPIO_PIN_RESET)
#define VS_XDCS_H()    HAL_GPIO_WritePin(VS_XDCS_PORT, VS_XDCS_PIN, GPIO_PIN_SET)
#define VS_RST_L()     HAL_GPIO_WritePin(VS_RST_PORT,  VS_RST_PIN, GPIO_PIN_RESET)
#define VS_RST_H()     HAL_GPIO_WritePin(VS_RST_PORT,  VS_RST_PIN, GPIO_PIN_SET)
#define VS_DREQ_IS_H() (HAL_GPIO_ReadPin(VS_DREQ_PORT, VS_DREQ_PIN) == GPIO_PIN_SET)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern SPI_HandleTypeDef  hspi1;    // Provided by CubeMX
extern UART_HandleTypeDef huart2;   // Provided by CubeMX
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */
static void uprintln(const char *fmt, ...);
static HAL_StatusTypeDef VS_SCI_Write(uint8_t reg, uint16_t val);
static HAL_StatusTypeDef VS_SCI_Read(uint8_t reg, uint16_t *out);
static void VS_HardReset(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void uprintln(const char *fmt, ...) {
  char buf[160];
  va_list ap; va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n <= 0) return;
  HAL_UART_Transmit(&huart2, (uint8_t*)buf, (uint16_t)strlen(buf), HAL_MAX_DELAY);
  const char crlf[2] = {'\r','\n'};
  HAL_UART_Transmit(&huart2, (uint8_t*)crlf, 2, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef VS_SCI_Write(uint8_t reg, uint16_t val) {
  uint8_t tx[4] = {SCI_WRITE_OP, reg, (uint8_t)(val >> 8), (uint8_t)val};
  while (!VS_DREQ_IS_H());         // Wait codec ready
  VS_XCS_L();
  HAL_StatusTypeDef s = HAL_SPI_Transmit(&hspi1, tx, sizeof(tx), 20);
  VS_XCS_H();
  return s;
}

static HAL_StatusTypeDef VS_SCI_Read(uint8_t reg, uint16_t *out) {
  uint8_t tx[2] = {SCI_READ_OP, reg};
  uint8_t rx[2] = {0};
  while (!VS_DREQ_IS_H());         // Wait codec ready
  VS_XCS_L();
  HAL_StatusTypeDef s  = HAL_SPI_Transmit(&hspi1, tx, 2, 20);
  s |= HAL_SPI_Receive(&hspi1, rx, 2, 20);
  VS_XCS_H();
  *out = ((uint16_t)rx[0] << 8) | rx[1];
  return s;
}

static void VS_HardReset(void) {
  // Idle chip-selects high
  VS_XCS_H();
  VS_XDCS_H();

  // Hardware reset pulse
  VS_RST_L();
  HAL_Delay(5);
  VS_RST_H();

  // Wait for DREQ to rise -> device ready for SCI
  uint32_t t0 = HAL_GetTick();
  while (!VS_DREQ_IS_H()) {
    if (HAL_GetTick() - t0 > 300) {
      uprintln("ERR: DREQ timeout");
      break;
    }
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals (from CubeMX) */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();

  /* Initialize interrupts (optional; left if generated) */
  MX_NVIC_Init();

  /* USER CODE BEGIN 2 */
  uprintln("VS10xx bring-up starting...");
  VS_HardReset();

  uint16_t mode=0, status=0, clockf=0;
  if (VS_SCI_Read(SCI_MODE,   &mode)   != HAL_OK) uprintln("SCI_MODE read HAL error");
  if (VS_SCI_Read(SCI_STATUS, &status) != HAL_OK) uprintln("SCI_STATUS read HAL error");
  if (VS_SCI_Read(SCI_CLOCKF, &clockf) != HAL_OK) uprintln("SCI_CLOCKF read HAL error");

  uprintln("SCI_MODE  = 0x%04X", mode);
  uprintln("SCI_STATUS= 0x%04X", status);
  uprintln("SCI_CLOCKF= 0x%04X", clockf);

  if ((mode==0x0000 || mode==0xFFFF) ||
      (status==0x0000 || status==0xFFFF)) {
    uprintln("=> Likely wiring/SPI mode/CS/reset issue (saw 0x0000/0xFFFF).");
  } else {
    uprintln("=> SCI reads look sane. Next: set CLOCKF/VOL and do sine test in step 02.");
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    HAL_Delay(500);
  }
  /* USER CODE END WHILE */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  /* Keep your CubeMX-generated version.
     Placeholder to avoid undefined reference if you paste into a fresh project. */
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* Keep or remove depending on your CubeMX config.
     Safe to leave enabled even if no IRQ used. */
}

/* USER CODE BEGIN 4 */
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
/* USER CODE END 4 */
