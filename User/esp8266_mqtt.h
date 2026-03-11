/********************************** (C) COPYRIGHT *******************************
 * File Name          : esp8266_mqtt.h
 * Author             : Custom Implementation
 * Version            : V1.0.0
 * Date               : 2026-02-15
 * Description        : ESP8266 MQTT 非阻塞接口
 *********************************************************************************/

#ifndef __ESP8266_MQTT_H
#define __ESP8266_MQTT_H

#include "ch32v30x.h"
#include "esp8266_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================= 类型定义 ================= */

typedef enum
{
    ESP8266_MQTT_STATE_DISCONNECTED = 0,
    ESP8266_MQTT_STATE_CONFIGURING,
    ESP8266_MQTT_STATE_CONNECTING,
    ESP8266_MQTT_STATE_CONNECTED,
    ESP8266_MQTT_STATE_SUBSCRIBED,
    ESP8266_MQTT_STATE_ERROR
} esp8266_mqtt_state_t;

typedef void (*esp8266_mqtt_on_message_t)(const char *topic,
                                          const uint8_t *payload,
                                          uint16_t len,
                                          void *user);

/* ================= Public API ================= */

/**
 * @brief  MQTT 模块初始化（不阻塞）
 * @retval None
 */
void esp8266_mqtt_init(void);

/**
 * @brief  5ms Tick 驱动函数（放入 scheduler 的 5ms 任务）
 * @retval None
 */
void esp8266_mqtt_tick_5ms(void);

/**
 * @brief  使用 esp8266_config.h 的默认配置发起 MQTT 连接
 * @retval 0=已受理，-1=参数/状态错误
 */
int esp8266_mqtt_connect(void);

/**
 * @brief  断开 MQTT 连接（发送 MQTTCLEAN）
 * @retval 0=已受理，-1=状态错误
 */
int esp8266_mqtt_disconnect(void);

/**
 * @brief  发布消息（AT+MQTTPUB）
 * @note   payload 中的双引号会自动转义，数据长度受 AT 命令长度限制
 * @param  topic: 发布主题
 * @param  payload: 消息内容
 * @param  len: 消息长度
 * @param  qos: QoS 等级（0/1）
 * @param  retain: retain 标志（0/1）
 * @retval 0=已受理，-1=状态错误
 */
int esp8266_mqtt_publish(const char *topic,
                         const uint8_t *payload,
                         uint16_t len,
                         uint8_t qos,
                         uint8_t retain);

/**
 * @brief  订阅主题（AT+MQTTSUB）
 * @param  topic: 订阅主题
 * @param  qos: QoS 等级（0/1）
 * @retval 0=已受理，-1=状态错误
 */
int esp8266_mqtt_subscribe(const char *topic, uint8_t qos);

/**
 * @brief  设置订阅消息回调
 * @param  cb: 回调函数
 * @param  user: 用户参数
 * @retval None
 */
void esp8266_mqtt_set_on_message(esp8266_mqtt_on_message_t cb, void *user);

/**
 * @brief  获取当前 MQTT 状态
 * @retval esp8266_mqtt_state_t
 */
esp8266_mqtt_state_t esp8266_mqtt_get_state(void);

/**
 * @brief  重置 MQTT 状态（内部使用）
 * @retval None
 */
void esp8266_mqtt_reset(void);

/**
 * @brief  处理 URC 文本行（内部使用）
 * @param  line: URC 文本行
 * @retval None
 */
void esp8266_mqtt_handle_urc_line(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* __ESP8266_MQTT_H */
