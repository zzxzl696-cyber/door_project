/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2021/06/06
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/*
 *@Note
 USART Print debugging routine:
 USART1_Tx(PA9).
 This example demonstrates using USART1(PA9) as a print debug port output.

*/

#include "debug.h"
#include "timer_config.h"
#include "bsp_system.h"
#include "usart1_dma_rx.h"
#include "lcd_init.h"
#include "lcd_test_task.h"
#include "menu_task.h"
#include "door_control.h"
#include "door_status_ui.h"

/* Global typedef */

/* Global define */

/* Global Variable */
volatile uint32_t led_toggle_count = 0;

/*********************************************************************
 * @fn      TIM_1ms_Callback
 *
 * @brief   定时器1ms回调函数（用户实现）
 *          每1ms调用一次，可在此执行周期性任务
 *
 * @return  none
 */
// void TIM_1ms_Callback(void)
// {
//     static uint16_t count = 0;
//
//     count++;
//
//     // 每500ms翻转一次LED（GPIOC Pin2）
//     if(count >= 500)
//     {
//         count = 0;
//         GPIO_WriteBit(GPIOC, GPIO_Pin_2,
//                      (BitAction)(1 - GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_2)));
//         led_toggle_count++;
//     }
// }

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
	RFID_Result ret;
	led_init();
	scheduler_init();

	/* 配置系统 */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	SystemCoreClockUpdate();
	Delay_Init();

	/* 初始化USART1 DMA接收 (115200波特率) */
	USART1_DMA_RX_FullInit(115200, &g_usart1_ringbuf);

	/* 打印系统信息 */
	printf("\r\n=================================\r\n");
	printf("System Clock: %d Hz\r\n", SystemCoreClock);
	printf("Chip ID: %08x\r\n", DBGMCU_GetCHIPID());
	printf("CH32V30x LCD + UART DMA Demo\r\n");
	printf("=================================\r\n\r\n");

	/* 初始化1ms定时器 */
	TIM_1ms_Init();

	/* 初始化门禁控制模块 */
	printf("[INFO] Initializing door control...\r\n");
	door_control_init();
	printf("[INFO] Door control initialized!\r\n");

	/* 初始化用户数据库和日志 */
	printf("[INFO] Initializing user database...\r\n");
	user_db_init();
	printf("[INFO] Initializing access log...\r\n");
	access_log_init();

	/* 初始化认证管理器 */
	printf("[INFO] Initializing auth manager...\r\n");
	auth_manager_init();
	user_admin_init();
	printf("[INFO] Auth system initialized!\r\n");

	/* 初始化LCD显示屏和状态UI */
	printf("[INFO] Initializing ST7735S LCD...\r\n");
	LCD_Init();            // 初始化SPI2和ST7735S屏幕
	door_status_ui_init(); // 使用状态显示界面替代菜单系统
	printf("[INFO] LCD initialized successfully!\r\n");

	/* 注册认证管理器回调 */
	auth_manager_set_callback(door_status_ui_on_auth_result);
	auth_manager_set_start_callback(door_status_ui_on_auth_start);

	/* 注册密码输入UI回调 */
	pwd_input_set_ui_callback(door_status_ui_on_password_input);

	/* 显示欢迎界面 */
	// lcd_welcome_screen();  // 如果需要欢迎界面,取消注释
	key_init();
	matrix_key_init();
	/* 延迟一下再启动定时器 */
	Delay_Ms(1000);
	/* 启动定时器 */
	TIM_1ms_Start();
	BY8301_Init();

	printf("[INFO] System initialized successfully!\r\n");
	printf("[INFO] USART1 DMA RX enabled, waiting for data...\r\n");
	printf("[INFO] Door control system running...\r\n");
	printf("[INFO] User count: %d, Log count: %d\r\n",
		   user_db_get_count(), access_log_get_count());
	printf("[INFO] Master password: 0000 (default)\r\n\r\n");

	printf("\r\n========== 示例2：调度器框架下的自动模式 ==========\r\n");
	RFID_Init(115200);
	// 设置为自动上传卡号+块8数据

	ret = RFID_SetWorkMode(RFID_MODE_AUTO_ID_BLOCK, 8, 2000);
	if (ret != RFID_RET_OK)
	{
		printf("设置自动模式失败！\r\n");
	}
	// 等待模块重启
	Delay_Ms(2000);
	printf("自动模式已启动，请刷卡...\r\n");

	while (1)
	{
		scheduler_run();
	}
}
