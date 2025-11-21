/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : VS10xx (VS1003/VS1053) + SD/FatFS MP3 streaming
  *                   Blue USER button (PA0 / EXTI0) skips to next track.
  *                   Falls back to embedded MP3 sample if SD open/mount fails.
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
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "MP3Sample.h"    // provides: sampleMp3[], sampleMp3_len (fallback)
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
#define SM_SDINEW          (1<<11)

/* Pin macros */
#define VS_XCS_L()        HAL_GPIO_WritePin(VS_XCS_PORT,  VS_XCS_PIN,  GPIO_PIN_RESET)
#define VS_XCS_H()        HAL_GPIO_WritePin(VS_XCS_PORT,  VS_XCS_PIN,  GPIO_PIN_SET)
#define VS_XDCS_L()       HAL_GPIO_WritePin(VS_XDCS_PORT, VS_XDCS_PIN, GPIO_PIN_RESET)
#define VS_XDCS_H()       HAL_GPIO_WritePin(VS_XDCS_PORT, VS_XDCS_PIN, GPIO_PIN_SET)
#define VS_RST_L()        HAL_GPIO_WritePin(VS_RST_PORT,  VS_RST_PIN,  GPIO_PIN_RESET)
#define VS_RST_H()        HAL_GPIO_WritePin(VS_RST_PORT,  VS_RST_PIN,  GPIO_PIN_SET)
#define VS_DREQ_IS_H()   (HAL_GPIO_ReadPin(VS_DREQ_PORT, VS_DREQ_PIN) == GPIO_PIN_SET)

/* --- SD (SPI2) chip-select on PB12 --- */
#define SD_CS_PORT   GPIOB
#define SD_CS_PIN    GPIO_PIN_12
#define SD_CS_LOW()  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET)
#define SD_CS_HIGH() HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET)

/* SD commands for quick check (SPI mode) */
#define CMD0    (0x40+0)
#define CMD8    (0x40+8)
#define CMD55   (0x40+55)
#define CMD58   (0x40+58)
#define ACMD41  (0xC0+41)

/* Streaming params */
#define MP3_CHUNK   32u
#define BUF_SIZE    2048u

/* USER button (blue, PA0 / EXTI0) */
#define BTN_PORT    GPIOA
#define BTN_PIN     GPIO_PIN_0   // USER/WKUP button on F4-Discovery (active HIGH)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern SPI_HandleTypeDef hspi1;   // VS10xx (SPI1)
extern SPI_HandleTypeDef hspi2;   // SD (SPI2)
extern UART_HandleTypeDef huart2;

static uint8_t s_buf[2][BUF_SIZE];   // double buffer for SD reads

/* Playlist (files) */
static const char* kTracks[] = {
  "/music/track1.mp3",
  "/music/track2.mp3",
  "/music/track3.mp3",
};
static const char* kTitles[] = {
  "Western Road — Drake",
  "Starboy — The Weeknd",
  "Stronger — Kanye West",
};
static const uint32_t kNumTracks = sizeof(kTracks)/sizeof(kTracks[0]);

/* Skip flag + debounce */
static volatile uint8_t  g_next_requested = 0;
static volatile uint32_t g_btn_last_ms    = 0;
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
static void VS_InitSimple(void);
static uint8_t spi2_xchg(uint8_t b);
static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg);
static void   SD_SPI2_Check(void);
static int    PlayFile(const char *path);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void uprintln(const char *fmt, ...)
{
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  HAL_UART_Transmit(&huart2, (uint8_t*)buf, (uint16_t)strlen(buf), HAL_MAX_DELAY);
  const uint8_t crlf[2] = {'\r','\n'};
  HAL_UART_Transmit(&huart2, (uint8_t*)crlf, 2, HAL_MAX_DELAY);
}

/* ===== VS10xx inline driver bits ===== */
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
    if (HAL_GetTick() - t0 > 300) { uprintln("ERR: DREQ timeout"); break; }
  }
}

