/********************************** (C) COPYRIGHT *******************************
 * File Name          : led.h
 * Author             : Custom Implementation
 * Version            : V1.0.0
 * Date               : 2025-12-29
 * Description        : LED驱动模块头文件
 *********************************************************************************
 * 功能说明：
 *   - LED1: PC2引脚
 *   - LED2: PC3引脚
 *   - 提供初始化、开关、翻转、状态查询等功能
 *******************************************************************************/

#ifndef __LED_H__
#define __LED_H__

#include "bsp_system.h"

/* LED引脚定义 */
#define LED1_GPIO_PORT GPIOC
#define LED1_GPIO_PIN GPIO_Pin_2
#define LED1_GPIO_CLK RCC_APB2Periph_GPIOC

#define LED2_GPIO_PORT GPIOC
#define LED2_GPIO_PIN GPIO_Pin_3
#define LED2_GPIO_CLK RCC_APB2Periph_GPIOC

/* LED编号枚举 */
typedef enum
{
	LED1 = 0, // LED1 - PC2
	LED2 = 1  // LED2 - PC3
} LED_TypeDef;

/* LED状态枚举 */
typedef enum
{
	LED_OFF = 0, // LED关闭
	LED_ON = 1	 // LED开启
} LED_StateTypeDef;

/* 基础函数 */
void LED_Init(void);   // 初始化所有LED
void LED_DeInit(void); // 反初始化LED

/* 单个LED控制函数 */
void LED_On(LED_TypeDef led);						   // 打开指定LED
void LED_Off(LED_TypeDef led);						   // 关闭指定LED
void LED_Toggle(LED_TypeDef led);					   // 翻转指定LED状态
void LED_Set(LED_TypeDef led, LED_StateTypeDef state); // 设置指定LED状态

/* LED状态查询 */
LED_StateTypeDef LED_GetState(LED_TypeDef led); // 获取指定LED状态

/* 所有LED控制函数 */
void LED_AllOn(void);	  // 打开所有LED
void LED_AllOff(void);	  // 关闭所有LED
void LED_AllToggle(void); // 翻转所有LED

/* 兼容旧接口 */
void led_init(void); // 兼容旧函数名
void led_proc(void); // 兼容旧函数名（预留）

#endif /* __LED_H__ */
