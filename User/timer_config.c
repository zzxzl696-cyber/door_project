/********************************** (C) COPYRIGHT *******************************
* File Name          : timer_config.c
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-29
* Description        : 定时器配置实现文件 - 1ms精确定时中断
*********************************************************************************
* 功能说明：
*   - 实现TIM2的1ms精确定时中断
*   - 提供毫秒计数器功能
*   - 支持启动/停止控制
*   - 提供用户回调函数接口
*******************************************************************************/

#include "timer_config.h"
#include "debug.h"

/* 全局变量 */
static volatile uint32_t g_millis_counter = 0;  // 毫秒计数器（1ms递增）

/*********************************************************************
 * @fn      TIM_1ms_Init
 *
 * @brief   初始化定时器，配置为1ms中断周期
 *          系统时钟96MHz，定时器时钟96MHz
 *          预分频96倍后为1MHz，计数1000次为1ms
 *
 * @return  none
 */
void TIM_1ms_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    /* 1. 使能定时器时钟 */
    RCC_APBxPeriphClockCmd(RCC_APBxPeriph_TIMx, ENABLE);

    /* 2. 配置定时器基本参数 */
    TIM_TimeBaseInitStructure.TIM_Period = TIM_PERIOD;              // 自动重载值：999
    TIM_TimeBaseInitStructure.TIM_Prescaler = TIM_PRESCALER;        // 预分频值：95
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;     // 时钟分频：不分频
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up; // 向上计数模式
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;            // 重复计数器（TIM2不使用）

    TIM_TimeBaseInit(TIMx, &TIM_TimeBaseInitStructure);

    /* 3. 配置定时器中断 */
    TIM_ITConfig(TIMx, TIM_IT_Update, ENABLE);  // 使能更新中断

    /* 4. 配置NVIC中断优先级 */
    NVIC_InitStructure.NVIC_IRQChannel = TIMx_IRQn;                             // 中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = TIM_IRQ_PREEMPTION_PRIORITY; // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = TIM_IRQ_SUB_PRIORITY;       // 子优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                             // 使能中断
    NVIC_Init(&NVIC_InitStructure);

    /* 5. 清除中断标志（可选，确保启动干净） */
    TIM_ClearITPendingBit(TIMx, TIM_IT_Update);

    /* 6. 不自动启动，由用户调用TIM_1ms_Start()启动 */
    // TIM_Cmd(TIMx, ENABLE);

    printf("[TIM] 1ms定时器初始化完成\r\n");
    printf("[TIM] 定时器时钟: 96MHz\r\n");
    printf("[TIM] 预分频: %d, 重载值: %d\r\n", TIM_PRESCALER+1, TIM_PERIOD+1);
    printf("[TIM] 中断频率: %dHz (周期: %dms)\r\n", TIM_INTERRUPT_FREQ, 1);
}

/*********************************************************************
 * @fn      TIM_1ms_Start
 *
 * @brief   启动定时器
 *
 * @return  none
 */
void TIM_1ms_Start(void)
{
    TIM_Cmd(TIMx, ENABLE);
    printf("[TIM] 1ms定时器已启动\r\n");
}

/*********************************************************************
 * @fn      TIM_1ms_Stop
 *
 * @brief   停止定时器
 *
 * @return  none
 */
void TIM_1ms_Stop(void)
{
    TIM_Cmd(TIMx, DISABLE);
    printf("[TIM] 1ms定时器已停止\r\n");
}

/*********************************************************************
 * @fn      TIM_Get_MillisCounter
 *
 * @brief   获取从启动以来的毫秒计数值
 *
 * @return  毫秒计数值（uint32_t）
 *          注意：每约49.7天会溢出（2^32 ms）
 */
uint32_t TIM_Get_MillisCounter(void)
{
    return g_millis_counter;
}

/*********************************************************************
 * @fn      TIMx_IRQHandler
 *
 * @brief   定时器中断服务函数（每1ms触发一次）
 *          RISC-V架构使用WCH专用中断属性
 *
 * @return  none
 */
void TIMx_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIMx_IRQHandler(void)
{
    /* 检查更新中断标志 */
    if(TIM_GetITStatus(TIMx, TIM_IT_Update) != RESET)
    {
        /* 清除中断标志（必须，否则会持续触发） */
        TIM_ClearITPendingBit(TIMx, TIM_IT_Update);

        /* 毫秒计数器递增 */
        g_millis_counter++;

        // /* 调用用户回调函数 */
        // TIM_1ms_Callback();
    }
}

/*********************************************************************
 * @fn      TIM_1ms_Callback (弱定义)
 *
 * @brief   定时器1ms回调函数（用户可在其他文件重新实现此函数）
 *          此为默认的弱定义实现，用户可以覆盖
 *
 * @return  none
 */
__attribute__((weak)) void TIM_1ms_Callback(void)
{
    /* 默认实现：空函数
     * 用户可以在main.c或其他文件中重新实现此函数来执行自定义操作
     *
     * 示例：
     * void TIM_1ms_Callback(void)
     * {
     *     static uint16_t count = 0;
     *     count++;
     *     if(count >= 1000) // 每1000ms执行一次
     *     {
     *         count = 0;
     *         // 用户代码
     *     }
     * }
     */
}
