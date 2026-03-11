/********************************** (C) COPYRIGHT *******************************
 * File Name          : esp8266.h
 * Author             : Custom Implementation
 * Version            : V1.0.0
 * Date               : 2026-02-15
 * Description        : ESP8266 WiFi 驱动（AT 指令，上云控制）
 *                      三层非阻塞结构：
 *                      1) usart1_dma_rx：USART1 DMA RX + 软件环形缓冲 + 阻塞 TX
 *                      2) esp_at：AT 命令队列/解析/超时
 *                      3) esp8266：WiFi/TCP 状态机与异步 API
 *
 * 状态机：BOOT -> BASIC_SETUP -> JOIN_AP -> TCP_CONNECT -> ONLINE
 *
 * 注意：本模块不使用 delay/阻塞等待，必须在 5ms 调度器中周期调用 esp8266_tick_5ms()。
 *********************************************************************************/

#ifndef __ESP8266_H
#define __ESP8266_H

#include "ch32v30x.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================= 类型定义 ================= */

typedef enum
{
    ESP8266_STATE_DISABLED = 0,
    ESP8266_STATE_BOOT,
    ESP8266_STATE_BASIC_SETUP,
    ESP8266_STATE_JOIN_AP,
    ESP8266_STATE_TCP_CONNECT,
    ESP8266_STATE_ONLINE,
    ESP8266_STATE_MQTT_DISCONNECTED,
    ESP8266_STATE_MQTT_CONFIGURING,
    ESP8266_STATE_MQTT_CONNECTING,
    ESP8266_STATE_MQTT_CONNECTED,
    ESP8266_STATE_MQTT_SUBSCRIBED,
    ESP8266_STATE_ERROR
} esp8266_state_t;

typedef enum
{
    ESP8266_EVENT_STATE_CHANGED = 0,
    ESP8266_EVENT_WIFI_CONNECTED,
    ESP8266_EVENT_WIFI_DISCONNECTED,
    ESP8266_EVENT_TCP_CONNECTED,
    ESP8266_EVENT_TCP_DISCONNECTED,
    ESP8266_EVENT_TCP_SEND_OK,
    ESP8266_EVENT_TCP_SEND_FAIL,
    ESP8266_EVENT_MQTT_CONNECTED,
    ESP8266_EVENT_MQTT_DISCONNECTED,
    ESP8266_EVENT_MQTT_SUBSCRIBED,
    ESP8266_EVENT_ERROR
} esp8266_event_t;

typedef void (*esp8266_on_rx_t)(const uint8_t *data, uint16_t len, void *user);
typedef void (*esp8266_on_event_t)(esp8266_event_t evt, esp8266_state_t state, void *user);

/* ================= Public API ================= */

/**
 * @brief  初始化 ESP8266 驱动（不阻塞）
 * @note   使用 USART1 (PA9/PA10) 通信，USART1 需在调用前由 main.c 初始化。
 * @retval None
 */
void esp8266_init(void);

/**
 * @brief  5ms Tick 驱动函数（放入 scheduler 的 5ms 任务）
 * @retval None
 */
void esp8266_tick_5ms(void);

/**
 * @brief  注册回调
 * @param  on_rx: TCP 数据接收回调（+IPD 推送数据）
 * @param  on_evt: 事件回调（状态变化/连接变化/发送结果等）
 * @param  user: 用户参数
 * @retval None
 */
void esp8266_set_callbacks(esp8266_on_rx_t on_rx, esp8266_on_event_t on_evt, void *user);

/**
 * @brief  请求连接 WiFi（异步）
 * @param  ssid: WiFi SSID（ASCII）
 * @param  password: WiFi 密码（ASCII）
 * @retval 0=已受理，-1=参数错误
 */
int esp8266_connect_wifi(const char *ssid, const char *password);

/**
 * @brief  请求连接 TCP（异步）
 * @param  host: 域名或 IP（ASCII）
 * @param  port: 端口
 * @retval 0=已受理，-1=参数错误
 */
int esp8266_tcp_connect(const char *host, uint16_t port);

/**
 * @brief  TCP 发送（异步，内部使用 AT+CIPSEND）
 * @note   data 指针必须在发送完成事件返回前保持有效（不拷贝数据）。
 * @param  data: 数据指针
 * @param  len: 数据长度
 * @retval 0=已入队，-1=忙/状态不允许
 */
int esp8266_tcp_send(const uint8_t *data, uint16_t len);

/**
 * @brief  璇锋眰杩炴帴 MQTT锛堝紓姝ワ級
 * @note   MQTT 具体发布/订阅接口见 esp8266_mqtt.h
 * @retval 0=宸插彈鐞嗭紝-1=鐘舵€佷笉鍏佽
 */
int esp8266_connect_mqtt(void);

/**
 * @brief  断开 MQTT 连接（发送 MQTTCLEAN）
 * @retval 0=宸插彈鐞嗭紝-1=状态错误
 */
int esp8266_disconnect_mqtt(void);

/**
 * @brief  获取当前状态
 * @retval esp8266_state_t
 */
esp8266_state_t esp8266_get_state(void);

/**
 * @brief  立即重置驱动状态（不会硬复位模块，仅清空队列并回到 BOOT）
 * @retval None
 */
void esp8266_reset(void);

/*
 * ================= 调度器集成示例 =================
 *
 * 1) 在 scheduler.c 顶部增加：
 *      #include "esp8266.h"
 *
 * 2) 在 task 数组里加入 5ms 任务：
 *      {esp8266_tick_5ms, 5, 0},
 *
 * 3) 在 main.c 初始化阶段调用：
 *      esp8266_init();
 *      esp8266_connect_wifi("ssid", "pwd");
 *      esp8266_tcp_connect("example.com", 1883);
 *
 */

#ifdef __cplusplus
}
#endif

#endif /* __ESP8266_H */
