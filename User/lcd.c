/********************************** (C) COPYRIGHT *******************************
* File Name          : lcd.c
* Author             : Ported from STM32 to CH32V30x
* Version            : V1.0.0
* Date               : 2025-12-30
* Description        : ST7735S LCD绘图函数实现（精简版）
*********************************************************************************/

#include "bsp_system.h"
#include "lcdfont.h"

/**
 * @brief  填充指定矩形区域
 */
void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color)
{
    uint16_t i, j;
    LCD_Address_Set(xsta, ysta, xend - 1, yend - 1);  // 设置显示范围

    for (i = ysta; i < yend; i++)
    {

        for (j = xsta; j < xend; j++)
        {
            LCD_WR_DATA(color);
        }
    }
    if(color==0)return;
}

/**
 * @brief  在指定位置画一个点
 */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_Address_Set(x, y, x, y);  // 设置光标位置
    LCD_WR_DATA(color);
}

/**
 * @brief  画直线（Bresenham算法）
 */
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;

    delta_x = x2 - x1;  // 计算坐标增量
    delta_y = y2 - y1;
    uRow = x1;  // 画线起点坐标
    uCol = y1;

    if (delta_x > 0)
        incx = 1;  // 设置单步方向
    else if (delta_x == 0)
        incx = 0;  // 垂直线
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }

    if (delta_y > 0)
        incy = 1;
    else if (delta_y == 0)
        incy = 0;  // 水平线
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }

    if (delta_x > delta_y)
        distance = delta_x;  // 选取基本增量坐标轴
    else
        distance = delta_y;

    for (t = 0; t < distance + 1; t++)
    {
        LCD_DrawPoint(uRow, uCol, color);  // 画点
        xerr += delta_x;
        yerr += delta_y;

        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }

        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
}

/**
 * @brief  画矩形边框
 */
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    LCD_DrawLine(x1, y1, x2, y1, color);  // 上边
    LCD_DrawLine(x1, y1, x1, y2, color);  // 左边
    LCD_DrawLine(x1, y2, x2, y2, color);  // 下边
    LCD_DrawLine(x2, y1, x2, y2, color);  // 右边
}

/**
 * @brief  画圆（中点圆算法）
 */
void Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int a, b;
    a = 0;
    b = r;

    while (a <= b)
    {
        LCD_DrawPoint(x0 - b, y0 - a, color);  // 3
        LCD_DrawPoint(x0 + b, y0 - a, color);  // 0
        LCD_DrawPoint(x0 - a, y0 + b, color);  // 1
        LCD_DrawPoint(x0 - a, y0 - b, color);  // 2
        LCD_DrawPoint(x0 + b, y0 + a, color);  // 4
        LCD_DrawPoint(x0 + a, y0 - b, color);  // 5
        LCD_DrawPoint(x0 + a, y0 + b, color);  // 6
        LCD_DrawPoint(x0 - b, y0 + a, color);  // 7
        a++;

        // 判断要画的点是否过远
        if ((a * a + b * b) > (r * r))
        {
            b--;
        }
    }
}

/**
 * @brief  工具函数：计算m的n次方
 */
uint32_t mypow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;

    while (n--)
    {
        result *= m;
    }

    return result;
}

/**
 * @brief  显示单个ASCII字符
 * @note   支持12x6和16x8字体
 */
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint8_t temp, sizex, t, m;
    uint16_t i;

    sizex = sizey / 2;
    num = num - ' ';

    LCD_Address_Set(x, y, x + sizex - 1, y + sizey - 1);

    for (i = 0; i < sizey; i++)
    {
        if (sizey == 12)
        {
            temp = ascii_1206[num][i];
        }
        else if (sizey == 16)
        {
            temp = ascii_1608[num][i];
        }
        else
            return;

        for (m = 0; m < sizex; m++)
        {
            if (temp & 0x01)
                LCD_WR_DATA(fc);
            else
                LCD_WR_DATA(bc);
            temp >>= 1;
        }
    }
}

/**
 * @brief  显示ASCII字符串
 */
void LCD_ShowString(uint16_t x, uint16_t y, const uint8_t *p, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    while (*p != '\0')
    {
        LCD_ShowChar(x, y, *p, fc, bc, sizey, mode);
        x += sizey / 2;
        p++;
    }
}

/**
 * @brief  显示整数
 */
void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / mypow(10, len - t - 1)) % 10;

        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                LCD_ShowChar(x + (sizey / 2) * t, y, ' ', fc, bc, sizey, 0);
                continue;
            }
            else
            {
                enshow = 1;
            }
        }

        LCD_ShowChar(x + (sizey / 2) * t, y, temp + '0', fc, bc, sizey, 0);
    }
}

/**
 * @brief  显示浮点数（保留1位小数）
 */
void LCD_ShowFloatNum1(uint16_t x, uint16_t y, float num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t t, temp, sizex;
    uint16_t num1;

    sizex = sizey / 2;
    num1 = num * 100;

    for (t = 0; t < len; t++)
    {
        temp = (num1 / mypow(10, len - t - 1)) % 10;

        if (t == (len - 2))
        {
            LCD_ShowChar(x + (sizex) * (len - 2), y, '.', fc, bc, sizey, 0);
            t++;
            len += 1;
        }

        LCD_ShowChar(x + (sizex) * t, y, temp + '0', fc, bc, sizey, 0);
    }
}