/* send ≤32 bytes when DREQ is high */
static void VS_SDI_SendChunk(const uint8_t *p, uint16_t n)
{
  while (n) {
    while (!VS_DREQ_IS_H());
    uint16_t chunk = (n > MP3_CHUNK) ? MP3_CHUNK : n;
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
    uint16_t chunk = (len - i > 512) ? 512 : (uint16_t)(len - i);
    VS_SDI_SendChunk(&buf[i], chunk);
    i += chunk;
  }
}

/* a small end-fill to flush decoder */
static void VS_SendZeros(uint16_t n)
{
  uint8_t z[32] = {0};
  while (n) {
    uint16_t c = (n > sizeof(z)) ? sizeof(z) : n;
    VS_SDI_SendChunk(z, c);
    n -= c;
  }
}

/* Minimal VS init */
static void VS_InitSimple(void)
{
  VS_HardReset();
  VS_SCI_Write(SCI_MODE, SM_SDINEW);
  HAL_Delay(2);

  uint16_t mode=0, status=0, clockf=0;
  VS_SCI_Read(SCI_MODE,   &mode);
  VS_SCI_Read(SCI_STATUS, &status);
  VS_SCI_Read(SCI_CLOCKF, &clockf);
  uprintln("SCI_MODE  = 0x%04X", mode);
  uprintln("SCI_STATUS= 0x%04X", status);
  uprintln("SCI_CLOCKF= 0x%04X", clockf);

  uprintln("Set CLOCKF & VOL...");
  VS_SCI_Write(SCI_CLOCKF, 0x8800);
  VS_SCI_Write(SCI_VOL,     0x2020);
  HAL_Delay(20);
}

/* ===== SD quick-check helpers (SPI mode) ===== */
static uint8_t spi2_xchg(uint8_t b)
{
  uint8_t rx;
  HAL_SPI_TransmitReceive(&hspi2, &b, &rx, 1, HAL_MAX_DELAY);
  return rx;
}

static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg)
{
  uint8_t res, pkt[6];
  if (cmd & 0x80) {
    cmd &= 0x7F;
    res = sd_send_cmd(CMD55, 0);
    if (res > 1) return res;
  }

  SD_CS_HIGH(); spi2_xchg(0xFF);
  SD_CS_LOW();  spi2_xchg(0xFF);

  pkt[0] = cmd;
  pkt[1] = (uint8_t)(arg >> 24);
  pkt[2] = (uint8_t)(arg >> 16);
  pkt[3] = (uint8_t)(arg >> 8);
  pkt[4] = (uint8_t)(arg);
  pkt[5] = (cmd == CMD0) ? 0x95 : (cmd == CMD8 ? 0x87 : 0x01);

  HAL_SPI_Transmit(&hspi2, pkt, 6, HAL_MAX_DELAY);
  for (int i = 0; i < 10; i++) {
    res = spi2_xchg(0xFF);
    if (!(res & 0x80)) return res;
  }
  return 0xFF;
}

static void SD_SPI2_Check(void)
{
  SD_CS_HIGH();
  for (int i = 0; i < 10; i++) spi2_xchg(0xFF);

  uint8_t r1 = sd_send_cmd(CMD0, 0);
  uint8_t r7[5] = {0}, ver = 0;
  if (r1 == 0x01) {
    r1 = sd_send_cmd(CMD8, 0x1AA);
    r7[0] = r1;
    for (int i = 1; i < 5; i++) r7[i] = spi2_xchg(0xFF);
    if (r7[0] == 0x01 && r7[3] == 0x01 && r7[4] == 0xAA) ver = 2;
  }

  uint32_t t0 = HAL_GetTick();
  do {
    r1 = sd_send_cmd(ACMD41, ver ? (1UL << 30) : 0);
    if (r1 == 0x00) break;
  } while ((HAL_GetTick() - t0) < 1500);

  (void)sd_send_cmd(CMD58, 0);
  uint8_t ocr[4];
  for (int i = 0; i < 4; i++) ocr[i] = spi2_xchg(0xFF);
  SD_CS_HIGH(); spi2_xchg(0xFF);

  uint32_t ocr32 = ((uint32_t)ocr[0] << 24) | ((uint32_t)ocr[1] << 16) |
                   ((uint32_t)ocr[2] << 8) | ocr[3];
  int ccs = (ocr[0] & 0x40) ? 1 : 0;

  uprintln("=== SPI2 microSD Quick Check ===");
  uprintln("SD_VERSION=%s  ACMD41=%s  OCR=0x%08lX  CCS=%d",
           ver ? "V2" : "V1/Unknown",
           (r1 == 0x00) ? "OK" : "FAIL",
           (unsigned long)ocr32, ccs);
}

