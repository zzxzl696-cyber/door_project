/********************************** (C) COPYRIGHT *******************************
 * File Name          : user_database.h
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-05
 * Description        : 用户数据库模块 - Flash存储用户信息
 *                      支持50用户，含RFID、指纹、密码认证
 *********************************************************************************/

#ifndef __USER_DATABASE_H
#define __USER_DATABASE_H

#include <stdint.h>

/* Flash存储配置 */
#define USER_DB_FLASH_BASE 0x08046000 /* 用户数据库起始地址 */
#define USER_DB_FLASH_SIZE 0x2000	  /* 8KB空间 */
#define USER_DB_MAX_USERS 50		  /* 最大用户数 */

/* 用户结构体大小 */
#define USER_ENTRY_SIZE 32 /* 32字节/用户 */

/* CH32V307 Flash擦除值（非标准0xFF，而是0xE339） */
#define FLASH_ERASED_HALFWORD 0xE339
#define FLASH_ERASED_WORD 0x39E339E3

/* 数据库头部魔数 (V4: 适配CH32V307 Flash擦除值，强制重新格式化) */
#define USER_DB_MAGIC 0x55AA1237

/* 用户标志位定义 */
#define USER_FLAG_VALID 0x0001	  /* 用户有效 */
#define USER_FLAG_ADMIN 0x0002	  /* 管理员权限 */
#define USER_FLAG_RFID_EN 0x0004  /* RFID已录入 */
#define USER_FLAG_FP_EN 0x0008	  /* 指纹已录入 */
#define USER_FLAG_PWD_EN 0x0010	  /* 密码已设置 */
#define USER_FLAG_DISABLED 0x0020 /* 用户禁用 */

/* 密码长度 */
#define USER_PASSWORD_LEN 4 /* 4位密码 */
#define USER_NAME_LEN 12	/* 用户名最大长度 */

/* 主密码相关 */
#define MASTER_PASSWORD_DEFAULT {0, 0, 0, 0} /* 默认主密码0000 */

/* 返回值定义 */
typedef enum
{
	USER_DB_OK = 0,		   /* 成功 */
	USER_DB_ERR_FULL,	   /* 数据库已满 */
	USER_DB_ERR_NOT_FOUND, /* 用户不存在 */
	USER_DB_ERR_EXISTS,	   /* 用户已存在 */
	USER_DB_ERR_FLASH,	   /* Flash操作失败 */
	USER_DB_ERR_INVALID,   /* 无效参数 */
	USER_DB_ERR_PASSWORD,  /* 密码错误 */
	USER_DB_ERR_DISABLED   /* 用户已禁用 */
} user_db_result_t;

/* 用户条目结构体 (32字节) */
typedef struct
{
	uint16_t id;						 /* 用户ID (2字节) */
	char name[USER_NAME_LEN];			 /* 用户名 (12字节) */
	uint8_t rfid_uid[4];				 /* RFID卡号 (4字节) */
	uint16_t fingerprint_id;			 /* 指纹模板ID (2字节) */
	uint8_t password[USER_PASSWORD_LEN]; /* 4位密码 (4字节) */
	uint16_t flags;						 /* 用户标志 (2字节) */
	uint8_t reserved[6];				 /* 保留字段 (6字节) */
} user_entry_t;							 /* 总计: 32字节 */

/* 数据库头部结构体 (32字节) */
typedef struct
{
	uint32_t magic;								/* 魔数标识 */
	uint16_t version;							/* 数据库版本 */
	uint16_t user_count;						/* 当前用户数 */
	uint16_t next_id;							/* 下一个用户ID */
	uint8_t master_password[USER_PASSWORD_LEN]; /* 主密码 */
	uint8_t reserved[18];						/* 保留字段 */
} user_db_header_t;								/* 总计: 32字节 */

/* ================= 初始化函数 ================= */

/**
 * @brief 初始化用户数据库
 * @note  检查Flash中的数据库头部，如无效则格式化
 * @return USER_DB_OK 成功
 */
