/********************************** (C) COPYRIGHT *******************************
 * File Name          : mqtt_app.c
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-26
 * Description        : MQTT 应用层实现
 *                      - 事件队列：8 槽位循环缓冲，入队 O(1)
 *                      - JSON 格式化：snprintf 生成 OneNET 兼容 JSON
 *                      - 命令解析：strstr 轻量级 JSON 解析
 *                      - 心跳定时器：30 秒周期上报系统状态
 *********************************************************************************/

#include "bsp_system.h"

/* ================= 配置 ================= */

#define MQTT_APP_EVT_QUEUE_SIZE  8
#define MQTT_APP_JSON_BUF_SIZE   384
#define MQTT_APP_HEARTBEAT_MS    30000   /* 30 秒心跳 */
#define MQTT_APP_MAX_LOG_QUERY   10      /* 日志查询最大条数 */
#define MQTT_APP_MAX_USER_QUERY  10      /* 用户查询最大条数 */

/* ================= 事件类型 ================= */

typedef enum
{
    MQTT_APP_EVT_AUTH_SUCCESS = 0,
    MQTT_APP_EVT_AUTH_FAIL,
    MQTT_APP_EVT_DOOR_UNLOCKED,
    MQTT_APP_EVT_DOOR_LOCKED,
    MQTT_APP_EVT_ALARM_LOCKOUT,
    MQTT_APP_EVT_HEARTBEAT
} mqtt_app_evt_type_t;

typedef struct
{
    uint8_t type;       /* mqtt_app_evt_type_t */
    uint16_t user_id;
    uint8_t method;     /* auth_method_t */
    uint8_t result;     /* auth_result_t */
} mqtt_app_event_t;

/* ================= 内部状态 ================= */

static struct
{
    /* 事件队列 */
    mqtt_app_event_t queue[MQTT_APP_EVT_QUEUE_SIZE];
    uint8_t q_head;     /* 写入位置 */
    uint8_t q_tail;     /* 读取位置 */
    uint8_t q_count;

    /* 消息计数与定时器 */
    uint32_t msg_id;
    uint32_t heartbeat_timer;
    uint8_t mqtt_connected;

    /* 共享 JSON 格式化缓冲区 */
    char json_buf[MQTT_APP_JSON_BUF_SIZE];
} s_app;

/* ================= 辅助函数 ================= */

/**
 * @brief 获取认证方式字符串（用于 JSON value）
 */
static const char *mqtt_app_method_str(uint8_t method)
{
    switch ((auth_method_t)method)
    {
    case AUTH_PASSWORD:    return "Password";
    case AUTH_RFID:        return "RFID";
    case AUTH_FINGERPRINT: return "Finger";
    case AUTH_FACE:        return "Face";
    case AUTH_REMOTE:      return "Remote";
    default:               return "None";
    }
}

/**
 * @brief 获取认证结果字符串（用于 JSON value）
 */
static const char *mqtt_app_result_str(uint8_t result)
{
    switch ((auth_result_t)result)
    {
    case AUTH_RESULT_SUCCESS:        return "success";
    case AUTH_RESULT_FAILED:         return "fail";
    case AUTH_RESULT_USER_NOT_FOUND: return "not_found";
    case AUTH_RESULT_USER_DISABLED:  return "disabled";
    case AUTH_RESULT_TIMEOUT:        return "timeout";
    case AUTH_RESULT_DOOR_FORCED:    return "forced";
    default:                         return "unknown";
    }
}

/* ================= JSON 解析辅助（轻量级） ================= */

/**
 * @brief 从 JSON 中提取字符串值
 * @param json JSON 字符串
 * @param key  键名
 * @param buf  输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 成功返回 buf 指针，失败返回 NULL
 */
static const char *json_find_string(const char *json, const char *key,
                                    char *buf, uint16_t buf_size)
{
    char search[32];
    const char *p, *end;
    uint16_t len;

    snprintf(search, sizeof(search), "\"%s\":\"", key);
    p = strstr(json, search);
    if (p == NULL)
    {
        return NULL;
    }

    p += strlen(search);
    end = strchr(p, '"');
    if (end == NULL)
    {
        return NULL;
    }

    len = (uint16_t)(end - p);
    if (len >= buf_size)
    {
        len = buf_size - 1;
    }

    memcpy(buf, p, len);
    buf[len] = '\0';
    return buf;
}

