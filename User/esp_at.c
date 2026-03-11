/********************************** (C) COPYRIGHT *******************************
 * File Name          : esp_at.c
 * Author             : Custom Implementation
 * Version            : V1.0.0
 * Date               : 2026-02-15
 * Description        : ESP8266 AT 命令引擎（非阻塞）实现
 *********************************************************************************/

#include "bsp_system.h"

typedef enum
{
    ESP_AT_PHASE_IDLE = 0,
    ESP_AT_PHASE_TX_CMD,
    ESP_AT_PHASE_WAIT_PROMPT,
    ESP_AT_PHASE_TX_PAYLOAD,
    ESP_AT_PHASE_WAIT_RESP,
    ESP_AT_PHASE_WAIT_SEND_OK
} esp_at_phase_t;

typedef struct
{
    esp_at_io_t io;

    esp_at_on_urc_t urc_cb;
    void *urc_user;

    esp_at_cmd_t queue[ESP_AT_QUEUE_LEN];
    uint8_t q_head;
    uint8_t q_tail;
    uint8_t q_count;

    esp_at_cmd_t cur;
    uint8_t cur_valid;
    esp_at_phase_t phase;
    uint16_t timeout_ticks;

    /* 发送残留（避免阻塞，分 tick 发送） */
    const uint8_t *tx_ptr;
    uint16_t tx_len;
    uint16_t tx_off;

    /* 文本行解析 */
    char line_buf[ESP_AT_LINE_BUF_SIZE];
    uint16_t line_len;

    /* +IPD / +CIPRECVDATA payload */
    uint16_t ipd_remain;
    uint8_t ipd_chunk[64];
    uint16_t ipd_chunk_len;

    uint8_t prompt_just_seen; /* 用于忽略 prompt 后的空格（\"> \"） */
} esp_at_engine_t;

static esp_at_engine_t s_eng = {0};

static uint16_t esp_at_ms_to_ticks(uint32_t ms)
{
    uint32_t t = (ms + 4U) / 5U;
    if (t == 0)
    {
        t = 1;
    }
    if (t > 0xFFFF)
    {
        t = 0xFFFF;
    }
    return (uint16_t)t;
}

static uint8_t esp_at_queue_is_full(void)
{
    return (s_eng.q_count >= ESP_AT_QUEUE_LEN) ? 1 : 0;
}

static uint8_t esp_at_queue_is_empty(void)
{
    return (s_eng.q_count == 0) ? 1 : 0;
}

static void esp_at_queue_push(const esp_at_cmd_t *cmd)
{
    s_eng.queue[s_eng.q_head] = *cmd;
    s_eng.q_head++;
    if (s_eng.q_head >= ESP_AT_QUEUE_LEN)
    {
        s_eng.q_head = 0;
    }
    s_eng.q_count++;
}

static void esp_at_queue_pop(esp_at_cmd_t *out)
{
    *out = s_eng.queue[s_eng.q_tail];
    s_eng.q_tail++;
    if (s_eng.q_tail >= ESP_AT_QUEUE_LEN)
    {
        s_eng.q_tail = 0;
    }
    s_eng.q_count--;
}

static void esp_at_start_tx(const uint8_t *ptr, uint16_t len)
{
    s_eng.tx_ptr = ptr;
    s_eng.tx_len = len;
    s_eng.tx_off = 0;
}

static void esp_at_finish_cmd(esp_at_result_t res)
{
    if (s_eng.cur_valid && s_eng.cur.done_cb != NULL)
    {
        s_eng.cur.done_cb(res, s_eng.cur.user);
    }

    s_eng.cur_valid = 0;
    s_eng.phase = ESP_AT_PHASE_IDLE;
    s_eng.timeout_ticks = 0;
    s_eng.tx_ptr = NULL;
    s_eng.tx_len = 0;
    s_eng.tx_off = 0;
}

