/********************************** (C) COPYRIGHT *******************************
 * File Name          : user_admin.h
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-05
 * Description        : 用户管理模块 - 添加/删除/查看用户
 *                      需要主密码验证后才能进入管理模式
 *********************************************************************************/

#ifndef __USER_ADMIN_H
#define __USER_ADMIN_H

#include <stdint.h>
#include "user_database.h"

/* 管理模式状态 */
typedef enum {
    ADMIN_STATE_LOCKED = 0,         /* 未验证主密码 */
    ADMIN_STATE_UNLOCKED,           /* 已验证，等待操作 */
    ADMIN_STATE_ADD_USER,           /* 添加用户流程 */
    ADMIN_STATE_DELETE_USER,        /* 删除用户流程 */
    ADMIN_STATE_LIST_USERS,         /* 查看用户列表 */
    ADMIN_STATE_CHANGE_MASTER_PWD   /* 修改主密码 */
} admin_state_t;

/* 添加用户流程步骤 */
typedef enum {
    ADD_STEP_NAME = 0,              /* 输入用户名 */
    ADD_STEP_RFID,                  /* 录入RFID卡 */
    ADD_STEP_FINGERPRINT,           /* 录入指纹 */
    ADD_STEP_PASSWORD,              /* 设置密码 */
    ADD_STEP_CONFIRM,               /* 确认添加 */
    ADD_STEP_DONE                   /* 添加完成 */
} add_user_step_t;

/* 管理操作回调 */
typedef void (*admin_callback_t)(admin_state_t state, uint8_t success);

/* ================= 初始化函数 ================= */

/**
 * @brief 初始化用户管理模块
 */
void user_admin_init(void);

/**
 * @brief 设置管理操作回调
 * @param callback 回调函数
 */
void user_admin_set_callback(admin_callback_t callback);

/* ================= 管理模式控制 ================= */

/**
 * @brief 尝试进入管理模式（需要主密码）
 * @param password 主密码
 * @return 1=成功进入, 0=密码错误
 */
uint8_t user_admin_enter(const uint8_t password[4]);

/**
 * @brief 退出管理模式
 */
void user_admin_exit(void);

/**
 * @brief 获取管理模式状态
 * @return 当前状态
 */
admin_state_t user_admin_get_state(void);

/**
 * @brief 检查是否在管理模式中
 * @return 1=是, 0=否
 */
uint8_t user_admin_is_active(void);

/* ================= 用户管理操作 ================= */

/**
 * @brief 开始添加用户流程
 */
void user_admin_start_add_user(void);

/**
 * @brief 开始删除用户流程
 */
void user_admin_start_delete_user(void);

/**
 * @brief 开始查看用户列表
 */
void user_admin_start_list_users(void);

/**
 * @brief 开始修改主密码
 */
void user_admin_start_change_master_pwd(void);

/* ================= 流程输入处理 ================= */

/**
 * @brief 处理按键输入（用于流程控制）
 * @param key_id 按键ID
 */
void user_admin_on_key(uint8_t key_id);

/**
 * @brief 处理RFID输入（用于录入RFID）
 * @param uid 4字节卡号
 */
void user_admin_on_rfid(const uint8_t uid[4]);

/**
 * @brief 处理指纹输入（用于录入指纹）
 * @param fp_id 指纹模板ID
 */
void user_admin_on_fingerprint(uint16_t fp_id);

/**
 * @brief 取消当前操作
 */
void user_admin_cancel(void);

/**
 * @brief 确认当前操作
 */
void user_admin_confirm(void);

/* ================= 信息获取 ================= */

/**
 * @brief 获取添加用户当前步骤
 * @return 当前步骤
 */
add_user_step_t user_admin_get_add_step(void);

/**
 * @brief 获取当前操作提示文本
 * @return 提示字符串
 */
const char *user_admin_get_prompt(void);

/**
 * @brief 获取用户列表（用于显示）
 * @param index 索引
 * @param out 输出用户信息
 * @return 1=成功, 0=超出范围
 */
uint8_t user_admin_get_user_at(uint16_t index, user_entry_t *out);

/**
 * @brief 获取当前选中的用户ID（用于删除）
 * @return 用户ID
 */
uint16_t user_admin_get_selected_user_id(void);

/**
 * @brief 设置选中的用户ID
 * @param user_id 用户ID
 */
void user_admin_set_selected_user_id(uint16_t user_id);

/* ================= 周期更新 ================= */

/**
 * @brief 管理模块周期更新
 * @note  由调度器调用，处理超时等
 */
void user_admin_update(void);

#endif /* __USER_ADMIN_H */
