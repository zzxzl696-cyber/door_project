/********************************** (C) COPYRIGHT *******************************
 * File Name          : access_log.c
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-05
 * Description        : 开门日志模块实现 - Flash循环缓冲存储
 *********************************************************************************/

#include "access_log.h"
#include "ch32v30x.h"
#include "timer_config.h"
#include "debug.h"
#include <string.h>

/* Flash操作配置 */
#define FLASH_PAGE_SIZE             4096  /* CH32V307标准页擦除大小4KB */
#define ACCESS_LOG_HEADER_ADDR      ACCESS_LOG_FLASH_BASE
#define ACCESS_LOG_DATA_ADDR        (ACCESS_LOG_FLASH_BASE + FLASH_PAGE_SIZE)  /* 跳过一整页头部 */

/* 内部缓存 */
static access_log_header_t s_log_header;
static uint8_t s_initialized = 0;

/* ================= 内部函数声明 ================= */
static void flash_unlock(void);
static void flash_lock(void);
static FLASH_Status flash_erase_page(uint32_t addr);
static FLASH_Status flash_write_word(uint32_t addr, uint32_t data);
static void flash_read(uint32_t addr, uint8_t *buf, uint32_t len);
static access_log_result_t write_header(void);
static access_log_result_t write_record(uint16_t slot, const access_record_t *record);
static void read_record(uint16_t slot, access_record_t *record);

/* ================= Flash操作函数 ================= */

static void flash_unlock(void)
{
    FLASH_Unlock();
}

static void flash_lock(void)
{
    FLASH_Lock();
}

static FLASH_Status flash_erase_page(uint32_t addr)
{
    return FLASH_ErasePage(addr);
}

static FLASH_Status flash_write_word(uint32_t addr, uint32_t data)
{
    return FLASH_ProgramWord(addr, data);
}

static void flash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t *p = (uint8_t *)addr;
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = p[i];
    }
}

/* ================= 日志操作函数 ================= */

/**
 * @brief 写入日志头部到Flash
 */
static access_log_result_t write_header(void)
{
    uint32_t *p = (uint32_t *)&s_log_header;
    uint32_t addr = ACCESS_LOG_HEADER_ADDR;
    FLASH_Status status;

    flash_unlock();

    /* 擦除头部所在页 */
    status = flash_erase_page(addr);
    if (status != FLASH_COMPLETE) {
        flash_lock();
        return ACCESS_LOG_ERR_FLASH;
    }

    /* 写入头部 */
    for (uint32_t i = 0; i < sizeof(access_log_header_t) / 4; i++) {
        status = flash_write_word(addr + i * 4, p[i]);
        if (status != FLASH_COMPLETE) {
            flash_lock();
            return ACCESS_LOG_ERR_FLASH;
        }
    }

    flash_lock();
    return ACCESS_LOG_OK;
}

/**
 * @brief 写入记录到指定槽位
 */
static access_log_result_t write_record(uint16_t slot, const access_record_t *record)
{
    if (slot >= ACCESS_LOG_MAX_RECORDS) {
        return ACCESS_LOG_ERR_INVALID;
    }

    uint32_t addr = ACCESS_LOG_DATA_ADDR + slot * ACCESS_LOG_RECORD_SIZE;
    uint32_t *p = (uint32_t *)record;
    FLASH_Status status;

    flash_unlock();

    /* 擦除记录所在页 */
    uint32_t page_start = addr & ~(FLASH_PAGE_SIZE - 1);
    status = flash_erase_page(page_start);
    if (status != FLASH_COMPLETE) {
        flash_lock();
        return ACCESS_LOG_ERR_FLASH;
    }

    /* 写入记录数据（16字节 = 4个字） */
    for (int i = 0; i < (int)(ACCESS_LOG_RECORD_SIZE / 4); i++) {
        status = flash_write_word(addr + i * 4, p[i]);
        if (status != FLASH_COMPLETE) {
            flash_lock();
            return ACCESS_LOG_ERR_FLASH;
        }
    }

    flash_lock();
    return ACCESS_LOG_OK;
}

/**
 * @brief 从指定槽位读取记录
 */
static void read_record(uint16_t slot, access_record_t *record)
{
    if (slot >= ACCESS_LOG_MAX_RECORDS || record == NULL) {
        return;
    }

    uint32_t addr = ACCESS_LOG_DATA_ADDR + slot * ACCESS_LOG_RECORD_SIZE;
    flash_read(addr, (uint8_t *)record, ACCESS_LOG_RECORD_SIZE);
}

/* ================= 公开API实现 ================= */

access_log_result_t access_log_init(void)
{
    /* 读取头部 */
    flash_read(ACCESS_LOG_HEADER_ADDR, (uint8_t *)&s_log_header, sizeof(access_log_header_t));

    /* 检查魔数 */
    if (s_log_header.magic != ACCESS_LOG_MAGIC) {
        printf("[AccessLog] Invalid magic, formatting log...\r\n");
        return access_log_format();
    }

    s_initialized = 1;
    printf("[AccessLog] Log initialized, %d records, seq=%lu\r\n",
           s_log_header.total_records, s_log_header.sequence_number);
    return ACCESS_LOG_OK;
}

