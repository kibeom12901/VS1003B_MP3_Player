#include "vs1003.h"
#include "util_uart.h"      // for uprintln()
#include "spi.h"
#include "gpio.h"

extern SPI_HandleTypeDef hspi1;

/* ---------------------------------------------------- */
HAL_StatusTypeDef VS_SCI_Write(uint8_t reg,uint16_t val)
{
    uint8_t tx[4]={SCI_WRITE_OP,reg,(uint8_t)(val>>8),(uint8_t)val};
    while(!VS_DREQ_IS_H());
    HAL_GPIO_WritePin(VS_XCS_PORT,VS_XCS_PIN,GPIO_PIN_RESET);
    HAL_StatusTypeDef s=HAL_SPI_Transmit(&hspi1,tx,sizeof(tx),HAL_MAX_DELAY);
    HAL_GPIO_WritePin(VS_XCS_PORT,VS_XCS_PIN,GPIO_PIN_SET);
    return s;
}
/* ---------------------------------------------------- */
HAL_StatusTypeDef VS_SCI_Read(uint8_t reg,uint16_t *out)
{
    uint8_t tx[2]={SCI_READ_OP,reg};
    uint8_t rx[2]={0};
    while(!VS_DREQ_IS_H());
    HAL_GPIO_WritePin(VS_XCS_PORT,VS_XCS_PIN,GPIO_PIN_RESET);
    HAL_StatusTypeDef s=HAL_SPI_Transmit(&hspi1,tx,2,HAL_MAX_DELAY);
    s|=HAL_SPI_Receive(&hspi1,rx,2,HAL_MAX_DELAY);
    HAL_GPIO_WritePin(VS_XCS_PORT,VS_XCS_PIN,GPIO_PIN_SET);
    *out=((uint16_t)rx[0]<<8)|rx[1];
    return s;
}
/* ---------------------------------------------------- */
void VS_HardReset(void)
{
    HAL_GPIO_WritePin(VS_XCS_PORT,VS_XCS_PIN,GPIO_PIN_SET);
    HAL_GPIO_WritePin(VS_XDCS_PORT,VS_XDCS_PIN,GPIO_PIN_SET);
    HAL_GPIO_WritePin(VS_RST_PORT,VS_RST_PIN,GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(VS_RST_PORT,VS_RST_PIN,GPIO_PIN_SET);
    uint32_t t0=HAL_GetTick();
    while(!VS_DREQ_IS_H()){
        if(HAL_GetTick()-t0>300){
            uprintln("ERR: VS1003 DREQ timeout");
            break;
        }
    }
}
/* ---------------------------------------------------- */
void VS_SDI_SendChunk(const uint8_t *p,uint16_t n)
{
    while(n){
        while(!VS_DREQ_IS_H());
        uint16_t c=(n>MP3_CHUNK)?MP3_CHUNK:n;
        HAL_GPIO_WritePin(VS_XDCS_PORT,VS_XDCS_PIN,GPIO_PIN_RESET);
        HAL_SPI_Transmit(&hspi1,(uint8_t*)p,c,HAL_MAX_DELAY);
        HAL_GPIO_WritePin(VS_XDCS_PORT,VS_XDCS_PIN,GPIO_PIN_SET);
        p+=c; n-=c;
    }
}
/* ---------------------------------------------------- */
void VS_SendMusic(const uint8_t *buf,uint32_t len)
{
    uint32_t i=0;
    while(i<len){
        uint16_t chunk=(len-i>512)?512:(uint16_t)(len-i);
        VS_SDI_SendChunk(&buf[i],chunk);
        i+=chunk;
    }
}
/* ---------------------------------------------------- */
void VS_SendZeros(uint16_t n)
{
    uint8_t z[32]={0};
    while(n){
        uint16_t c=(n>sizeof(z))?sizeof(z):n;
        VS_SDI_SendChunk(z,c);
        n-=c;
    }
}
/* ---------------------------------------------------- */
void VS_SetVolume(uint8_t att_left,uint8_t att_right)
{
    uint16_t v=((uint16_t)att_left<<8)|att_right;
    VS_SCI_Write(SCI_VOL,v);
}
/* ---------------------------------------------------- */
void VS_InitSimple(void)
{
    VS_HardReset();
    VS_SCI_Write(SCI_MODE,SM_SDINEW);
    HAL_Delay(2);

    uint16_t mode=0,status=0,clockf=0;
    VS_SCI_Read(SCI_MODE,&mode);
    VS_SCI_Read(SCI_STATUS,&status);
    VS_SCI_Read(SCI_CLOCKF,&clockf);
    uprintln("SCI_MODE  = 0x%04X",mode);
    uprintln("SCI_STATUS= 0x%04X",status);
    uprintln("SCI_CLOCKF= 0x%04X",clockf);

    uprintln("Set CLOCKF & VOL...");
    VS_SCI_Write(SCI_CLOCKF,0x8800);     // x3 multiplier
    VS_SetVolume(0x20,0x20);             // ≈ –10 dB
    HAL_Delay(20);
}
