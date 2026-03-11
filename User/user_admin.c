/********************************** (C) COPYRIGHT *******************************
 * File Name          : user_admin.c
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-05
 * Description        : 用户管理模块实现
 *********************************************************************************/

#include "bsp_system.h"

/* 管理模式超时 */
#define ADMIN_TIMEOUT_MS 60000 /* 60秒无操作自动退出 */

#define FP_ENROLL_MAX_ATTEMPTS 3
#define FP_ENROLL_RETRY_DELAY_MS 800
#define FP_ENROLL_DONE_HOLD_MS 600

/* 内部状态 */
static struct
{
	admin_state_t state;
	add_user_step_t add_step;
	uint32_t last_activity;
	admin_callback_t callback;

	/* 添加用户临时数据 */
	user_entry_t new_user;
	uint8_t new_password[4];
	uint8_t password_set;

	/* 删除用户 */
	uint16_t selected_user_id;

	/* 修改主密码 */
	uint8_t old_master_pwd[4];
	uint8_t new_master_pwd[4];
	uint8_t master_pwd_step; /* 0=旧密码, 1=新密码, 2=确认 */
} s_admin;

typedef struct
{
	fingerprint_enroll_state_t state;
	uint16_t page_id;
	uint8_t attempt;
	uint8_t last_error;
	uint8_t retry_pending;
	uint32_t state_enter_time;
} fingerprint_enroll_ctx_t;

static fingerprint_enroll_ctx_t s_fp_enroll;

/* ================= 内部函数 ================= */

static void fingerprint_enroll_reset(void);
static void fingerprint_enroll_set_state(fingerprint_enroll_state_t state);
static void fingerprint_enroll_fail(uint8_t ensure);

static void reset_state(void)
{
	s_admin.state = ADMIN_STATE_LOCKED;
	s_admin.add_step = ADD_STEP_NAME;
	memset(&s_admin.new_user, 0, sizeof(user_entry_t));
	memset(s_admin.new_password, 0, 4);
	s_admin.password_set = 0;
	s_admin.selected_user_id = 0;
	s_admin.master_pwd_step = 0;
	fingerprint_enroll_reset();
}

static void update_activity(void)
{
	s_admin.last_activity = TIM_Get_MillisCounter();
}

static void fingerprint_enroll_reset(void)
{
	s_fp_enroll.state = FP_ENROLL_IDLE;
	s_fp_enroll.page_id = 0;
	s_fp_enroll.attempt = 0;
	s_fp_enroll.last_error = 0;
	s_fp_enroll.retry_pending = 0;
	s_fp_enroll.state_enter_time = 0;
}

static void fingerprint_enroll_set_state(fingerprint_enroll_state_t state)
{
	if (s_fp_enroll.state == state)
	{
		return;
	}

	s_fp_enroll.state = state;
	s_fp_enroll.state_enter_time = TIM_Get_MillisCounter();
	update_activity();
	door_status_ui_on_admin_state_change();
}

static void fingerprint_enroll_fail(uint8_t ensure)
{
	s_fp_enroll.last_error = ensure;
	printf("[Admin] Fingerprint enroll failed (err=%d)\r\n", ensure);

	if (s_fp_enroll.attempt < FP_ENROLL_MAX_ATTEMPTS)
	{
		s_fp_enroll.attempt++;
		s_fp_enroll.retry_pending = 1;
		printf("[Admin] Fingerprint enroll retry %d/%d\r\n",
			   s_fp_enroll.attempt, FP_ENROLL_MAX_ATTEMPTS);
	}
	else
	{
		s_fp_enroll.retry_pending = 0;
		printf("[Admin] Fingerprint enroll stopped after %d attempts\r\n",
			   FP_ENROLL_MAX_ATTEMPTS);
	}

	fingerprint_enroll_set_state(FP_ENROLL_ERROR);
}