/**
 * @brief 从 JSON 中提取整数值
 * @param json JSON 字符串
 * @param key  键名
 * @return 成功返回整数值，失败返回 -1
 */
static int32_t json_find_int(const char *json, const char *key)
{
    char search[32];
    const char *p;

    snprintf(search, sizeof(search), "\"%s\":", key);
    p = strstr(json, search);
    if (p == NULL)
    {
        return -1;
    }

    p += strlen(search);
    while (*p == ' ')
    {
        p++;
    }

    return (int32_t)strtol(p, NULL, 10);
}

/* ================= 事件队列 ================= */

static int mqtt_app_evt_enqueue(const mqtt_app_event_t *evt)
{
    if (s_app.q_count >= MQTT_APP_EVT_QUEUE_SIZE)
    {
        return -1; /* 队列已满，丢弃 */
    }

    s_app.queue[s_app.q_head] = *evt;
    s_app.q_head = (s_app.q_head + 1) % MQTT_APP_EVT_QUEUE_SIZE;
    s_app.q_count++;
    return 0;
}

static int mqtt_app_evt_dequeue(mqtt_app_event_t *evt)
{
    if (s_app.q_count == 0)
    {
        return -1; /* 队列为空 */
    }

    *evt = s_app.queue[s_app.q_tail];
    s_app.q_tail = (s_app.q_tail + 1) % MQTT_APP_EVT_QUEUE_SIZE;
    s_app.q_count--;
    return 0;
}

/* ================= JSON 发布 ================= */

/**
 * @brief 发布 JSON 到 OneNET property/post 主题
 */
static int mqtt_app_pub_json(const char *json, uint16_t len)
{
    return esp8266_mqtt_publish(ESP8266_MQTT_PUB_TOPIC,
                                (const uint8_t *)json, len,
                                ESP8266_MQTT_DEFAULT_QOS,
                                ESP8266_MQTT_DEFAULT_RETAIN);
}

/**
 * @brief 检查 MQTT 是否可以发布
 */
static uint8_t mqtt_app_can_publish(void)
{
    esp8266_mqtt_state_t st = esp8266_mqtt_get_state();
    return (st == ESP8266_MQTT_STATE_CONNECTED ||
            st == ESP8266_MQTT_STATE_SUBSCRIBED) ? 1 : 0;
}

/* ================= 事件格式化与发布 ================= */

static void mqtt_app_publish_auth_event(const mqtt_app_event_t *evt)
{
    int n;
    uint8_t is_success = (evt->type == MQTT_APP_EVT_AUTH_SUCCESS);

    if (is_success && evt->user_id > 0)
    {
        /* 尝试查找用户名 */
        user_entry_t user;
        if (user_db_find_by_id(evt->user_id, &user) == USER_DB_OK)
        {
            n = snprintf(s_app.json_buf, sizeof(s_app.json_buf),
                "{\"id\":\"%u\",\"params\":{"
                "\"auth_event\":{\"value\":\"%s\"},"
                "\"auth_method\":{\"value\":\"%s\"},"
                "\"user_id\":{\"value\":%u},"
                "\"user_name\":{\"value\":\"%s\"}"
                "}}",
                (unsigned)++s_app.msg_id,
                mqtt_app_result_str(evt->result),
                mqtt_app_method_str(evt->method),
                (unsigned)evt->user_id,
                user.name);
        }
        else
        {
            n = snprintf(s_app.json_buf, sizeof(s_app.json_buf),
                "{\"id\":\"%u\",\"params\":{"
                "\"auth_event\":{\"value\":\"%s\"},"
                "\"auth_method\":{\"value\":\"%s\"},"
                "\"user_id\":{\"value\":%u}"
                "}}",
                (unsigned)++s_app.msg_id,
                mqtt_app_result_str(evt->result),
                mqtt_app_method_str(evt->method),
                (unsigned)evt->user_id);
        }
    }
    else
    {
        n = snprintf(s_app.json_buf, sizeof(s_app.json_buf),
            "{\"id\":\"%u\",\"params\":{"
            "\"auth_event\":{\"value\":\"%s\"},"
            "\"auth_method\":{\"value\":\"%s\"}"
            "}}",
            (unsigned)++s_app.msg_id,
            mqtt_app_result_str(evt->result),
            mqtt_app_method_str(evt->method));
    }

    if (n > 0 && n < (int)sizeof(s_app.json_buf))
    {
        mqtt_app_pub_json(s_app.json_buf, (uint16_t)n);
        printf("[MQTT-APP] Auth event published\r\n");
    }
}

