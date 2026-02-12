/********************************** (C) COPYRIGHT *******************************
* File Name          : simple_menu_c.c
* Author             : Simple Menu System for CH32V30x (Pure C)
* Version            : V1.0.0
* Date               : 2025-01-14
* Description        : 纯C语言实现的简单菜单系统
*********************************************************************************/

#include "simple_menu_c.h"
#include "lcd.h"
#include <stdio.h>

// 内部函数声明
static void draw_item(menu_item_t* item, uint16_t y, uint8_t selected);
static void next_item(simple_menu_t* menu);
static void prev_item(simple_menu_t* menu);
static void enter_edit(simple_menu_t* menu);
static void exit_edit(simple_menu_t* menu);

/**
 * @brief 初始化菜单
 */
void menu_c_init(simple_menu_t* menu)
{
    menu->root = NULL;
    menu->current = NULL;
    menu->selected = 0;
    menu->editing = 0;

    // 清屏
    LCD_Fill(0, 0, 128, 160, BLACK);
}

/**
 * @brief 添加菜单项
 */
void menu_c_add_item(simple_menu_t* menu, menu_item_t* item)
{
	item->next = NULL;  // 关键：先断开 item 可能存在的链接
    if (!menu->root) {
        menu->root = item;
        menu->current = item;
    } else {
        menu_item_t* p = menu->root;
        while (p->next) p = p->next;
        p->next = item;
    }
}

/**
 * @brief 绘制菜单
 */
void menu_c_draw(simple_menu_t* menu)
{
    // 清屏
    LCD_Fill(0, 0, 128, 160, BLACK);

    // 绘制标题
    LCD_ShowString(5, 5, "Menu", CYAN, BLACK, MENU_FONT_SIZE, 0);

    // 绘制菜单项
    menu_item_t* item = menu->root;
    uint16_t y = 20;
    uint8_t idx = 0;

    while (item && y < 150) {
        draw_item(item, y, idx == menu->selected);
        item = item->next;
        y += 15;
        idx++;
    }
}

/**
 * @brief 绘制单个菜单项
 */
static void draw_item(menu_item_t* item, uint16_t y, uint8_t selected)
{
    uint16_t bg_color = selected ? BLUE : BLACK;
    uint16_t fg_color = WHITE;

    // 绘制背景
    if (selected) {
        LCD_Fill(0, y-2, 128, y+10, bg_color);
    }

    // 绘制标题
    LCD_ShowString(5, y, (char*)item->title, fg_color, bg_color, MENU_FONT_SIZE, 0);

    // 绘制值
    if (item->type == MENU_ITEM_INT) {
        char buf[16];
        sprintf(buf, "%d", *(int*)item->value);
        LCD_ShowString(80, y, buf, fg_color, bg_color, MENU_FONT_SIZE, 0);
    } else if (item->type == MENU_ITEM_BOOL) {
        const char* str = *(uint8_t*)item->value ? "ON" : "OFF";
        LCD_ShowString(80, y, (char*)str, fg_color, bg_color, MENU_FONT_SIZE, 0);
    } else if (item->type == MENU_ITEM_STRING) {
        // 字符串类型：直接显示字符串指针指向的内容
        const char* str = (const char*)item->value;
        LCD_ShowString(70, y, (char*)str, fg_color, bg_color, MENU_FONT_SIZE, 0);
    }
}

/**
 * @brief 处理按键
 */
void menu_c_handle_key(simple_menu_t* menu, uint8_t key)
{
    if (menu->editing) {
        // 编辑模式
        menu_item_t* item = menu->root;
        for (uint8_t i = 0; i < menu->selected && item; i++) {
            item = item->next;
        }

        if (!item) return;

        if (key == MENU_KEY_UP && item->type == MENU_ITEM_INT) {
            (*(int*)item->value)++;
        } else if (key == MENU_KEY_DOWN && item->type == MENU_ITEM_INT) {
            (*(int*)item->value)--;
        } else if (key == MENU_KEY_OK && item->type == MENU_ITEM_BOOL) {
            *(uint8_t*)item->value = !(*(uint8_t*)item->value);
        } else if (key == MENU_KEY_CANCEL) {
            exit_edit(menu);
        }
        menu_c_draw(menu);
    } else {
        // 浏览模式
        if (key == MENU_KEY_UP) {
            prev_item(menu);
        } else if (key == MENU_KEY_DOWN) {
            next_item(menu);
        } else if (key == MENU_KEY_OK) {
            enter_edit(menu);
        }
    }
}

/**
 * @brief 下一个菜单项
 */
static void next_item(simple_menu_t* menu)
{
    menu_item_t* item = menu->root;
    uint8_t count = 0;
    while (item) {
        count++;
        item = item->next;
    }

    menu->selected++;
    if (menu->selected >= count) {
        menu->selected = 0;
    }
    menu_c_draw(menu);
}

/**
 * @brief 上一个菜单项
 */
static void prev_item(simple_menu_t* menu)
{
    if (menu->selected == 0) {
        menu_item_t* item = menu->root;
        uint8_t count = 0;
        while (item) {
            count++;
            item = item->next;
        }
        menu->selected = count - 1;
    } else {
        menu->selected--;
    }
    menu_c_draw(menu);
}

/**
 * @brief 进入编辑模式
 */
static void enter_edit(simple_menu_t* menu)
{
    menu->editing = 1;
    menu_c_draw(menu);
}

/**
 * @brief 退出编辑模式
 */
static void exit_edit(simple_menu_t* menu)
{
    menu->editing = 0;
    menu_c_draw(menu);
}
