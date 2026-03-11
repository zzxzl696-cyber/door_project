/********************************** (C) COPYRIGHT *******************************
 * File Name          : user_database.c
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-05
 * Description        : 用户数据库模块实现 - Flash存储用户信息
 *********************************************************************************/

#include "bsp_system.h"

/* Flash操作配置 */
#define FLASH_PAGE_SIZE 4096 /* CH32V30x标准擦除页大小4KB（见FLASH_ErasePage） */
#define USER_DB_HEADER_ADDR USER_DB_FLASH_BASE
#define USER_DB_DATA_ADDR (USER_DB_FLASH_BASE + FLASH_PAGE_SIZE) /* 页对齐：用户数据与头部在不同Flash页 */

/* 内部缓存 */
static user_db_header_t s_db_header;
static uint8_t s_initialized = 0;
static uint8_t s_page_buf[FLASH_PAGE_SIZE]; /* Flash页缓冲区，用于页级读-改-写 */

/* ================= 内部函数声明 ================= */
static void flash_unlock(void);
static void flash_lock(void);
static FLASH_Status flash_erase_page(uint32_t addr);
static FLASH_Status flash_write_word(uint32_t addr, uint32_t data);
static void flash_read(uint32_t addr, uint8_t *buf, uint32_t len);
static user_db_result_t flash_write_safe(uint32_t addr, const uint8_t *data, uint32_t len);
static user_db_result_t write_header(void);
static user_db_result_t write_user(uint16_t slot, const user_entry_t *user);
static void read_user(uint16_t slot, user_entry_t *user);
static uint8_t is_slot_valid(const user_entry_t *user);
static int16_t find_empty_slot(void);
static int16_t find_user_slot(uint16_t user_id);

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
	for (uint32_t i = 0; i < len; i++)
	{
		buf[i] = p[i];
	}
}

/* ================= 数据库操作函数 ================= */

/**
 * @brief 页级安全写入Flash数据（读-改-写）
 * @note  先读取目标地址所在整页到缓冲区，修改目标区域，
 *        再擦除整页并回写，避免同页内其他数据被破坏
 */
static user_db_result_t flash_write_safe(uint32_t addr, const uint8_t *data, uint32_t len)
{
	uint32_t page_start = addr & ~((uint32_t)FLASH_PAGE_SIZE - 1);
	uint32_t offset = addr - page_start;
	FLASH_Status status;

	/* 读取整页到缓冲区 */
	flash_read(page_start, s_page_buf, FLASH_PAGE_SIZE);

	/* 修改目标区域 */
	memcpy(s_page_buf + offset, data, len);

	/* 擦除整页并回写 */
	flash_unlock();

	status = flash_erase_page(page_start);
	if (status != FLASH_COMPLETE)
	{
		flash_lock();
		return USER_DB_ERR_FLASH;
	}

	uint32_t *p = (uint32_t *)s_page_buf;
	for (uint32_t i = 0; i < FLASH_PAGE_SIZE / 4; i++)
	{
		status = flash_write_word(page_start + i * 4, p[i]);
		if (status != FLASH_COMPLETE)
		{
			flash_lock();
			return USER_DB_ERR_FLASH;
		}
	}

	flash_lock();
	return USER_DB_OK;
}

/**
 * @brief 写入数据库头部到Flash
 */
static user_db_result_t write_header(void)
{
	return flash_write_safe(USER_DB_HEADER_ADDR, (const uint8_t *)&s_db_header, sizeof(user_db_header_t));
}

/**
 * @brief 写入用户到指定槽位
 */
static user_db_result_t write_user(uint16_t slot, const user_entry_t *user)
{
	if (slot >= USER_DB_MAX_USERS)
	{
		return USER_DB_ERR_INVALID;
	}

	uint32_t addr = USER_DB_DATA_ADDR + slot * USER_ENTRY_SIZE;
	return flash_write_safe(addr, (const uint8_t *)user, USER_ENTRY_SIZE);
}

/**
 * @brief 从指定槽位读取用户
 */
