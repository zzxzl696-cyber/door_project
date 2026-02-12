/********************************** (C) COPYRIGHT *******************************
* File Name          : simple_menu_c.h
* Author             : Simple Menu System for CH32V30x (Pure C)
* Version            : V1.0.0
* Date               : 2025-01-14
* Description        : 纯C语言实现的简单菜单系统
*********************************************************************************/

#ifndef __SIMPLE_MENU_C_H
#define __SIMPLE_MENU_C_H

#include <stdint.h>
#include "lcd.h"

// 菜单配置
#define MENU_FONT_SIZE  12  // 字体大小(12或16)

// 按键定义
#define MENU_KEY_NONE   0
#define MENU_KEY_UP     1
#define MENU_KEY_DOWN   2
#define MENU_KEY_OK     3
#define MENU_KEY_CANCEL 4

// 菜单项类型
#define MENU_ITEM_INT   0
#define MENU_ITEM_BOOL  1
#define MENU_ITEM_LINK  2
#define MENU_ITEM_STRING 3  // 字符串显示项(只读)

// 菜单项结构
typedef struct menu_item {
    const char* title;
    uint8_t type;
    void* value;
    struct menu_item* next;
    struct menu_item* child;
} menu_item_t;

// 菜单结构
typedef struct {
    menu_item_t* root;
    menu_item_t* current;
    uint8_t selected;
    uint8_t editing;
} simple_menu_t;

// 菜单函数
void menu_c_init(simple_menu_t* menu);
void menu_c_add_item(simple_menu_t* menu, menu_item_t* item);
void menu_c_draw(simple_menu_t* menu);
void menu_c_handle_key(simple_menu_t* menu, uint8_t key);

#endif // __SIMPLE_MENU_C_H
