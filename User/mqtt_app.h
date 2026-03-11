/********************************** (C) COPYRIGHT *******************************
 * File Name          : mqtt_app.h
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-26
 * Description        : MQTT 应用层 - 门禁业务与 MQTT 传输的桥梁
 *                      事件驱动上报 + 远程命令处理 + 心跳状态上报
 *                      兼容 OneNET 物联网平台 MQTT 协议
 *********************************************************************************/

#ifndef __MQTT_APP_H
#define __MQTT_APP_H

#include <stdint.h>
#include "door_control.h"
#include "access_log.h"
#include "user_database.h"

/**
 * @brief  初始化 MQTT 应用层
 */
void mqtt_app_init(void);

/**
 * @brief  MQTT 应用层周期任务（500ms 周期调用）
 * @note   处理事件队列（每次最多 1 个事件）和心跳定时上报
 */
void mqtt_app_tick(void);

/**
 * @brief  认证事件通知（由 auth_manager 回调触发）
 * @param  method 认证方式
 * @param  result 认证结果
 * @param  user   用户信息（成功时非 NULL）
 */
void mqtt_app_on_auth(auth_method_t method, auth_result_t result, const user_entry_t *user);

/**
 * @brief  门禁状态变化通知（由 door_control 调用）
 * @param  locked 1=锁定, 0=解锁
 */
void mqtt_app_on_door_state(uint8_t locked);

/**
 * @brief  MQTT 订阅消息回调（注册到 esp8266_mqtt）
 * @param  topic   主题
 * @param  payload 消息内容（非 null-terminated）
 * @param  len     消息长度
 * @param  user    用户参数（未使用）
 */
void mqtt_app_on_message(const char *topic, const uint8_t *payload, uint16_t len, void *user);

/**
 * @brief  通知 MQTT 已连接（重置心跳定时器）
 */
void mqtt_app_notify_connected(void);

/**
 * @brief  通知 MQTT 已断开
 */
void mqtt_app_notify_disconnected(void);

#endif /* __MQTT_APP_H */
