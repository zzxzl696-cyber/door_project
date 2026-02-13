/********************************** (C) COPYRIGHT *******************************
 * File Name          : door_status_ui.c
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-13
 * Description        : 门禁状态显示界面实现 - 实时状态反馈和用户交互
 *********************************************************************************/

#include "door_status_ui.h"
#include "lcd.h"
#include "lcd_init.h"
#include "timer_config.h"
#include "auth_manager.h"
#include <stdio.h>
#include <string.h>

/* ================= 布局常量定义 ================= */

// 屏幕尺寸
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   160

// 区域Y坐标
#define TITLE_Y_START   0
#define TITLE_Y_END     16
#define STATUS_Y_START  16
#define STATUS_Y_END    66
#define INFO_Y_START    66
#define INFO_Y_END      116
#define PROMPT_Y_START  116
#define PROMPT_Y_END    160

// 字体大小
#define FONT_SIZE_12    12
#define FONT_SIZE_16    16

/* ================= 全局变量 ================= */

static door_status_ui_t s_ui_state = {
    .current_state = UI_STATE_IDLE,
    .previous_state = UI_STATE_IDLE,
    .user_name = "",
    .password_display = "",
    .password_length = 0,
    .lockout_seconds = 0,
    .last_update_time = 0,
    .state_enter_time = 0,
    .blink_state = 0
};

/* ================= 内部函数声明 ================= */

static void draw_title_bar(void);
static void draw_status_area(void);
static void draw_info_area(void);
static void draw_prompt_area(void);
static void refresh_status_area(void);
static void refresh_info_area(void);
static void refresh_prompt_area(void);
static const char* get_prompt_message(void);
static uint16_t get_prompt_color(void);

/* ================= 初始化函数 ================= */

/**
 * @brief 初始化状态显示界面
 */
void door_status_ui_init(void)
{
    // 清空整个屏幕
    LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);

    // 绘制标题栏（仅一次）
    draw_title_bar();

    // 绘制初始状态
    draw_status_area();
    draw_info_area();
    draw_prompt_area();

    // 记录初始化时间
    s_ui_state.last_update_time = TIM_Get_MillisCounter();
    s_ui_state.state_enter_time = s_ui_state.last_update_time;
}

/* ================= 更新函数 ================= */

/**
 * @brief 更新状态显示界面
 */
void door_status_ui_update(void)
{
    uint32_t current_time = TIM_Get_MillisCounter();
    uint32_t elapsed = current_time - s_ui_state.last_update_time;

    // 根据当前状态更新界面
    switch (s_ui_state.current_state) {
        case UI_STATE_IDLE:
            // 待机状态，无需频繁更新
            break;

        case UI_STATE_AUTH_SUCCESS:
            // 认证成功3秒后返回待机
            if (current_time - s_ui_state.state_enter_time >= 3000) {
                door_status_ui_set_state(UI_STATE_IDLE);
            } else if (g_door_status.lock_state == DOOR_UNLOCKED) {
                // 更新倒计时
                if (elapsed >= 1000) {  // 每秒更新一次
                    refresh_info_area();
                    s_ui_state.last_update_time = current_time;
                }
            }
            break;

        case UI_STATE_AUTH_FAILED:
            // 认证失败2秒后返回待机
            if (current_time - s_ui_state.state_enter_time >= 2000) {
                door_status_ui_set_state(UI_STATE_IDLE);
            }
            break;

        case UI_STATE_LOCKED:
            // 更新锁定倒计时
            if (elapsed >= 1000) {  // 每秒更新一次
                uint32_t lockout_remaining = auth_manager_get_lockout_remaining();
                s_ui_state.lockout_seconds = (lockout_remaining + 999) / 1000;  // 向上取整

                if (s_ui_state.lockout_seconds == 0) {
                    // 锁定解除，返回待机
                    door_status_ui_set_state(UI_STATE_IDLE);
                } else {
                    // 更新闪烁状态
                    s_ui_state.blink_state = !s_ui_state.blink_state;
                    refresh_prompt_area();
                }
                s_ui_state.last_update_time = current_time;
            }
            break;

        case UI_STATE_INPUT_PASSWORD:
        case UI_STATE_READING_CARD:
        case UI_STATE_SCANNING_FINGER:
            // 这些状态由事件驱动更新
            break;
    }
}

/* ================= 状态控制函数 ================= */

/**
 * @brief 设置 UI 状态
 */
void door_status_ui_set_state(ui_state_t state)
{
    if (s_ui_state.current_state == state) {
        return;  // 状态未变化
    }

    s_ui_state.previous_state = s_ui_state.current_state;
    s_ui_state.current_state = state;
    s_ui_state.state_enter_time = TIM_Get_MillisCounter();

    // 根据状态更新界面
    switch (state) {
        case UI_STATE_IDLE:
            // 清空用户信息
            s_ui_state.user_name[0] = '\0';
            s_ui_state.password_length = 0;
            refresh_status_area();
            refresh_info_area();
            refresh_prompt_area();
            break;

        case UI_STATE_LOCKED:
            // 获取锁定剩余时间
            s_ui_state.lockout_seconds = (auth_manager_get_lockout_remaining() + 999) / 1000;
            refresh_prompt_area();
            break;

        case UI_STATE_AUTH_SUCCESS:
        case UI_STATE_AUTH_FAILED:
            refresh_status_area();
            refresh_info_area();
            refresh_prompt_area();
            break;

        default:
            refresh_prompt_area();
            break;
    }
}

