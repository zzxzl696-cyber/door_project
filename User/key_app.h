/********************************** (C) COPYRIGHT *******************************
 * File Name          : key_app.h
 * Author             : Key driver for CH32V30x
 * Version            : V1.0.0
 * Date               : 2025-01-14
 * Description        : 按键驱动头文件
 *********************************************************************************/

#ifndef __KEY_APP_H__
#define __KEY_APP_H__

#include "bsp_system.h"

// 按键结构体定义(供外部使用)
typedef struct
{
	GPIO_TypeDef *gpiox;
	uint16_t pin;
	uint16_t ticks;
	uint8_t level;
	uint8_t id;
	uint8_t state;
	uint8_t debouce_cnt;
	uint8_t repeat;
} button;

// 按键返回值定义
#define KEY_NONE 0
#define KEY_USER 1 // PC4用户按键

// 矩阵键盘返回值定义 (1-16)
#define MATRIX_KEY_1 1
#define MATRIX_KEY_2 2
#define MATRIX_KEY_3 3
#define MATRIX_KEY_4 4
#define MATRIX_KEY_5 5
#define MATRIX_KEY_6 6
#define MATRIX_KEY_7 7
#define MATRIX_KEY_8 8
#define MATRIX_KEY_9 9
#define MATRIX_KEY_10 10
#define MATRIX_KEY_11 11
#define MATRIX_KEY_12 12
#define MATRIX_KEY_13 13
#define MATRIX_KEY_14 14
#define MATRIX_KEY_15 15
#define MATRIX_KEY_16 16

/**
 * @brief 初始化按键GPIO
 * @note 配置PC4为上拉输入模式,初始化按键状态机
 */
void key_init(void);

/**
 * @brief 按键状态扫描函数
 * @note 需要在定时任务中周期调用(建议10-20ms)
 *       内部实现3次采样消抖、短按/长按/双击检测
 */
void key_state(void);

/**
 * @brief 初始化4x4矩阵键盘
 * @note 行: PC5, PE7, PE9, PE11 (输出)
 *       列: PE13, PE15, PD9, PD11 (输入上拉)
 */
void matrix_key_init(void);

/**
 * @brief 扫描矩阵键盘
 * @return 按键值(1-16), 无按键返回0
 */
void matrix_key_scan(void);

#endif // __KEY_APP_H__
