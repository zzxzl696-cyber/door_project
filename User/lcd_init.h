/********************************** (C) COPYRIGHT *******************************
* File Name          : lcd_init.h
* Author             : Ported from STM32 to CH32V30x
* Version            : V2.0.0
* Date               : 2025-12-30
* Description        : ST7735S 1.8" LCD初始化模块头文件 (硬件SPI版本)
*                      分辨率：128×160，颜色深度：RGB565
*                      使用SPI2硬件外设 (PB13=SCK, PB15=MOSI)
*********************************************************************************/

#ifndef __LCD_INIT_H
#define __LCD_INIT_H

#include "ch32v30x.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SPI硬件配置 */
#define LCD_SPI                 SPI2
#define LCD_SPI_CLK             RCC_APB1Periph_SPI2
#define LCD_SPI_GPIO_CLK        RCC_APB2Periph_GPIOB

/* LCD显示方向配置 */
#define USE_HORIZONTAL 0  // 0/1为竖屏，2/3为横屏

#if USE_HORIZONTAL==0||USE_HORIZONTAL==1
    #define LCD_W 128
    #define LCD_H 160
#else
    #define LCD_W 160
    #define LCD_H 128
#endif

/* SPI引脚映射 - 使用SPI2 (GPIOB) */
// SPI2标准引脚
#define LCD_SPI_SCK_PORT    GPIOB
#define LCD_SPI_SCK_PIN     GPIO_Pin_13    // SPI2_SCK

#define LCD_SPI_MOSI_PORT   GPIOB
#define LCD_SPI_MOSI_PIN    GPIO_Pin_15    // SPI2_MOSI

// 控制引脚
#define LCD_CS_PORT         GPIOB
#define LCD_CS_PIN          GPIO_Pin_12

#define LCD_DC_PORT         GPIOB
#define LCD_DC_PIN          GPIO_Pin_14

#define LCD_RES_PORT        GPIOB
#define LCD_RES_PIN         GPIO_Pin_10

#define LCD_BLK_PORT        GPIOB
#define LCD_BLK_PIN         GPIO_Pin_11

/* GPIO控制宏定义 */
#define LCD_CS_Clr()    GPIO_ResetBits(LCD_CS_PORT, LCD_CS_PIN)
#define LCD_CS_Set()    GPIO_SetBits(LCD_CS_PORT, LCD_CS_PIN)

#define LCD_DC_Clr()    GPIO_ResetBits(LCD_DC_PORT, LCD_DC_PIN)
#define LCD_DC_Set()    GPIO_SetBits(LCD_DC_PORT, LCD_DC_PIN)

#define LCD_RES_Clr()   GPIO_ResetBits(LCD_RES_PORT, LCD_RES_PIN)
#define LCD_RES_Set()   GPIO_SetBits(LCD_RES_PORT, LCD_RES_PIN)

#define LCD_BLK_Clr()   GPIO_ResetBits(LCD_BLK_PORT, LCD_BLK_PIN)
#define LCD_BLK_Set()   GPIO_SetBits(LCD_BLK_PORT, LCD_BLK_PIN)

/* 函数声明 */

/**
 * @brief  SPI2硬件初始化
 * @retval None
 */
void LCD_SPI_Init(void);

/**
 * @brief  硬件SPI写入一个字节
 * @param  dat: 要写入的字节
 * @retval None
 */
void LCD_Writ_Bus(uint8_t dat);

/**
 * @brief  写入8位数据
 * @param  dat: 8位数据
 * @retval None
 */
void LCD_WR_DATA8(uint8_t dat);

/**
 * @brief  写入16位数据
 * @param  dat: 16位数据
 * @retval None
 */
void LCD_WR_DATA(uint16_t dat);

/**
 * @brief  写入寄存器/命令
 * @param  dat: 命令字节
 * @retval None
 */
void LCD_WR_REG(uint8_t dat);

/**
 * @brief  设置显示窗口地址
 * @param  x1,y1: 起始坐标
 * @param  x2,y2: 结束坐标
 * @retval None
 */
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/**
 * @brief  LCD完整初始化
 * @retval None
 */
void LCD_Init(void);

#ifdef __cplusplus
}
#endif


#endif /* __LCD_INIT_H */