user_db_result_t user_db_init(void);

/**
 * @brief 格式化数据库（清空所有用户）
 * @return USER_DB_OK 成功
 */
user_db_result_t user_db_format(void);

/* ================= 用户管理函数 ================= */

/**
 * @brief 添加新用户
 * @param user 用户信息指针（id字段将被自动分配）
 * @return USER_DB_OK 成功，其他失败
 */
user_db_result_t user_db_add_user(user_entry_t *user);

/**
 * @brief 删除用户
 * @param user_id 用户ID
 * @return USER_DB_OK 成功
 */
user_db_result_t user_db_delete_user(uint16_t user_id);

/**
 * @brief 更新用户信息
 * @param user 用户信息指针
 * @return USER_DB_OK 成功
 */
user_db_result_t user_db_update_user(const user_entry_t *user);

/**
 * @brief 获取用户数量
 * @return 当前用户数
 */
uint16_t user_db_get_count(void);

/* ================= 查询函数 ================= */

/**
 * @brief 通过用户ID查找
 * @param user_id 用户ID
 * @param out 输出用户信息
 * @return USER_DB_OK 找到
 */
user_db_result_t user_db_find_by_id(uint16_t user_id, user_entry_t *out);

/**
 * @brief 通过RFID卡号查找
 * @param uid 4字节RFID卡号
 * @param out 输出用户信息
 * @return USER_DB_OK 找到
 */
user_db_result_t user_db_find_by_rfid(const uint8_t uid[4], user_entry_t *out);

/**
 * @brief 通过指纹ID查找
 * @param fp_id 指纹模板ID
 * @param out 输出用户信息
 * @return USER_DB_OK 找到
 */
user_db_result_t user_db_find_by_fingerprint(uint16_t fp_id, user_entry_t *out);

/**
 * @brief 通过索引获取用户（用于遍历）
 * @param index 索引号 (0 ~ user_count-1)
 * @param out 输出用户信息
 * @return USER_DB_OK 成功
 */
user_db_result_t user_db_get_by_index(uint16_t index, user_entry_t *out);

/* ================= 认证函数 ================= */

/**
 * @brief 验证用户密码
 * @param password 4位密码
 * @param out 如匹配则输出用户信息
 * @return USER_DB_OK 验证成功
 */
user_db_result_t user_db_verify_password(const uint8_t password[USER_PASSWORD_LEN], user_entry_t *out);

/**
 * @brief 验证主密码（管理员密码）
 * @param password 4位密码
 * @return USER_DB_OK 验证成功
 */
user_db_result_t user_db_verify_master_password(const uint8_t password[USER_PASSWORD_LEN]);

/**
 * @brief 修改主密码
 * @param old_password 旧密码
 * @param new_password 新密码
 * @return USER_DB_OK 成功
 */
user_db_result_t user_db_change_master_password(const uint8_t old_password[USER_PASSWORD_LEN],
												const uint8_t new_password[USER_PASSWORD_LEN]);

/* ================= 用户属性设置 ================= */

/**
 * @brief 为用户绑定RFID卡
 * @param user_id 用户ID
 * @param uid 4字节RFID卡号
 * @return USER_DB_OK 成功
 */
user_db_result_t user_db_set_rfid(uint16_t user_id, const uint8_t uid[4]);

/**
 * @brief 为用户绑定指纹
 * @param user_id 用户ID
 * @param fp_id 指纹模板ID
 * @return USER_DB_OK 成功
 */
user_db_result_t user_db_set_fingerprint(uint16_t user_id, uint16_t fp_id);

/**
 * @brief 为用户设置密码
 * @param user_id 用户ID
 * @param password 4位密码
 * @return USER_DB_OK 成功
 */
user_db_result_t user_db_set_password(uint16_t user_id, const uint8_t password[USER_PASSWORD_LEN]);

#endif /* __USER_DATABASE_H */
