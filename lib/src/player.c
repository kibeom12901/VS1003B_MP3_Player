#include "player.h"
#include "vs1003.h"
#include "playlist.h"
#include "volume.h"
#include "button.h"
#include "util_uart.h"
#include "fatfs.h"
#include "spi.h"
#include "MP3Sample.h"

extern SPI_HandleTypeDef hspi2;

/* Chip select macros for SD */
#define SD_CS_PORT   GPIOB
#define SD_CS_PIN    GPIO_PIN_12
#define SD_CS_LOW()  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET)
#define SD_CS_HIGH() HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET)

/* SPI exchange helper */
static uint8_t spi2_xchg(uint8_t b)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi2, &b, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

/* CMD constants */
#define CMD0    (0x40+0)
#define CMD8    (0x40+8)
#define CMD55   (0x40+55)
#define CMD58   (0x40+58)
#define ACMD41  (0xC0+41)

/* Low-level SD command */
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
    pkt[0]=cmd;
    pkt[1]=(uint8_t)(arg>>24);
    pkt[2]=(uint8_t)(arg>>16);
    pkt[3]=(uint8_t)(arg>>8);
    pkt[4]=(uint8_t)(arg);
    pkt[5]=(cmd==CMD0)?0x95:(cmd==CMD8?0x87:0x01);
    HAL_SPI_Transmit(&hspi2,pkt,6,HAL_MAX_DELAY);

    for(int i=0;i<10;i++){
        res=spi2_xchg(0xFF);
        if(!(res&0x80)) return res;
    }
    return 0xFF;
}

/* Quick check for SPI2 SD wiring */
void SD_SPI2_Check(void)
{
    SD_CS_HIGH();
    for (int i = 0; i < 10; i++) spi2_xchg(0xFF);
    uint8_t r1 = sd_send_cmd(CMD0, 0);
    uint8_t r7[5]={0}, ver=0;
    if (r1==0x01){
        r1=sd_send_cmd(CMD8,0x1AA);
        for(int i=0;i<5;i++) r7[i]=spi2_xchg(0xFF);
        if(r7[0]==0x01 && r7[3]==0x01 && r7[4]==0xAA) ver=2;
    }
    uint32_t t0=HAL_GetTick();
    do {
        r1=sd_send_cmd(ACMD41, ver ? (1UL<<30):0);
        if(r1==0x00) break;
    } while((HAL_GetTick()-t0)<1500);
    (void)sd_send_cmd(CMD58,0);
    uint8_t ocr[4];
    for(int i=0;i<4;i++) ocr[i]=spi2_xchg(0xFF);
    SD_CS_HIGH(); spi2_xchg(0xFF);
    uint32_t ocr32=((uint32_t)ocr[0]<<24)|((uint32_t)ocr[1]<<16)|
                    ((uint32_t)ocr[2]<<8)|ocr[3];
    int ccs=(ocr[0]&0x40)?1:0;
    uprintln("=== SPI2 microSD Quick Check ===");
    uprintln("SD_VERSION=%s  OCR=0x%08lX  CCS=%d", ver?"V2":"V1/Unknown",
             (unsigned long)ocr32, ccs);
}

/* Retry mount helper */
int TryMount(void)
{
    for(int i=0;i<5;i++){
        FRESULT r=f_mount(&USERFatFS,USERPath,1);
        if(r==FR_OK) return 1;
        uprintln("f_mount retry %d (%d)", i+1, r);
        HAL_Delay(500);
    }
    return 0;
}

/* ===== Player Core ===== */
static uint8_t s_buf[2][2048];

int PlayTrack(Track *t)
{
    FIL f;
    FRESULT fr;
    UINT br = 0;
    uint8_t a = 0;

    fr = f_open(&f, t->path, FA_READ);
    if (fr != FR_OK) {
        uprintln("ERR: open %s (%d)", t->path, fr);
        uprintln("Fallback to embedded sample");
        VS_SendMusic(sampleMp3, sampleMp3_len);
        VS_SendZeros(2052);
        return 0;
    }

    /* ID3v1 */
    ID3v1 id = {0};
    if (ReadID3v1(&f, &id)) {
        if (id.title[0])  strncpy(t->title,  id.title,  sizeof(t->title));
        if (id.artist[0]) strncpy(t->artist, id.artist, sizeof(t->artist));
    }
    (void)f_lseek(&f, 0);

    if (t->title[0] || t->artist[0])
        uprintln("♪ %s — %s",
                 t->title[0]?t->title:"(no title)",
                 t->artist[0]?t->artist:"(no artist)");
    uprintln("▶ %s", t->path);

    fr = f_read(&f, s_buf[a], sizeof(s_buf[a]), &br);
    if (fr != FR_OK || br == 0) {
        f_close(&f);
        uprintln("ERR: empty/rd");
        return 0;
    }

    while (1) {
        ButtonTask();
        VolumeTask();
        if (g_next_requested || g_prev_requested)
            break;

        uint8_t n = a ^ 1;
        UINT br_next = 0;
        FRESULT fr_next = f_read(&f, s_buf[n], sizeof(s_buf[n]), &br_next);

        uint32_t i = 0;
        while (i < br) {
            ButtonTask();
            VolumeTask();
            if (g_next_requested || g_prev_requested)
                goto abort_now;
            if (VS_DREQ_IS_H()) {
                uint32_t take = (br - i > 32) ? 32 : (br - i);
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
    uprintln("✅ Done");
    return 0;
}
