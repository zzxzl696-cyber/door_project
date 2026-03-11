/********************************** (C) COPYRIGHT *******************************
 * File Name          : esp8266.c
 * Author             : Custom Implementation
 * Version            : V1.0.0
 * Date               : 2026-02-15
 * Description        : ESP8266 WiFi 驱动（AT 指令，上云控制）实现
 *********************************************************************************/

#include "bsp_system.h"

/* 调试开关（默认关闭，避免打印影响时序） */
#ifndef ESP8266_DEBUG
#define ESP8266_DEBUG 0
#endif

#if ESP8266_DEBUG
#define ESP8266_LOG(...) printf(__VA_ARGS__)
#else
#define ESP8266_LOG(...)
#endif

typedef enum
{
	ESP8266_CMD_AT = 1,
	ESP8266_CMD_RST,
	ESP8266_CMD_ATE0,
	ESP8266_CMD_CWMODE,
	ESP8266_CMD_CIPRECVMODE,
	ESP8266_CMD_CWJAP,
	ESP8266_CMD_CIPSTART,
	ESP8266_CMD_CIPCLOSE,
	ESP8266_CMD_CIPSEND
} esp8266_cmd_tag_t;

typedef struct
{
	uint8_t inited;

	esp8266_state_t state;

	uint8_t wifi_req;
	uint8_t tcp_req;
	uint8_t mqtt_req;

	uint8_t wifi_connected;
	uint8_t tcp_connected;
	uint8_t tcp_need_close;

	char ssid[64];
	char password[64];
	char host[64];
	uint16_t port;

	uint8_t boot_delay_ticks; /* 5ms tick */
	uint8_t boot_step;		  /* 0=wait,1=AT sent,2=RST pending,3=RST sent */
	uint8_t setup_step;

	esp8266_on_rx_t on_rx;
	esp8266_on_event_t on_evt;
	void *cb_user;

	esp8266_mqtt_state_t last_mqtt_state;

	/* TCP send in-flight（不拷贝 payload） */
	const uint8_t *send_ptr;
	uint16_t send_len;
	uint8_t send_busy;

	char cmd_buf[ESP_AT_CMD_MAX_LEN];
} esp8266_ctx_t;

static esp8266_ctx_t s_ctx = {0};

static void esp8266_set_state(esp8266_state_t st, esp8266_event_t reason_evt)
{
	if (s_ctx.state != st)
	{
		s_ctx.state = st;

		if (s_ctx.on_evt != NULL)
		{
			if (reason_evt != ESP8266_EVENT_STATE_CHANGED)
			{
				s_ctx.on_evt(reason_evt, s_ctx.state, s_ctx.cb_user);
			}
			s_ctx.on_evt(ESP8266_EVENT_STATE_CHANGED, s_ctx.state, s_ctx.cb_user);
		}
	}
	else
	{
		if (s_ctx.on_evt != NULL && reason_evt != ESP8266_EVENT_STATE_CHANGED)
		{
			s_ctx.on_evt(reason_evt, s_ctx.state, s_ctx.cb_user);
		}
	}
}

static uint8_t esp8266_str_contains(const char *s, const char *sub)
{
	if (s == NULL || sub == NULL)
	{
		return 0;
	}
	return (strstr(s, sub) != NULL) ? 1 : 0;
}

static esp8266_state_t esp8266_map_mqtt_state(esp8266_mqtt_state_t st)
{
	switch (st)
	{
	case ESP8266_MQTT_STATE_DISCONNECTED:
		return ESP8266_STATE_MQTT_DISCONNECTED;

	case ESP8266_MQTT_STATE_CONFIGURING:
		return ESP8266_STATE_MQTT_CONFIGURING;

	case ESP8266_MQTT_STATE_CONNECTING:
		return ESP8266_STATE_MQTT_CONNECTING;

	case ESP8266_MQTT_STATE_CONNECTED:
		return ESP8266_STATE_MQTT_CONNECTED;

	case ESP8266_MQTT_STATE_SUBSCRIBED:
		return ESP8266_STATE_MQTT_SUBSCRIBED;

	case ESP8266_MQTT_STATE_ERROR:
	default:
		return ESP8266_STATE_ERROR;
	}
}

