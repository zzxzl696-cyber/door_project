/********************************** (C) COPYRIGHT *******************************
 * File Name          : esp_at.h
 * Author             : Custom Implementation
 * Version            : V1.0.0
 * Date               : 2026-02-15
 * Description        : ESP8266 AT 命令引擎（非阻塞）
 *                      - 命令队列（8~16 条）
 *                      - 响应匹配（OK/ERROR/SEND OK/>/+IPD）
 *                      - 超时处理（5ms Tick 驱动）
 *
 * 使用方式：
 * 1) esp_at_init() 绑定底层 IO（write/readByte）与 URC 回调
 * 2) 周期性调用 esp_at_tick_5ms()
 * 3) 通过 esp_at_enqueue_cmd() / esp_at_enqueue_cmd_with_payload() 异步发送命令
 *********************************************************************************/

#ifndef __ESP_AT_H
#define __ESP_AT_H

#include "ch32v30x.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================= 配置参数 ================= */

#define ESP_AT_QUEUE_LEN        12
#define ESP_AT_CMD_MAX_LEN      256  /* 增大以容纳 AT+MQTTUSERCFG 长 token（~178 字节）*/
#define ESP_AT_LINE_BUF_SIZE    512

/* 每次 tick 最多处理的接收字节数（防止任务饥饿） */
#ifndef ESP_AT_RX_BUDGET_PER_TICK
#define ESP_AT_RX_BUDGET_PER_TICK 256
#endif

/* ================= 类型定义 ================= */

typedef enum
{
    ESP_AT_RES_OK = 0,
    ESP_AT_RES_ERROR,
    ESP_AT_RES_TIMEOUT,
    ESP_AT_RES_QUEUE_FULL,
    ESP_AT_RES_IO_BUSY
} esp_at_result_t;

typedef enum
{
    ESP_AT_URC_LINE = 0,  /* 异步文本行（不区分是否属于命令响应） */
    ESP_AT_URC_IPD,       /* TCP 推送数据（+IPD,<len>:...） */
    ESP_AT_URC_PROMPT     /* 发送提示符（>） */
} esp_at_urc_type_t;

typedef uint16_t (*esp_at_uart_write_t)(const uint8_t *data, uint16_t len);
typedef int (*esp_at_uart_read_byte_t)(uint8_t *data);

typedef void (*esp_at_on_cmd_done_t)(esp_at_result_t result, void *user);
typedef void (*esp_at_on_urc_t)(esp_at_urc_type_t type,
                               const uint8_t *data,
                               uint16_t len,
                               void *user);

typedef struct
{
    esp_at_uart_write_t write;
    esp_at_uart_read_byte_t read_byte;
} esp_at_io_t;

typedef struct
{
    char cmd[ESP_AT_CMD_MAX_LEN];
    uint16_t cmd_len;

    uint16_t timeout_ticks; /* 命令阶段超时（5ms tick） */

    const uint8_t *payload;
    uint16_t payload_len;
    uint16_t payload_timeout_ticks; /* payload 后等待 SEND OK 的超时 */

    uint8_t wait_prompt; /* 1=等待 '>' 再发送 payload */
    uint8_t wait_send_ok;/* 1=等待 SEND OK/SEND FAIL */

    esp_at_on_cmd_done_t done_cb;
    void *user;
} esp_at_cmd_t;

/* ================= Public API ================= */

/**
 * @brief  初始化 AT 引擎（绑定 IO 与 URC 回调）
 * @param  io: 底层 IO
 * @param  urc_cb: URC 回调（可为 NULL）
 * @param  urc_user: URC 回调用户参数
 * @retval None
 */
void esp_at_init(const esp_at_io_t *io, esp_at_on_urc_t urc_cb, void *urc_user);

/**
 * @brief  5ms Tick 驱动函数（必须在调度器 5ms 任务中调用）
 * @retval None
 */
void esp_at_tick_5ms(void);

/**
 * @brief  入队普通 AT 命令（默认等待 OK/ERROR）
 * @param  cmd: AT 命令字符串（建议包含 \\r\\n；若未包含会自动补齐）
 * @param  timeout_ms: 超时时间（毫秒）
 * @param  done_cb: 完成回调（可为 NULL）
 * @param  user: 回调用户参数
 * @retval ESP_AT_RES_OK 或错误码
 */
esp_at_result_t esp_at_enqueue_cmd(const char *cmd,
                                  uint32_t timeout_ms,
                                  esp_at_on_cmd_done_t done_cb,
                                  void *user);

/**
 * @brief  入队带 payload 的命令（用于 AT+CIPSEND 场景）
 * @note   payload 指针必须在命令完成前保持有效（不拷贝数据）。
 * @param  cmd: 命令字符串（例如 \"AT+CIPSEND=12\\r\\n\"）
 * @param  cmd_timeout_ms: 等待 '>' 的超时（毫秒）
 * @param  payload: payload 数据
 * @param  payload_len: payload 长度
 * @param  payload_timeout_ms: 发送 payload 后等待 SEND OK 的超时（毫秒）
 * @param  done_cb: 完成回调（可为 NULL）
 * @param  user: 回调用户参数
 * @retval ESP_AT_RES_OK 或错误码
 */
esp_at_result_t esp_at_enqueue_cmd_with_payload(const char *cmd,
                                               uint32_t cmd_timeout_ms,
                                               const uint8_t *payload,
                                               uint16_t payload_len,
                                               uint32_t payload_timeout_ms,
                                               esp_at_on_cmd_done_t done_cb,
                                               void *user);

/**
 * @brief  查询 AT 引擎是否空闲（无当前命令、无发送残留、队列为空）
 * @retval 1=空闲，0=忙
 */
uint8_t esp_at_is_idle(void);

/**
 * @brief  清空队列并复位内部解析状态
 * @retval None
 */
void esp_at_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __ESP_AT_H */

