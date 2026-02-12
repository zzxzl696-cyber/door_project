/********************************** (C) COPYRIGHT *******************************
 * File Name          : bsp_system.h
 * Author             : System header file for CH32V30x project
 * Version            : V2.0.0
 * Date               : 2025-01-14
 * Description        : 系统头文件,包含所有C模块(纯C实现,无C++依赖)
 *********************************************************************************/

#ifndef BSP_SYSTEM_H
#define BSP_SYSTEM_H

// ========== 标准库 ==========
#include "stdio.h"
#include "stdarg.h"
#include "string.h"
#include "stdint.h"
#include "stdbool.h"

// ========== CH32V30x 芯片库 ==========
#include "ch32v30x.h"

// ========== 系统模块 ==========
#include "timer_config.h"
#include "scheduler.h"

// ========== 外设驱动 ==========
#include "led.h"
#include "key_app.h"

// ========== LCD 驱动 ==========
#include "lcd_init.h"
#include "lcd.h"

// ========== 功能模块 ==========
#include "func_num.h"
// ========== 菜单系统 (纯C) ==========
#include "simple_menu_c.h"
#include "menu_task.h"

#include "door_control.h"
#include "as608.h"
#include "by8301.h"
#include "rfid_reader.h"

// ========== 门禁认证系统 ==========
#include "user_database.h"
#include "access_log.h"
#include "password_input.h"
#include "auth_manager.h"
#include "user_admin.h"
#endif // BSP_SYSTEM_H
