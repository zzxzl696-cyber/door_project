/********************************** (C) COPYRIGHT *******************************
 * File Name          : door_control.h
 * Author             : Door Control Module
 * Version            : V1.0.0
 * Date               : 2026-01-20
 * Description        : 门禁控制模块 - 状态管理和控制接口
 *********************************************************************************/

#ifndef __DOOR_CONTROL_H
#define __DOOR_CONTROL_H

#include <stdint.h>

// 门锁状态枚举
typedef enum
{
	DOOR_LOCKED = 0,   // 门已锁定
	DOOR_UNLOCKED = 1, // 门已解锁
	DOOR_OPENING = 2,  // 门正在开启
	DOOR_ALARM = 3	   // 门异常告警
} door_lock_state_t;

// 舵机角度定义
#define SERVO_ANGLE_LOCKED 45	 // 锁定角度(0度)
#define SERVO_ANGLE_UNLOCKED 180 // 解锁角度(90度)

// 认证方式枚举
typedef enum
{
	AUTH_NONE = 0,		  // 未认证
	AUTH_PASSWORD = 1,	  // 密码认证
	AUTH_RFID = 2,		  // RFID卡认证
	AUTH_FINGERPRINT = 3, // 指纹认证
	AUTH_FACE = 4,		  // 人脸认证
	AUTH_REMOTE = 5		  // 远程开门
} auth_method_t;

// 门禁控制状态结构
typedef struct
{
	door_lock_state_t lock_state;	// 门锁状态
	uint8_t servo_angle;			// 舵机当前角度
	auth_method_t last_auth_method; // 最后认证方式
	uint32_t unlock_timestamp;		// 解锁时间戳(ms)
	uint32_t unlock_duration;		// 解锁持续时间(ms)
	uint8_t auth_fail_count;		// 认证失败次数
	uint8_t alarm_active;			// 告警激活标志
} door_control_status_t;

// 全局门禁状态
extern door_control_status_t g_door_status;

// 门禁控制函数
void door_control_init(void);
void door_control_unlock(auth_method_t method, uint32_t duration_ms);
void door_control_lock(void);
void door_control_update(void);
uint8_t door_control_is_locked(void);
const char *door_control_get_lock_state_str(void);
const char *door_control_get_auth_method_str(void);

#endif // __DOOR_CONTROL_H
