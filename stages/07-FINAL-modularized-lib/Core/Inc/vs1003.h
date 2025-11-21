#ifndef __VS1003_H
#define __VS1003_H
#ifdef __cplusplus
extern "C" {
#endif
#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ==== VS1003 Pin Map (match CubeMX) ==== */
#define VS_XCS_PORT   GPIOA
#define VS_XCS_PIN    GPIO_PIN_4     // SCI CS
#define VS_XDCS_PORT  GPIOE
#define VS_XDCS_PIN   GPIO_PIN_3     // SDI CS
#define VS_RST_PORT   GPIOE
#define VS_RST_PIN    GPIO_PIN_2     // Reset
#define VS_DREQ_PORT  GPIOB
#define VS_DREQ_PIN   GPIO_PIN_0     // Data Request (input)

/* ==== SCI register map ==== */
#define SCI_MODE      0x00
#define SCI_STATUS    0x01
#define SCI_CLOCKF    0x03
#define SCI_VOL       0x0B
#define SCI_HDAT0     0x08
#define SCI_HDAT1     0x09

/* ==== SPI opcodes ==== */
#define SCI_WRITE_OP  0x02
#define SCI_READ_OP   0x03

/* ==== MODE bits ==== */
#define SM_SDINEW     (1U<<11)
#define SM_CANCEL     (1U<<3)

/* ==== Stream parameters ==== */
#define MP3_CHUNK     32U

/* ==== DREQ helper ==== */
#define VS_DREQ_IS_H()  (HAL_GPIO_ReadPin(VS_DREQ_PORT,VS_DREQ_PIN)==GPIO_PIN_SET)

/* ==== API ==== */
void     VS_InitSimple(void);
void     VS_HardReset(void);
void     VS_SetVolume(uint8_t att_left,uint8_t att_right);
void     VS_SendMusic(const uint8_t *buf,uint32_t len);
void     VS_SendZeros(uint16_t n);
void     VS_SDI_SendChunk(const uint8_t *p,uint16_t n);
HAL_StatusTypeDef VS_SCI_Write(uint8_t reg,uint16_t val);
HAL_StatusTypeDef VS_SCI_Read(uint8_t reg,uint16_t *out);

#ifdef __cplusplus
}
#endif
#endif /* __VS1003_H */
