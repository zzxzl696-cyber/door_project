/********************************** (C) COPYRIGHT *******************************
 * File Name          : bsp_system.h
 * Author             : System header file for CH32V30x project
 * Version            : V3.0.0
 * Date               : 2026-02-21
 * Description        : 系统统一头文件，包含所有模块声明
 *                      各 .c 文件只需 #include "bsp_system.h" 即可
 *********************************************************************************/

#ifndef BSP_SYSTEM_H
#define BSP_SYSTEM_H

/* ================= 标准库 ================= */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>

/* ================= CH32V30x 芯片库 ================= */

#include "ch32v30x.h"
#include "debug.h"

/* ================= 系统模块 ================= */

#include "timer_config.h"
#include "scheduler.h"

/* ================= 通信驱动 ================= */

#include "ringbuffer_large.h"
#include "usart1_dma_rx.h"

/* ================= 外设驱动 ================= */

#include "led.h"
#include "key_app.h"
#include "lcd_init.h"
#include "lcd.h"
/* 注意: lcdfont.h 含字体数组定义，不能放在此处，仅在 lcd.c 中单独 include */

/* ================= 功能模块 ================= */

#include "door_control.h"
#include "as608.h"
#include "by8301.h"
#include "rfid_reader.h"

/* ================= 任务模块 ================= */

#include "rfid_task.h"
#include "simple_menu_c.h"
#include "menu_task.h"

/* ================= 门禁认证系统 ================= */

#include "user_database.h"
#include "access_log.h"
#include "password_input.h"
#include "auth_manager.h"
#include "user_admin.h"
#include "door_status_ui.h"

/* ================= WiFi / MQTT ================= */

#include "esp8266_config.h"
#include "esp_at.h"
#include "esp8266.h"
#include "esp8266_mqtt.h"
#include "mqtt_app.h"

#endif /* BSP_SYSTEM_H */