access_log_result_t access_log_format(void)
{
    printf("[AccessLog] Formatting log...\r\n");

    /* 初始化头部 */
    memset(&s_log_header, 0, sizeof(access_log_header_t));
    s_log_header.magic = ACCESS_LOG_MAGIC;
    s_log_header.version = 0x0100;  /* v1.0 */
    s_log_header.total_records = 0;
    s_log_header.head_index = 0;
    s_log_header.tail_index = 0;
    s_log_header.sequence_number = 0;

    /* 写入头部 */
    access_log_result_t ret = write_header();
    if (ret != ACCESS_LOG_OK) {
        return ret;
    }

    /* 擦除记录数据区 */
    flash_unlock();
    for (uint32_t addr = ACCESS_LOG_DATA_ADDR;
         addr < ACCESS_LOG_DATA_ADDR + ACCESS_LOG_MAX_RECORDS * ACCESS_LOG_RECORD_SIZE;
         addr += FLASH_PAGE_SIZE) {
        flash_erase_page(addr);
    }
    flash_lock();

    s_initialized = 1;
    printf("[AccessLog] Log formatted\r\n");
    return ACCESS_LOG_OK;
}

access_log_result_t access_log_record(uint16_t user_id,
                                       auth_method_t method,
                                       auth_result_t result,
                                       const uint8_t *uid)
{
    if (!s_initialized) {
        return ACCESS_LOG_ERR_INVALID;
    }

    access_record_t record;
    memset(&record, 0, sizeof(access_record_t));

    /* 填充记录 */
    record.timestamp = TIM_Get_MillisCounter();
    record.user_id = user_id;
    record.auth_method = (uint8_t)method;
    record.result = (uint8_t)result;

    if (uid != NULL) {
        memcpy(record.rfid_uid, uid, 4);
    }

    /* 确定写入槽位 */
    uint16_t write_slot = s_log_header.head_index;

    /* 写入记录 */
    access_log_result_t ret = write_record(write_slot, &record);
    if (ret != ACCESS_LOG_OK) {
        return ret;
    }

    /* 更新索引 */
    s_log_header.head_index = (s_log_header.head_index + 1) % ACCESS_LOG_MAX_RECORDS;
    s_log_header.sequence_number++;

    if (s_log_header.total_records < ACCESS_LOG_MAX_RECORDS) {
        s_log_header.total_records++;
    } else {
        /* 循环覆盖，移动尾索引 */
        s_log_header.tail_index = (s_log_header.tail_index + 1) % ACCESS_LOG_MAX_RECORDS;
    }

    /* 更新头部 */
    ret = write_header();

    printf("[AccessLog] Record: user=%d, method=%d, result=%d\r\n",
           user_id, method, result);
    return ret;
}

uint16_t access_log_get_count(void)
{
    if (!s_initialized) {
        return 0;
    }
    return s_log_header.total_records;
}

access_log_result_t access_log_get_record(uint16_t index, access_record_t *out)
{
    if (!s_initialized || out == NULL) {
        return ACCESS_LOG_ERR_INVALID;
    }

    if (index >= s_log_header.total_records) {
        return ACCESS_LOG_ERR_INVALID;
    }

    /* 索引0=最新记录，需要从head向后计算 */
    /* head指向下一个写入位置，最新记录在head-1 */
    int16_t slot = (int16_t)s_log_header.head_index - 1 - (int16_t)index;
    if (slot < 0) {
        slot += ACCESS_LOG_MAX_RECORDS;
    }

    read_record((uint16_t)slot, out);
    return ACCESS_LOG_OK;
}

access_log_result_t access_log_get_latest(access_record_t *out)
{
    if (!s_initialized || out == NULL) {
        return ACCESS_LOG_ERR_INVALID;
    }

    if (s_log_header.total_records == 0) {
        return ACCESS_LOG_ERR_EMPTY;
    }

    return access_log_get_record(0, out);
}

access_log_result_t access_log_clear(void)
{
    return access_log_format();
}

uint32_t access_log_get_sequence(void)
{
    if (!s_initialized) {
        return 0;
    }
    return s_log_header.sequence_number;
}

const char *access_log_method_str(auth_method_t method)
{
    switch (method) {
        case AUTH_NONE:        return "None";
        case AUTH_PASSWORD:    return "Password";
        case AUTH_RFID:        return "RFID";
        case AUTH_FINGERPRINT: return "Fingerprint";
        case AUTH_FACE:        return "Face";
        case AUTH_REMOTE:      return "Remote";
        default:               return "Unknown";
    }
}

const char *access_log_result_str(auth_result_t result)
{
    switch (result) {
        case AUTH_RESULT_SUCCESS:        return "Success";
        case AUTH_RESULT_FAILED:         return "Failed";
        case AUTH_RESULT_USER_NOT_FOUND: return "Not Found";
        case AUTH_RESULT_USER_DISABLED:  return "Disabled";
        case AUTH_RESULT_TIMEOUT:        return "Timeout";
        case AUTH_RESULT_DOOR_FORCED:    return "Forced";
        default:                         return "Unknown";
    }
}
