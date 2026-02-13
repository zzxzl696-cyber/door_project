/********************************** (C) COPYRIGHT *******************************
 * File Name          : door_status_ui.h
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-13
 * Description        : 门禁状态显示界面 - 实时状态反馈和用户交互
 *********************************************************************************/

#ifndef __DOOR_STATUS_UI_H
#define __DOOR_STATUS_UI_H

#include <stdint.h>
#include "door_control.h"
#include "user_database.h"
#include "access_log.h"

/* ================= UI 状态枚举 ================= */

/**
 * @brief UI 状态枚举
 */
typedef enum {
    UI_STATE_IDLE = 0,           /* 待机状态 */
    UI_STATE_READING_CARD,       /* 读卡中 */
    UI_STATE_SCANNING_FINGER,    /* 扫描指纹中 */
    UI_STATE_INPUT_PASSWORD,     /* 输入密码中 */
    UI_STATE_AUTH_SUCCESS,       /* 认证成功 */
    UI_STATE_AUTH_FAILED,        /* 认证失败 */
    UI_STATE_LOCKED              /* 系统锁定 */
} ui_state_t;

/* ================= UI 状态结构 ================= */

/**
 * @brief UI 状态结构体
 */
typedef struct {
    ui_state_t current_state;      /* 当前状态 */
    ui_state_t previous_state;     /* 上一状态 */
    char user_name[16];            /* 用户名缓冲 */
    char password_display[8];      /* 密码显示缓冲 "****" */
    uint8_t password_length;       /* 密码长度 */
    uint8_t lockout_seconds;       /* 锁定倒计时（秒） */
    uint32_t last_update_time;     /* 上次更新时间戳（ms） */
    uint32_t state_enter_time;     /* 状态进入时间戳（ms） */
    uint8_t blink_state;           /* 闪烁状态（用于告警） */
} door_status_ui_t;

/* ================= 初始化函数 ================= */

/**
 * @brief 初始化状态显示界面
 * @note  必须在LCD初始化之后调用
 */
void door_status_ui_init(void);

/* ================= 更新函数 ================= */

/**
 * @brief 更新状态显示界面
 * @note  由调度器周期调用（100ms）
 */
void door_status_ui_update(void);

/* ================= 状态控制函数 ================= */

/**
 * @brief 设置 UI 状态
 * @param state 新的 UI 状态
 */
void door_status_ui_set_state(ui_state_t state);

/**
 * @brief 获取当前 UI 状态
 * @return 当前状态
 */
ui_state_t door_status_ui_get_state(void);

/* ================= 回调函数（由其他模块调用） ================= */

/**
 * @brief 认证结果回调（由认证管理器调用）
 * @param method 认证方式
 * @param result 认证结果
 * @param user 用户信息（成功时非NULL）
 */
void door_status_ui_on_auth_result(auth_method_t method,
                                   auth_result_t result,
                                   const user_entry_t *user);

/**
 * @brief 密码输入更新回调（由密码输入模块调用）
 * @param length 当前输入长度（0-4）
 */
void door_status_ui_on_password_input(uint8_t length);

/**
 * @brief 认证开始回调（由认证管理器调用）
 * @param method 认证方式
 */
void door_status_ui_on_auth_start(auth_method_t method);

#endif /* __DOOR_STATUS_UI_H */