static void mqtt_app_publish_door_event(const mqtt_app_event_t *evt)
{
    int n = snprintf(s_app.json_buf, sizeof(s_app.json_buf),
        "{\"id\":\"%u\",\"params\":{"
        "\"door_locked\":{\"value\":%d},"
        "\"auth_method\":{\"value\":\"%s\"}"
        "}}",
        (unsigned)++s_app.msg_id,
        (evt->type == MQTT_APP_EVT_DOOR_LOCKED) ? 1 : 0,
        mqtt_app_method_str(evt->method));

    if (n > 0 && n < (int)sizeof(s_app.json_buf))
    {
        mqtt_app_pub_json(s_app.json_buf, (uint16_t)n);
        printf("[MQTT-APP] Door event published\r\n");
    }
}

static void mqtt_app_publish_alarm(const mqtt_app_event_t *evt)
{
    int n = snprintf(s_app.json_buf, sizeof(s_app.json_buf),
        "{\"id\":\"%u\",\"params\":{"
        "\"alarm\":{\"value\":\"lockout\"},"
        "\"fail_count\":{\"value\":%u}"
        "}}",
        (unsigned)++s_app.msg_id,
        (unsigned)evt->result);

    if (n > 0 && n < (int)sizeof(s_app.json_buf))
    {
        mqtt_app_pub_json(s_app.json_buf, (uint16_t)n);
        printf("[MQTT-APP] Alarm published\r\n");
    }
}

static void mqtt_app_publish_heartbeat(void)
{
    uint32_t uptime_s = TIM_Get_MillisCounter() / 1000;

    int n = snprintf(s_app.json_buf, sizeof(s_app.json_buf),
        "{\"id\":\"%u\",\"params\":{"
        "\"door_locked\":{\"value\":%d},"
        "\"user_count\":{\"value\":%u},"
        "\"log_count\":{\"value\":%u},"
        "\"uptime_s\":{\"value\":%u}"
        "}}",
        (unsigned)++s_app.msg_id,
        door_control_is_locked(),
        (unsigned)user_db_get_count(),
        (unsigned)access_log_get_count(),
        (unsigned)uptime_s);

    if (n > 0 && n < (int)sizeof(s_app.json_buf))
    {
        mqtt_app_pub_json(s_app.json_buf, (uint16_t)n);
        printf("[MQTT-APP] Heartbeat published\r\n");
    }
}

/**
 * @brief 处理一个出队事件
 */
static void mqtt_app_process_event(const mqtt_app_event_t *evt)
{
    switch (evt->type)
    {
    case MQTT_APP_EVT_AUTH_SUCCESS:
    case MQTT_APP_EVT_AUTH_FAIL:
        mqtt_app_publish_auth_event(evt);
        break;

    case MQTT_APP_EVT_DOOR_UNLOCKED:
    case MQTT_APP_EVT_DOOR_LOCKED:
        mqtt_app_publish_door_event(evt);
        break;

    case MQTT_APP_EVT_ALARM_LOCKOUT:
        mqtt_app_publish_alarm(evt);
        break;

    case MQTT_APP_EVT_HEARTBEAT:
        mqtt_app_publish_heartbeat();
        break;

    default:
        break;
    }
}

/* ================= 远程命令处理 ================= */

