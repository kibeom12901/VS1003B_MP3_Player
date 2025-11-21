/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : VS10xx (VS1003/VS1053) + SD/FatFS MP3 streaming
 *                   - Scans /music for *.mp3 at boot (LFN-aware if enabled)
 *                   - Shows Title — Artist via ID3v1 if present
 *                   - Button gestures on PA0:
 *                       * Single click: Next
 *                       * Double click: Previous
 *                       * Long press:  Toggle Shuffle (and reshuffle)
 *                   - Live volume via ADC1_IN1 (PA1) potentiometer
 *                     (clockwise = louder; writes VS10xx SCI_VOL)
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
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
#include <ctype.h>
#include <stdlib.h>
#include "ff.h"
#include "MP3Sample.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  char path[260];   // /music/filename.mp3 (allow LFN)
  char title[64];   // ID3v1 (optional)
  char artist[64];  // ID3v1 (optional)
} Track;

typedef struct { char title[31], artist[31]; } ID3v1;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define USE_SM_CANCEL   0

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
#define SCI_HDAT0     0x08
#define SCI_HDAT1     0x09

/* SCI opcodes */
#define SCI_WRITE_OP  0x02
#define SCI_READ_OP   0x03

/* VS10xx MODE bits */
#define SM_SDINEW          (1<<11)
#define SM_CANCEL          (1<<3)

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

/* SD commands (SPI mode) */
#define CMD0    (0x40+0)
#define CMD8    (0x40+8)
#define CMD55   (0x40+55)
#define CMD58   (0x40+58)
#define ACMD41  (0xC0+41)

/* Streaming params */
#define MP3_CHUNK   32u
#define BUF_SIZE    2048u

/* USER button (PA0 / EXTI0) — Discovery: pressed = HIGH */
#define BTN_PORT    GPIOA
#define BTN_PIN     GPIO_PIN_0

/* Playlist capacity */
#define MAX_TRACKS  128

/* Gesture timing */
#define BTN_LONG_MS     700u
#define BTN_DOUBLE_MS   350u

/* Volume control (ADC sampling) */
#define VOL_TASK_PERIOD_MS  80u      // update rate
#define VOL_STEP_HYST       2        // write SCI only if attenuation changed ≥2
#define VOL_MIN_ATT         0        // 0   = loudest   (0 dB)
#define VOL_MAX_ATT         254      // 254 = quiet/mute (~-127 dB)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern SPI_HandleTypeDef hspi1;      // VS10xx (SPI1)
extern SPI_HandleTypeDef hspi2;      // SD (SPI2)
extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef hadc1;

static uint8_t  s_buf[2][BUF_SIZE];  // double buffer
static Track    g_tracks[MAX_TRACKS];
static uint32_t g_track_count = 0;

/* Player control flags */
static volatile uint8_t  g_next_requested = 0;
static volatile uint8_t  g_prev_requested = 0;
static volatile uint8_t  g_shuffle_enabled = 0;

/* Button gesture state (polled) */
static uint8_t  g_btn_prev    = 0;     // idle low
static uint32_t g_btn_press_t = 0;
static uint32_t g_btn_last_up = 0;
static uint8_t  g_btn_clicks  = 0;
static uint8_t  g_btn_long_fired = 0;

/* Volume control state */
static int      g_vol_att      = 0x20;   // current attenuation (0..254), init ~ OK
static int      g_vol_att_last = -999;   // last written to VS10xx
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */
static void uprintln(const char *fmt, ...);

/* VS10xx helpers */
static HAL_StatusTypeDef VS_SCI_Write(uint8_t reg, uint16_t val);
static HAL_StatusTypeDef VS_SCI_Read(uint8_t reg, uint16_t *out);
static void VS_HardReset(void);
static void VS_SDI_SendChunk(const uint8_t *p, uint16_t n);
static void VS_SendMusic(const uint8_t *buf, uint32_t len);
static void VS_SendZeros(uint16_t n);
static void VS_InitSimple(void);
static void VS_SetVolume(uint8_t att_left, uint8_t att_right);
#if USE_SM_CANCEL
static void VS_Cancel(void);
#endif

