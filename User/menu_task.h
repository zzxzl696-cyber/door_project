/********************************** (C) COPYRIGHT *******************************
* File Name          : menu_task.h
* Author             : Menu task for CH32V30x (Pure C)
* Version            : V2.0.0
* Date               : 2025-01-14
* Description        : 纯C语言菜单任务头文件
*********************************************************************************/

#ifndef __MENU_TASK_H
#define __MENU_TASK_H

#include <stdint.h>
#include "bsp_system.h"

// 全局菜单对象
extern simple_menu_t menu;

// 初始化菜单系统
void menu_init(void);

// 菜单输入任务(20ms周期调用)
void menu_input_task(void);

// 更新门禁状态显示
void menu_update_door_status(void);

#endif // __MENU_TASK_H
