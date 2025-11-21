#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart2;

#define SD_CS_PORT GPIOB
#define SD_CS_PIN  GPIO_PIN_12
#define SD_CS_LOW()  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET)
#define SD_CS_HIGH() HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET)

#define CMD0    (0x40+0)
#define CMD8    (0x40+8)
#define CMD55   (0x40+55)
#define CMD58   (0x40+58)
#define ACMD41  (0xC0+41)

static void uprintln(const char *fmt, ...) {
  char buf[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
  const char crlf[2] = {'\r','\n'};
  HAL_UART_Transmit(&huart2, (uint8_t*)crlf, 2, HAL_MAX_DELAY);
}

static uint8_t spi_xchg(uint8_t b) {
  uint8_t rx;
  HAL_SPI_TransmitReceive(&hspi2, &b, &rx, 1, HAL_MAX_DELAY);
  return rx;
}

static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg) {
  uint8_t res, buf[6];
  if (cmd & 0x80) { cmd &= 0x7F; res = sd_send_cmd(CMD55, 0); if (res > 1) return res; }
  SD_CS_HIGH(); spi_xchg(0xFF);
  SD_CS_LOW(); spi_xchg(0xFF);
  buf[0] = cmd;
  buf[1] = (uint8_t)(arg >> 24);
  buf[2] = (uint8_t)(arg >> 16);
  buf[3] = (uint8_t)(arg >> 8);
  buf[4] = (uint8_t)(arg);
  buf[5] = (cmd == CMD0) ? 0x95 : (cmd == CMD8 ? 0x87 : 0x01);
  HAL_SPI_Transmit(&hspi2, buf, 6, HAL_MAX_DELAY);
  for (int i = 0; i < 10; i++) {
    res = spi_xchg(0xFF);
    if (!(res & 0x80)) return res;
  }
  return 0xFF;
}

void SD_SPI2_Check(void) {
  SD_CS_HIGH();
  for (int i = 0; i < 10; i++) spi_xchg(0xFF); // 80 dummy clocks

  uint8_t r1 = sd_send_cmd(CMD0, 0);
  uint8_t r7[5] = {0};
  uint8_t ver = 0;
  if (r1 == 0x01) {
    r1 = sd_send_cmd(CMD8, 0x1AA);
    r7[0] = r1; for (int i = 1; i < 5; i++) r7[i] = spi_xchg(0xFF);
    if (r7[0] == 0x01 && r7[3] == 0x01 && r7[4] == 0xAA) ver = 2;
  }
  uint32_t t0 = HAL_GetTick();
  do {
    r1 = sd_send_cmd(ACMD41, ver ? 1UL << 30 : 0);
    if (r1 == 0x00) break;
  } while (HAL_GetTick() - t0 < 1500);

  r1 = sd_send_cmd(CMD58, 0);
  uint8_t ocr[4];
  for (int i = 0; i < 4; i++) ocr[i] = spi_xchg(0xFF);
  SD_CS_HIGH(); spi_xchg(0xFF);

  uint32_t ocr32 = ((uint32_t)ocr[0] << 24) | ((uint32_t)ocr[1] << 16) |
                   ((uint32_t)ocr[2] << 8) | ocr[3];
  int ccs = (ocr[0] & 0x40) ? 1 : 0;

  uprintln("SD_VERSION=%s  ACMD41=%s  OCR=0x%08lX  CCS=%d",
           ver ? "V2" : "V1/Unknown",
           (r1 == 0x00) ? "OK" : "FAIL",
           (unsigned long)ocr32, ccs);
}

int main(void) {
  HAL_Init();
  SystemClock_Config();   // keep your CubeMX version
  MX_GPIO_Init();
  MX_SPI2_Init();
  MX_USART2_UART_Init();

  SD_CS_HIGH();
  uprintln("=== SPI2 microSD Quick Check ===");
  SD_SPI2_Check();

  while (1) HAL_Delay(1000);
}

void SystemClock_Config(void) {}  // Use CubeMX version in real project
void Error_Handler(void) { while (1); }