/* SD quick check (SPI mode) */
static uint8_t spi2_xchg(uint8_t b);
static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg);
static void   SD_SPI2_Check(void);

/* Player */
static int    PlayTrack(Track* t);

/* Playlist helpers */
static int    ends_with_ci(const char* s, const char* suf);
static int    ci_strcmp(const char* a, const char* b);
static int    cmp_tracks(const void* A, const void* B);
static void   BuildPlaylist(const char* dir);

/* Shuffle helpers */
static void   ShuffleTracks(void);
static uint32_t xorshift32(void);

/* ID3v1 */
static void   rtrim(char* s);
static int    ReadID3v1(FIL* f, ID3v1* out);

/* Button */
static void   ButtonTask(void);

/* Volume via ADC */
static int    ReadADC1_IN1_Filtered(void);
static void   VolumeTask(void);

/* FatFS mount retry */
static int    TryMount(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void uprintln(const char *fmt, ...)
{
  char buf[200];
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

/* small end-fill to flush decoder */
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
  VS_SCI_Write(SCI_CLOCKF, 0x8800);       // x3.0 multiplier typical
  VS_SetVolume(0x20, 0x20);               // start moderate (~-10 dB each)
  HAL_Delay(20);
}

/* VS10xx volume helper (attenuation per channel) */
static void VS_SetVolume(uint8_t att_left, uint8_t att_right)
{
  uint16_t v = ((uint16_t)att_left << 8) | att_right;
  VS_SCI_Write(SCI_VOL, v);
}

#if USE_SM_CANCEL
/* Optional: smooth cancel to avoid pops on skip */
static void VS_Cancel(void)
{
  uint16_t mode;
  VS_SCI_Read(SCI_MODE, &mode);
  VS_SCI_Write(SCI_MODE, mode | SM_CANCEL);
  uint32_t t0 = HAL_GetTick();
  for (;;) {
    uint16_t h0=1, h1=1;
    static const uint8_t zero8[8] = {0};
    VS_SDI_SendChunk(zero8, sizeof(zero8));
    VS_SCI_Read(SCI_HDAT0, &h0);
    VS_SCI_Read(SCI_HDAT1, &h1);
    if (h0 == 0 && h1 == 0) break;
    if (HAL_GetTick() - t0 > 800) { uprintln("SM_CANCEL timeout"); break; }
  }
  VS_SendZeros(2052);  // end-fill
  VS_SCI_Read(SCI_MODE, &mode);
  VS_SCI_Write(SCI_MODE, mode & ~SM_CANCEL);
}
#endif

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

/* ===== ID3v1 helpers ===== */
static void rtrim(char* s){
  size_t n = strlen(s);
  while (n && (s[n-1] == ' ' || s[n-1] == '\0')) s[--n] = '\0';
}

static int ReadID3v1(FIL* f, ID3v1* out)
{
  if (f_size(f) < 128) return 0;
  FSIZE_t cur = f_tell(f);
  if (f_lseek(f, f_size(f) - 128) != FR_OK) return 0;
  UINT br=0; uint8_t tag[128];
  if (f_read(f, tag, 128, &br) != FR_OK || br != 128) { (void)f_lseek(f, cur); return 0; }
  if (memcmp(tag, "TAG", 3) != 0)        { (void)f_lseek(f, cur); return 0; }
  memcpy(out->title,  tag + 3,  30); out->title[30]  = 0; rtrim(out->title);
  memcpy(out->artist, tag + 33, 30); out->artist[30] = 0; rtrim(out->artist);
  (void)f_lseek(f, cur);
  return 1;
}

/* ===== Shuffle helpers ===== */
static uint32_t g_rng = 0xC001BEEF;
static uint32_t xorshift32(void){
  uint32_t x = g_rng;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  g_rng = x ? x : 0x1234567u;
  return g_rng;
}
static void ShuffleTracks(void){
  if (g_track_count < 2) return;
  for (int i = (int)g_track_count - 1; i > 0; --i){
    int j = (int)(xorshift32() % (uint32_t)(i + 1));
    if (j != i){
      Track tmp = g_tracks[i];
      g_tracks[i] = g_tracks[j];
      g_tracks[j] = tmp;
    }
  }
}

/* ===== Playlist scanning ===== */
static int ends_with_ci(const char* s, const char* suf)
{
  size_t ls = strlen(s), lsf = strlen(suf);
  if (ls < lsf) return 0;
  for (size_t i = 0; i < lsf; ++i) {
    char a = s[ls - lsf + i], b = suf[i];
    if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
    if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
    if (a != b) return 0;
  }
  return 1;
}
static int ci_strcmp(const char* a, const char* b)
{
  while (*a || *b) {
    unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
    if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
    if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
    if (ca != cb) return (int)ca - (int)cb;
    if (*a) ++a;
    if (*b) ++b;
  }
  return 0;
}
static int cmp_tracks(const void* A, const void* B)
{
  const Track* a = (const Track*)A;
  const Track* b = (const Track*)B;
  return ci_strcmp(a->path, b->path);   // sort by path (alphabetical)
}
static void BuildPlaylist(const char* dir)
{
  DIR d; FILINFO fno;
  g_track_count = 0;
#if FF_USE_LFN
  static char lfn[256];
  fno.lfname = lfn;
  fno.lfsize = sizeof(lfn);
#endif
  if (f_opendir(&d, dir) != FR_OK) {
    uprintln("ERR: cannot open %s", dir);
    return;
  }
  for (;;) {
    FRESULT fr = f_readdir(&d, &fno);
    if (fr != FR_OK || fno.fname[0] == 0) break;
#if FF_USE_LFN
    const char* name = (fno.lfname && fno.lfname[0]) ? fno.lfname : fno.fname;
#else
    const char* name = fno.fname;
#endif
    if (fno.fattrib & AM_DIR) continue;
    if (!ends_with_ci(name, ".mp3")) continue;

    if (g_track_count < MAX_TRACKS) {
      snprintf(g_tracks[g_track_count].path,
               sizeof(g_tracks[g_track_count].path),
               "%s/%s", dir, name);
      g_tracks[g_track_count].title[0]  = 0;
      g_tracks[g_track_count].artist[0] = 0;
      g_track_count++;
    } else {
      uprintln("Playlist full (%d)", MAX_TRACKS);
      break;
    }
  }
  f_closedir(&d);
  if (g_track_count > 1) {
    qsort(g_tracks, g_track_count, sizeof(Track), cmp_tracks);
  }
  uprintln("Playlist built: %lu track(s)", (unsigned long)g_track_count);
}

/* ===== Volume (ADC) ===== */
static int ReadADC1_IN1_Filtered(void)
{
  static int filt = -1;
  HAL_ADC_Start(&hadc1);
  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
  int raw = (int)HAL_ADC_GetValue(&hadc1);   // 0..4095
  HAL_ADC_Stop(&hadc1);
  if (filt < 0) filt = raw;
  // simple IIR
  filt = (filt * 3 + raw) / 4;
  return filt;
}

/* Map pot (0..4095) -> attenuation (0..254), invert so clockwise = louder. */
static void VolumeTask(void)
{
  static uint32_t last_ms = 0;
  uint32_t now = HAL_GetTick();
  if ((now - last_ms) < VOL_TASK_PERIOD_MS) return;
  last_ms = now;

  int raw = ReadADC1_IN1_Filtered();      // 0..4095
  // percentage
  int pct = (raw * 100 + 2047) / 4095;    // 0..100
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;

  // attenuation: 0 (loudest) .. 254 (quietest), use full span
  int att = (int)((100 - pct) * VOL_MAX_ATT / 100); // 100% -> 0, 0% -> 254

  // write only on meaningful change to reduce I2S/SCI traffic
  if (g_vol_att_last < 0 || (att >= g_vol_att_last + VOL_STEP_HYST) || (att <= g_vol_att_last - VOL_STEP_HYST)) {
    g_vol_att = att;
    VS_SetVolume((uint8_t)g_vol_att, (uint8_t)g_vol_att);
    g_vol_att_last = g_vol_att;
    // Optional: print occasionally
    static uint8_t print_div = 0;
    if ((print_div++ & 0x07) == 0) {
      uprintln("VOL: pot=%4d  ~%3d%%  att=%d (0=loud)", raw, pct, g_vol_att);
    }
  }
}

/* ===== Player: SD -> VS10xx (double buffer) ===== */
/* Return: 0 = normal EOF, 1 = aborted (skip/prev) */
static int PlayTrack(Track* t)
{
  FIL f; FRESULT fr; UINT br = 0;
  uint8_t a = 0;

  fr = f_open(&f, t->path, FA_READ);
  if (fr != FR_OK) {
    uprintln("ERR: f_open %s (%d)", t->path, fr);
    uprintln("Fallback: playing embedded sample.");
    VS_SendMusic(sampleMp3, sampleMp3_len);
    VS_SendZeros(2052);
    return 0;
  }

  /* ID3v1 (if present) */
  ID3v1 id = {0};
  if (ReadID3v1(&f, &id)) {
    if (id.title[0])  strncpy(t->title,  id.title,  sizeof(t->title));
    if (id.artist[0]) strncpy(t->artist, id.artist, sizeof(t->artist));
  }
  (void)f_lseek(&f, 0);

  if (t->title[0] || t->artist[0]) {
    uprintln("♪ %s — %s",
             t->title[0]  ? t->title  : "(no title)",
             t->artist[0] ? t->artist : "(no artist)");
  }
  uprintln("▶ %s", t->path);

  fr = f_read(&f, s_buf[a], BUF_SIZE, &br);
  if (fr != FR_OK || br == 0) { f_close(&f); uprintln("ERR: empty/rd"); return 0; }

  while (1) {
    ButtonTask();         // gestures while streaming
    VolumeTask();         // live volume from ADC
    if (g_next_requested || g_prev_requested)
      { uprintln("⏭/⏮ request"); break; }

    uint8_t n = a ^ 1;
    UINT br_next = 0;
    FRESULT fr_next = f_read(&f, s_buf[n], BUF_SIZE, &br_next);

    uint32_t i = 0;
    while (i < br) {
      ButtonTask();
      VolumeTask();
      if (g_next_requested || g_prev_requested)
        { uprintln("⏭/⏮ abort during feed"); goto abort_now; }
      if (VS_DREQ_IS_H()) {
        uint32_t take = (br - i > MP3_CHUNK) ? MP3_CHUNK : (br - i);
        VS_SDI_SendChunk(&s_buf[a][i], (uint16_t)take);
        i += take;
      }
    }
    if (fr_next != FR_OK || br_next == 0) break;  // EOF
    a = n; br = br_next;
  }

abort_now:
  f_close(&f);
  if (g_next_requested || g_prev_requested) {
#if USE_SM_CANCEL
    VS_Cancel();
#else
    VS_SendZeros(2048);
#endif
    return 1;
  }
  VS_SendZeros(2048);     // normal EOF tail
  uprintln("✅ Done");
  return 0;
}

/* ===== Button gestures (polled) ===== */
static void ButtonTask(void){
  uint8_t now = (HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) == GPIO_PIN_SET);
  uint32_t t   = HAL_GetTick();

  if (!g_btn_prev && now){
    g_btn_press_t = t;
    g_btn_long_fired = 0;
    if (t - g_btn_last_up <= BTN_DOUBLE_MS) g_btn_clicks = 2;
    else                                    g_btn_clicks = 1;
  }

  if (now && !g_btn_long_fired) {
    if (t - g_btn_press_t >= BTN_LONG_MS) {
      g_btn_long_fired = 1;
      g_shuffle_enabled ^= 1;
      uprintln("Shuffle %s", g_shuffle_enabled ? "ON" : "OFF");
      if (g_shuffle_enabled) ShuffleTracks();
      g_btn_clicks = 0;   // long press wins
    }
  }

  if (g_btn_prev && !now){
    g_btn_last_up = t;
    if (!g_btn_long_fired){
      if (g_btn_clicks == 2){
        g_prev_requested = 1;   // double -> previous
        g_btn_clicks = 0;
      }
    }
  }

  if (g_btn_clicks == 1 && (t - g_btn_last_up) > BTN_DOUBLE_MS && !now){
    g_next_requested = 1;       // single -> next
    g_btn_clicks = 0;
  }

  g_btn_prev = now;
}