/**
 * @brief Start fingerprint enroll state machine
 * @note  Non-blocking; progress handled in fingerprint_enroll_update()
 */
static void start_fingerprint_enroll(void)
{
	/* 使用用户数据库当前数量作为指纹模板ID */
	uint16_t fp_id = user_db_get_count() + 1;

	printf("[Admin] Starting fingerprint enrollment, PageID=%d\r\n", fp_id);
	printf("[Admin] Place your finger on the sensor...\r\n");

	fingerprint_enroll_reset();
	s_fp_enroll.page_id = fp_id;
	s_fp_enroll.attempt = 1;
	fingerprint_enroll_set_state(FP_ENROLL_WAIT_FINGER_1);

}

/**
 * @brief 完成添加用户
 */
static void finish_add_user(void)
{
	/* 设置密码（如果有） */
	if (s_admin.password_set)
	{
		memcpy(s_admin.new_user.password, s_admin.new_password, 4);
		s_admin.new_user.flags |= USER_FLAG_PWD_EN;
	}

	/* 添加到数据库 */
	user_db_result_t ret = user_db_add_user(&s_admin.new_user);

	if (ret == USER_DB_OK)
	{
		printf("[Admin] User added: ID=%d, Name=%s\r\n",
			   s_admin.new_user.id, s_admin.new_user.name);

		if (s_admin.callback)
		{
			s_admin.callback(ADMIN_STATE_ADD_USER, 1);
		}
	}
	else
	{
		printf("[Admin] Failed to add user: %d\r\n", ret);

		if (s_admin.callback)
		{
			s_admin.callback(ADMIN_STATE_ADD_USER, 0);
		}
	}

	s_admin.add_step = ADD_STEP_DONE;
	s_admin.state = ADMIN_STATE_UNLOCKED;
	door_status_ui_on_admin_state_change();
}

/**
 * @brief 密码输入完成回调（用于添加用户流程）
 */
static void add_user_password_callback(const uint8_t *password, uint8_t length, pwd_input_state_t state)
{
	if (state == PWD_STATE_COMPLETE && length == PWD_INPUT_LENGTH)
	{
		memcpy(s_admin.new_password, password, 4);
		s_admin.password_set = 1;
		printf("[Admin] Password set for new user\r\n");

		s_admin.add_step = ADD_STEP_CONFIRM;
		update_activity();
		door_status_ui_on_admin_state_change();
	}
	else if (state == PWD_STATE_TIMEOUT || state == PWD_STATE_CANCELLED)
	{
		/* 跳过密码设置 */
		s_admin.password_set = 0;
		s_admin.add_step = ADD_STEP_CONFIRM;
		door_status_ui_on_admin_state_change();
	}
}

/**
 * @brief 主密码修改回调
 */
static void master_pwd_callback(const uint8_t *password, uint8_t length, pwd_input_state_t state)
{
	if (state != PWD_STATE_COMPLETE || length != PWD_INPUT_LENGTH)
	{
		s_admin.state = ADMIN_STATE_UNLOCKED;
		door_status_ui_on_admin_state_change();
		return;
	}

	switch (s_admin.master_pwd_step)
	{
	case 0: /* 旧密码 */
		memcpy(s_admin.old_master_pwd, password, 4);
		s_admin.master_pwd_step = 1;
		printf("[Admin] Enter new master password\r\n");
		door_status_ui_on_admin_state_change();
		pwd_input_start(master_pwd_callback);
		break;

	case 1: /* 新密码 */
		memcpy(s_admin.new_master_pwd, password, 4);
		s_admin.master_pwd_step = 2;
		printf("[Admin] Confirm new master password\r\n");
		door_status_ui_on_admin_state_change();
		pwd_input_start(master_pwd_callback);
		break;

	case 2: /* 确认新密码 */
		if (memcmp(s_admin.new_master_pwd, password, 4) == 0)
		{
			/* 尝试修改 */
			user_db_result_t ret = user_db_change_master_password(
				s_admin.old_master_pwd, s_admin.new_master_pwd);

			if (ret == USER_DB_OK)
			{
				printf("[Admin] Master password changed\r\n");
				if (s_admin.callback)
				{
					s_admin.callback(ADMIN_STATE_CHANGE_MASTER_PWD, 1);
				}
			}
			else
			{
				printf("[Admin] Failed to change master password: %d\r\n", ret);
				if (s_admin.callback)
				{
					s_admin.callback(ADMIN_STATE_CHANGE_MASTER_PWD, 0);
				}
			}
		}
		else
		{
			printf("[Admin] Password confirmation mismatch\r\n");
			if (s_admin.callback)
			{
				s_admin.callback(ADMIN_STATE_CHANGE_MASTER_PWD, 0);
			}
		}
		s_admin.state = ADMIN_STATE_UNLOCKED;
		door_status_ui_on_admin_state_change();
		break;
	}

	update_activity();
}

