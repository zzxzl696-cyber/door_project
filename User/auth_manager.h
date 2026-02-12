/********************************** (C) COPYRIGHT *******************************
 * File Name          : auth_manager.h
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-05
 * Description        : 认证管理器 - 统一处理RFID、指纹、密码认证
 *********************************************************************************/

#ifndef __AUTH_MANAGER_H
#define __AUTH_MANAGER_H

#include <stdint.h>
#include "door_control.h"
#include "rfid_reader.h"
#include "as608.h"
#include "user_database.h"
#include "access_log.h"

/* 认证配置 */
#define AUTH_DOOR_OPEN_DURATION_MS  5000  /* 认证成功后开门时间 */
#define AUTH_MAX_FAIL_COUNT         5     /* 最大失败次数 */
#define AUTH_LOCKOUT_TIME_MS        60000 /* 锁定时间60秒 */

/* 认证管理器状态 */
typedef enum {
    AUTH_MGR_IDLE = 0,          /* 空闲，等待认证 */
    AUTH_MGR_PROCESSING,        /* 正在处理认证 */
    AUTH_MGR_PASSWORD_INPUT,    /* 等待密码输入 */
    AUTH_MGR_LOCKED             /* 认证锁定（失败太多次） */
} auth_mgr_state_t;

/* 认证结果回调 */
typedef void (*auth_result_callback_t)(auth_method_t method, auth_result_t result, const user_entry_t *user);

/* ================= 初始化函数 ================= */

/**
 * @brief 初始化认证管理器
 * @note  必须在user_db_init()和access_log_init()之后调用
 */
void auth_manager_init(void);

/**
 * @brief 设置认证结果回调
 * @param callback 回调函数
 */
void auth_manager_set_callback(auth_result_callback_t callback);

/* ================= 认证触发函数 ================= */

/**
 * @brief 处理RFID刷卡认证
 * @param frame RFID帧数据
 * @return 认证结果
 */
auth_result_t auth_process_rfid(const RFID_Frame *frame);

/**
 * @brief 处理指纹认证
 * @param result 指纹搜索结果
 * @return 认证结果
 */
auth_result_t auth_process_fingerprint(const SearchResult *result);

/**
 * @brief 开始密码认证
 * @note  启动密码输入流程，完成后自动处理
 */
void auth_start_password(void);

/**
 * @brief 处理密码认证（内部调用或直接调用）
 * @param password 4位密码
 * @return 认证结果
 */
auth_result_t auth_process_password(const uint8_t password[4]);

/* ================= 状态管理函数 ================= */

/**
 * @brief 获取认证管理器状态
 * @return 当前状态
 */
auth_mgr_state_t auth_manager_get_state(void);

/**
 * @brief 获取认证失败次数
 * @return 失败次数
 */
uint8_t auth_manager_get_fail_count(void);

/**
 * @brief 重置认证失败计数
 */
void auth_manager_reset_fail_count(void);

/**
 * @brief 检查是否处于锁定状态
 * @return 1=锁定, 0=未锁定
 */
uint8_t auth_manager_is_locked(void);

/**
 * @brief 获取锁定剩余时间（毫秒）
 * @return 剩余时间，0表示未锁定
 */
uint32_t auth_manager_get_lockout_remaining(void);

/* ================= 周期更新函数 ================= */

/**
 * @brief 认证管理器周期更新
 * @note  由调度器周期调用，处理超时和状态
 */
void auth_manager_update(void);

#endif /* __AUTH_MANAGER_H */
