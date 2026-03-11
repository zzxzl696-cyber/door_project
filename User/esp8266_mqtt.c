/********************************** (C) COPYRIGHT *******************************
 * File Name          : esp8266_mqtt.c
 * Author             : Custom Implementation
 * Version            : V1.0.0
 * Date               : 2026-02-15
 * Description        : ESP8266 MQTT 非阻塞实现（ESP-AT 2.2 命令集）
 *********************************************************************************/

#include "bsp_system.h"

typedef enum
{
	ESP8266_MQTT_CMD_USERCFG = 0,
	ESP8266_MQTT_CMD_CLIENTID,
	ESP8266_MQTT_CMD_CONN,
	ESP8266_MQTT_CMD_SUB,
	ESP8266_MQTT_CMD_PUB,
	ESP8266_MQTT_CMD_CLEAN
} esp8266_mqtt_cmd_tag_t;

typedef struct
{
	uint8_t inited;
	esp8266_mqtt_state_t state;

	uint8_t connect_req;
	uint8_t disconnect_req;

	uint8_t connected;
	uint8_t subscribed;

	uint8_t publish_busy;
	uint8_t subscribe_busy;
	uint8_t subscribe_req;

	uint8_t cfg_step;

	char host[64];
	uint16_t port;
	char username[64];
	char password[192]; /* 足够容纳 OneNET token（~126字节）*/
	char client_id[64];

	uint16_t keepalive;
	uint8_t clean_session;

	char sub_topic[ESP8266_MQTT_TOPIC_MAX_LEN];
	uint8_t sub_qos;
	uint8_t auto_subscribe;

	char rx_topic[ESP8266_MQTT_TOPIC_MAX_LEN];
	uint8_t rx_payload[ESP8266_MQTT_PAYLOAD_MAX_LEN];

	char cmd_buf[384]; /* AT+MQTTUSERCFG with 126-char password ~178 bytes */

	esp8266_mqtt_on_message_t on_message;
	void *cb_user;
} esp8266_mqtt_ctx_t;

static esp8266_mqtt_ctx_t s_ctx = {0};

static void esp8266_mqtt_cmd_done(esp_at_result_t result, void *user);

static void esp8266_mqtt_copy_str(char *dst, size_t dst_len, const char *src)
{
	if (dst == NULL || dst_len == 0)
	{
		return;
	}

	if (src == NULL)
	{
		dst[0] = '\0';
		return;
	}

	strncpy(dst, src, dst_len - 1);
	dst[dst_len - 1] = '\0';
}

static void esp8266_mqtt_load_defaults(void)
{
	esp8266_mqtt_copy_str(s_ctx.host, sizeof(s_ctx.host), ESP8266_MQTT_BROKER_HOST);
	esp8266_mqtt_copy_str(s_ctx.username, sizeof(s_ctx.username), ESP8266_MQTT_USERNAME);
	esp8266_mqtt_copy_str(s_ctx.password, sizeof(s_ctx.password), ESP8266_MQTT_PASSWORD);
	esp8266_mqtt_copy_str(s_ctx.client_id, sizeof(s_ctx.client_id), ESP8266_MQTT_CLIENT_ID);
	esp8266_mqtt_copy_str(s_ctx.sub_topic, sizeof(s_ctx.sub_topic), ESP8266_MQTT_SUB_TOPIC);

	s_ctx.port = ESP8266_MQTT_BROKER_PORT;
	s_ctx.keepalive = ESP8266_MQTT_KEEPALIVE;
	s_ctx.clean_session = ESP8266_MQTT_CLEAN_SESSION;
	s_ctx.sub_qos = ESP8266_MQTT_SUB_QOS;
	s_ctx.auto_subscribe = ESP8266_MQTT_AUTO_SUBSCRIBE ? 1 : 0;
}

static uint16_t esp8266_mqtt_escape_payload(const uint8_t *src, uint16_t len, char *dst, uint16_t dst_len)
{
	uint16_t out = 0;

	if (dst == NULL || dst_len == 0)
	{
		return 0;
	}

	for (uint16_t i = 0; i < len; i++)
	{
		char ch = (char)src[i];
		if (ch == '\\' || ch == '"')
		{
			if ((uint16_t)(out + 2) >= dst_len)
			{
				return 0;
			}
			dst[out++] = '\\';
			dst[out++] = ch;
		}
		else
		{
			if ((uint16_t)(out + 1) >= dst_len)
			{
				return 0;
			}
			dst[out++] = ch;
		}
	}

	dst[out] = '\0';
	return out;
}