static esp8266_event_t esp8266_map_mqtt_event(esp8266_mqtt_state_t st)
{
	switch (st)
	{
	case ESP8266_MQTT_STATE_CONNECTED:
		return ESP8266_EVENT_MQTT_CONNECTED;

	case ESP8266_MQTT_STATE_DISCONNECTED:
		return ESP8266_EVENT_MQTT_DISCONNECTED;

	case ESP8266_MQTT_STATE_SUBSCRIBED:
		return ESP8266_EVENT_MQTT_SUBSCRIBED;

	default:
		return ESP8266_EVENT_STATE_CHANGED;
	}
}

static void esp8266_sync_mqtt_state(void)
{
	esp8266_mqtt_state_t cur = esp8266_mqtt_get_state();
	esp8266_state_t mapped = esp8266_map_mqtt_state(cur);

	if (cur == s_ctx.last_mqtt_state && s_ctx.state == mapped)
	{
		return;
	}

	s_ctx.last_mqtt_state = cur;
	esp8266_set_state(mapped, esp8266_map_mqtt_event(cur));
}

static void esp8266_at_cmd_done(esp_at_result_t result, void *user)
{
	esp8266_cmd_tag_t tag = (esp8266_cmd_tag_t)(uint32_t)user;

	if (result != ESP_AT_RES_OK)
	{
		ESP8266_LOG("[ESP] CMD fail tag=%d res=%d\r\n", (int)tag, (int)result);

		if (tag == ESP8266_CMD_AT)
		{
			/* 上电阶段 AT 探测失败：延迟后重试 */
			s_ctx.boot_delay_ticks = 20; /* 100ms */
			s_ctx.boot_step = 0;
			return;
		}

		if (tag == ESP8266_CMD_CWJAP)
		{
			/* Join AP 失败：仍保持 JOIN_AP，等待上层重试或下一次 tick 再发起 */
			s_ctx.wifi_connected = 0;
			esp8266_set_state(ESP8266_STATE_JOIN_AP, ESP8266_EVENT_ERROR);
			return;
		}

		if (tag == ESP8266_CMD_CIPSTART)
		{
			s_ctx.tcp_connected = 0;
			esp8266_set_state(ESP8266_STATE_TCP_CONNECT, ESP8266_EVENT_TCP_DISCONNECTED);
			return;
		}

		if (tag == ESP8266_CMD_CIPSEND)
		{
			s_ctx.send_busy = 0;
			if (s_ctx.on_evt != NULL)
			{
				s_ctx.on_evt(ESP8266_EVENT_TCP_SEND_FAIL, s_ctx.state, s_ctx.cb_user);
			}
			return;
		}

		esp8266_set_state(ESP8266_STATE_ERROR, ESP8266_EVENT_ERROR);
		return;
	}

	/* 成功路径 */
	switch (tag)
	{
	case ESP8266_CMD_AT:
		/* AT 探测通过，下一步发 RST */
		s_ctx.boot_delay_ticks = 2; /* 10ms */
		s_ctx.boot_step = 2;
		break;

	case ESP8266_CMD_RST:
		esp8266_set_state(ESP8266_STATE_BASIC_SETUP, ESP8266_EVENT_STATE_CHANGED);
		s_ctx.setup_step = 0;
		s_ctx.boot_delay_ticks = 400; /* 2000ms 等待模块重启完成（ready URC 可提前清零） */
		s_ctx.boot_step = 3;
		break;

	case ESP8266_CMD_ATE0:
	case ESP8266_CMD_CWMODE:
	case ESP8266_CMD_CIPRECVMODE:
		s_ctx.setup_step++;
		break;

	case ESP8266_CMD_CWJAP:
		s_ctx.wifi_connected = 1;
		if (s_ctx.on_evt != NULL)
		{
			s_ctx.on_evt(ESP8266_EVENT_WIFI_CONNECTED, s_ctx.state, s_ctx.cb_user);
		}
		if (s_ctx.mqtt_req)
		{
			esp8266_set_state(ESP8266_STATE_MQTT_CONFIGURING, ESP8266_EVENT_STATE_CHANGED);
		}
		else
		{
			esp8266_set_state(ESP8266_STATE_TCP_CONNECT, ESP8266_EVENT_STATE_CHANGED);
		}
		break;

	case ESP8266_CMD_CIPSTART:
		s_ctx.tcp_connected = 1;
		if (s_ctx.on_evt != NULL)
		{
			s_ctx.on_evt(ESP8266_EVENT_TCP_CONNECTED, s_ctx.state, s_ctx.cb_user);
		}
		esp8266_set_state(ESP8266_STATE_ONLINE, ESP8266_EVENT_STATE_CHANGED);
		break;

	case ESP8266_CMD_CIPCLOSE:
		s_ctx.tcp_connected = 0;
		if (s_ctx.on_evt != NULL)
		{
			s_ctx.on_evt(ESP8266_EVENT_TCP_DISCONNECTED, s_ctx.state, s_ctx.cb_user);
		}
		break;

	case ESP8266_CMD_CIPSEND:
		s_ctx.send_busy = 0;
		if (s_ctx.on_evt != NULL)
		{
			s_ctx.on_evt(ESP8266_EVENT_TCP_SEND_OK, s_ctx.state, s_ctx.cb_user);
		}
		break;

	default:
		break;
	}
}

