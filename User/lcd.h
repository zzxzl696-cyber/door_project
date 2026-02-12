/********************************** (C) COPYRIGHT *******************************
* File Name          : lcd.h
* Author             : Ported from STM32 to CH32V30x
* Version            : V1.0.0
* Date               : 2025-12-30
* Description        : ST7735S LCD绘图函数头文件
*********************************************************************************/

#ifndef __LCD_H
#define __LCD_H

#include "lcd_init.h"

/* RGB565颜色定义 */
#define WHITE         0xFFFF  // 白色
#define BLACK         0x0000  // 黑色
#define BLUE          0x001F  // 蓝色
#define BRED          0XF81F  //
#define GRED          0XFFE0  //
#define GBLUE         0X07FF  //
#define RED           0xF800  // 红色
#define MAGENTA       0xF81F  // 洋红
#define GREEN         0x07E0  // 绿色
#define CYAN          0x7FFF  // 青色
#define YELLOW        0xFFE0  // 黄色
#define BROWN         0XBC40  // 棕色
#define BRRED         0XFC07  // 棕红色
#define GRAY          0X8430  // 灰色
#define DARKBLUE      0X01CF  // 深蓝色
#define LIGHTBLUE     0X7D7C  // 浅蓝色
#define GRAYBLUE      0X5458  // 灰蓝色
#define LIGHTGREEN    0X841F  // 浅绿色
#define LGRAY         0XC618  // 浅灰色
#define LGRAYBLUE     0XA651  // 浅灰蓝色
#define LBBLUE        0X2B12  // 浅棕蓝色

#ifdef __cplusplus
extern "C" {
#endif
/* 绘图函数 */

/**
 * @brief  填充指定矩形区域
 * @param  xsta,ysta: 起始坐标
 * @param  xend,yend: 结束坐标
 * @param  color: 填充颜色(RGB565)
 */
void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color);

/**
 * @brief  在指定位置画一个点
 * @param  x,y: 坐标
 * @param  color: 颜色
 */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief  画直线
 * @param  x1,y1: 起点坐标
 * @param  x2,y2: 终点坐标
 * @param  color: 颜色
 */
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/**
 * @brief  画矩形边框
 * @param  x1,y1: 左上角坐标
 * @param  x2,y2: 右下角坐标
 * @param  color: 颜色
 */
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/**
 * @brief  画圆
 * @param  x0,y0: 圆心坐标
 * @param  r: 半径
 * @param  color: 颜色
 */
void Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);

/* 字符显示函数 */

/**
 * @brief  显示单个ASCII字符
 * @param  x,y: 起始坐标
 * @param  num: 要显示的字符
 * @param  fc: 前景色
 * @param  bc: 背景色
 * @param  sizey: 字体大小(12/16/24/32)
 * @param  mode: 0=非叠加模式, 1=叠加模式
 */
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);

/**
 * @brief  显示ASCII字符串
 * @param  x,y: 起始坐标
 * @param  p: 字符串指针
 * @param  fc: 前景色
 * @param  bc: 背景色
 * @param  sizey: 字体大小
 * @param  mode: 0=非叠加模式, 1=叠加模式
 */
void LCD_ShowString(uint16_t x, uint16_t y, const uint8_t *p, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);

/**
 * @brief  显示整数
 * @param  x,y: 起始坐标
 * @param  num: 要显示的整数
 * @param  len: 数字长度
 * @param  fc: 前景色
 * @param  bc: 背景色
 * @param  sizey: 字体大小
 */
void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey);

/**
 * @brief  显示浮点数
 * @param  x,y: 起始坐标
 * @param  num: 要显示的浮点数
 * @param  len: 数字长度
 * @param  fc: 前景色
 * @param  bc: 背景色
 * @param  sizey: 字体大小
 */
void LCD_ShowFloatNum1(uint16_t x, uint16_t y, float num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey);

/**
 * @brief  工具函数：计算m的n次方
 */
uint32_t mypow(uint8_t m, uint8_t n);



#ifdef __cplusplus
}
#endif

#endif /* __LCD_H */