static int esp8266_mqtt_enqueue_cmd(const char *cmd, uint32_t timeout_ms, esp8266_mqtt_cmd_tag_t tag)
{
	esp_at_result_t r = esp_at_enqueue_cmd(cmd, timeout_ms, esp8266_mqtt_cmd_done, (void *)(uint32_t)tag);
	return (r == ESP_AT_RES_OK) ? 0 : -1;
}

static void esp8266_mqtt_cmd_done(esp_at_result_t result, void *user)
{
	esp8266_mqtt_cmd_tag_t tag = (esp8266_mqtt_cmd_tag_t)(uint32_t)user;

	if (result != ESP_AT_RES_OK)
	{
		if (tag == ESP8266_MQTT_CMD_PUB)
		{
			s_ctx.publish_busy = 0;
			return;
		}

		if (tag == ESP8266_MQTT_CMD_SUB)
		{
			s_ctx.subscribe_busy = 0;
			s_ctx.subscribe_req = 0;
			s_ctx.state = ESP8266_MQTT_STATE_CONNECTED;
			return;
		}

		if (tag == ESP8266_MQTT_CMD_CLEAN)
		{
			s_ctx.disconnect_req = 0;
			s_ctx.connected = 0;
			s_ctx.subscribed = 0;
			s_ctx.state = ESP8266_MQTT_STATE_DISCONNECTED;
			return;
		}

		s_ctx.cfg_step = 0;
		s_ctx.connected = 0;
		s_ctx.subscribed = 0;
		s_ctx.state = ESP8266_MQTT_STATE_DISCONNECTED;
		return;
	}

	switch (tag)
	{
	case ESP8266_MQTT_CMD_USERCFG:
		s_ctx.cfg_step = 1;
		break;

	case ESP8266_MQTT_CMD_CLIENTID:
		s_ctx.cfg_step = 2;
		s_ctx.state = ESP8266_MQTT_STATE_CONNECTING;
		break;

	case ESP8266_MQTT_CMD_CONN:
		s_ctx.connected = 1;
		s_ctx.state = ESP8266_MQTT_STATE_CONNECTED;
		break;

	case ESP8266_MQTT_CMD_SUB:
		s_ctx.subscribe_busy = 0;
		s_ctx.subscribe_req = 0;
		s_ctx.subscribed = 1;
		s_ctx.state = ESP8266_MQTT_STATE_SUBSCRIBED;
		break;

	case ESP8266_MQTT_CMD_PUB:
		s_ctx.publish_busy = 0;
		break;

	case ESP8266_MQTT_CMD_CLEAN:
		s_ctx.disconnect_req = 0;
		s_ctx.connected = 0;
		s_ctx.subscribed = 0;
		s_ctx.state = ESP8266_MQTT_STATE_DISCONNECTED;
		break;

	default:
		break;
	}
}

static int esp8266_mqtt_start_usercfg(void)
{
	int n = snprintf(s_ctx.cmd_buf,
					 sizeof(s_ctx.cmd_buf),
					 "AT+MQTTUSERCFG=%u,%u,\"%s\",\"%s\",\"%s\",%u,%u,\"%s\"\r\n",
					 (unsigned)ESP8266_MQTT_LINK_ID,
					 (unsigned)ESP8266_MQTT_SCHEME,
					 s_ctx.client_id,
					 s_ctx.username,
					 s_ctx.password,
					 (unsigned)ESP8266_MQTT_CERT_KEY_ID,
					 (unsigned)ESP8266_MQTT_CA_ID,
					 ESP8266_MQTT_PATH);

	if (n <= 0 || n >= (int)sizeof(s_ctx.cmd_buf))
	{
		return -1;
	}

	return esp8266_mqtt_enqueue_cmd(s_ctx.cmd_buf, ESP8266_MQTT_CMD_TIMEOUT_MS, ESP8266_MQTT_CMD_USERCFG);
}