/* ================= 公开API实现 ================= */

void user_admin_init(void)
{
	reset_state();
	printf("[Admin] Initialized\r\n");
}

void user_admin_set_callback(admin_callback_t callback)
{
	s_admin.callback = callback;
}

uint8_t user_admin_enter(const uint8_t password[4])
{
	if (password == NULL)
	{
		return 0;
	}

	if (user_db_verify_master_password(password) == USER_DB_OK)
	{
		s_admin.state = ADMIN_STATE_UNLOCKED;
		update_activity();
		printf("[Admin] Admin mode entered\r\n");
		printf("[Admin] ---- Admin Menu ----\r\n");
		printf("[Admin] 1=Add User  2=Delete User\r\n");
		printf("[Admin] 3=List Users  4=Change Master Pwd\r\n");
		printf("[Admin] 15=Exit\r\n");
		printf("[Admin] -----------------------\r\n");
		return 1;
	}

	printf("[Admin] Invalid master password\r\n");
	return 0;
}

void user_admin_exit(void)
{
	reset_state();
	printf("[Admin] Admin mode exited\r\n");
	door_status_ui_on_admin_exit();
}

admin_state_t user_admin_get_state(void)
{
	return s_admin.state;
}

uint8_t user_admin_is_active(void)
{
	return (s_admin.state != ADMIN_STATE_LOCKED) ? 1 : 0;
}

void user_admin_start_add_user(void)
{
	if (s_admin.state != ADMIN_STATE_UNLOCKED)
	{
		return;
	}

	s_admin.state = ADMIN_STATE_ADD_USER;
	s_admin.add_step = ADD_STEP_NAME;
	memset(&s_admin.new_user, 0, sizeof(user_entry_t));
	s_admin.password_set = 0;
	fingerprint_enroll_reset();

	/* 设置默认用户名 */
	snprintf(s_admin.new_user.name, USER_NAME_LEN, "User%d", user_db_get_count() + 1);

	update_activity();
	printf("[Admin] Start adding user, step=NAME\r\n");
	door_status_ui_on_admin_state_change();
}

void user_admin_start_delete_user(void)
{
	if (s_admin.state != ADMIN_STATE_UNLOCKED)
	{
		return;
	}

	s_admin.state = ADMIN_STATE_DELETE_USER;
	s_admin.selected_user_id = 0;

	update_activity();
	printf("[Admin] Start delete user flow\r\n");
	door_status_ui_on_admin_state_change();
}