static void esp_at_try_send_pending(void)
{
    if (s_eng.tx_ptr == NULL || s_eng.tx_off >= s_eng.tx_len)
    {
        return;
    }

    uint16_t remain = (uint16_t)(s_eng.tx_len - s_eng.tx_off);
    uint16_t wrote = 0;

    if (s_eng.io.write != NULL)
    {
        wrote = s_eng.io.write((const uint8_t *)&s_eng.tx_ptr[s_eng.tx_off], remain);
    }

    if (wrote > 0)
    {
        s_eng.tx_off = (uint16_t)(s_eng.tx_off + wrote);

        if (s_eng.tx_off >= s_eng.tx_len)
        {
            /* 当前段发送完毕 */
            s_eng.tx_ptr = NULL;
            s_eng.tx_len = 0;
            s_eng.tx_off = 0;

            if (s_eng.phase == ESP_AT_PHASE_TX_CMD)
            {
                if (s_eng.cur_valid && s_eng.cur.wait_prompt)
                {
                    s_eng.phase = ESP_AT_PHASE_WAIT_PROMPT;
                    s_eng.timeout_ticks = s_eng.cur.timeout_ticks;
                }
                else
                {
                    s_eng.phase = ESP_AT_PHASE_WAIT_RESP;
                    s_eng.timeout_ticks = s_eng.cur_valid ? s_eng.cur.timeout_ticks : 0;
                }
            }
            else if (s_eng.phase == ESP_AT_PHASE_TX_PAYLOAD)
            {
                s_eng.phase = s_eng.cur_valid && s_eng.cur.wait_send_ok ? ESP_AT_PHASE_WAIT_SEND_OK : ESP_AT_PHASE_WAIT_RESP;
                s_eng.timeout_ticks = s_eng.cur_valid ? s_eng.cur.payload_timeout_ticks : 0;
            }
        }
    }
}

static uint8_t esp_at_starts_with(const char *s, const char *prefix)
{
    while (*prefix)
    {
        if (*s++ != *prefix++)
        {
            return 0;
        }
    }
    return 1;
}

static uint16_t esp_at_parse_payload_len_from_header(const char *hdr, uint16_t hdr_len)
{
    /* hdr: "+IPD,..." 或 "+CIPRECVDATA,..."，长度字段在最后一个 ',' 之后 */
    int last_comma = -1;
    for (uint16_t i = 0; i < hdr_len; i++)
    {
        if (hdr[i] == ',')
        {
            last_comma = (int)i;
        }
    }

    if (last_comma < 0 || (uint16_t)(last_comma + 1) >= hdr_len)
    {
        return 0;
    }

    uint32_t val = 0;
    uint16_t i = (uint16_t)(last_comma + 1);
    for (; i < hdr_len; i++)
    {
        char c = hdr[i];
        if (c < '0' || c > '9')
        {
            break;
        }
        val = val * 10U + (uint32_t)(c - '0');
        if (val > 0xFFFFU)
        {
            val = 0xFFFFU;
            break;
        }
    }

    return (uint16_t)val;
}

static void esp_at_emit_urc(esp_at_urc_type_t type, const uint8_t *data, uint16_t len)
{
    if (s_eng.urc_cb != NULL)
    {
        s_eng.urc_cb(type, data, len, s_eng.urc_user);
    }
}

static void esp_at_on_prompt(void)
{
    /* 通知上层 */
    esp_at_emit_urc(ESP_AT_URC_PROMPT, NULL, 0);
    s_eng.prompt_just_seen = 1;

    /* 若当前命令在等待 prompt，则开始发送 payload */
    if (s_eng.cur_valid && s_eng.phase == ESP_AT_PHASE_WAIT_PROMPT)
    {
        if (s_eng.cur.payload != NULL && s_eng.cur.payload_len > 0)
        {
            s_eng.phase = ESP_AT_PHASE_TX_PAYLOAD;
            esp_at_start_tx(s_eng.cur.payload, s_eng.cur.payload_len);
        }
        else
        {
            /* 无 payload 也视为异常完成 */
            esp_at_finish_cmd(ESP_AT_RES_ERROR);
        }
    }
}

static void esp_at_on_line(const char *line)
{
    /* 所有行都作为 URC 上报，便于上层做日志/状态判断 */
    esp_at_emit_urc(ESP_AT_URC_LINE, (const uint8_t *)line, (uint16_t)strlen(line));

    if (!s_eng.cur_valid)
    {
        return;
    }

    if (s_eng.phase == ESP_AT_PHASE_WAIT_PROMPT)
    {
        /* 等待 prompt 阶段也要识别 ERROR，避免一直超时 */
        if ((strcmp(line, "ERROR") == 0) || (strcmp(line, "FAIL") == 0))
        {
            esp_at_finish_cmd(ESP_AT_RES_ERROR);
        }
    }
    else if (s_eng.phase == ESP_AT_PHASE_WAIT_RESP)
    {
        if (strcmp(line, "OK") == 0)
        {
            esp_at_finish_cmd(ESP_AT_RES_OK);
        }
        else if ((strcmp(line, "ERROR") == 0) || (strcmp(line, "FAIL") == 0))
        {
            esp_at_finish_cmd(ESP_AT_RES_ERROR);
        }
    }
    else if (s_eng.phase == ESP_AT_PHASE_WAIT_SEND_OK)
    {
        if (strcmp(line, "SEND OK") == 0)
        {
            esp_at_finish_cmd(ESP_AT_RES_OK);
        }
        else if ((strcmp(line, "SEND FAIL") == 0) || (strcmp(line, "ERROR") == 0) || (strcmp(line, "FAIL") == 0))
        {
            esp_at_finish_cmd(ESP_AT_RES_ERROR);
        }
    }
}

