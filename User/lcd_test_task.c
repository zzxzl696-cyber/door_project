/********************************** (C) COPYRIGHT *******************************
* File Name          : lcd_test_task.c
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-30
* Description        : LCD测试任务实现
*********************************************************************************/

#include "lcd_test_task.h"
#include "lcd.h"
#include "lcd_init.h"
#include "timer_config.h"
#include "debug.h"

/**
 * @brief  LCD欢迎界面
 */
void lcd_welcome_screen(void)
{
    // 清屏为白色
    LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);

    // 绘制标题背景
    LCD_Fill(0, 0, LCD_W, 30, BLUE);

    // 绘制边框
    LCD_DrawRectangle(2, 32, LCD_W - 3, LCD_H - 3, BLUE);

    // 显示文字（简化版，无字库时显示占位符）
    // LCD_ShowString(10, 8, "CH32V30x", WHITE, BLUE, 16, 0);
    // LCD_ShowString(10, 40, "ST7735S LCD", BLUE, WHITE, 16, 0);
    // LCD_ShowString(10, 60, "128x160 RGB", RED, WHITE, 16, 0);

    printf("[LCD] Welcome screen displayed\r\n");
}

/**
 * @brief  LCD测试任务
 * @note   每秒更新一次显示内容
 */
void lcd_test_task(void)
{
    static uint8_t test_step = 0;
    static uint32_t last_time = 0;
    static uint16_t counter = 0;

    uint32_t current_time = TIM_Get_MillisCounter();

    // 每1秒执行一次
    if (current_time - last_time < 1000)
    {
        return;
    }

    last_time = current_time;
    counter++;

    switch (test_step)
    {
        case 0:  // 测试填充颜色
        {
            LCD_Fill(0, 0, LCD_W, LCD_H, RED);
            printf("[LCD Test %u] Fill RED\r\n", counter);
            test_step++;
            break;
        }

        case 1:  // 测试填充颜色
        {
            LCD_Fill(0, 0, LCD_W, LCD_H, GREEN);
            printf("[LCD Test %u] Fill GREEN\r\n", counter);
            test_step++;
            break;
        }

        case 2:  // 测试填充颜色
        {
            LCD_Fill(0, 0, LCD_W, LCD_H, BLUE);
            printf("[LCD Test %u] Fill BLUE\r\n", counter);
            test_step++;
            break;
        }

        case 3:  // 测试绘制线条
        {
            LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);
            LCD_DrawLine(0, 0, LCD_W - 1, LCD_H - 1, WHITE);
            LCD_DrawLine(0, LCD_H - 1, LCD_W - 1, 0, WHITE);
            LCD_DrawLine(LCD_W / 2, 0, LCD_W / 2, LCD_H - 1, RED);
            LCD_DrawLine(0, LCD_H / 2, LCD_W - 1, LCD_H / 2, GREEN);
            printf("[LCD Test %u] Draw Lines\r\n", counter);
            test_step++;
            break;
        }

        case 4:  // 测试绘制矩形
        {
            LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);
            LCD_DrawRectangle(10, 10, LCD_W - 10, LCD_H - 10, RED);
            LCD_DrawRectangle(20, 20, LCD_W - 20, LCD_H - 20, BLUE);
            LCD_DrawRectangle(30, 30, LCD_W - 30, LCD_H - 30, GREEN);
            printf("[LCD Test %u] Draw Rectangles\r\n", counter);
            test_step++;
            break;
        }

        case 5:  // 测试绘制圆形
        {
            LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);
            Draw_Circle(LCD_W / 2, LCD_H / 2, 50, RED);
            Draw_Circle(LCD_W / 2, LCD_H / 2, 40, GREEN);
            Draw_Circle(LCD_W / 2, LCD_H / 2, 30, BLUE);
            Draw_Circle(LCD_W / 2, LCD_H / 2, 20, YELLOW);
            printf("[LCD Test %u] Draw Circles\r\n", counter);
            test_step++;
            break;
        }

        case 6:  // 测试混合图形
        {
            LCD_Fill(0, 0, LCD_W, LCD_H, YELLOW);
            LCD_Fill(10, 10, 50, 50, RED);
            LCD_Fill(70, 10, 110, 50, BLUE);
            LCD_Fill(10, 60, 50, 100, GREEN);
            LCD_Fill(70, 60, 110, 100, MAGENTA);
            Draw_Circle(64, 130, 20, RED);
            printf("[LCD Test %u] Mixed Graphics\r\n", counter);
            test_step++;
            break;
        }

        case 7:  // 测试显示数字
        {
            LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);
            LCD_DrawRectangle(5, 5, LCD_W - 5, LCD_H - 5, BLUE);

            // 显示计数器
            LCD_ShowIntNum(20, 40, counter, 5, RED, WHITE, 16);
            LCD_ShowIntNum(20, 60, 12345, 5, BLUE, WHITE, 16);
            LCD_ShowFloatNum1(20, 80, 3.14, 4, GREEN, WHITE, 16);

            printf("[LCD Test %u] Show Numbers\r\n", counter);
            test_step++;
            break;
        }

        default:  // 重新开始
        {
            test_step = 0;
            lcd_welcome_screen();
            printf("[LCD Test %u] Restart\r\n", counter);
            break;
        }
    }
}