void user_admin_start_list_users(void)
{
	if (s_admin.state != ADMIN_STATE_UNLOCKED)
	{
		return;
	}

	s_admin.state = ADMIN_STATE_LIST_USERS;
	update_activity();

	uint16_t count = user_db_get_count();
	printf("[Admin] ---- User List (%d users) ----\r\n", count);

	user_entry_t user;
	for (uint16_t i = 0; i < count; i++)
	{
		if (user_db_get_by_index(i, &user) == USER_DB_OK)
		{
			printf("[Admin] [%d] ID=%d Name=%-12s", i + 1, user.id, user.name);
			if (user.flags & USER_FLAG_RFID_EN)
			{
				printf(" RFID:%02X%02X%02X%02X",
					   user.rfid_uid[0], user.rfid_uid[1],
					   user.rfid_uid[2], user.rfid_uid[3]);
			}
			if (user.flags & USER_FLAG_FP_EN)
			{
				printf(" FP:%d", user.fingerprint_id);
			}
			if (user.flags & USER_FLAG_PWD_EN)
			{
				printf(" PWD:Yes");
			}
			if (user.flags & USER_FLAG_DISABLED)
			{
				printf(" [DISABLED]");
			}
			printf("\r\n");
		}
	}
	printf("[Admin] ---- Press any key to return ----\r\n");
	door_status_ui_on_admin_state_change();
}

void user_admin_start_change_master_pwd(void)
{
	if (s_admin.state != ADMIN_STATE_UNLOCKED)
	{
		return;
	}

	s_admin.state = ADMIN_STATE_CHANGE_MASTER_PWD;
	s_admin.master_pwd_step = 0;

	printf("[Admin] Enter old master password\r\n");
	pwd_input_start(master_pwd_callback);

	update_activity();
	door_status_ui_on_admin_state_change();
}

void user_admin_on_key(uint8_t key_id)
{
	if (s_admin.state == ADMIN_STATE_LOCKED)
	{
		return;
	}

	update_activity();

	/* 密码输入模式下传递给密码模块 */
	if (pwd_input_is_active())
	{
		pwd_input_on_key(key_id);
		return;
	}

	/* 根据状态处理 */
	switch (s_admin.state)
	{
	case ADMIN_STATE_UNLOCKED:
		/*
		 * 管理菜单按键映射:
		 *   键1 = 添加用户
		 *   键2 = 删除用户
		 *   键3 = 查看用户列表
		 *   键4 = 修改主密码
		 *   键15/键12 = 退出管理模式
		 */
		switch (key_id)
		{
		case 1:
			printf("[Admin] >> Add user\r\n");
			user_admin_start_add_user();
			break;
		case 2:
			printf("[Admin] >> Delete user\r\n");
			user_admin_start_delete_user();
			break;
		case 3:
			printf("[Admin] >> List users\r\n");
			user_admin_start_list_users();
			break;
		case 4:
			printf("[Admin] >> Change master password\r\n");
			user_admin_start_change_master_pwd();
			break;
		case 12:
		case 15:
			printf("[Admin] >> Exit admin mode\r\n");
			user_admin_exit();
			break;
		default:
			printf("[Admin] Menu: 1=Add 2=Del 3=List 4=ChgPwd 15=Exit\r\n");
			break;
		}
		break;

	case ADMIN_STATE_ADD_USER:
		/* 确认键：推进添加用户步骤 */
		if (key_id == 16)
		{
			user_admin_confirm();
		}
		/* 取消键：取消当前操作 */
		else if (key_id == 15 || key_id == 12)
		{
			user_admin_cancel();
		}
		/* 密码输入步骤：将数字键传递给密码模块 */
		else if (s_admin.add_step == ADD_STEP_PASSWORD)
		{
			if (!pwd_input_is_active())
			{
				pwd_input_start(add_user_password_callback);
			}
			pwd_input_on_key(key_id);
		}
		break;

	case ADMIN_STATE_DELETE_USER:
		/* 确认键：执行删除 */
		if (key_id == 16)
		{
			user_admin_confirm();
		}
		/* 取消键：取消删除操作 */
		else if (key_id == 15 || key_id == 12)
		{
			user_admin_cancel();
		}
		/* 数字键选择用户序号 */
		else if (key_id >= 1 && key_id <= 9)
		{
			user_entry_t user;
			uint16_t index = key_id - 1;
			if (user_db_get_by_index(index, &user) == USER_DB_OK)
			{
				s_admin.selected_user_id = user.id;
				printf("[Admin] Selected user: ID=%d, Name=%s\r\n",
					   user.id, user.name);
				printf("[Admin] Press 16(confirm) to delete, 15(cancel)\r\n");
				door_status_ui_on_admin_state_change();
			}
			else
			{
				printf("[Admin] No user at index %d\r\n", index);
			}
		}
		break;

	case ADMIN_STATE_LIST_USERS:
		/* 任意键返回菜单 */
		s_admin.state = ADMIN_STATE_UNLOCKED;
		door_status_ui_on_admin_state_change();
		break;

	default:
		break;
	}
}