static void esp_at_rx_byte(uint8_t ch)
{
    /* payload 模式：直接下发，不做行解析 */
    if (s_eng.ipd_remain > 0)
    {
        s_eng.ipd_chunk[s_eng.ipd_chunk_len++] = ch;
        s_eng.ipd_remain--;

        if (s_eng.ipd_chunk_len >= sizeof(s_eng.ipd_chunk) || s_eng.ipd_remain == 0)
        {
            esp_at_emit_urc(ESP_AT_URC_IPD, s_eng.ipd_chunk, s_eng.ipd_chunk_len);
            s_eng.ipd_chunk_len = 0;
        }
        return;
    }

    /* prompt 检测（通常为 '>' 或 '> '，不以 CRLF 结束） */
    if (ch == '>' && s_eng.line_len == 0)
    {
        esp_at_on_prompt();
        return;
    }

    /* 忽略 prompt 后的一个空格："> " */
    if (s_eng.prompt_just_seen && ch == ' ' && s_eng.line_len == 0)
    {
        s_eng.prompt_just_seen = 0;
        return;
    }
    s_eng.prompt_just_seen = 0;

    if (ch == '\r')
    {
        return;
    }

    if (ch == '\n')
    {
        if (s_eng.line_len > 0)
        {
            s_eng.line_buf[s_eng.line_len] = '\0';
            esp_at_on_line(s_eng.line_buf);
            s_eng.line_len = 0;
        }
        return;
    }

    /* +IPD / +CIPRECVDATA 头部以 ':' 结束，payload 紧随其后 */
    if (ch == ':')
    {
        if (s_eng.line_len >= 5 &&
            (esp_at_starts_with(s_eng.line_buf, "+IPD,") || esp_at_starts_with(s_eng.line_buf, "+CIPRECVDATA,")))
        {
            uint16_t payload_len = esp_at_parse_payload_len_from_header(s_eng.line_buf, s_eng.line_len);
            s_eng.line_len = 0;

            if (payload_len > 0)
            {
                s_eng.ipd_remain = payload_len;
                s_eng.ipd_chunk_len = 0;
            }
            return;
        }
    }

    if (s_eng.line_len < (ESP_AT_LINE_BUF_SIZE - 1))
    {
        s_eng.line_buf[s_eng.line_len++] = (char)ch;
    }
    else
    {
        /* 行过长：丢弃并复位（避免溢出） */
        s_eng.line_len = 0;
    }
}

void esp_at_init(const esp_at_io_t *io, esp_at_on_urc_t urc_cb, void *urc_user)
{
    memset(&s_eng, 0, sizeof(s_eng));

    if (io != NULL)
    {
        s_eng.io = *io;
    }

    s_eng.urc_cb = urc_cb;
    s_eng.urc_user = urc_user;

    s_eng.phase = ESP_AT_PHASE_IDLE;
}

void esp_at_reset(void)
{
    s_eng.q_head = 0;
    s_eng.q_tail = 0;
    s_eng.q_count = 0;

    s_eng.cur_valid = 0;
    s_eng.phase = ESP_AT_PHASE_IDLE;
    s_eng.timeout_ticks = 0;

    s_eng.tx_ptr = NULL;
    s_eng.tx_len = 0;
    s_eng.tx_off = 0;

    s_eng.line_len = 0;
    s_eng.ipd_remain = 0;
    s_eng.ipd_chunk_len = 0;
    s_eng.prompt_just_seen = 0;
}

static esp_at_result_t esp_at_enqueue_common(esp_at_cmd_t *c)
{
    if (esp_at_queue_is_full())
    {
        return ESP_AT_RES_QUEUE_FULL;
    }

    esp_at_queue_push(c);
    return ESP_AT_RES_OK;
}

