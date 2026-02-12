/********************************** (C) COPYRIGHT *******************************
 * File Name          : access_log.h
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-05
 * Description        : 开门日志模块 - Flash循环缓冲存储
 *                      支持256条记录，自动循环覆盖
 *********************************************************************************/

#ifndef __ACCESS_LOG_H
#define __ACCESS_LOG_H

#include <stdint.h>
#include "door_control.h"  /* 使用auth_method_t */

/* Flash存储配置 */
#define ACCESS_LOG_FLASH_BASE   0x08040000  /* 日志起始地址 */
#define ACCESS_LOG_FLASH_SIZE   0x6000      /* 24KB空间 */
#define ACCESS_LOG_MAX_RECORDS  256         /* 最大记录数 */

/* 记录结构体大小 */
#define ACCESS_LOG_RECORD_SIZE  16          /* 16字节/记录 */

/* 日志头部魔数 */
#define ACCESS_LOG_MAGIC        0xAA55BEEF

/* 认证结果定义 */
typedef enum {
    AUTH_RESULT_SUCCESS = 0,        /* 认证成功 */
    AUTH_RESULT_FAILED,             /* 认证失败 */
    AUTH_RESULT_USER_NOT_FOUND,     /* 用户不存在 */
    AUTH_RESULT_USER_DISABLED,      /* 用户已禁用 */
    AUTH_RESULT_TIMEOUT,            /* 超时 */
    AUTH_RESULT_DOOR_FORCED         /* 门被强制打开 */
} auth_result_t;

/* 日志记录结构体 (16字节) */
typedef struct {
    uint32_t timestamp;             /* 时间戳/毫秒计数 (4字节) */
    uint16_t user_id;               /* 用户ID，0表示未知用户 (2字节) */
    uint8_t auth_method;            /* 认证方式 (1字节) */
    uint8_t result;                 /* 认证结果 (1字节) */
    uint8_t rfid_uid[4];            /* RFID卡号 (4字节) */
    uint8_t reserved[4];            /* 保留字段 (4字节) */
} access_record_t;                  /* 总计: 16字节 */

/* 日志头部结构体 (256字节，用于管理) */
typedef struct {
    uint32_t magic;                 /* 魔数标识 */
    uint16_t version;               /* 日志版本 */
    uint16_t total_records;         /* 当前记录总数 */
    uint16_t head_index;            /* 头索引（最新记录） */
    uint16_t tail_index;            /* 尾索引（最旧记录） */
    uint32_t sequence_number;       /* 全局序列号 */
    uint8_t reserved[240];          /* 保留对齐到256字节 */
} access_log_header_t;

/* 返回值定义 */
typedef enum {
    ACCESS_LOG_OK = 0,              /* 成功 */
    ACCESS_LOG_ERR_FLASH,           /* Flash操作失败 */
    ACCESS_LOG_ERR_EMPTY,           /* 日志为空 */
    ACCESS_LOG_ERR_INVALID          /* 无效参数 */
} access_log_result_t;

/* ================= 初始化函数 ================= */

/**
 * @brief 初始化日志模块
 * @note  检查Flash中的日志头部，如无效则格式化
 * @return ACCESS_LOG_OK 成功
 */
access_log_result_t access_log_init(void);

/**
 * @brief 格式化日志（清空所有记录）
 * @return ACCESS_LOG_OK 成功
 */
access_log_result_t access_log_format(void);

/* ================= 记录操作函数 ================= */

/**
 * @brief 添加一条日志记录
 * @param user_id 用户ID（0表示未知用户）
 * @param method 认证方式
 * @param result 认证结果
 * @param uid RFID卡号（可为NULL）
 * @return ACCESS_LOG_OK 成功
 */
access_log_result_t access_log_record(uint16_t user_id,
                                       auth_method_t method,
                                       auth_result_t result,
                                       const uint8_t *uid);

/**
 * @brief 获取日志记录总数
 * @return 当前记录数
 */
uint16_t access_log_get_count(void);

/**
 * @brief 获取指定索引的记录（0=最新，count-1=最旧）
 * @param index 索引号
 * @param out 输出记录
 * @return ACCESS_LOG_OK 成功
 */
access_log_result_t access_log_get_record(uint16_t index, access_record_t *out);

/**
 * @brief 获取最新一条记录
 * @param out 输出记录
 * @return ACCESS_LOG_OK 成功
 */
access_log_result_t access_log_get_latest(access_record_t *out);

/**
 * @brief 清空所有日志
 * @return ACCESS_LOG_OK 成功
 */
access_log_result_t access_log_clear(void);

/* ================= 统计函数 ================= */

/**
 * @brief 获取全局序列号（总记录次数）
 * @return 序列号
 */
uint32_t access_log_get_sequence(void);

/**
 * @brief 获取认证方式字符串
 * @param method 认证方式
 * @return 字符串
 */
const char *access_log_method_str(auth_method_t method);

/**
 * @brief 获取认证结果字符串
 * @param result 认证结果
 * @return 字符串
 */
const char *access_log_result_str(auth_result_t result);

#endif /* __ACCESS_LOG_H */
