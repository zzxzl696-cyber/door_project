/********************************** (C) COPYRIGHT *******************************
 * File Name          : menu_task.c
 * Author             : Menu task for CH32V30x (Pure C)
 * Version            : V2.0.0
 * Date               : 2025-01-14
 * Description        : 纯C语言菜单任务实现
 *********************************************************************************/

#include "bsp_system.h"

// 菜单对象(全局)
simple_menu_t menu;

// 菜单变量
static int test_value = 100;
static uint8_t test_bool = 0;
static int brightness = 50;

// 门禁状态显示缓冲区
static char door_lock_status_buf[16] = "Locked";
static char door_servo_angle_buf[16] = "0";
static char door_auth_method_buf[16] = "None";

// 菜单项
static menu_item_t item1 = {"Value:", MENU_ITEM_INT, &test_value, NULL, NULL};
static menu_item_t item2 = {"Enable:", MENU_ITEM_BOOL, &test_bool, NULL, NULL};
static menu_item_t item3 = {"Bright:", MENU_ITEM_INT, &brightness, NULL, NULL};

// 门禁状态菜单项
static menu_item_t item_door_lock = {"Lock:", MENU_ITEM_STRING, door_lock_status_buf, NULL, NULL};
static menu_item_t item_door_servo = {"Servo:", MENU_ITEM_STRING, door_servo_angle_buf, NULL, NULL};
static menu_item_t item_door_auth = {"Auth:", MENU_ITEM_STRING, door_auth_method_buf, NULL, NULL};

/**
 * @brief 初始化菜单系统
 */
void menu_init(void)
{
	printf("Menu Init (Pure C)...\r\n");

	// 初始化LCD
	LCD_Init();
	// 初始化菜单
	menu_c_init(&menu);

	// 添加菜单项
	menu_c_add_item(&menu, &item1);
	menu_c_add_item(&menu, &item2);
	menu_c_add_item(&menu, &item3);

	// 添加门禁状态菜单项
	menu_c_add_item(&menu, &item_door_lock);
	menu_c_add_item(&menu, &item_door_servo);
	menu_c_add_item(&menu, &item_door_auth);

	// 绘制菜单
	menu_c_draw(&menu);

	printf("Menu Init Complete\r\n");
}

/**
 * @brief 更新门禁状态显示
 * @note 从door_control模块读取最新状态并更新显示缓冲区
 */
void menu_update_door_status(void)
{
	// 更新门锁状态
	const char *lock_str = door_control_get_lock_state_str();
	snprintf(door_lock_status_buf, sizeof(door_lock_status_buf), "%s", lock_str);

	// 更新舵机角度
	snprintf(door_servo_angle_buf, sizeof(door_servo_angle_buf), "%d deg", g_door_status.servo_angle);

	// 更新认证方式
	const char *auth_str = door_control_get_auth_method_str();
	snprintf(door_auth_method_buf, sizeof(door_auth_method_buf), "%s", auth_str);

	// 重绘菜单以显示更新
	menu_c_draw(&menu);
}

/**
 * @brief 菜单输入任务
 * @note 20ms周期调用,处理按键输入
 */
void menu_input_task(void)
{
	menu_c_handle_key(&menu, MENU_KEY_OK);
}
