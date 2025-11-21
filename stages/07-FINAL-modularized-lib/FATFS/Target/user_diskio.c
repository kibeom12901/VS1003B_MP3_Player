/* FATFS/Target/user_diskio.c — SPI2 microSD bridge (PB12..PB15) */
#include "main.h"
#include "spi.h"
#include "gpio.h"
#include "ff_gen_drv.h"
#include "diskio.h"

/* ---- Pins (edit if different) ---- */
#define SD_CS_PORT GPIOB
#define SD_CS_PIN  GPIO_PIN_12
#define CS_LOW()   HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET)

extern SPI_HandleTypeDef hspi2;

/* ---- SD commands ---- */
#define CMD0    (0x40+0)
#define CMD8    (0x40+8)
#define CMD17   (0x40+17)
#define CMD24   (0x40+24)
#define CMD55   (0x40+55)
#define CMD58   (0x40+58)
#define ACMD41  (0xC0+41)

/* ---- Driver state ---- */
static volatile DSTATUS Stat = STA_NOINIT;
static uint8_t sd_v2 = 0, sd_ccs = 0;   /* CCS=1 => SDHC/SDXC (block addr) */

static uint8_t spi_xchg(uint8_t d){ uint8_t r; HAL_SPI_TransmitReceive(&hspi2,&d,&r,1,HAL_MAX_DELAY); return r; }
static void    spi_send(const uint8_t* p, uint16_t n){ HAL_SPI_Transmit(&hspi2,(uint8_t*)p,n,HAL_MAX_DELAY); }
static void    spi_recv(uint8_t* p, uint16_t n){ while(n--) *p++ = spi_xchg(0xFF); }

static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg){
  uint8_t res, buf[6];
  if(cmd & 0x80){ cmd &= 0x7F; res = sd_send_cmd(CMD55,0); if(res>1) return res; }
  CS_HIGH(); spi_xchg(0xFF);   /* idle */
  CS_LOW();  spi_xchg(0xFF);
  buf[0]=cmd; buf[1]=arg>>24; buf[2]=arg>>16; buf[3]=arg>>8; buf[4]=arg;
  buf[5]=(cmd==CMD0)?0x95: (cmd==CMD8?0x87:0x01);
  spi_send(buf,6);
  for(int i=0;i<10;i++){ res = spi_xchg(0xFF); if(!(res&0x80)) return res; }
  return 0xFF;
}
static int wait_ready(uint32_t ms){
  uint32_t t0=HAL_GetTick(); do{ if(spi_xchg(0xFF)==0xFF) return 1; }while(HAL_GetTick()-t0<ms); return 0;
}
static int recv_data(uint8_t* buff, uint32_t ms){
  uint8_t t; uint32_t t0=HAL_GetTick();
  do{ t=spi_xchg(0xFF); if(t==0xFE) break; }while(HAL_GetTick()-t0<ms);
  if(t!=0xFE) return 0; spi_recv(buff,512); spi_xchg(0xFF); spi_xchg(0xFF); return 1;
}
static int send_data(const uint8_t* buff){
  if(!wait_ready(500)) return 0;
  spi_xchg(0xFE); spi_send(buff,512); spi_xchg(0xFF); spi_xchg(0xFF);
  if((spi_xchg(0xFF)&0x1F)!=0x05) return 0; return 1;
}

/* ---------------- FatFS hooks ---------------- */

DSTATUS USER_initialize (BYTE pdrv){
  if(pdrv) return STA_NOINIT;

  CS_HIGH(); for(int i=0;i<10;i++) spi_xchg(0xFF);      /* 80 clocks */

  uint8_t r1 = sd_send_cmd(CMD0,0);
  if(r1==0x01){
    uint8_t r7[5]={0};
    r1 = sd_send_cmd(CMD8,0x1AA);
    r7[0]=r1; for(int i=1;i<5;i++) r7[i]=spi_xchg(0xFF);
    if(r7[0]==0x01 && r7[3]==0x01 && r7[4]==0xAA) sd_v2=1;
  }

  uint32_t t0=HAL_GetTick();
  do{
    r1 = sd_send_cmd(ACMD41, sd_v2 ? (1UL<<30) : 0);
    if(r1==0x00) break;
  }while(HAL_GetTick()-t0<1500);

  r1 = sd_send_cmd(CMD58,0);
  uint8_t ocr[4]; for(int i=0;i<4;i++) ocr[i]=spi_xchg(0xFF);
  CS_HIGH(); spi_xchg(0xFF);

  if(r1==0x00){ sd_ccs = (ocr[0]&0x40)?1:0; Stat &= ~STA_NOINIT; }
  else        { Stat = STA_NOINIT; }

  return Stat;
}

DSTATUS USER_status (BYTE pdrv){ return (pdrv? STA_NOINIT : Stat); }

DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count){
  if(pdrv || (Stat&STA_NOINIT)) return RES_NOTRDY;
  if(!count) return RES_PARERR;
  if(!sd_ccs) sector*=512;

  for(UINT i=0;i<count;i++){
    if(sd_send_cmd(CMD17, sector)!=0 || !recv_data(buff,200)){ CS_HIGH(); spi_xchg(0xFF); return RES_ERROR; }
    CS_HIGH(); spi_xchg(0xFF);
    buff+=512; sector += sd_ccs?1:512;
  }
  return RES_OK;
}

#if FF_FS_READONLY == 0
DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count){
  if(pdrv || (Stat&STA_NOINIT)) return RES_NOTRDY;
  if(!count) return RES_PARERR;
  if(!sd_ccs) sector*=512;

  for(UINT i=0;i<count;i++){
    if(sd_send_cmd(CMD24, sector)!=0 || !send_data(buff)){ CS_HIGH(); spi_xchg(0xFF); return RES_ERROR; }
    if(!wait_ready(500)){ CS_HIGH(); spi_xchg(0xFF); return RES_ERROR; }
    CS_HIGH(); spi_xchg(0xFF);
    buff+=512; sector += sd_ccs?1:512;
  }
  return RES_OK;
}
#endif

DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff){
  if(pdrv) return RES_PARERR;
  if(Stat & STA_NOINIT) return RES_NOTRDY;
  switch(cmd){
    case CTRL_SYNC:        return wait_ready(500)?RES_OK:RES_ERROR;
    case GET_SECTOR_SIZE:  *(WORD*)buff = 512;  return RES_OK;
    case GET_BLOCK_SIZE:   *(DWORD*)buff = 1;   return RES_OK;
    case GET_SECTOR_COUNT: return RES_PARERR;   /* not provided */
    default:               return RES_PARERR;
  }
}

/* Exported driver object */
const Diskio_drvTypeDef USER_Driver = {
  USER_initialize,
  USER_status,
  USER_read,
#if FF_FS_READONLY == 0
  USER_write,
#endif
  USER_ioctl
};