static void read_user(uint16_t slot, user_entry_t *user)
{
	if (slot >= USER_DB_MAX_USERS || user == NULL)
	{
		return;
	}

	uint32_t addr = USER_DB_DATA_ADDR + slot * USER_ENTRY_SIZE;
	flash_read(addr, (uint8_t *)user, USER_ENTRY_SIZE);
}

/**
 * @brief 判断槽位是否为有效用户
 * @note  CH32V307 Flash擦除后flags=0xE339，需排除此状态
 */
static uint8_t is_slot_valid(const user_entry_t *user)
{
	return (user->flags != FLASH_ERASED_HALFWORD) && (user->flags & USER_FLAG_VALID);
}

/**
 * @brief 查找空闲槽位
 * @return 槽位索引，-1表示已满
 */
static int16_t find_empty_slot(void)
{
	user_entry_t user;

	for (uint16_t i = 0; i < USER_DB_MAX_USERS; i++)
	{
		read_user(i, &user);
		if (!is_slot_valid(&user))
		{
			return i;
		}
	}
	return -1;
}

/**
 * @brief 查找用户槽位
 * @return 槽位索引，-1表示未找到
 */
static int16_t find_user_slot(uint16_t user_id)
{
	user_entry_t user;

	for (uint16_t i = 0; i < USER_DB_MAX_USERS; i++)
	{
		read_user(i, &user);
		if (is_slot_valid(&user) && user.id == user_id)
		{
			return i;
		}
	}
	return -1;
}

/* ================= 公开API实现 ================= */

user_db_result_t user_db_init(void)
{
	/* 读取头部 */
	flash_read(USER_DB_HEADER_ADDR, (uint8_t *)&s_db_header, sizeof(user_db_header_t));

	/* 检查魔数 */
	if (s_db_header.magic != USER_DB_MAGIC)
	{
		printf("[UserDB] Invalid magic, formatting database...\r\n");
		return user_db_format();
	}

	s_initialized = 1;
	printf("[UserDB] Database initialized, %d users\r\n", s_db_header.user_count);
	return USER_DB_OK;
}

user_db_result_t user_db_format(void)
{
	user_db_result_t ret;

	printf("[UserDB] Formatting database...\r\n");

	/* 初始化头部 */
	memset(&s_db_header, 0, sizeof(user_db_header_t));
	s_db_header.magic = USER_DB_MAGIC;
	s_db_header.version = 0x0100; /* v1.0 */
	s_db_header.user_count = 0;
	s_db_header.next_id = 1;

	/* 设置默认主密码 0000 */
	s_db_header.master_password[0] = 0;
	s_db_header.master_password[1] = 0;
	s_db_header.master_password[2] = 0;
	s_db_header.master_password[3] = 0;

	/* 写入头部 */
	ret = write_header();
	if (ret != USER_DB_OK)
	{
		return ret;
	}

	/* 擦除用户数据区（页对齐，覆盖所有用户槽位） */
	flash_unlock();
	for (uint32_t addr = USER_DB_DATA_ADDR; addr < USER_DB_DATA_ADDR + USER_DB_MAX_USERS * USER_ENTRY_SIZE; addr += FLASH_PAGE_SIZE)
	{
		flash_erase_page(addr);
	}
	flash_lock();

	s_initialized = 1;
	printf("[UserDB] Database formatted\r\n");
	return USER_DB_OK;
}

user_db_result_t user_db_add_user(user_entry_t *user)
{
	if (!s_initialized)
	{
		return USER_DB_ERR_INVALID;
	}

	if (user == NULL)
	{
		return USER_DB_ERR_INVALID;
	}

	/* 检查是否已满 */
	if (s_db_header.user_count >= USER_DB_MAX_USERS)
	{
		return USER_DB_ERR_FULL;
	}

	/* 查找空闲槽位 */
	int16_t slot = find_empty_slot();
	if (slot < 0)
	{
		return USER_DB_ERR_FULL;
	}

	/* 分配用户ID */
	user->id = s_db_header.next_id++;
	user->flags |= USER_FLAG_VALID;

	/* 写入用户 */
	user_db_result_t ret = write_user(slot, user);
	if (ret != USER_DB_OK)
	{
		return ret;
	}

	/* 更新头部 */
	s_db_header.user_count++;
	ret = write_header();

	printf("[UserDB] Added user: ID=%d, Name=%s\r\n", user->id, user->name);
	return ret;
}