/**
 * @brief 获取当前 UI 状态
 */
ui_state_t door_status_ui_get_state(void)
{
    return s_ui_state.current_state;
}

/* ================= 回调函数 ================= */

/**
 * @brief 认证开始回调
 */
void door_status_ui_on_auth_start(auth_method_t method)
{
    switch (method) {
        case AUTH_RFID:
            door_status_ui_set_state(UI_STATE_READING_CARD);
            break;
        case AUTH_FINGERPRINT:
            door_status_ui_set_state(UI_STATE_SCANNING_FINGER);
            break;
        case AUTH_PASSWORD:
            door_status_ui_set_state(UI_STATE_INPUT_PASSWORD);
            break;
        default:
            break;
    }
}

/**
 * @brief 认证结果回调
 */
void door_status_ui_on_auth_result(auth_method_t method,
                                   auth_result_t result,
                                   const user_entry_t *user)
{
    // 检查是否进入锁定状态
    if (auth_manager_is_locked()) {
        door_status_ui_set_state(UI_STATE_LOCKED);
        return;
    }

    if (result == AUTH_RESULT_SUCCESS) {
        // 认证成功，保存用户名
        if (user != NULL) {
            strncpy(s_ui_state.user_name, (const char*)user->name, sizeof(s_ui_state.user_name) - 1);
            s_ui_state.user_name[sizeof(s_ui_state.user_name) - 1] = '\0';
        } else {
            strcpy(s_ui_state.user_name, "Unknown");
        }
        door_status_ui_set_state(UI_STATE_AUTH_SUCCESS);
    } else {
        // 认证失败
        door_status_ui_set_state(UI_STATE_AUTH_FAILED);
    }
}

/**
 * @brief 密码输入更新回调
 */
void door_status_ui_on_password_input(uint8_t length)
{
    s_ui_state.password_length = length;

    // 更新密码显示缓冲
    uint8_t i;
    for (i = 0; i < length && i < 4; i++) {
        s_ui_state.password_display[i] = '*';
    }
    s_ui_state.password_display[i] = '\0';

    // 仅刷新提示区
    refresh_prompt_area();
}

/* ================= 绘制函数 ================= */

/**
 * @brief 绘制标题栏（仅初始化时调用一次）
 */
static void draw_title_bar(void)
{
    // 填充背景色（深蓝色）
    LCD_Fill(0, TITLE_Y_START, SCREEN_WIDTH, TITLE_Y_END, DARKBLUE);

    // 显示标题文字（白色，12pt，居中）
    const char* title = "DOOR ACCESS SYS";
    uint8_t title_len = strlen(title);
    uint8_t char_width = 6;  // 12pt字体每个字符宽约6像素
    uint16_t title_x = (SCREEN_WIDTH - title_len * char_width) / 2;

    LCD_ShowString(title_x, 2, (const uint8_t*)title, WHITE, DARKBLUE, FONT_SIZE_12, 0);
}

/**
 * @brief 绘制状态区
 */
static void draw_status_area(void)
{
    char buffer[32];
    uint16_t color;

    // 锁定状态
    if (g_door_status.lock_state == DOOR_LOCKED) {
        snprintf(buffer, sizeof(buffer), "Status: LOCKED");
        color = RED;
    } else {
        snprintf(buffer, sizeof(buffer), "Status: UNLOCKED");
        color = GREEN;
    }
    LCD_ShowString(2, 20, (const uint8_t*)buffer, color, BLACK, FONT_SIZE_12, 0);

    // 认证方式
    const char* method_str = door_control_get_auth_method_str();
    snprintf(buffer, sizeof(buffer), "Method: %s", method_str);
    LCD_ShowString(2, 35, (const uint8_t*)buffer, WHITE, BLACK, FONT_SIZE_12, 0);

    // 舵机角度
    snprintf(buffer, sizeof(buffer), "Angle:  %3d deg", g_door_status.servo_angle);
    LCD_ShowString(2, 50, (const uint8_t*)buffer, YELLOW, BLACK, FONT_SIZE_12, 0);
}

/**
 * @brief 绘制信息区
 */