static void mqtt_app_cmd_status(void)
{
    uint32_t uptime_s = TIM_Get_MillisCounter() / 1000;

    int n = snprintf(s_app.json_buf, sizeof(s_app.json_buf),
        "{\"id\":\"%u\",\"params\":{"
        "\"door_locked\":{\"value\":%d},"
        "\"auth_method\":{\"value\":\"%s\"},"
        "\"user_count\":{\"value\":%u},"
        "\"log_count\":{\"value\":%u},"
        "\"fail_count\":{\"value\":%u},"
        "\"uptime_s\":{\"value\":%u}"
        "}}",
        (unsigned)++s_app.msg_id,
        door_control_is_locked(),
        door_control_get_auth_method_str(),
        (unsigned)user_db_get_count(),
        (unsigned)access_log_get_count(),
        (unsigned)auth_manager_get_fail_count(),
        (unsigned)uptime_s);

    if (n > 0 && n < (int)sizeof(s_app.json_buf))
    {
        mqtt_app_pub_json(s_app.json_buf, (uint16_t)n);
        printf("[MQTT-APP] Status published\r\n");
    }
}

static void mqtt_app_cmd_get_logs(uint8_t count)
{
    uint16_t total = access_log_get_count();
    int n, i;

    if (count > MQTT_APP_MAX_LOG_QUERY)
    {
        count = MQTT_APP_MAX_LOG_QUERY;
    }
    if (count > total)
    {
        count = (uint8_t)total;
    }

    n = snprintf(s_app.json_buf, sizeof(s_app.json_buf),
        "{\"id\":\"%u\",\"params\":{\"log_count\":{\"value\":%u},\"logs\":{\"value\":\"",
        (unsigned)++s_app.msg_id, (unsigned)count);

    for (i = 0; i < count; i++)
    {
        access_record_t rec;
        if (access_log_get_record((uint16_t)i, &rec) != ACCESS_LOG_OK)
        {
            break;
        }
        if (i > 0)
        {
            n += snprintf(s_app.json_buf + n, sizeof(s_app.json_buf) - n, ";");
        }
        n += snprintf(s_app.json_buf + n, sizeof(s_app.json_buf) - n,
            "%s:%s:u%u",
            mqtt_app_method_str(rec.auth_method),
            mqtt_app_result_str(rec.result),
            (unsigned)rec.user_id);

        /* 防止缓冲区溢出，预留尾部空间 */
        if (n > (int)(sizeof(s_app.json_buf) - 20))
        {
            break;
        }
    }

    n += snprintf(s_app.json_buf + n, sizeof(s_app.json_buf) - n, "\"}}}");

    if (n > 0 && n < (int)sizeof(s_app.json_buf))
    {
        mqtt_app_pub_json(s_app.json_buf, (uint16_t)n);
        printf("[MQTT-APP] Logs published (%d records)\r\n", i);
    }
}

static void mqtt_app_cmd_get_users(void)
{
    uint16_t total = user_db_get_count();
    uint16_t show = (total > MQTT_APP_MAX_USER_QUERY) ? MQTT_APP_MAX_USER_QUERY : total;
    int n, i;

    n = snprintf(s_app.json_buf, sizeof(s_app.json_buf),
        "{\"id\":\"%u\",\"params\":{\"user_count\":{\"value\":%u},\"users\":{\"value\":\"",
        (unsigned)++s_app.msg_id, (unsigned)total);

    for (i = 0; i < show; i++)
    {
        user_entry_t user;
        if (user_db_get_by_index((uint16_t)i, &user) != USER_DB_OK)
        {
            break;
        }
        if (i > 0)
        {
            n += snprintf(s_app.json_buf + n, sizeof(s_app.json_buf) - n, ";");
        }
        n += snprintf(s_app.json_buf + n, sizeof(s_app.json_buf) - n,
            "%u:%s", (unsigned)user.id, user.name);

        if (n > (int)(sizeof(s_app.json_buf) - 20))
        {
            break;
        }
    }

    n += snprintf(s_app.json_buf + n, sizeof(s_app.json_buf) - n, "\"}}}");

    if (n > 0 && n < (int)sizeof(s_app.json_buf))
    {
        mqtt_app_pub_json(s_app.json_buf, (uint16_t)n);
        printf("[MQTT-APP] Users published (%d entries)\r\n", i);
    }
}

