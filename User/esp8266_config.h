/********************************** (C) COPYRIGHT *******************************
 * File Name          : esp8266_config.h
 * Author             : Custom Implementation
 * Version            : V1.0.0
 * Date               : 2026-02-15
 * Description        : ESP8266 WiFi/MQTT 集中配置
 *********************************************************************************/

#ifndef __ESP8266_CONFIG_H
#define __ESP8266_CONFIG_H

/* ================= WiFi 配置 ================= */

#define ESP8266_WIFI_SSID "TP-LINK_E6E4"
#define ESP8266_WIFI_PASSWORD "23886414clmzgl"

/* ================= MQTT Broker 配置 ================= */

#define ESP8266_MQTT_BROKER_HOST "mqtts.heclouds.com"
#define ESP8266_MQTT_BROKER_PORT 1883
#define ESP8266_MQTT_USERNAME "3H2GSwZ0fO"
#define ESP8266_MQTT_PASSWORD "version=2018-10-31&res=products%2F3H2GSwZ0fO%2Fdevices%2Fch32&et=1777199156&method=md5&sign=WKYgeCjS4%2FHX%2BTV4a9w%2F6A%3D%3D"

/* ================= 设备信息 ================= */

#define ESP8266_PRODUCT_ID "3H2GSwZ0fO"
#define ESP8266_DEVICE_ID "ch32"
#define ESP8266_DEVICE_TOKEN ""

/* ================= MQTT 连接参数 ================= */

#define ESP8266_MQTT_LINK_ID 0
#define ESP8266_MQTT_SCHEME 1 /* 1=MQTT over TCP */
#define ESP8266_MQTT_CLIENT_ID "ch32"
#define ESP8266_MQTT_KEEPALIVE 60
#define ESP8266_MQTT_CLEAN_SESSION 1
#define ESP8266_MQTT_DEFAULT_QOS 1
#define ESP8266_MQTT_DEFAULT_RETAIN 0

/* TLS 相关配置（scheme=1 时可保持默认） */
#define ESP8266_MQTT_CERT_KEY_ID 0
#define ESP8266_MQTT_CA_ID 0
#define ESP8266_MQTT_PATH ""

/* ================= MQTT 主题配置 ================= */

#define ESP8266_MQTT_PUB_TOPIC "$sys/3H2GSwZ0fO/ch32/thing/property/post"
#define ESP8266_MQTT_SUB_TOPIC "$sys/3H2GSwZ0fO/ch32/#"
#define ESP8266_MQTT_SUB_QOS 1
#define ESP8266_MQTT_AUTO_SUBSCRIBE 1

/* ================= 缓冲与超时配置 ================= */

#define ESP8266_MQTT_TOPIC_MAX_LEN 128
#define ESP8266_MQTT_PAYLOAD_MAX_LEN 256

#define ESP8266_MQTT_CMD_TIMEOUT_MS 1000
#define ESP8266_MQTT_CONN_TIMEOUT_MS 10000
#define ESP8266_MQTT_SUB_TIMEOUT_MS 3000
#define ESP8266_MQTT_PUB_TIMEOUT_MS 3000
#define ESP8266_MQTT_CLEAN_TIMEOUT_MS 2000

#endif /* __ESP8266_CONFIG_H */