user_db_result_t user_db_delete_user(uint16_t user_id)
{
	if (!s_initialized)
	{
		return USER_DB_ERR_INVALID;
	}

	int16_t slot = find_user_slot(user_id);
	if (slot < 0)
	{
		return USER_DB_ERR_NOT_FOUND;
	}

	/* 读取用户 */
	user_entry_t user;
	read_user(slot, &user);

	/* 清除有效标志 */
	user.flags &= ~USER_FLAG_VALID;

	/* 写回 */
	user_db_result_t ret = write_user(slot, &user);
	if (ret != USER_DB_OK)
	{
		return ret;
	}

	/* 更新头部 */
	s_db_header.user_count--;
	ret = write_header();

	printf("[UserDB] Deleted user: ID=%d\r\n", user_id);
	return ret;
}

user_db_result_t user_db_update_user(const user_entry_t *user)
{
	if (!s_initialized || user == NULL)
	{
		return USER_DB_ERR_INVALID;
	}

	int16_t slot = find_user_slot(user->id);
	if (slot < 0)
	{
		return USER_DB_ERR_NOT_FOUND;
	}

	return write_user(slot, user);
}

uint16_t user_db_get_count(void)
{
	if (!s_initialized)
	{
		return 0;
	}
	return s_db_header.user_count;
}

user_db_result_t user_db_find_by_id(uint16_t user_id, user_entry_t *out)
{
	if (!s_initialized || out == NULL)
	{
		return USER_DB_ERR_INVALID;
	}

	int16_t slot = find_user_slot(user_id);
	if (slot < 0)
	{
		return USER_DB_ERR_NOT_FOUND;
	}

	read_user(slot, out);

	if (out->flags & USER_FLAG_DISABLED)
	{
		return USER_DB_ERR_DISABLED;
	}

	return USER_DB_OK;
}

user_db_result_t user_db_find_by_rfid(const uint8_t uid[4], user_entry_t *out)
{
	if (!s_initialized || uid == NULL || out == NULL)
	{
		return USER_DB_ERR_INVALID;
	}

	user_entry_t user;
	for (uint16_t i = 0; i < USER_DB_MAX_USERS; i++)
	{
		read_user(i, &user);

		if (!is_slot_valid(&user))
		{
			continue;
		}

		if (!(user.flags & USER_FLAG_RFID_EN))
		{
			continue;
		}

		if (memcmp(user.rfid_uid, uid, 4) == 0)
		{
			*out = user;

			if (user.flags & USER_FLAG_DISABLED)
			{
				return USER_DB_ERR_DISABLED;
			}

			return USER_DB_OK;
		}
	}

	return USER_DB_ERR_NOT_FOUND;
}

user_db_result_t user_db_find_by_fingerprint(uint16_t fp_id, user_entry_t *out)
{
	if (!s_initialized || out == NULL)
	{
		return USER_DB_ERR_INVALID;
	}

	user_entry_t user;
	for (uint16_t i = 0; i < USER_DB_MAX_USERS; i++)
	{
		read_user(i, &user);

		if (!is_slot_valid(&user))
		{
			continue;
		}

		if (!(user.flags & USER_FLAG_FP_EN))
		{
			continue;
		}

		if (user.fingerprint_id == fp_id)
		{
			*out = user;

			if (user.flags & USER_FLAG_DISABLED)
			{
				return USER_DB_ERR_DISABLED;
			}

			return USER_DB_OK;
		}
	}

	return USER_DB_ERR_NOT_FOUND;
}

user_db_result_t user_db_get_by_index(uint16_t index, user_entry_t *out)
{
	if (!s_initialized || out == NULL)
	{
		return USER_DB_ERR_INVALID;
	}

	uint16_t valid_count = 0;
	user_entry_t user;

	for (uint16_t i = 0; i < USER_DB_MAX_USERS; i++)
	{
		read_user(i, &user);

		if (is_slot_valid(&user))
		{
			if (valid_count == index)
			{
				*out = user;
				return USER_DB_OK;
			}
			valid_count++;
		}
	}

	return USER_DB_ERR_NOT_FOUND;
}