static void mqtt_app_cmd_add_user(const char *name)
{
    user_entry_t user;
    user_db_result_t ret;
    int n;

    memset(&user, 0, sizeof(user));
    if (name != NULL && name[0] != '\0')
    {
        strncpy(user.name, name, USER_NAME_LEN - 1);
    }
    else
    {
        strcpy(user.name, "Remote");
    }
    user.flags = USER_FLAG_VALID;

    ret = user_db_add_user(&user);

    n = snprintf(s_app.json_buf, sizeof(s_app.json_buf),
        "{\"id\":\"%u\",\"params\":{"
        "\"cmd_result\":{\"value\":\"%s\"},"
        "\"cmd\":{\"value\":\"add_user\"},"
        "\"user_id\":{\"value\":%u}"
        "}}",
        (unsigned)++s_app.msg_id,
        (ret == USER_DB_OK) ? "ok" : "fail",
        (unsigned)user.id);

    if (n > 0 && n < (int)sizeof(s_app.json_buf))
    {
        mqtt_app_pub_json(s_app.json_buf, (uint16_t)n);
    }
    printf("[MQTT-APP] CMD add_user: %s (id=%u)\r\n",
           (ret == USER_DB_OK) ? "OK" : "FAIL", (unsigned)user.id);
}

static void mqtt_app_cmd_del_user(uint16_t user_id)
{
    user_db_result_t ret = user_db_delete_user(user_id);
    int n;

    n = snprintf(s_app.json_buf, sizeof(s_app.json_buf),
        "{\"id\":\"%u\",\"params\":{"
        "\"cmd_result\":{\"value\":\"%s\"},"
        "\"cmd\":{\"value\":\"del_user\"},"
        "\"user_id\":{\"value\":%u}"
        "}}",
        (unsigned)++s_app.msg_id,
        (ret == USER_DB_OK) ? "ok" : "fail",
        (unsigned)user_id);

    if (n > 0 && n < (int)sizeof(s_app.json_buf))
    {
        mqtt_app_pub_json(s_app.json_buf, (uint16_t)n);
    }
    printf("[MQTT-APP] CMD del_user: %s (id=%u)\r\n",
           (ret == USER_DB_OK) ? "OK" : "FAIL", (unsigned)user_id);
}

/**
 * @brief 解析并分发远程命令
 */
static void mqtt_app_dispatch_cmd(const char *json)
{
    char cmd[16] = {0};

    if (json_find_string(json, "cmd", cmd, sizeof(cmd)) == NULL)
    {
        printf("[MQTT-APP] No cmd field in payload\r\n");
        return;
    }

    printf("[MQTT-APP] CMD: %s\r\n", cmd);

    if (strcmp(cmd, "unlock") == 0)
    {
        int32_t duration = json_find_int(json, "duration");
        if (duration <= 0)
        {
            duration = 5000;
        }
        door_control_unlock(AUTH_REMOTE, (uint32_t)duration);
    }
    else if (strcmp(cmd, "lock") == 0)
    {
        door_control_lock();
    }
    else if (strcmp(cmd, "status") == 0)
    {
        mqtt_app_cmd_status();
    }
    else if (strcmp(cmd, "get_logs") == 0)
    {
        int32_t count = json_find_int(json, "count");
        if (count <= 0)
        {
            count = 5;
        }
        mqtt_app_cmd_get_logs((uint8_t)count);
    }
    else if (strcmp(cmd, "get_users") == 0)
    {
        mqtt_app_cmd_get_users();
    }
    else if (strcmp(cmd, "add_user") == 0)
    {
        char name[USER_NAME_LEN] = {0};
        json_find_string(json, "name", name, sizeof(name));
        mqtt_app_cmd_add_user(name);
    }
    else if (strcmp(cmd, "del_user") == 0)
    {
        int32_t uid = json_find_int(json, "user_id");
        if (uid > 0)
        {
            mqtt_app_cmd_del_user((uint16_t)uid);
        }
    }
    else
    {
        printf("[MQTT-APP] Unknown cmd: %s\r\n", cmd);
    }
}