/* EXTI callback: kept empty (we poll) */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  (void)GPIO_Pin;
}

/* --- SD/FatFS mount retry helper ----------------------------------------- */
static int TryMount(void)
{
  for (int i = 0; i < 5; i++) {
    FRESULT r = f_mount(&USERFatFS, USERPath, 1);
    if (r == FR_OK) return 1;
    uprintln("f_mount retry %d (%d)", i + 1, r);
    HAL_Delay(500);
  }
  return 0;
}
/* USER CODE END 0 */

/* ------------------------------------------------------------------------- */
/*                                   main                                    */
/* ------------------------------------------------------------------------- */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_TIM7_Init();
  MX_TIM4_Init();
  MX_TIM10_Init();
  MX_TIM2_Init();
  MX_TIM5_Init();
  MX_ADC1_Init();           // <-- PA1 pot
  MX_TIM3_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();           // VS10xx on SPI1
  MX_SPI2_Init();           // SD card on SPI2
  MX_FATFS_Init();
  MX_NVIC_Init();

  /* Seed PRNG for shuffle (include tick) */
  g_rng ^= HAL_GetTick();

  uprintln("VS1003/VS1053 + SD/FatFS MP3 streaming starting...");
  SD_CS_HIGH();
  SD_SPI2_Check();

  if (!TryMount()) {
    uprintln("f_mount FAILED -> fallback to embedded sample");
    VS_InitSimple();
    // also set initial volume from pot once
    VolumeTask();
    uprintln("♪ Now Playing: Embedded Sample");
    VS_SendMusic(sampleMp3, sampleMp3_len);
    VS_SendZeros(2052);
    while (1) { VolumeTask(); HAL_Delay(50); }
  }

  uprintln("f_mount OK");
  VS_InitSimple();
  // set initial volume from pot right after init
  for (int i=0;i<6;i++){ VolumeTask(); HAL_Delay(30); }
  SD_CS_HIGH();

  /* Scan /music */
  BuildPlaylist("/music");
  if (g_track_count == 0) {
    uprintln("No MP3s in /music -> playing embedded sample");
    uprintln("Tip: enable LFN in ffconf.h (FF_USE_LFN = 1 or 3).");
    VS_SendMusic(sampleMp3, sampleMp3_len);
    VS_SendZeros(2052);
    while (1) { VolumeTask(); HAL_Delay(100); }
  }

  uint32_t idx = 0;
  while (1) {
    if (g_tracks[idx].title[0] || g_tracks[idx].artist[0]) {
      uprintln("♪ Now Playing: %s — %s",
               g_tracks[idx].title[0]  ? g_tracks[idx].title  : "(no title)",
               g_tracks[idx].artist[0] ? g_tracks[idx].artist : "(no artist)");
    } else {
      uprintln("♪ Now Playing: %s", g_tracks[idx].path);
    }

    (void)PlayTrack(&g_tracks[idx]);

    if (g_prev_requested) {
      g_prev_requested = 0;
      if (idx == 0) idx = g_track_count - 1;
      else          idx = (idx - 1);
    } else {
      idx = (idx + 1) % g_track_count;  // default next
      g_next_requested = 0;
    }
    HAL_Delay(20);
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
