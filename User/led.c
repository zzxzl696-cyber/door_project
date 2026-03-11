/********************************** (C) COPYRIGHT *******************************
* File Name          : led.c
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-29
* Description        : LED驱动模块实现文件
*********************************************************************************
* 功能说明：
*   - LED1: PC2引脚（低电平点亮，高电平熄灭）- 共阳极设计
*   - LED2: PC3引脚（低电平点亮，高电平熄灭）- 共阳极设计
*   - 提供完整的LED控制接口
*******************************************************************************/

#include "bsp_system.h"

/* 全局变量 - LED状态数组 */
static uint8_t g_led_state[2] = {LED_OFF, LED_OFF};  // LED1, LED2状态

/*********************************************************************
 * @fn      LED_Init
 *
 * @brief   初始化所有LED（LED1和LED2）
 *          配置PC2和PC3为推挽输出，50MHz
 *
 * @return  none
 */
void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    /* 使能GPIOC时钟 */
    RCC_APB2PeriphClockCmd(LED1_GPIO_CLK, ENABLE);

    /* 配置LED1 - PC2 */
    GPIO_InitStructure.GPIO_Pin = LED1_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;        // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       // 50MHz
    GPIO_Init(LED1_GPIO_PORT, &GPIO_InitStructure);

    /* 配置LED2 - PC3 */
    GPIO_InitStructure.GPIO_Pin = LED2_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED2_GPIO_PORT, &GPIO_InitStructure);

    /* 初始化时关闭所有LED */
    LED_AllOff();
}

/*********************************************************************
 * @fn      LED_DeInit
 *
 * @brief   反初始化LED，恢复为浮空输入状态
 *
 * @return  none
 */
void LED_DeInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    /* 配置为浮空输入 */
    GPIO_InitStructure.GPIO_Pin = LED1_GPIO_PIN | LED2_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(LED1_GPIO_PORT, &GPIO_InitStructure);

    /* 清除状态 */
    g_led_state[LED1] = LED_OFF;
    g_led_state[LED2] = LED_OFF;
}

/*********************************************************************
 * @fn      LED_On
 *
 * @brief   打开指定的LED
 *
 * @param   led - LED编号（LED1或LED2）
 *
 * @return  none
 */
void LED_On(LED_TypeDef led)
{
    if(led == LED1)
    {
        GPIO_ResetBits(LED1_GPIO_PORT, LED1_GPIO_PIN);  // 低电平点亮
        g_led_state[LED1] = LED_ON;
    }
    else if(led == LED2)
    {
        GPIO_ResetBits(LED2_GPIO_PORT, LED2_GPIO_PIN);  // 低电平点亮
        g_led_state[LED2] = LED_ON;
    }
}

/*********************************************************************
 * @fn      LED_Off
 *
 * @brief   关闭指定的LED
 *
 * @param   led - LED编号（LED1或LED2）
 *
 * @return  none
 */
void LED_Off(LED_TypeDef led)
{
    if(led == LED1)
    {
        GPIO_SetBits(LED1_GPIO_PORT, LED1_GPIO_PIN);    // 高电平熄灭
        g_led_state[LED1] = LED_OFF;
    }
    else if(led == LED2)
    {
        GPIO_SetBits(LED2_GPIO_PORT, LED2_GPIO_PIN);    // 高电平熄灭
        g_led_state[LED2] = LED_OFF;
    }
}

/*********************************************************************
 * @fn      LED_Toggle
 *
 * @brief   翻转指定LED的状态（开→关，关→开）
 *
 * @param   led - LED编号（LED1或LED2）
 *
 * @return  none
 */
void LED_Toggle(LED_TypeDef led)
{
    if(led == LED1)
    {
        GPIO_WriteBit(LED1_GPIO_PORT, LED1_GPIO_PIN,
                     (BitAction)(1 - GPIO_ReadOutputDataBit(LED1_GPIO_PORT, LED1_GPIO_PIN)));
        g_led_state[LED1] = !g_led_state[LED1];
    }
    else if(led == LED2)
    {
        GPIO_WriteBit(LED2_GPIO_PORT, LED2_GPIO_PIN,
                     (BitAction)(1 - GPIO_ReadOutputDataBit(LED2_GPIO_PORT, LED2_GPIO_PIN)));
        g_led_state[LED2] = !g_led_state[LED2];
    }
}

/*********************************************************************
 * @fn      LED_Set
 *
 * @brief   设置指定LED的状态
 *
 * @param   led - LED编号（LED1或LED2）
 * @param   state - LED状态（LED_ON或LED_OFF）
 *
 * @return  none
 */
void LED_Set(LED_TypeDef led, LED_StateTypeDef state)
{
    if(state == LED_ON)
    {
        LED_On(led);
    }
    else
    {
        LED_Off(led);
    }
}

/*********************************************************************
 * @fn      LED_GetState
 *
 * @brief   获取指定LED的当前状态
 *
 * @param   led - LED编号（LED1或LED2）
 *
 * @return  LED_StateTypeDef - LED状态（LED_ON或LED_OFF）
 */
LED_StateTypeDef LED_GetState(LED_TypeDef led)
{
    if(led == LED1)
    {
        return (LED_StateTypeDef)g_led_state[LED1];
    }
    else if(led == LED2)
    {
        return (LED_StateTypeDef)g_led_state[LED2];
    }
    return LED_OFF;
}

/*********************************************************************
 * @fn      LED_AllOn
 *
 * @brief   打开所有LED
 *
 * @return  none
 */
void LED_AllOn(void)
{
    LED_On(LED1);
    LED_On(LED2);
}

/*********************************************************************
 * @fn      LED_AllOff
 *
 * @brief   关闭所有LED
 *
 * @return  none
 */
void LED_AllOff(void)
{
    LED_Off(LED1);
    LED_Off(LED2);
}

/*********************************************************************
 * @fn      LED_AllToggle
 *
 * @brief   翻转所有LED的状态
 *
 * @return  none
 */
void LED_AllToggle(void)
{
    LED_Toggle(LED1);
    LED_Toggle(LED2);
}

/*********************************************************************
 * @fn      led_init (兼容接口)
 *
 * @brief   LED初始化函数（兼容旧代码）
 *
 * @return  none
 */
void led_init(void)
{
    LED_Init();
}

/*********************************************************************
 * @fn      led_proc (兼容接口)
 *
 * @brief   LED处理函数（预留接口，用于周期性任务）
 *
 * @return  none
 */
void led_proc(void)
{
    /* 预留接口，可用于LED状态更新、闪烁处理等 */
	
}
