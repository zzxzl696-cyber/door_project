/********************************** (C) COPYRIGHT *******************************
 * File Name          : password_input.h
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-05
 * Description        : 密码输入模块 - 4位数字密码输入处理
 *                      键盘映射：键1-9=数字1-9, 键10=数字0
 *********************************************************************************/

#ifndef __PASSWORD_INPUT_H
#define __PASSWORD_INPUT_H

#include <stdint.h>

/* 密码配置 */
#define PWD_INPUT_LENGTH        4   /* 密码长度 */
#define PWD_INPUT_TIMEOUT_MS    10000  /* 输入超时10秒 */

/* 密码输入状态 */
typedef enum {
    PWD_STATE_IDLE = 0,         /* 空闲状态 */
    PWD_STATE_INPUTTING,        /* 正在输入 */
    PWD_STATE_COMPLETE,         /* 输入完成 */
    PWD_STATE_TIMEOUT,          /* 输入超时 */
    PWD_STATE_CANCELLED         /* 已取消 */
} pwd_input_state_t;

/* 密码输入回调函数类型 */
typedef void (*pwd_input_callback_t)(const uint8_t *password, uint8_t length, pwd_input_state_t state);

/* UI 更新回调函数类型 */
typedef void (*pwd_ui_update_callback_t)(uint8_t length);

/* ================= 初始化函数 ================= */

/**
 * @brief 初始化密码输入模块
 */
void pwd_input_init(void);

/**
 * @brief 设置 UI 更新回调
 * @param callback UI 更新回调函数
 */
void pwd_input_set_ui_callback(pwd_ui_update_callback_t callback);

/* ================= 输入控制函数 ================= */

/**
 * @brief 开始密码输入
 * @param callback 完成回调（可为NULL）
 */
void pwd_input_start(pwd_input_callback_t callback);

/**
 * @brief 取消密码输入
 */
void pwd_input_cancel(void);

/**
 * @brief 处理按键输入
 * @param key_id 按键ID (1-16，矩阵键盘)
 * @note  键1-9=数字1-9, 键10=数字0, 键11-16保留
 */
void pwd_input_on_key(uint8_t key_id);

/**
 * @brief 周期更新（检查超时）
 * @note  由调度器周期调用
 */
void pwd_input_update(void);

/* ================= 状态查询函数 ================= */

/**
 * @brief 获取当前输入状态
 * @return 输入状态
 */
pwd_input_state_t pwd_input_get_state(void);

/**
 * @brief 获取已输入的密码长度
 * @return 已输入位数
 */
uint8_t pwd_input_get_length(void);

/**
 * @brief 获取已输入的密码（仅在COMPLETE状态有效）
 * @param out 输出缓冲区（至少PWD_INPUT_LENGTH字节）
 * @return 密码长度
 */
uint8_t pwd_input_get_password(uint8_t *out);

/**
 * @brief 清除密码输入
 */
void pwd_input_clear(void);

/**
 * @brief 检查是否正在输入
 * @return 1=正在输入, 0=否
 */
uint8_t pwd_input_is_active(void);

#endif /* __PASSWORD_INPUT_H */