static void draw_info_area(void)
{
    char buffer[32];

    // 用户名称
    if (s_ui_state.user_name[0] != '\0') {
        snprintf(buffer, sizeof(buffer), "User: %s", s_ui_state.user_name);
        LCD_ShowString(2, 70, (const uint8_t*)buffer, CYAN, BLACK, FONT_SIZE_12, 0);
    } else {
        LCD_ShowString(2, 70, (const uint8_t*)"User: ---", GRAY, BLACK, FONT_SIZE_12, 0);
    }

    // 剩余时间
    if (g_door_status.lock_state == DOOR_UNLOCKED && g_door_status.unlock_duration > 0) {
        uint32_t current_time = TIM_Get_MillisCounter();
        uint32_t elapsed = current_time - g_door_status.unlock_timestamp;
        int32_t remaining_ms = (int32_t)g_door_status.unlock_duration - (int32_t)elapsed;

        if (remaining_ms > 0) {
            uint8_t remaining_sec = (remaining_ms + 999) / 1000;  // 向上取整
            snprintf(buffer, sizeof(buffer), "Time: %d sec left", remaining_sec);
            LCD_ShowString(2, 85, (const uint8_t*)buffer, YELLOW, BLACK, FONT_SIZE_12, 0);
        } else {
            LCD_ShowString(2, 85, (const uint8_t*)"Time: Expired", GRAY, BLACK, FONT_SIZE_12, 0);
        }
    } else {
        LCD_ShowString(2, 85, (const uint8_t*)"Time: ---", GRAY, BLACK, FONT_SIZE_12, 0);
    }

    // 失败次数
    uint8_t fail_count = auth_manager_get_fail_count();
    uint16_t fail_color = (fail_count >= 3) ? RED : WHITE;
    snprintf(buffer, sizeof(buffer), "Fails: %d/5", fail_count);
    LCD_ShowString(2, 100, (const uint8_t*)buffer, fail_color, BLACK, FONT_SIZE_12, 0);
}

/**
 * @brief 绘制提示区
 */
static void draw_prompt_area(void)
{
    const char* message = get_prompt_message();
    uint16_t color = get_prompt_color();

    // 计算居中位置
    uint8_t msg_len = strlen(message);
    uint8_t char_width = 6;  // 12pt字体每个字符宽约6像素
    uint16_t msg_x = (SCREEN_WIDTH - msg_len * char_width) / 2;
    uint16_t msg_y = (PROMPT_Y_START + PROMPT_Y_END) / 2 - 6;  // 垂直居中

    LCD_ShowString(msg_x, msg_y, (const uint8_t*)message, color, BLACK, FONT_SIZE_12, 0);
}

/* ================= 刷新函数（分区域优化） ================= */

/**
 * @brief 刷新状态区
 */
static void refresh_status_area(void)
{
    LCD_Fill(0, STATUS_Y_START, SCREEN_WIDTH, STATUS_Y_END, BLACK);
    draw_status_area();
}

/**
 * @brief 刷新信息区
 */
static void refresh_info_area(void)
{
    LCD_Fill(0, INFO_Y_START, SCREEN_WIDTH, INFO_Y_END, BLACK);
    draw_info_area();
}

/**
 * @brief 刷新提示区
 */
static void refresh_prompt_area(void)
{
    LCD_Fill(0, PROMPT_Y_START, SCREEN_WIDTH, PROMPT_Y_END, BLACK);
    draw_prompt_area();
}

/* ================= 辅助函数 ================= */

/**
 * @brief 获取提示消息
 */
static const char* get_prompt_message(void)
{
    char buffer[32];

    switch (s_ui_state.current_state) {
        case UI_STATE_IDLE:
            return ">>> Waiting Auth <<<";

        case UI_STATE_READING_CARD:
            return "Reading Card...";

        case UI_STATE_SCANNING_FINGER:
            return "Scanning Finger...";

        case UI_STATE_INPUT_PASSWORD:
            // 动态构建密码显示
            snprintf(buffer, sizeof(buffer), "Password: %s", s_ui_state.password_display);
            // 注意：这里返回静态缓冲区，需要立即使用
            static char pwd_msg[32];
            strncpy(pwd_msg, buffer, sizeof(pwd_msg) - 1);
            pwd_msg[sizeof(pwd_msg) - 1] = '\0';
            return pwd_msg;

        case UI_STATE_AUTH_SUCCESS:
            return "Access Granted!";

        case UI_STATE_AUTH_FAILED:
            return "Access Denied!";

        case UI_STATE_LOCKED:
            // 动态构建锁定消息
            if (s_ui_state.blink_state) {
                snprintf(buffer, sizeof(buffer), "!!! LOCKED %ds !!!", s_ui_state.lockout_seconds);
                static char lock_msg[32];
                strncpy(lock_msg, buffer, sizeof(lock_msg) - 1);
                lock_msg[sizeof(lock_msg) - 1] = '\0';
                return lock_msg;
            } else {
                return "";  // 闪烁效果
            }

        default:
            return "";
    }
}

/**
 * @brief 获取提示消息颜色
 */
static uint16_t get_prompt_color(void)
{
    switch (s_ui_state.current_state) {
        case UI_STATE_IDLE:
            return CYAN;

        case UI_STATE_READING_CARD:
        case UI_STATE_SCANNING_FINGER:
        case UI_STATE_INPUT_PASSWORD:
            return YELLOW;

        case UI_STATE_AUTH_SUCCESS:
            return GREEN;

        case UI_STATE_AUTH_FAILED:
        case UI_STATE_LOCKED:
            return RED;

        default:
            return WHITE;
    }
}
