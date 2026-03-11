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

#include "bsp_system.h"

/* ================= ESP8266 状态名称（调试用） ================= */

static const char* esp8266_state_name(esp8266_state_t st)
{
	static const char* names[] = {
		"DISABLED", "BOOT", "BASIC_SETUP", "JOIN_AP",
		"TCP_CONNECT", "ONLINE",
		"MQTT_DISCONN", "MQTT_CFG", "MQTT_CONN",
		"MQTT_OK", "MQTT_SUB", "ERROR"
	};
	if (st <= ESP8266_STATE_ERROR)
		return names[st];
	return "UNKNOWN";
}

/* ================= ESP8266 事件回调 ================= */

static void on_esp8266_event(esp8266_event_t evt, esp8266_state_t state, void *user)
{
	(void)user;

	switch (evt)
	{
	case ESP8266_EVENT_STATE_CHANGED:
		printf("[WiFi] state -> %s\r\n", esp8266_state_name(state));
		break;
	case ESP8266_EVENT_WIFI_CONNECTED:
		printf("[WiFi] WiFi connected\r\n");
		break;
	case ESP8266_EVENT_WIFI_DISCONNECTED:
		printf("[WiFi] WiFi disconnected\r\n");
		break;
	case ESP8266_EVENT_MQTT_CONNECTED:
		printf("[WiFi] MQTT connected\r\n");
		mqtt_app_notify_connected();
		break;
	case ESP8266_EVENT_MQTT_SUBSCRIBED:
		printf("[WiFi] MQTT subscribed to: %s\r\n", ESP8266_MQTT_SUB_TOPIC);
		mqtt_app_notify_connected();
		break;
	case ESP8266_EVENT_MQTT_DISCONNECTED:
		printf("[WiFi] MQTT disconnected\r\n");
		mqtt_app_notify_disconnected();
		break;
	case ESP8266_EVENT_ERROR:
		printf("[WiFi] ERROR in state %s\r\n", esp8266_state_name(state));
		break;
	default:
		break;
	}
}

/* ================= 认证结果包装回调 ================= */

static void on_auth_result(auth_method_t method, auth_result_t result, const user_entry_t *user)
{
	/* 通知 UI 更新 */
	door_status_ui_on_auth_result(method, result, user);
	/* 通知 MQTT 应用层上报 */
	mqtt_app_on_auth(method, result, user);
}

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

	/* 初始化调试串口 USART3 (PB10-TX, 115200) 用于 printf 输出 */
	USART_Printf_Init(115200);

	/* 初始化USART1 DMA接收 (115200波特率) 用于 ESP8266 通信 */
	USART1_DMA_RX_FullInit(115200, &g_usart1_ringbuf);

	/* 打印系统信息 */
	printf("\r\n=================================\r\n");
	printf("System Clock: %d Hz\r\n", SystemCoreClock);
	printf("Chip ID: %08x\r\n", DBGMCU_GetCHIPID());
	printf("CH32V30x Door Access Control\r\n");
	printf("=================================\r\n\r\n");

	/* 初始化1ms定时器 */
	TIM_1ms_Init();

	/* 初始化门禁控制模块 */
	printf("[INIT] Door control...\r\n");
	door_control_init();

	/* 初始化用户数据库和日志 */
	printf("[INIT] User database & access log...\r\n");
	user_db_init();
	access_log_init();

	/* 初始化认证管理器 */
	printf("[INIT] Auth manager...\r\n");
	auth_manager_init();
	user_admin_init();

	/* 初始化LCD显示屏和状态UI */
	printf("[INIT] ST7735S LCD...\r\n");
	LCD_Init();
	door_status_ui_init();

	/* 注册认证管理器回调（包装回调同时通知 UI 和 MQTT） */
	auth_manager_set_callback(on_auth_result);
	auth_manager_set_start_callback(door_status_ui_on_auth_start);
	pwd_input_set_ui_callback(door_status_ui_on_password_input);

	/* 按键初始化 */
	key_init();
	matrix_key_init();

	/* 语音模块 */
	BY8301_Init();

	/* RFID 初始化（自动上传卡号+块数据模式） */
	printf("[INIT] RFID reader...\r\n");
	RFID_Init(115200);
	ret = RFID_SetWorkMode(RFID_MODE_AUTO_ID_BLOCK, 8, 2000);
	if (ret != RFID_RET_OK)
	{
		printf("[WARN] RFID set auto mode failed\r\n");
	}

	/* MQTT 应用层初始化（必须在 esp8266 之前，以便回调注册） */
	printf("[INIT] MQTT app layer...\r\n");
	mqtt_app_init();

	/* ESP8266 WiFi/MQTT 初始化（非阻塞，状态机由调度器驱动） */
	/* ESP8266 使用 USART1 (PA9/PA10) 通信，USART1 已在上方初始化 */
	printf("[INIT] ESP8266 WiFi module...\r\n");
	esp8266_init();
	esp8266_set_callbacks(NULL, on_esp8266_event, NULL);
	esp8266_mqtt_set_on_message(mqtt_app_on_message, NULL);
	esp8266_connect_wifi(ESP8266_WIFI_SSID, ESP8266_WIFI_PASSWORD);
	esp8266_connect_mqtt();
	printf("[INIT] ESP8266 configured: SSID=%s, MQTT=%s:%d\r\n",
		   ESP8266_WIFI_SSID, ESP8266_MQTT_BROKER_HOST, ESP8266_MQTT_BROKER_PORT);

	/* 延迟后启动调度器 */
	Delay_Ms(1000);
	TIM_1ms_Start();

	printf("\r\n[INFO] System ready. Users=%d, Logs=%d\r\n",
		   user_db_get_count(), access_log_get_count());
	printf("[INFO] Scheduler running...\r\n\r\n");

	while (1)
	{
		scheduler_run();
	}
}