user_db_result_t user_db_verify_password(const uint8_t password[USER_PASSWORD_LEN], user_entry_t *out)
{
	if (!s_initialized || password == NULL || out == NULL)
	{
		return USER_DB_ERR_INVALID;
	}

	user_entry_t user;
	for (uint16_t i = 0; i < USER_DB_MAX_USERS; i++)
	{
		read_user(i, &user);

		if (!is_slot_valid(&user))
		{
			continue;
		}

		if (!(user.flags & USER_FLAG_PWD_EN))
		{
			continue;
		}

		if (memcmp(user.password, password, USER_PASSWORD_LEN) == 0)
		{
			*out = user;

			if (user.flags & USER_FLAG_DISABLED)
			{
				return USER_DB_ERR_DISABLED;
			}

			return USER_DB_OK;
		}
	}

	return USER_DB_ERR_NOT_FOUND;
}

user_db_result_t user_db_verify_master_password(const uint8_t password[USER_PASSWORD_LEN])
{
	if (!s_initialized || password == NULL)
	{
		return USER_DB_ERR_INVALID;
	}

	if (memcmp(s_db_header.master_password, password, USER_PASSWORD_LEN) == 0)
	{
		return USER_DB_OK;
	}

	return USER_DB_ERR_PASSWORD;
}

user_db_result_t user_db_change_master_password(const uint8_t old_password[USER_PASSWORD_LEN],
												const uint8_t new_password[USER_PASSWORD_LEN])
{
	if (!s_initialized || old_password == NULL || new_password == NULL)
	{
		return USER_DB_ERR_INVALID;
	}

	/* 验证旧密码 */
	if (memcmp(s_db_header.master_password, old_password, USER_PASSWORD_LEN) != 0)
	{
		return USER_DB_ERR_PASSWORD;
	}

	/* 更新密码 */
	memcpy(s_db_header.master_password, new_password, USER_PASSWORD_LEN);

	/* 写入头部 */
	return write_header();
}

user_db_result_t user_db_set_rfid(uint16_t user_id, const uint8_t uid[4])
{
	if (!s_initialized || uid == NULL)
	{
		return USER_DB_ERR_INVALID;
	}

	/* 检查RFID是否已被其他用户使用 */
	user_entry_t temp;
	if (user_db_find_by_rfid(uid, &temp) == USER_DB_OK && temp.id != user_id)
	{
		return USER_DB_ERR_EXISTS;
	}

	int16_t slot = find_user_slot(user_id);
	if (slot < 0)
	{
		return USER_DB_ERR_NOT_FOUND;
	}

	user_entry_t user;
	read_user(slot, &user);

	memcpy(user.rfid_uid, uid, 4);
	user.flags |= USER_FLAG_RFID_EN;

	return write_user(slot, &user);
}

user_db_result_t user_db_set_fingerprint(uint16_t user_id, uint16_t fp_id)
{
	if (!s_initialized)
	{
		return USER_DB_ERR_INVALID;
	}

	/* 检查指纹ID是否已被其他用户使用 */
	user_entry_t temp;
	if (user_db_find_by_fingerprint(fp_id, &temp) == USER_DB_OK && temp.id != user_id)
	{
		return USER_DB_ERR_EXISTS;
	}

	int16_t slot = find_user_slot(user_id);
	if (slot < 0)
	{
		return USER_DB_ERR_NOT_FOUND;
	}

	user_entry_t user;
	read_user(slot, &user);

	user.fingerprint_id = fp_id;
	user.flags |= USER_FLAG_FP_EN;

	return write_user(slot, &user);
}

user_db_result_t user_db_set_password(uint16_t user_id, const uint8_t password[USER_PASSWORD_LEN])
{
	if (!s_initialized || password == NULL)
	{
		return USER_DB_ERR_INVALID;
	}

	int16_t slot = find_user_slot(user_id);
	if (slot < 0)
	{
		return USER_DB_ERR_NOT_FOUND;
	}

	user_entry_t user;
	read_user(slot, &user);

	memcpy(user.password, password, USER_PASSWORD_LEN);
	user.flags |= USER_FLAG_PWD_EN;

	return write_user(slot, &user);
}