void user_admin_on_rfid(const uint8_t uid[4])
{
	if (s_admin.state != ADMIN_STATE_ADD_USER || uid == NULL)
	{
		return;
	}

	if (s_admin.add_step == ADD_STEP_RFID)
	{
		/* 检查RFID是否已被使用 */
		user_entry_t temp;
		if (user_db_find_by_rfid(uid, &temp) == USER_DB_OK)
		{
			printf("[Admin] RFID already registered to user %d\r\n", temp.id);
			return;
		}

		memcpy(s_admin.new_user.rfid_uid, uid, 4);
		s_admin.new_user.flags |= USER_FLAG_RFID_EN;

		printf("[Admin] RFID recorded: %02X %02X %02X %02X\r\n",
			   uid[0], uid[1], uid[2], uid[3]);

		s_admin.add_step = ADD_STEP_FINGERPRINT;
		update_activity();

		/* RFID成功后自动启动指纹录入 */
		door_status_ui_on_admin_state_change();
		start_fingerprint_enroll();
	}
}

void user_admin_on_fingerprint(uint16_t fp_id)
{
	if (s_admin.state != ADMIN_STATE_ADD_USER)
	{
		return;
	}

	if (s_admin.add_step == ADD_STEP_FINGERPRINT)
	{
		fingerprint_enroll_reset();

		/* 检查指纹是否已被使用 */
		user_entry_t temp;
		if (user_db_find_by_fingerprint(fp_id, &temp) == USER_DB_OK)
		{
			printf("[Admin] Fingerprint already registered to user %d\r\n", temp.id);
			return;
		}

		s_admin.new_user.fingerprint_id = fp_id;
		s_admin.new_user.flags |= USER_FLAG_FP_EN;

		printf("[Admin] Fingerprint recorded: ID=%d\r\n", fp_id);

		s_admin.add_step = ADD_STEP_PASSWORD;
		update_activity();
		door_status_ui_on_admin_state_change();
	}
}

void user_admin_cancel(void)
{
	if (s_admin.state == ADMIN_STATE_LOCKED)
	{
		return;
	}

	if (pwd_input_is_active())
	{
		pwd_input_cancel();
	}

	fingerprint_enroll_reset();
	s_admin.state = ADMIN_STATE_UNLOCKED;
	printf("[Admin] Operation cancelled\r\n");
	door_status_ui_on_admin_state_change();
}

