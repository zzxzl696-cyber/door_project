/********************************** (C) COPYRIGHT *******************************
 * File Name          : door_control.c
 * Author             : Door Control Module
 * Version            : V1.0.0
 * Date               : 2026-01-20
 * Description        : 门禁控制模块实现
 *********************************************************************************/

#include "bsp_system.h"

// 全局门禁状态
door_control_status_t g_door_status = {
	.lock_state = DOOR_LOCKED,
	.servo_angle = SERVO_ANGLE_LOCKED,
	.last_auth_method = AUTH_NONE,
	.unlock_timestamp = 0,
	.unlock_duration = 0,
	.auth_fail_count = 0,
	.alarm_active = 0};

// 舵机PWM配置 (根据实际硬件调整)
#define SERVO_TIM TIM3
#define SERVO_TIM_CHANNEL TIM_Channel_1
#define SERVO_GPIO_PORT GPIOA
#define SERVO_GPIO_PIN GPIO_Pin_6
#define SERVO_TIM_RCC RCC_APB1Periph_TIM3
#define SERVO_GPIO_RCC RCC_APB2Periph_GPIOA

// 舵机PWM参数 (50Hz, 20ms周期)
#define SERVO_PWM_FREQ 50	   // 50Hz
#define SERVO_PWM_PERIOD 20000 // 20ms (单位: us)
#define SERVO_PULSE_MIN 500	   // 0.5ms (0度)
#define SERVO_PULSE_MAX 2500   // 2.5ms (180度)

// 内部函数声明
static void servo_pwm_init(void);
static void servo_set_angle(uint8_t angle);

/**
 * @brief 初始化门禁控制模块
 */
void door_control_init(void)
{
	printf("[DOOR] Initializing door control module...\r\n");

	// 初始化舵机PWM
	servo_pwm_init();

	// 初始化AS608指纹模块
	PS_StaGPIO_Init();

	// 初始化状态
	g_door_status.lock_state = DOOR_LOCKED;
	g_door_status.servo_angle = SERVO_ANGLE_LOCKED;
	g_door_status.last_auth_method = AUTH_NONE;
	g_door_status.unlock_timestamp = 0;
	g_door_status.unlock_duration = 0;
	g_door_status.auth_fail_count = 0;
	g_door_status.alarm_active = 0;

	// 确保舵机处于锁定角度
	servo_set_angle(SERVO_ANGLE_LOCKED);

	printf("[DOOR] Door control initialized\r\n");
}

/**
 * @brief 初始化舵机PWM
 */
static void servo_pwm_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
	TIM_OCInitTypeDef TIM_OCInitStructure = {0};

	// 使能时钟
	RCC_APB2PeriphClockCmd(SERVO_GPIO_RCC, ENABLE);
	RCC_APB1PeriphClockCmd(SERVO_TIM_RCC, ENABLE);

	// 配置GPIO为复用推挽输出
	GPIO_InitStructure.GPIO_Pin = SERVO_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SERVO_GPIO_PORT, &GPIO_InitStructure);

	// 配置定时器基础参数
	// 假设系统时钟72MHz, 预分频器设置为72-1, 得到1MHz计数频率
	// 周期设置为20000-1, 得到50Hz的PWM频率 (20ms周期)
	TIM_TimeBaseStructure.TIM_Period = 20000 - 1; // 20ms周期
	TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1; // 1MHz计数频率
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(SERVO_TIM, &TIM_TimeBaseStructure);

	// 配置PWM模式
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 1500; // 初始脉宽1.5ms (90度中位)
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC1Init(SERVO_TIM, &TIM_OCInitStructure);

	TIM_OC1PreloadConfig(SERVO_TIM, TIM_OCPreload_Enable);
	TIM_ARRPreloadConfig(SERVO_TIM, ENABLE);

	// 启动定时器
	TIM_Cmd(SERVO_TIM, ENABLE);

	printf("[SERVO] PWM initialized (50Hz, PA6/TIM3_CH1)\r\n");
}

/**
 * @brief 设置舵机角度
 * @param angle: 目标角度 (0-180度)
 */