static void esp8266_at_urc_cb(esp_at_urc_type_t type, const uint8_t *data, uint16_t len, void *user)
{
	(void)user;

	if (type == ESP_AT_URC_IPD)
	{
		if (s_ctx.on_rx != NULL && s_ctx.state == ESP8266_STATE_ONLINE)
		{
			s_ctx.on_rx(data, len, s_ctx.cb_user);
		}
		return;
	}

	if (type != ESP_AT_URC_LINE || data == NULL || len == 0)
	{
		return;
	}

	/* data 为以 '\0' 结尾的行字符串（esp_at.c 内部保证） */
	const char *line = (const char *)data;

	/* 模块重启完成信号：收到后立即允许进行 BASIC_SETUP，无需等满 2000ms */
	if (s_ctx.state == ESP8266_STATE_BASIC_SETUP && s_ctx.boot_delay_ticks > 0)
	{
		if (strcmp(line, "ready") == 0 || strcmp(line, "READY") == 0)
		{
			s_ctx.boot_delay_ticks = 0;
		}
		return;
	}

	/* 典型 URC：WIFI DISCONNECT / WIFI GOT IP / CONNECT / CLOSED */
	if (esp8266_str_contains(line, "WIFI DISCONNECT") || esp8266_str_contains(line, "WIFI DISCONNECTED"))
	{
		s_ctx.wifi_connected = 0;
		s_ctx.tcp_connected = 0;
		esp8266_mqtt_reset();
		if (s_ctx.on_evt != NULL)
		{
			s_ctx.on_evt(ESP8266_EVENT_WIFI_DISCONNECTED, s_ctx.state, s_ctx.cb_user);
			s_ctx.on_evt(ESP8266_EVENT_TCP_DISCONNECTED, s_ctx.state, s_ctx.cb_user);
		}
		esp8266_set_state(ESP8266_STATE_JOIN_AP, ESP8266_EVENT_STATE_CHANGED);
		return;
	}

	if (esp8266_str_contains(line, "CLOSED"))
	{
		s_ctx.tcp_connected = 0;
		if (s_ctx.on_evt != NULL)
		{
			s_ctx.on_evt(ESP8266_EVENT_TCP_DISCONNECTED, s_ctx.state, s_ctx.cb_user);
		}
		if (s_ctx.tcp_req)
		{
			esp8266_set_state(ESP8266_STATE_TCP_CONNECT, ESP8266_EVENT_STATE_CHANGED);
		}
		return;
	}

	esp8266_mqtt_handle_urc_line(line);
}

void esp8266_init(void)
{
	if (s_ctx.inited)
	{
		return;
	}

	memset(&s_ctx, 0, sizeof(s_ctx));

	/* USART1 已在 main.c 中初始化，此处直接绑定 IO */

	/* 初始化 AT 引擎 */
	esp_at_io_t io = {0};
	io.write = USART1_TX_Write;
	io.read_byte = USART1_RX_ReadByte;
	esp_at_init(&io, esp8266_at_urc_cb, NULL);
	esp8266_mqtt_init();
	s_ctx.last_mqtt_state = esp8266_mqtt_get_state();

	s_ctx.inited = 1;
	s_ctx.state = ESP8266_STATE_BOOT;
	s_ctx.boot_delay_ticks = 40; /* 200ms：等待模块上电稳定 */
	s_ctx.boot_step = 0;

	ESP8266_LOG("[ESP] init done\r\n");
}

void esp8266_set_callbacks(esp8266_on_rx_t on_rx, esp8266_on_event_t on_evt, void *user)
{
	s_ctx.on_rx = on_rx;
	s_ctx.on_evt = on_evt;
	s_ctx.cb_user = user;
}