void user_admin_confirm(void)
{
	update_activity(); // 更新时间

	switch (s_admin.state)
	{
	case ADMIN_STATE_ADD_USER:
		if (s_admin.add_step == ADD_STEP_CONFIRM)
		{
			finish_add_user();
		}
		else if (s_admin.add_step == ADD_STEP_NAME)
		{
			/* 跳到下一步 */
			s_admin.add_step = ADD_STEP_RFID;
			printf("[Admin] Name confirmed, step=RFID\r\n");
			door_status_ui_on_admin_state_change();
		}
		else if (s_admin.add_step == ADD_STEP_RFID)
		{
			/* 跳过RFID，进入指纹步骤并自动启动录入 */
			s_admin.add_step = ADD_STEP_FINGERPRINT;
			printf("[Admin] Skipping RFID, step=FINGERPRINT\r\n");
			door_status_ui_on_admin_state_change();
			start_fingerprint_enroll();
		}
		else if (s_admin.add_step == ADD_STEP_FINGERPRINT)
		{
			/* 跳过指纹 */
			fingerprint_enroll_reset();
			s_admin.add_step = ADD_STEP_PASSWORD;
			printf("[Admin] Skipping fingerprint, step=PASSWORD\r\n");
			door_status_ui_on_admin_state_change();
			pwd_input_start(add_user_password_callback);
		}
		break;

	case ADMIN_STATE_DELETE_USER:
		if (s_admin.selected_user_id > 0)
		{
			user_db_result_t ret = user_db_delete_user(s_admin.selected_user_id);
			if (ret == USER_DB_OK)
			{
				printf("[Admin] User %d deleted\r\n", s_admin.selected_user_id);
				if (s_admin.callback)
				{
					s_admin.callback(ADMIN_STATE_DELETE_USER, 1);
				}
			}
			else
			{
				printf("[Admin] Failed to delete user: %d\r\n", ret);
				if (s_admin.callback)
				{
					s_admin.callback(ADMIN_STATE_DELETE_USER, 0);
				}
			}
			s_admin.state = ADMIN_STATE_UNLOCKED;
			door_status_ui_on_admin_state_change();
		}
		break;

	case ADMIN_STATE_LIST_USERS:
		s_admin.state = ADMIN_STATE_UNLOCKED;
		door_status_ui_on_admin_state_change();
		break;

	default:
		break;
	}
}

add_user_step_t user_admin_get_add_step(void)
{
	return s_admin.add_step;
}

fingerprint_enroll_state_t user_admin_get_fingerprint_enroll_state(void)
{
	return s_fp_enroll.state;
}

const char *user_admin_get_prompt(void)
{
	switch (s_admin.state)
	{
	case ADMIN_STATE_LOCKED:
		return "Enter master password";

	case ADMIN_STATE_UNLOCKED:
		return "Select operation";

	case ADMIN_STATE_ADD_USER:
		switch (s_admin.add_step)
		{
		case ADD_STEP_NAME:
			return "Enter user name";
		case ADD_STEP_RFID:
			return "Swipe RFID card";
		case ADD_STEP_FINGERPRINT:
			return "Press finger";
		case ADD_STEP_PASSWORD:
			return "Enter password";
		case ADD_STEP_CONFIRM:
			return "Confirm to add?";
		case ADD_STEP_DONE:
			return "User added!";
		default:
			return "";
		}

	case ADMIN_STATE_DELETE_USER:
		return "Select user to delete";

	case ADMIN_STATE_LIST_USERS:
		return "User list";

	case ADMIN_STATE_CHANGE_MASTER_PWD:
		switch (s_admin.master_pwd_step)
		{
		case 0:
			return "Enter old password";
		case 1:
			return "Enter new password";
		case 2:
			return "Confirm password";
		default:
			return "";
		}

	default:
		return "";
	}
}

uint8_t user_admin_get_user_at(uint16_t index, user_entry_t *out)
{
	if (out == NULL)
	{
		return 0;
	}

	if (user_db_get_by_index(index, out) == USER_DB_OK)
	{
		return 1;
	}

	return 0;
}

uint16_t user_admin_get_selected_user_id(void)
{
	return s_admin.selected_user_id;
}

void user_admin_set_selected_user_id(uint16_t user_id)
{
	s_admin.selected_user_id = user_id;
	update_activity();
}

