/********************************** (C) COPYRIGHT *******************************
* File Name          : lcd_test_task.h
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-30
* Description        : LCD测试任务头文件
*********************************************************************************/

#ifndef __LCD_TEST_TASK_H
#define __LCD_TEST_TASK_H

#include "ch32v30x.h"

/**
 * @brief  LCD测试任务（调度器调用）
 * @note   周期：1000ms
 *         功能：循环显示各种图形和文字
 * @retval None
 */
void lcd_test_task(void);

/**
 * @brief  LCD初始化并显示欢迎界面
 * @retval None
 */
void lcd_welcome_screen(void);

#endif /* __LCD_TEST_TASK_H */