static void esp_at_prepare_cmd_string(esp_at_cmd_t *c, const char *cmd_in)
{
    memset(c->cmd, 0, sizeof(c->cmd));

    /* 复制并确保以 \r\n 结尾 */
    size_t in_len = strlen(cmd_in);
    if (in_len >= (ESP_AT_CMD_MAX_LEN - 1))
    {
        in_len = (ESP_AT_CMD_MAX_LEN - 1);
    }

    memcpy(c->cmd, cmd_in, in_len);
    c->cmd_len = (uint16_t)in_len;

    if (c->cmd_len >= 2)
    {
        if (c->cmd[c->cmd_len - 2] == '\r' && c->cmd[c->cmd_len - 1] == '\n')
        {
            return;
        }
    }

    if (c->cmd_len < (ESP_AT_CMD_MAX_LEN - 2))
    {
        c->cmd[c->cmd_len++] = '\r';
        c->cmd[c->cmd_len++] = '\n';
        c->cmd[c->cmd_len] = '\0';
    }
}

esp_at_result_t esp_at_enqueue_cmd(const char *cmd,
                                  uint32_t timeout_ms,
                                  esp_at_on_cmd_done_t done_cb,
                                  void *user)
{
    if (cmd == NULL || s_eng.io.write == NULL || s_eng.io.read_byte == NULL)
    {
        return ESP_AT_RES_ERROR;
    }

    esp_at_cmd_t c = {0};
    esp_at_prepare_cmd_string(&c, cmd);
    c.timeout_ticks = esp_at_ms_to_ticks(timeout_ms);
    c.done_cb = done_cb;
    c.user = user;
    c.wait_prompt = 0;
    c.wait_send_ok = 0;

    return esp_at_enqueue_common(&c);
}

esp_at_result_t esp_at_enqueue_cmd_with_payload(const char *cmd,
                                               uint32_t cmd_timeout_ms,
                                               const uint8_t *payload,
                                               uint16_t payload_len,
                                               uint32_t payload_timeout_ms,
                                               esp_at_on_cmd_done_t done_cb,
                                               void *user)
{
    if (cmd == NULL || payload == NULL || payload_len == 0 || s_eng.io.write == NULL || s_eng.io.read_byte == NULL)
    {
        return ESP_AT_RES_ERROR;
    }

    esp_at_cmd_t c = {0};
    esp_at_prepare_cmd_string(&c, cmd);
    c.timeout_ticks = esp_at_ms_to_ticks(cmd_timeout_ms);
    c.payload = payload;
    c.payload_len = payload_len;
    c.payload_timeout_ticks = esp_at_ms_to_ticks(payload_timeout_ms);
    c.wait_prompt = 1;
    c.wait_send_ok = 1;
    c.done_cb = done_cb;
    c.user = user;

    return esp_at_enqueue_common(&c);
}

uint8_t esp_at_is_idle(void)
{
    if (s_eng.phase != ESP_AT_PHASE_IDLE)
    {
        return 0;
    }

    if (!esp_at_queue_is_empty())
    {
        return 0;
    }

    if (s_eng.tx_ptr != NULL)
    {
        return 0;
    }

    return 1;
}

static void esp_at_start_next_cmd_if_needed(void)
{
    if (s_eng.phase != ESP_AT_PHASE_IDLE || s_eng.cur_valid)
    {
        return;
    }

    if (esp_at_queue_is_empty())
    {
        return;
    }

    esp_at_queue_pop(&s_eng.cur);
    s_eng.cur_valid = 1;

    s_eng.phase = ESP_AT_PHASE_TX_CMD;
    esp_at_start_tx((const uint8_t *)s_eng.cur.cmd, s_eng.cur.cmd_len);
}

static void esp_at_tick_timeout(void)
{
    if (s_eng.phase == ESP_AT_PHASE_WAIT_PROMPT ||
        s_eng.phase == ESP_AT_PHASE_WAIT_RESP ||
        s_eng.phase == ESP_AT_PHASE_WAIT_SEND_OK)
    {
        if (s_eng.timeout_ticks > 0)
        {
            s_eng.timeout_ticks--;
            if (s_eng.timeout_ticks == 0)
            {
                esp_at_finish_cmd(ESP_AT_RES_TIMEOUT);
            }
        }
    }
}

void esp_at_tick_5ms(void)
{
    /* 1) 继续发送残留 */
    esp_at_try_send_pending();

    /* 2) 接收解析（预算限制） */
    for (uint16_t i = 0; i < ESP_AT_RX_BUDGET_PER_TICK; i++)
    {
        uint8_t ch = 0;
        if (s_eng.io.read_byte == NULL)
        {
            break;
        }

        if (s_eng.io.read_byte(&ch) != 0)
        {
            break;
        }

        esp_at_rx_byte(ch);
    }

    /* 3) 超时推进 */
    esp_at_tick_timeout();

    /* 4) 启动下一条命令 */
    esp_at_start_next_cmd_if_needed();

    /* 5) 若刚启动命令，可尝试立刻送出（避免多等 1 个 tick） */
    esp_at_try_send_pending();
}
