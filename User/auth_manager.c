/********************************** (C) COPYRIGHT *******************************
 * File Name          : auth_manager.c
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-05
 * Description        : 认证管理器实现
 *********************************************************************************/

#include "bsp_system.h"

/* 内部状态 */
static struct
{
	auth_mgr_state_t state; // 认证状态
	uint8_t fail_count;
	uint32_t lockout_start;
	auth_result_callback_t callback;
	auth_start_callback_t start_callback;
	uint8_t last_rfid_uid[4]; /* 用于日志记录 */
} s_auth_mgr;

/* ================= 内部函数 ================= */

/**
 * @brief 认证成功处理
 */
static void auth_success(auth_method_t method, const user_entry_t *user, const uint8_t *uid)
{
	printf("[AuthMgr] Auth SUCCESS: method=%d, user_id=%d, name=%s\r\n",
		   method, user ? user->id : 0, user ? user->name : "Unknown");

	/* 记录日志 */
	access_log_record(user ? user->id : 0, method, AUTH_RESULT_SUCCESS, uid);

	/* 重置失败计数 */
	s_auth_mgr.fail_count = 0;

	/* 开门 */
	door_control_unlock(method, AUTH_DOOR_OPEN_DURATION_MS);

	/* 播放成功语音 */
	BY8301_PlayIndex(1); /* 假设1=认证成功 */

	/* 回调通知 */
	if (s_auth_mgr.callback)
	{
		s_auth_mgr.callback(method, AUTH_RESULT_SUCCESS, user);
	}

	s_auth_mgr.state = AUTH_MGR_IDLE;
}

/**
 * @brief 认证失败处理
 */
static void auth_failed(auth_method_t method, auth_result_t result, const uint8_t *uid)
{
	printf("[AuthMgr] Auth FAILED: method=%d, result=%d\r\n", method, result);

	/* 记录日志 */
	access_log_record(0, method, result, uid);

	/* 增加失败计数 */
	s_auth_mgr.fail_count++;

	/* 检查是否需要锁定 */
	if (s_auth_mgr.fail_count >= AUTH_MAX_FAIL_COUNT)
	{
		s_auth_mgr.state = AUTH_MGR_LOCKED;
		s_auth_mgr.lockout_start = TIM_Get_MillisCounter();
		printf("[AuthMgr] LOCKED! Too many failures\r\n");

		/* 播放警告语音 */
		BY8301_PlayIndex(3); /* 假设3=系统锁定 */
	}
	else
	{
		/* 播放失败语音 */
		BY8301_PlayIndex(2); /* 假设2=认证失败 */
	}

	/* 回调通知 */
	if (s_auth_mgr.callback)
	{
		s_auth_mgr.callback(method, result, NULL);
	}

	if (s_auth_mgr.state != AUTH_MGR_LOCKED)
	{
		s_auth_mgr.state = AUTH_MGR_IDLE;
	}
}

/**
 * @brief 密码输入完成回调
 */
static void password_complete_callback(const uint8_t *password, uint8_t length, pwd_input_state_t state)
{
	if (state == PWD_STATE_COMPLETE && length == PWD_INPUT_LENGTH)
	{
		auth_process_password(password);
	}
	else if (state == PWD_STATE_TIMEOUT)
	{
		auth_failed(AUTH_PASSWORD, AUTH_RESULT_TIMEOUT, NULL);
	}
	else if (state == PWD_STATE_CANCELLED)
	{
		s_auth_mgr.state = AUTH_MGR_IDLE;
	}
}

/* ================= 公开API实现 ================= */

void auth_manager_init(void)
{
	memset(&s_auth_mgr, 0, sizeof(s_auth_mgr));
	s_auth_mgr.state = AUTH_MGR_IDLE;

	/* 初始化密码输入模块 */
	pwd_input_init();

	printf("[AuthMgr] Initialized\r\n");
}

void auth_manager_set_callback(auth_result_callback_t callback)
{
	s_auth_mgr.callback = callback;
}

void auth_manager_set_start_callback(auth_start_callback_t callback)
{
	s_auth_mgr.start_callback = callback;
}

auth_result_t auth_process_rfid(const RFID_Frame *frame)
{
	if (frame == NULL)
	{
		return AUTH_RESULT_FAILED;
	}

	/* 检查锁定状态 */
	if (s_auth_mgr.state == AUTH_MGR_LOCKED)
	{
		printf("[AuthMgr] System locked, rejecting RFID\r\n");
		return AUTH_RESULT_FAILED;
	}

	/* 通知UI认证开始 */
	if (s_auth_mgr.start_callback)
	{
		s_auth_mgr.start_callback(AUTH_RFID);
	}

	/* 保存UID用于日志 */
	memcpy(s_auth_mgr.last_rfid_uid, frame->uid, 4);

	s_auth_mgr.state = AUTH_MGR_PROCESSING;

	printf("[AuthMgr] Processing RFID: %02X %02X %02X %02X\r\n",
		   frame->uid[0], frame->uid[1], frame->uid[2], frame->uid[3]);

	/* 查找用户 */
	user_entry_t user;
	user_db_result_t db_ret = user_db_find_by_rfid(frame->uid, &user);

	if (db_ret == USER_DB_OK)
	{
		auth_success(AUTH_RFID, &user, frame->uid);
		return AUTH_RESULT_SUCCESS;
	}
	else if (db_ret == USER_DB_ERR_DISABLED)
	{
		auth_failed(AUTH_RFID, AUTH_RESULT_USER_DISABLED, frame->uid);
		return AUTH_RESULT_USER_DISABLED;
	}
	else
	{
		auth_failed(AUTH_RFID, AUTH_RESULT_USER_NOT_FOUND, frame->uid);
		return AUTH_RESULT_USER_NOT_FOUND;
	}
}