static int esp8266_mqtt_start_clientid(void)
{
	int n = snprintf(s_ctx.cmd_buf,
					 sizeof(s_ctx.cmd_buf),
					 "AT+MQTTCLIENTID=%u,\"%s\"\r\n",
					 (unsigned)ESP8266_MQTT_LINK_ID,
					 s_ctx.client_id);

	if (n <= 0 || n >= (int)sizeof(s_ctx.cmd_buf))
	{
		return -1;
	}

	return esp8266_mqtt_enqueue_cmd(s_ctx.cmd_buf, ESP8266_MQTT_CMD_TIMEOUT_MS, ESP8266_MQTT_CMD_CLIENTID);
}

static int esp8266_mqtt_start_connect(void)
{
	int n = snprintf(s_ctx.cmd_buf,
					 sizeof(s_ctx.cmd_buf),
					 "AT+MQTTCONN=%u,\"%s\",%u,%u\r\n",
					 (unsigned)ESP8266_MQTT_LINK_ID,
					 s_ctx.host,
					 (unsigned)s_ctx.port,
					 0u); /* reconnect=0: 禁止模块自动重连，由状态机管理 */

	if (n <= 0 || n >= (int)sizeof(s_ctx.cmd_buf))
	{
		return -1;
	}

	return esp8266_mqtt_enqueue_cmd(s_ctx.cmd_buf, ESP8266_MQTT_CONN_TIMEOUT_MS, ESP8266_MQTT_CMD_CONN);
}

static int esp8266_mqtt_start_subscribe(void)
{
	int n = snprintf(s_ctx.cmd_buf,
					 sizeof(s_ctx.cmd_buf),
					 "AT+MQTTSUB=%u,\"%s\",%u\r\n",
					 (unsigned)ESP8266_MQTT_LINK_ID,
					 s_ctx.sub_topic,
					 (unsigned)s_ctx.sub_qos);

	if (n <= 0 || n >= (int)sizeof(s_ctx.cmd_buf))
	{
		return -1;
	}

	s_ctx.subscribe_busy = 1;
	return esp8266_mqtt_enqueue_cmd(s_ctx.cmd_buf, ESP8266_MQTT_SUB_TIMEOUT_MS, ESP8266_MQTT_CMD_SUB);
}

static int esp8266_mqtt_start_clean(void)
{
	int n = snprintf(s_ctx.cmd_buf,
					 sizeof(s_ctx.cmd_buf),
					 "AT+MQTTCLEAN=%u\r\n",
					 (unsigned)ESP8266_MQTT_LINK_ID);

	if (n <= 0 || n >= (int)sizeof(s_ctx.cmd_buf))
	{
		return -1;
	}

	return esp8266_mqtt_enqueue_cmd(s_ctx.cmd_buf, ESP8266_MQTT_CLEAN_TIMEOUT_MS, ESP8266_MQTT_CMD_CLEAN);
}

static void esp8266_mqtt_sm_configuring(void)
{
	if (!esp_at_is_idle())
	{
		return;
	}

	if (s_ctx.cfg_step == 0)
	{
		if (esp8266_mqtt_start_usercfg() != 0)
		{
			s_ctx.state = ESP8266_MQTT_STATE_ERROR;
		}
		return;
	}

	if (s_ctx.cfg_step == 1)
	{
		if (esp8266_mqtt_start_clientid() != 0)
		{
			s_ctx.state = ESP8266_MQTT_STATE_ERROR;
		}
		return;
	}

	s_ctx.state = ESP8266_MQTT_STATE_CONNECTING;
}

static void esp8266_mqtt_sm_connecting(void)
{
	if (!esp_at_is_idle())
	{
		return;
	}

	if (esp8266_mqtt_start_connect() != 0)
	{
		s_ctx.state = ESP8266_MQTT_STATE_ERROR;
	}
}

static void esp8266_mqtt_sm_connected(void)
{
	if (!s_ctx.subscribe_req)
	{
		return;
	}

	if (s_ctx.subscribe_busy)
	{
		return;
	}

	if (!esp_at_is_idle())
	{
		return;
	}

	if (esp8266_mqtt_start_subscribe() != 0)
	{
		s_ctx.state = ESP8266_MQTT_STATE_ERROR;
	}
}

void esp8266_mqtt_init(void)
{
	memset(&s_ctx, 0, sizeof(s_ctx));
	s_ctx.inited = 1;
	s_ctx.state = ESP8266_MQTT_STATE_DISCONNECTED;
	esp8266_mqtt_load_defaults();
}

