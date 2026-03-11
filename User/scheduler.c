#include "bsp_system.h"
uint8_t task_num;

typedef struct
{
	void (*task_func)(void);
	uint32_t rate_ms;
	uint32_t last_run;
} task_t;

static task_t scheduler_task[] =
	{
		{esp8266_tick_5ms, 5, 0}, // ESP8266 驱动：5ms 周期（非阻塞状态机，使用USART1）
		{rfid_task, 5, 0},
		// {key_state, 10, 0},
		{matrix_key_scan, 10, 0},	   // 矩阵按键扫描：10ms周期
		{door_control_update, 100, 0}, // 门禁状态更新：100ms周期
		{auth_manager_update, 100, 0}, // 认证管理器更新：100ms周期
		{door_status_ui_update, 100, 0}, // UI状态更新：100ms周期
		{fingerprint_enroll_update, 100, 0}, // Fingerprint enroll update: 100ms
		{mqtt_app_tick, 500, 0},       // MQTT应用层：500ms周期（事件上报+心跳）
		{user_admin_update, 500, 0},   // 用户管理更新：500ms周期
};

/**
 * @brief 初始化调度器
 * 该函数用于初始化任务调度器，计算并设置系统中的任务数量
 */
void scheduler_init(void)
{
	task_num = sizeof(scheduler_task) / sizeof(task_t);
}

void scheduler_run(void)
{
	for (uint8_t i = 0; i < task_num; i++)
	{
		uint32_t now_time = TIM_Get_MillisCounter();
		if (now_time >= scheduler_task[i].rate_ms + scheduler_task[i].last_run)
		{
			scheduler_task[i].last_run = now_time;
			scheduler_task[i].task_func();
		}
	}
}