void fingerprint_enroll_update(void)
{
	uint32_t now;
	uint8_t ensure;

	if (s_admin.state != ADMIN_STATE_ADD_USER || s_admin.add_step != ADD_STEP_FINGERPRINT)
	{
		if (s_fp_enroll.state != FP_ENROLL_IDLE)
		{
			fingerprint_enroll_reset();
		}
		return;
	}

	if (s_fp_enroll.state == FP_ENROLL_IDLE)
	{
		return;
	}

	now = TIM_Get_MillisCounter();

	switch (s_fp_enroll.state)
	{
	case FP_ENROLL_WAIT_FINGER_1:
		ensure = PS_GetImage();
		if (ensure == 0x00)
		{
			fingerprint_enroll_set_state(FP_ENROLL_CAPTURE_1);
		}
		else if (ensure != 0x02)
		{
			fingerprint_enroll_fail(ensure);
		}
		break;

	case FP_ENROLL_CAPTURE_1:
		ensure = PS_GenChar(CharBuffer1);
		if (ensure == 0x00)
		{
			fingerprint_enroll_set_state(FP_ENROLL_REMOVE);
		}
		else
		{
			fingerprint_enroll_fail(ensure);
		}
		break;

	case FP_ENROLL_REMOVE:
		ensure = PS_GetImage();
		if (ensure == 0x02)
		{
			fingerprint_enroll_set_state(FP_ENROLL_WAIT_FINGER_2);
		}
		else if (ensure != 0x00)
		{
			fingerprint_enroll_fail(ensure);
		}
		break;

	case FP_ENROLL_WAIT_FINGER_2:
		ensure = PS_GetImage();
		if (ensure == 0x00)
		{
			fingerprint_enroll_set_state(FP_ENROLL_CAPTURE_2);
		}
		else if (ensure != 0x02)
		{
			fingerprint_enroll_fail(ensure);
		}
		break;

	case FP_ENROLL_CAPTURE_2:
		ensure = PS_GenChar(CharBuffer2);
		if (ensure == 0x00)
		{
			fingerprint_enroll_set_state(FP_ENROLL_MATCH);
		}
		else
		{
			fingerprint_enroll_fail(ensure);
		}
		break;

	case FP_ENROLL_MATCH:
		ensure = PS_Match();
		if (ensure == 0x00)
		{
			fingerprint_enroll_set_state(FP_ENROLL_STORE);
		}
		else
		{
			fingerprint_enroll_fail(ensure);
		}
		break;

	case FP_ENROLL_STORE:
		ensure = PS_RegModel();
		if (ensure == 0x00)
		{
			ensure = PS_StoreChar(CharBuffer2, s_fp_enroll.page_id);
		}
		if (ensure == 0x00)
		{
			printf("[Admin] Fingerprint enrolled successfully! ID=%d\r\n", s_fp_enroll.page_id);
			fingerprint_enroll_set_state(FP_ENROLL_DONE);
		}
		else
		{
			fingerprint_enroll_fail(ensure);
		}
		break;

	case FP_ENROLL_DONE:
		if (now - s_fp_enroll.state_enter_time >= FP_ENROLL_DONE_HOLD_MS)
		{
			uint16_t page_id = s_fp_enroll.page_id;
			fingerprint_enroll_reset();
			user_admin_on_fingerprint(page_id);
		}
		break;

	case FP_ENROLL_ERROR:
		if (s_fp_enroll.retry_pending &&
			(now - s_fp_enroll.state_enter_time >= FP_ENROLL_RETRY_DELAY_MS))
		{
			s_fp_enroll.retry_pending = 0;
			fingerprint_enroll_set_state(FP_ENROLL_WAIT_FINGER_1);
		}
		break;

	default:
		break;
	}
}

void user_admin_update(void)
{
	if (s_admin.state == ADMIN_STATE_LOCKED)
	{
		return;
	}

	/* 检查超时 */
	uint32_t elapsed = TIM_Get_MillisCounter() - s_admin.last_activity;
	if (elapsed >= ADMIN_TIMEOUT_MS)
	{
		printf("[Admin] Timeout, exiting admin mode\r\n");
		user_admin_exit();
	}
}