int esp8266_connect_wifi(const char *ssid, const char *password)
{
	if (ssid == NULL || password == NULL)
	{
		return -1;
	}

	strncpy(s_ctx.ssid, ssid, sizeof(s_ctx.ssid) - 1);
	strncpy(s_ctx.password, password, sizeof(s_ctx.password) - 1);
	s_ctx.wifi_req = 1;
	s_ctx.wifi_connected = 0;

	if (s_ctx.state == ESP8266_STATE_ONLINE ||
		s_ctx.state == ESP8266_STATE_TCP_CONNECT ||
		s_ctx.state == ESP8266_STATE_MQTT_DISCONNECTED ||
		s_ctx.state == ESP8266_STATE_MQTT_CONFIGURING ||
		s_ctx.state == ESP8266_STATE_MQTT_CONNECTING ||
		s_ctx.state == ESP8266_STATE_MQTT_CONNECTED ||
		s_ctx.state == ESP8266_STATE_MQTT_SUBSCRIBED)
	{
		esp8266_set_state(ESP8266_STATE_JOIN_AP, ESP8266_EVENT_STATE_CHANGED);
	}

	return 0;
}

int esp8266_tcp_connect(const char *host, uint16_t port)
{
	if (host == NULL || port == 0)
	{
		return -1;
	}

	if (s_ctx.mqtt_req)
	{
		esp8266_mqtt_disconnect();
		s_ctx.mqtt_req = 0;
	}

	strncpy(s_ctx.host, host, sizeof(s_ctx.host) - 1);
	s_ctx.port = port;
	s_ctx.tcp_req = 1;
	if (s_ctx.tcp_connected)
	{
		s_ctx.tcp_need_close = 1;
	}
	s_ctx.tcp_connected = 0;

	if (s_ctx.state == ESP8266_STATE_ONLINE ||
		s_ctx.state == ESP8266_STATE_MQTT_DISCONNECTED ||
		s_ctx.state == ESP8266_STATE_MQTT_CONFIGURING ||
		s_ctx.state == ESP8266_STATE_MQTT_CONNECTING ||
		s_ctx.state == ESP8266_STATE_MQTT_CONNECTED ||
		s_ctx.state == ESP8266_STATE_MQTT_SUBSCRIBED)
	{
		esp8266_set_state(ESP8266_STATE_TCP_CONNECT, ESP8266_EVENT_STATE_CHANGED);
	}

	return 0;
}

int esp8266_tcp_send(const uint8_t *data, uint16_t len)
{
	if (data == NULL || len == 0)
	{
		return -1;
	}

	if (s_ctx.state != ESP8266_STATE_ONLINE || !s_ctx.tcp_connected)
	{
		return -1;
	}

	if (s_ctx.send_busy)
	{
		return -1;
	}

	/* 只在 AT 引擎空闲时发起，避免队列堆积导致 payload 生命周期难管理 */
	if (!esp_at_is_idle())
	{
		return -1;
	}

	snprintf(s_ctx.cmd_buf, sizeof(s_ctx.cmd_buf), "AT+CIPSEND=%u\r\n", (unsigned)len);

	s_ctx.send_ptr = data;
	s_ctx.send_len = len;
	s_ctx.send_busy = 1;

	esp_at_result_t r = esp_at_enqueue_cmd_with_payload(s_ctx.cmd_buf,
														2000,
														s_ctx.send_ptr,
														s_ctx.send_len,
														8000,
														esp8266_at_cmd_done,
														(void *)ESP8266_CMD_CIPSEND);
	if (r != ESP_AT_RES_OK)
	{
		s_ctx.send_busy = 0;
		return -1;
	}

	return 0;
}

int esp8266_connect_mqtt(void)
{
	if (!s_ctx.inited)
	{
		return -1;
	}

	if (s_ctx.tcp_connected)
	{
		if (!esp_at_is_idle())
		{
			return -1;
		}

		esp_at_result_t r = esp_at_enqueue_cmd("AT+CIPCLOSE\r\n", 2000, esp8266_at_cmd_done, (void *)ESP8266_CMD_CIPCLOSE);
		if (r != ESP_AT_RES_OK)
		{
			return -1;
		}
	}

	s_ctx.mqtt_req = 1;
	s_ctx.tcp_req = 0;

	esp8266_mqtt_connect();

	if (s_ctx.wifi_connected)
	{
		esp8266_set_state(ESP8266_STATE_MQTT_CONFIGURING, ESP8266_EVENT_STATE_CHANGED);
	}
	else if (s_ctx.state == ESP8266_STATE_ONLINE || s_ctx.state == ESP8266_STATE_TCP_CONNECT)
	{
		esp8266_set_state(ESP8266_STATE_JOIN_AP, ESP8266_EVENT_STATE_CHANGED);
	}

	return 0;
}