/* ================= 公开 API ================= */

void mqtt_app_init(void)
{
    memset(&s_app, 0, sizeof(s_app));
    s_app.heartbeat_timer = TIM_Get_MillisCounter();
    printf("[MQTT-APP] Init\r\n");
}

void mqtt_app_tick(void)
{
    mqtt_app_event_t evt;
    uint32_t now = TIM_Get_MillisCounter();

    /* 心跳定时器 */
    if ((now - s_app.heartbeat_timer) >= MQTT_APP_HEARTBEAT_MS)
    {
        s_app.heartbeat_timer = now;

        if (mqtt_app_can_publish())
        {
            mqtt_app_publish_heartbeat();
        }
    }

    /* 每次 tick 最多处理 1 个事件（避免阻塞调度器） */
    if (s_app.q_count > 0 && mqtt_app_can_publish())
    {
        if (mqtt_app_evt_dequeue(&evt) == 0)
        {
            mqtt_app_process_event(&evt);
        }
    }
}

void mqtt_app_on_auth(auth_method_t method, auth_result_t result, const user_entry_t *user)
{
    mqtt_app_event_t evt;

    evt.method = (uint8_t)method;
    evt.result = (uint8_t)result;
    evt.user_id = (user != NULL) ? user->id : 0;

    if (result == AUTH_RESULT_SUCCESS)
    {
        evt.type = MQTT_APP_EVT_AUTH_SUCCESS;
    }
    else
    {
        evt.type = MQTT_APP_EVT_AUTH_FAIL;

        /* 检查是否触发锁定告警 */
        if (auth_manager_is_locked())
        {
            mqtt_app_event_t alarm_evt;
            alarm_evt.type = MQTT_APP_EVT_ALARM_LOCKOUT;
            alarm_evt.method = (uint8_t)method;
            alarm_evt.result = auth_manager_get_fail_count();
            alarm_evt.user_id = 0;
            mqtt_app_evt_enqueue(&alarm_evt);
        }
    }

    if (mqtt_app_evt_enqueue(&evt) == 0)
    {
        printf("[MQTT-APP] Auth event queued: %s %s\r\n",
               mqtt_app_method_str(evt.method),
               mqtt_app_result_str(evt.result));
    }
}

void mqtt_app_on_door_state(uint8_t locked)
{
    mqtt_app_event_t evt;

    evt.type = locked ? MQTT_APP_EVT_DOOR_LOCKED : MQTT_APP_EVT_DOOR_UNLOCKED;
    evt.method = (uint8_t)g_door_status.last_auth_method;
    evt.result = 0;
    evt.user_id = 0;

    if (mqtt_app_evt_enqueue(&evt) == 0)
    {
        printf("[MQTT-APP] Door event queued: %s\r\n",
               locked ? "locked" : "unlocked");
    }
}

void mqtt_app_on_message(const char *topic, const uint8_t *payload,
                          uint16_t len, void *user)
{
    (void)user;
    printf("[MQTT-APP] Recv topic=%s len=%u\r\n", topic, (unsigned)len);

    /* 将 payload 复制到 json_buf 并确保 null-terminated */
    if (len >= sizeof(s_app.json_buf))
    {
        len = sizeof(s_app.json_buf) - 1;
    }
    memcpy(s_app.json_buf, payload, len);
    s_app.json_buf[len] = '\0';

    /* 解析并分发命令 */
    mqtt_app_dispatch_cmd(s_app.json_buf);
}

void mqtt_app_notify_connected(void)
{
    s_app.mqtt_connected = 1;
    /* 重置心跳定时器，连接后尽快发送第一次心跳 */
    s_app.heartbeat_timer = TIM_Get_MillisCounter() - MQTT_APP_HEARTBEAT_MS;
    printf("[MQTT-APP] MQTT connected\r\n");
}

void mqtt_app_notify_disconnected(void)
{
    s_app.mqtt_connected = 0;
    printf("[MQTT-APP] MQTT disconnected\r\n");
}