void esp8266_mqtt_reset(void)
{
	s_ctx.state = ESP8266_MQTT_STATE_DISCONNECTED;
	s_ctx.connect_req = 0;
	s_ctx.disconnect_req = 0;
	s_ctx.connected = 0;
	s_ctx.subscribed = 0;
	s_ctx.publish_busy = 0;
	s_ctx.subscribe_busy = 0;
	s_ctx.subscribe_req = 0;
	s_ctx.cfg_step = 0;
}

void esp8266_mqtt_tick_5ms(void)
{
	if (!s_ctx.inited)
	{
		return;
	}

	if (s_ctx.disconnect_req)
	{
		if (s_ctx.state == ESP8266_MQTT_STATE_DISCONNECTED)
		{
			s_ctx.disconnect_req = 0;
			return;
		}

		if (!esp_at_is_idle())
		{
			return;
		}

		if (esp8266_mqtt_start_clean() != 0)
		{
			s_ctx.state = ESP8266_MQTT_STATE_ERROR;
		}
		return;
	}

	if (!s_ctx.connect_req)
	{
		return;
	}

	if (s_ctx.state == ESP8266_MQTT_STATE_DISCONNECTED)
	{
		s_ctx.cfg_step = 0;
		s_ctx.state = ESP8266_MQTT_STATE_CONFIGURING;
	}

	switch (s_ctx.state)
	{
	case ESP8266_MQTT_STATE_CONFIGURING:
		esp8266_mqtt_sm_configuring();
		break;

	case ESP8266_MQTT_STATE_CONNECTING:
		esp8266_mqtt_sm_connecting();
		break;

	case ESP8266_MQTT_STATE_CONNECTED:
	case ESP8266_MQTT_STATE_SUBSCRIBED:
		esp8266_mqtt_sm_connected();
		break;

	case ESP8266_MQTT_STATE_ERROR:
		s_ctx.state = ESP8266_MQTT_STATE_DISCONNECTED;
		break;

	default:
		break;
	}
}

int esp8266_mqtt_connect(void)
{
	if (!s_ctx.inited)
	{
		esp8266_mqtt_init();
	}

	esp8266_mqtt_load_defaults();
	if (s_ctx.host[0] == '\0' || s_ctx.client_id[0] == '\0')
	{
		return -1;
	}
	s_ctx.connect_req = 1;
	s_ctx.disconnect_req = 0;
	s_ctx.cfg_step = 0;
	s_ctx.connected = 0;
	s_ctx.subscribed = 0;
	s_ctx.publish_busy = 0;
	s_ctx.subscribe_busy = 0;
	s_ctx.subscribe_req = 0;

	if (s_ctx.auto_subscribe && s_ctx.sub_topic[0] != '\0')
	{
		s_ctx.subscribe_req = 1;
	}

	s_ctx.state = ESP8266_MQTT_STATE_DISCONNECTED;
	return 0;
}

int esp8266_mqtt_disconnect(void)
{
	if (!s_ctx.inited)
	{
		return -1;
	}

	if (s_ctx.state == ESP8266_MQTT_STATE_DISCONNECTED)
	{
		return 0;
	}

	s_ctx.connect_req = 0;
	s_ctx.disconnect_req = 1;
	return 0;
}

int esp8266_mqtt_publish(const char *topic,
						 const uint8_t *payload,
						 uint16_t len,
						 uint8_t qos,
						 uint8_t retain)
{
	char payload_buf[ESP_AT_CMD_MAX_LEN] = {0};

	if (topic == NULL || payload == NULL || len == 0)
	{
		return -1;
	}

	if (s_ctx.state != ESP8266_MQTT_STATE_CONNECTED &&
		s_ctx.state != ESP8266_MQTT_STATE_SUBSCRIBED)
	{
		return -1;
	}

	if (qos > 1 || retain > 1)
	{
		return -1;
	}

	if (s_ctx.publish_busy || !esp_at_is_idle())
	{
		return -1;
	}

	if (esp8266_mqtt_escape_payload(payload, len, payload_buf, sizeof(payload_buf)) == 0)
	{
		return -1;
	}

	int n = snprintf(s_ctx.cmd_buf,
					 sizeof(s_ctx.cmd_buf),
					 "AT+MQTTPUB=%u,\"%s\",\"%s\",%u,%u\r\n",
					 (unsigned)ESP8266_MQTT_LINK_ID,
					 topic,
					 payload_buf,
					 (unsigned)qos,
					 (unsigned)retain);

	if (n <= 0 || n >= (int)sizeof(s_ctx.cmd_buf))
	{
		return -1;
	}

	s_ctx.publish_busy = 1;
	if (esp8266_mqtt_enqueue_cmd(s_ctx.cmd_buf, ESP8266_MQTT_PUB_TIMEOUT_MS, ESP8266_MQTT_CMD_PUB) != 0)
	{
		s_ctx.publish_busy = 0;
		return -1;
	}

	return 0;
}