int esp8266_disconnect_mqtt(void)
{
	if (!s_ctx.inited)
	{
		return -1;
	}

	s_ctx.mqtt_req = 0;
	esp8266_mqtt_disconnect();
	esp8266_set_state(ESP8266_STATE_MQTT_DISCONNECTED, ESP8266_EVENT_MQTT_DISCONNECTED);
	return 0;
}

esp8266_state_t esp8266_get_state(void)
{
	return s_ctx.state;
}

void esp8266_reset(void)
{
	esp_at_reset();
	esp8266_mqtt_reset();
	s_ctx.wifi_connected = 0;
	s_ctx.tcp_connected = 0;
	s_ctx.tcp_need_close = 0;
	s_ctx.send_busy = 0;
	s_ctx.mqtt_req = 0;
	s_ctx.last_mqtt_state = ESP8266_MQTT_STATE_DISCONNECTED;
	s_ctx.boot_delay_ticks = 40;
	esp8266_set_state(ESP8266_STATE_BOOT, ESP8266_EVENT_STATE_CHANGED);
}

static void esp8266_sm_boot(void)
{
	if (s_ctx.boot_delay_ticks > 0)
	{
		s_ctx.boot_delay_ticks--;
		return;
	}

	if (!esp_at_is_idle())
	{
		return;
	}

	if (s_ctx.boot_step == 0)
	{
		/* 先 AT 探测，再 RST，避免上电瞬间无响应 */
		esp_at_result_t r = esp_at_enqueue_cmd("AT\r\n", 500, esp8266_at_cmd_done, (void *)ESP8266_CMD_AT);
		if (r == ESP_AT_RES_OK)
		{
			s_ctx.boot_step = 1;
		}
		return;
	}

	if (s_ctx.boot_step == 2)
	{
		esp_at_result_t r = esp_at_enqueue_cmd("AT+RST\r\n", 2000, esp8266_at_cmd_done, (void *)ESP8266_CMD_RST);
		if (r != ESP_AT_RES_OK)
		{
			esp8266_set_state(ESP8266_STATE_ERROR, ESP8266_EVENT_ERROR);
		}
		return;
	}
}

static void esp8266_sm_basic_setup(void)
{
	if (!esp_at_is_idle())
	{
		return;
	}

	esp_at_result_t r = ESP_AT_RES_ERROR;

	switch (s_ctx.setup_step)
	{
	case 0:
		r = esp_at_enqueue_cmd("ATE0\r\n", 2000, esp8266_at_cmd_done, (void *)ESP8266_CMD_ATE0);
		break;

	case 1:
		r = esp_at_enqueue_cmd("AT+CWMODE=1\r\n", 1000, esp8266_at_cmd_done, (void *)ESP8266_CMD_CWMODE);
		break;

	case 2:
		/* 0=主动接收(+IPD)，1=被动接收(AT+CIPRECVDATA) */
		r = esp_at_enqueue_cmd("AT+CIPRECVMODE=0\r\n", 1000, esp8266_at_cmd_done, (void *)ESP8266_CMD_CIPRECVMODE);
		break;

	default:
		if (s_ctx.wifi_req)
		{
			esp8266_set_state(ESP8266_STATE_JOIN_AP, ESP8266_EVENT_STATE_CHANGED);
		}
		else
		{
			/* 未请求连接 WiFi：停留在 BASIC_SETUP，等待上层调用 esp8266_connect_wifi() */
		}
		return;
	}

	if (r != ESP_AT_RES_OK)
	{
		esp8266_set_state(ESP8266_STATE_ERROR, ESP8266_EVENT_ERROR);
	}
}

