/********************************** (C) COPYRIGHT *******************************
* File Name          : timer_config.h
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-29
* Description        : 定时器配置头文件 - 1ms精确定时中断
*********************************************************************************
* 功能说明：
*   本模块实现基于TIM2的1ms精确定时中断功能
*   系统时钟：96MHz
*   定时器时钟：96MHz（APB1=48MHz，定时器自动倍频）
*   中断频率：1000Hz (1ms周期)
*******************************************************************************/

#ifndef __TIMER_CONFIG_H
#define __TIMER_CONFIG_H

#include "ch32v30x.h"

/* 定时器选择配置 */
#define USE_TIM2    // 使用TIM2作为1ms定时器

/* 定时器参数宏定义 */
#ifdef USE_TIM2
    #define TIMx                    TIM2
    #define TIMx_IRQn               TIM2_IRQn
    #define TIMx_IRQHandler         TIM2_IRQHandler
    #define RCC_APBxPeriph_TIMx     RCC_APB1Periph_TIM2
    #define RCC_APBxPeriphClockCmd  RCC_APB1PeriphClockCmd
#endif

/* 定时器时钟计算参数
 * 系统时钟：96MHz
 * APB1总线：48MHz（PCLK1 = HCLK/2）
 * 定时器时钟：96MHz（当APB1预分频≠1时，定时器时钟自动×2）
 *
 * 计算公式：
 * 定时器中断频率 = 定时器时钟 / [(预分频+1) × (重载值+1)]
 * 1000Hz = 96,000,000Hz / [(95+1) × (999+1)]
 */
#define TIM_PRESCALER       95      // 预分频系数：96分频 (0-65535)
#define TIM_PERIOD          999     // 自动重载值：1000计数 (0-65535)
#define TIM_INTERRUPT_FREQ  1000    // 中断频率：1000Hz (1ms)

/* 中断优先级配置 */
#define TIM_IRQ_PREEMPTION_PRIORITY     2   // 抢占优先级(0-3)
#define TIM_IRQ_SUB_PRIORITY            0   // 子优先级(0-3)

/* 函数声明 */
void TIM_1ms_Init(void);                    // 初始化1ms定时器
void TIM_1ms_Start(void);                   // 启动定时器
void TIM_1ms_Stop(void);                    // 停止定时器
uint32_t TIM_Get_MillisCounter(void);       // 获取毫秒计数值（用户可选）

/* 用户回调函数（在中断中调用，需要用户在其他文件中实现） */
void TIM_1ms_Callback(void);                // 1ms定时器回调函数

#endif /* __TIMER_CONFIG_H */