int esp8266_mqtt_subscribe(const char *topic, uint8_t qos)
{
	if (topic == NULL || topic[0] == '\0')
	{
		return -1;
	}

	if (qos > 1)
	{
		return -1;
	}

	if (s_ctx.state != ESP8266_MQTT_STATE_CONNECTED &&
		s_ctx.state != ESP8266_MQTT_STATE_SUBSCRIBED)
	{
		return -1;
	}

	esp8266_mqtt_copy_str(s_ctx.sub_topic, sizeof(s_ctx.sub_topic), topic);
	s_ctx.sub_qos = qos;
	s_ctx.subscribe_req = 1;
	return 0;
}

void esp8266_mqtt_set_on_message(esp8266_mqtt_on_message_t cb, void *user)
{
	s_ctx.on_message = cb;
	s_ctx.cb_user = user;
}

esp8266_mqtt_state_t esp8266_mqtt_get_state(void)
{
	return s_ctx.state;
}

static void esp8266_mqtt_handle_subrecv(const char *line)
{
	const char *p = line;
	const char *topic_start = NULL;
	const char *topic_end = NULL;
	const char *payload_start = NULL;
	char *end = NULL;

	p += strlen("+MQTTSUBRECV:");
	(void)strtoul(p, &end, 10);
	if (end == NULL || *end != ',')
	{
		return;
	}

	p = end + 1;
	if (*p != '"')
	{
		return;
	}

	topic_start = p + 1;
	topic_end = strchr(topic_start, '"');
	if (topic_end == NULL)
	{
		return;
	}

	size_t topic_len = (size_t)(topic_end - topic_start);
	if (topic_len >= sizeof(s_ctx.rx_topic))
	{
		topic_len = sizeof(s_ctx.rx_topic) - 1;
	}
	memcpy(s_ctx.rx_topic, topic_start, topic_len);
	s_ctx.rx_topic[topic_len] = '\0';

	p = topic_end + 1;
	if (*p != ',')
	{
		return;
	}

	p++;
	uint32_t data_len = strtoul(p, &end, 10);
	if (end == NULL || *end != ',')
	{
		return;
	}

	payload_start = end + 1;
	size_t payload_avail = strlen(payload_start);
	if (data_len > payload_avail)
	{
		data_len = payload_avail;
	}
	if (data_len > sizeof(s_ctx.rx_payload))
	{
		data_len = sizeof(s_ctx.rx_payload);
	}

	memcpy(s_ctx.rx_payload, payload_start, data_len);

	if (s_ctx.on_message != NULL)
	{
		s_ctx.on_message(s_ctx.rx_topic, s_ctx.rx_payload, (uint16_t)data_len, s_ctx.cb_user);
	}
}

void esp8266_mqtt_handle_urc_line(const char *line)
{
	if (line == NULL)
	{
		return;
	}

	if (strncmp(line, "+MQTTSUBRECV:", 13) == 0)
	{
		esp8266_mqtt_handle_subrecv(line);
		return;
	}

	if (strstr(line, "+MQTTDISCONNECTED") != NULL || strstr(line, "MQTT DISCONNECTED") != NULL)
	{
		s_ctx.connected = 0;
		s_ctx.subscribed = 0;
		s_ctx.state = ESP8266_MQTT_STATE_DISCONNECTED;
		return;
	}

	if (strstr(line, "+MQTTCONNECTED") != NULL)
	{
		s_ctx.connected = 1;
		if (s_ctx.subscribed)
		{
			s_ctx.state = ESP8266_MQTT_STATE_SUBSCRIBED;
		}
		else
		{
			s_ctx.state = ESP8266_MQTT_STATE_CONNECTED;
		}
		return;
	}
}
