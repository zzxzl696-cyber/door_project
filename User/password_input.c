/********************************** (C) COPYRIGHT *******************************
 * File Name          : password_input.c
 * Author             : Door Access System
 * Version            : V1.0.0
 * Date               : 2026-02-05
 * Description        : 密码输入模块实现
 *********************************************************************************/

#include "password_input.h"
#include "timer_config.h"
#include "debug.h"
#include <string.h>

/* 内部状态 */
static struct
{
	pwd_input_state_t state;
	uint8_t password[PWD_INPUT_LENGTH];
	uint8_t input_count;
	uint32_t start_time;
	pwd_input_callback_t callback;
	pwd_ui_update_callback_t ui_callback;
} s_pwd_input;

/* ================= 公开API实现 ================= */

void pwd_input_init(void)
{
	memset(&s_pwd_input, 0, sizeof(s_pwd_input));
	s_pwd_input.state = PWD_STATE_IDLE;
	printf("[PwdInput] Initialized\r\n");
}

void pwd_input_set_ui_callback(pwd_ui_update_callback_t callback)
{
	s_pwd_input.ui_callback = callback;
}

void pwd_input_start(pwd_input_callback_t callback)
{
	memset(s_pwd_input.password, 0, PWD_INPUT_LENGTH);
	s_pwd_input.input_count = 0;
	s_pwd_input.start_time = TIM_Get_MillisCounter();
	s_pwd_input.callback = callback; // 仅仅只是保存回调，用来给
	s_pwd_input.state = PWD_STATE_INPUTTING;

	printf("[PwdInput] Started, waiting for input...\r\n");
}

void pwd_input_cancel(void)
{
	if (s_pwd_input.state == PWD_STATE_INPUTTING)
	{
		s_pwd_input.state = PWD_STATE_CANCELLED;
		printf("[PwdInput] Cancelled\r\n");

		if (s_pwd_input.callback)
		{
			s_pwd_input.callback(s_pwd_input.password, s_pwd_input.input_count, PWD_STATE_CANCELLED);
		}
	}
}

void pwd_input_on_key(uint8_t key_id)
{
	if (s_pwd_input.state != PWD_STATE_INPUTTING)
	{
		return;
	}

	/* 键盘映射：
	 * 键1-9 = 数字1-9
	 * 键10 = 数字0
	 * 键11-16 = 保留/功能键
	 */
	uint8_t digit = 0xFF; /* 无效值 */

	if (key_id >= 1 && key_id <= 9)
	{
		digit = key_id; /* 键1-9 = 数字1-9 */
	}
	else if (key_id == 10)
	{
		digit = 0; /* 键10 = 数字0 */
	}
	else if (key_id == 11)
	{
		/* 键11 = 退格/删除 */
		if (s_pwd_input.input_count > 0)
		{
			s_pwd_input.input_count--;
			s_pwd_input.password[s_pwd_input.input_count] = 0;
			printf("[PwdInput] Backspace, count=%d\r\n pwd = %d%d%d%d\r\n", s_pwd_input.input_count, s_pwd_input.password[0],
				   s_pwd_input.password[1],
				   s_pwd_input.password[2],
				   s_pwd_input.password[3]);

			/* 通知UI更新 */
			if (s_pwd_input.ui_callback)
			{
				s_pwd_input.ui_callback(s_pwd_input.input_count);
			}
		}
		return;
	}
	else if (key_id == 12)
	{
		/* 键12 = 取消 */
		pwd_input_cancel();
		return;
	}
	else
	{
		/* 其他键忽略 */
		return;
	}

	if (digit <= 9 && s_pwd_input.input_count < PWD_INPUT_LENGTH)
	{
		s_pwd_input.password[s_pwd_input.input_count] = digit;
		s_pwd_input.input_count++;

		printf("[PwdInput] Input digit %d, count=%d\r\n", digit, s_pwd_input.input_count);

		/* 通知UI更新 */
		if (s_pwd_input.ui_callback)
		{
			s_pwd_input.ui_callback(s_pwd_input.input_count);
		}

		/* 检查是否输入完成 */
		if (s_pwd_input.input_count >= PWD_INPUT_LENGTH)
		{
			s_pwd_input.state = PWD_STATE_COMPLETE;
			printf("[PwdInput] Complete: %d%d%d%d\r\n",
				   s_pwd_input.password[0], s_pwd_input.password[1],
				   s_pwd_input.password[2], s_pwd_input.password[3]);

			if (s_pwd_input.callback)
			{
				s_pwd_input.callback(s_pwd_input.password, s_pwd_input.input_count, PWD_STATE_COMPLETE);
			}
		}
	}
}

void pwd_input_update(void)
{
	if (s_pwd_input.state != PWD_STATE_INPUTTING)
	{
		return;
	}

	/* 检查超时 */
	uint32_t elapsed = TIM_Get_MillisCounter() - s_pwd_input.start_time;
	if (elapsed >= PWD_INPUT_TIMEOUT_MS)
	{
		s_pwd_input.state = PWD_STATE_TIMEOUT;
		printf("[PwdInput] Timeout\r\n");

		if (s_pwd_input.callback)
		{
			s_pwd_input.callback(s_pwd_input.password, s_pwd_input.input_count, PWD_STATE_TIMEOUT);
		}
	}
}

pwd_input_state_t pwd_input_get_state(void)
{
	return s_pwd_input.state;
}

uint8_t pwd_input_get_length(void)
{
	return s_pwd_input.input_count;
}

uint8_t pwd_input_get_password(uint8_t *out)
{
	if (out != NULL && s_pwd_input.state == PWD_STATE_COMPLETE)
	{
		memcpy(out, s_pwd_input.password, PWD_INPUT_LENGTH);
		return PWD_INPUT_LENGTH;
	}
	return 0;
}

void pwd_input_clear(void)
{
	memset(s_pwd_input.password, 0, PWD_INPUT_LENGTH);
	s_pwd_input.input_count = 0;
	s_pwd_input.state = PWD_STATE_IDLE;
	s_pwd_input.callback = NULL;
}

uint8_t pwd_input_is_active(void)
{
	return (s_pwd_input.state == PWD_STATE_INPUTTING) ? 1 : 0;
}