static void esp8266_sm_join_ap(void)
{
	if (!s_ctx.wifi_req)
	{
		return;
	}

	if (s_ctx.wifi_connected)
	{
		if (s_ctx.mqtt_req)
		{
			esp8266_set_state(ESP8266_STATE_MQTT_CONFIGURING, ESP8266_EVENT_STATE_CHANGED);
		}
		else
		{
			esp8266_set_state(ESP8266_STATE_TCP_CONNECT, ESP8266_EVENT_STATE_CHANGED);
		}
		return;
	}

	if (!esp_at_is_idle())
	{
		return;
	}

	/* CWJAP 可能耗时较长 */
	snprintf(s_ctx.cmd_buf, sizeof(s_ctx.cmd_buf),
			 "AT+CWJAP=\"%s\",\"%s\"\r\n",
			 s_ctx.ssid,
			 s_ctx.password);

	esp_at_result_t r = esp_at_enqueue_cmd(s_ctx.cmd_buf, 20000, esp8266_at_cmd_done, (void *)ESP8266_CMD_CWJAP);
	if (r != ESP_AT_RES_OK)
	{
		esp8266_set_state(ESP8266_STATE_ERROR, ESP8266_EVENT_ERROR);
	}
}

static void esp8266_sm_tcp_connect(void)
{
	if (!s_ctx.tcp_req)
	{
		return;
	}

	if (!s_ctx.wifi_connected)
	{
		esp8266_set_state(ESP8266_STATE_JOIN_AP, ESP8266_EVENT_STATE_CHANGED);
		return;
	}

	if (s_ctx.tcp_connected)
	{
		esp8266_set_state(ESP8266_STATE_ONLINE, ESP8266_EVENT_STATE_CHANGED);
		return;
	}

	if (!esp_at_is_idle())
	{
		return;
	}

	if (s_ctx.tcp_need_close)
	{
		s_ctx.tcp_need_close = 0;
		esp_at_result_t r = esp_at_enqueue_cmd("AT+CIPCLOSE\r\n", 2000, esp8266_at_cmd_done, (void *)ESP8266_CMD_CIPCLOSE);
		if (r != ESP_AT_RES_OK)
		{
			esp8266_set_state(ESP8266_STATE_ERROR, ESP8266_EVENT_ERROR);
		}
		return;
	}

	snprintf(s_ctx.cmd_buf, sizeof(s_ctx.cmd_buf),
			 "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
			 s_ctx.host,
			 (unsigned)s_ctx.port);

	esp_at_result_t r = esp_at_enqueue_cmd(s_ctx.cmd_buf, 10000, esp8266_at_cmd_done, (void *)ESP8266_CMD_CIPSTART);
	if (r != ESP_AT_RES_OK)
	{
		esp8266_set_state(ESP8266_STATE_ERROR, ESP8266_EVENT_ERROR);
	}
}

static void esp8266_sm_mqtt(void)
{
	if (!s_ctx.mqtt_req)
	{
		return;
	}

	if (!s_ctx.wifi_connected)
	{
		esp8266_set_state(ESP8266_STATE_JOIN_AP, ESP8266_EVENT_STATE_CHANGED);
		return;
	}

	esp8266_mqtt_tick_5ms();
	esp8266_sync_mqtt_state();
}

void esp8266_tick_5ms(void)
{
	if (!s_ctx.inited)
	{
		return;
	}

	/* 先驱动 AT 引擎（解析 RX / 推进命令） */
	esp_at_tick_5ms();

	if (s_ctx.state == ESP8266_STATE_BOOT)
	{
		esp8266_sm_boot();
		return;
	}

	/* 其他状态机分支 */
	switch (s_ctx.state)
	{
	case ESP8266_STATE_BASIC_SETUP:
		if (s_ctx.boot_delay_ticks > 0)
		{
			s_ctx.boot_delay_ticks--;
			return;
		}
		esp8266_sm_basic_setup();
		break;

	case ESP8266_STATE_JOIN_AP:
		esp8266_sm_join_ap();
		break;

	case ESP8266_STATE_TCP_CONNECT:
		esp8266_sm_tcp_connect();
		break;

	case ESP8266_STATE_ONLINE:
		/* 在线状态下主要由 API 触发发送；URC 负责掉线回退 */
		break;

	case ESP8266_STATE_MQTT_DISCONNECTED:
	case ESP8266_STATE_MQTT_CONFIGURING:
	case ESP8266_STATE_MQTT_CONNECTING:
	case ESP8266_STATE_MQTT_CONNECTED:
	case ESP8266_STATE_MQTT_SUBSCRIBED:
		esp8266_sm_mqtt();
		break;

	case ESP8266_STATE_ERROR:
	case ESP8266_STATE_DISABLED:
	default:
		break;
	}
}