/* ===== Player: SD -> VS10xx (double buffer) ===== */
/* Return: 0 = normal EOF, 1 = aborted (skip) */
static int PlayFile(const char *path)
{
  FIL f; FRESULT fr; UINT br = 0;
  uint8_t a = 0;

  fr = f_open(&f, path, FA_READ);
  if (fr != FR_OK) {
    uprintln("ERR: f_open %s (%d)", path, fr);
    uprintln("Fallback: playing embedded sample.");
    VS_SendMusic(sampleMp3, sampleMp3_len);
    VS_SendZeros(2052);
    return 0;
  }
  uprintln("▶ %s", path);

  fr = f_read(&f, s_buf[a], BUF_SIZE, &br);
  if (fr != FR_OK || br == 0) { f_close(&f); uprintln("ERR: empty/rd"); return 0; }

  while (1) {
    if (g_next_requested) { uprintln("⏭ skip requested"); break; }

    uint8_t n = a ^ 1;
    UINT br_next = 0;
    FRESULT fr_next = f_read(&f, s_buf[n], BUF_SIZE, &br_next);

    uint32_t i = 0;
    while (i < br) {
      if (g_next_requested) { uprintln("⏭ abort during feed"); goto abort_now; }
      if (VS_DREQ_IS_H()) {
        uint32_t take = (br - i > MP3_CHUNK) ? MP3_CHUNK : (br - i);
        VS_SDI_SendChunk(&s_buf[a][i], (uint16_t)take);
        i += take;
      }
    }

    if (fr_next != FR_OK || br_next == 0) break;
    a = n; br = br_next;
  }

abort_now:
  f_close(&f);
  VS_SendZeros(2048);

  if (g_next_requested) {
    g_next_requested = 0;
    return 1;   // skipped
  }
  uprintln("✅ Done");
  return 0;     // normal EOF
}

/* === EXTI callback for blue button (PA0) === */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BTN_PIN) {
    uint32_t now = HAL_GetTick();
    if ((now - g_btn_last_ms) > 200) {   // debounce
      g_btn_last_ms = now;
      g_next_requested = 1;
      uprintln("Button: NEXT");
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
  HAL_Init();
  SystemClock_Config();

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
  MX_SPI1_Init();        // VS10xx on SPI1
  MX_SPI2_Init();        // SD card on SPI2
  MX_FATFS_Init();
  MX_NVIC_Init();

  uprintln("VS1003 + SD/FatFS MP3 streaming starting...");
  SD_CS_HIGH();
  SD_SPI2_Check();

  FRESULT res = f_mount(&USERFatFS, USERPath, 1);
  if (res != FR_OK) {
    uprintln("f_mount FAILED (%d) -> fallback to embedded sample", res);
    VS_InitSimple();
    uprintln("♪ Now Playing: Embedded Sample");
    VS_SendMusic(sampleMp3, sampleMp3_len);
    VS_SendZeros(2052);
    while (1) HAL_Delay(1000);
  }
  uprintln("f_mount OK (%d)", res);

  VS_InitSimple();
  SD_CS_HIGH();

  uint32_t idx = 0;
  while (1) {
    uprintln("♪ Now Playing: %s", kTitles[idx]);
    (void)PlayFile(kTracks[idx]);
    idx = (idx + 1) % kNumTracks;
    HAL_Delay(50);
  }
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  HAL_NVIC_SetPriority(TIM7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM7_IRQn);

  HAL_NVIC_SetPriority(EXTI0_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief  Error Handler
  */
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
#endif /* USE_FULL_ASSERT */