static void servo_set_angle(uint8_t angle)
{
	uint16_t pulse_width;

	// 限制角度范围
	if (angle > 180)
	{
		angle = 180;
	}

	// 计算脉宽: 0度=0.5ms(500us), 180度=2.5ms(2500us)
	// pulse_width = 500 + (angle * 2000 / 180)
	pulse_width = SERVO_PULSE_MIN + (angle * (SERVO_PULSE_MAX - SERVO_PULSE_MIN) / 180);

	// 设置PWM占空比
	TIM_SetCompare1(SERVO_TIM, pulse_width);

	// 更新状态
	g_door_status.servo_angle = angle;

	printf("[SERVO] Angle set to %d degrees (pulse=%dus)\r\n", angle, pulse_width);
}

/**
 * @brief 解锁门禁
 * @param method: 认证方式
 * @param duration_ms: 解锁持续时间(毫秒)
 */
void door_control_unlock(auth_method_t method, uint32_t duration_ms)
{
	printf("[DOOR] Unlock request: method=%d, duration=%dms\r\n", method, duration_ms);

	// 更新状态
	g_door_status.lock_state = DOOR_UNLOCKED;
	g_door_status.last_auth_method = method;
	g_door_status.unlock_timestamp = TIM_Get_MillisCounter();
	g_door_status.unlock_duration = duration_ms;
	g_door_status.auth_fail_count = 0; // 成功认证，清零失败计数

	// 舵机转到解锁角度
	servo_set_angle(SERVO_ANGLE_UNLOCKED);

	// 通知 MQTT 应用层
	mqtt_app_on_door_state(0);

	printf("[DOOR] Door unlocked successfully\r\n");
}

/**
 * @brief 锁定门禁
 */
void door_control_lock(void)
{
	printf("[DOOR] Lock request\r\n");

	// 更新状态
	g_door_status.lock_state = DOOR_LOCKED;
	g_door_status.unlock_timestamp = 0;
	g_door_status.unlock_duration = 0;

	// 舵机转到锁定角度
	servo_set_angle(SERVO_ANGLE_LOCKED);

	// 通知 MQTT 应用层
	mqtt_app_on_door_state(1);

	printf("[DOOR] Door locked\r\n");
}

/**
 * @brief 更新门禁状态 (周期调用)
 * @note 应在主循环或定时任务中调用
 */
void door_control_update(void)
{
	// 检查自动锁定
	if (g_door_status.lock_state == DOOR_UNLOCKED &&
		g_door_status.unlock_duration > 0)
	{
		uint32_t current_time = TIM_Get_MillisCounter();
		uint32_t elapsed = current_time - g_door_status.unlock_timestamp;

		if (elapsed >= g_door_status.unlock_duration)
		{
			printf("[DOOR] Auto-lock timeout\r\n");
			door_control_lock();
		}
	}
}

/**
 * @brief 检查门是否锁定
 * @return 1=锁定, 0=未锁定
 */
uint8_t door_control_is_locked(void)
{
	return (g_door_status.lock_state == DOOR_LOCKED);
}

/**
 * @brief 获取门锁状态字符串
 */
const char *door_control_get_lock_state_str(void)
{
	switch (g_door_status.lock_state)
	{
	case DOOR_LOCKED:
		return "Locked";
	case DOOR_UNLOCKED:
		return "Unlocked";
	case DOOR_OPENING:
		return "Opening";
	case DOOR_ALARM:
		return "ALARM!";
	default:
		return "Unknown";
	}
}

/**
 * @brief 获取认证方式字符串
 */
const char *door_control_get_auth_method_str(void)
{
	switch (g_door_status.last_auth_method)
	{
	case AUTH_NONE:
		return "None";
	case AUTH_PASSWORD:
		return "Password";
	case AUTH_RFID:
		return "RFID";
	case AUTH_FINGERPRINT:
		return "Finger";
	case AUTH_FACE:
		return "Face";
	case AUTH_REMOTE:
		return "Remote";
	default:
		return "Unknown";
	}
}