auth_result_t auth_process_fingerprint(const SearchResult *result)
{
	if (result == NULL)
	{
		return AUTH_RESULT_FAILED;
	}

	/* 检查锁定状态 */
	if (s_auth_mgr.state == AUTH_MGR_LOCKED)
	{
		printf("[AuthMgr] System locked, rejecting fingerprint\r\n");
		return AUTH_RESULT_FAILED;
	}

	/* 通知UI认证开始 */
	if (s_auth_mgr.start_callback)
	{
		s_auth_mgr.start_callback(AUTH_FINGERPRINT);
	}

	s_auth_mgr.state = AUTH_MGR_PROCESSING;

	printf("[AuthMgr] Processing fingerprint: ID=%d, Score=%d\r\n",
		   result->pageID, result->mathscore);

	/* 查找用户 */
	user_entry_t user;
	user_db_result_t db_ret = user_db_find_by_fingerprint(result->pageID, &user);

	if (db_ret == USER_DB_OK)
	{
		auth_success(AUTH_FINGERPRINT, &user, NULL);
		return AUTH_RESULT_SUCCESS;
	}
	else if (db_ret == USER_DB_ERR_DISABLED)
	{
		auth_failed(AUTH_FINGERPRINT, AUTH_RESULT_USER_DISABLED, NULL);
		return AUTH_RESULT_USER_DISABLED;
	}
	else
	{
		auth_failed(AUTH_FINGERPRINT, AUTH_RESULT_USER_NOT_FOUND, NULL);
		return AUTH_RESULT_USER_NOT_FOUND;
	}
}

void auth_start_password(void)
{
	/* 检查锁定状态 */
	if (s_auth_mgr.state == AUTH_MGR_LOCKED)
	{
		printf("[AuthMgr] System locked, cannot start password input\r\n");
		return;
	}

	/* 通知UI认证开始 */
	if (s_auth_mgr.start_callback)
	{
		s_auth_mgr.start_callback(AUTH_PASSWORD);
	}

	s_auth_mgr.state = AUTH_MGR_PASSWORD_INPUT;
	pwd_input_start(password_complete_callback); // 输入完成或者没有完成的时候执行回调
	printf("[AuthMgr] Started password input mode\r\n");
}

auth_result_t auth_process_password(const uint8_t password[4])
{
	if (password == NULL)
	{
		return AUTH_RESULT_FAILED;
	}

	/* 检查锁定状态 */
	if (s_auth_mgr.state == AUTH_MGR_LOCKED)
	{
		printf("[AuthMgr] System locked, rejecting password\r\n");
		return AUTH_RESULT_FAILED;
	}

	s_auth_mgr.state = AUTH_MGR_PROCESSING;

	printf("[AuthMgr] Processing password: %d%d%d%d\r\n",
		   password[0], password[1], password[2], password[3]);

	/* 先检查是否是主密码（用于管理模式） */
	if (user_db_verify_master_password(password) == USER_DB_OK)
	{
		printf("[AuthMgr] Master password verified, entering admin mode\r\n");
		/* 主密码不开门，进入管理模式 */
		user_admin_enter(password);
		/* 切换 UI 到管理模式界面 */
		door_status_ui_on_admin_enter();
		s_auth_mgr.state = AUTH_MGR_IDLE;
		return AUTH_RESULT_SUCCESS;
	}

	/* 查找用户密码 */
	user_entry_t user;
	user_db_result_t db_ret = user_db_verify_password(password, &user);

	if (db_ret == USER_DB_OK)
	{
		auth_success(AUTH_PASSWORD, &user, NULL);
		return AUTH_RESULT_SUCCESS;
	}
	else if (db_ret == USER_DB_ERR_DISABLED)
	{
		auth_failed(AUTH_PASSWORD, AUTH_RESULT_USER_DISABLED, NULL);
		return AUTH_RESULT_USER_DISABLED;
	}
	else
	{
		auth_failed(AUTH_PASSWORD, AUTH_RESULT_USER_NOT_FOUND, NULL);
		return AUTH_RESULT_USER_NOT_FOUND;
	}
}

auth_mgr_state_t auth_manager_get_state(void)
{
	return s_auth_mgr.state;
}

uint8_t auth_manager_get_fail_count(void)
{
	return s_auth_mgr.fail_count;
}

void auth_manager_reset_fail_count(void)
{
	s_auth_mgr.fail_count = 0;
	if (s_auth_mgr.state == AUTH_MGR_LOCKED)
	{
		s_auth_mgr.state = AUTH_MGR_IDLE;
	}
}

uint8_t auth_manager_is_locked(void)
{
	return (s_auth_mgr.state == AUTH_MGR_LOCKED) ? 1 : 0;
}

uint32_t auth_manager_get_lockout_remaining(void)
{
	if (s_auth_mgr.state != AUTH_MGR_LOCKED)
	{
		return 0;
	}

	uint32_t elapsed = TIM_Get_MillisCounter() - s_auth_mgr.lockout_start;
	if (elapsed >= AUTH_LOCKOUT_TIME_MS)
	{
		return 0;
	}

	return AUTH_LOCKOUT_TIME_MS - elapsed;
}

void auth_manager_update(void)
{
	/* 更新密码输入模块 */
	pwd_input_update();

	/* 检查锁定超时 */
	if (s_auth_mgr.state == AUTH_MGR_LOCKED)
	{
		uint32_t elapsed = TIM_Get_MillisCounter() - s_auth_mgr.lockout_start;
		if (elapsed >= AUTH_LOCKOUT_TIME_MS)
		{
			printf("[AuthMgr] Lockout expired, resetting\r\n");
			s_auth_mgr.state = AUTH_MGR_IDLE;
			s_auth_mgr.fail_count = 0;
		}
	}
}
