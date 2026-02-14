/********************************** (C) COPYRIGHT *******************************
 * File Name          : door_status_ui.c
 * Author             : Door Access System
 * Version            : V1.1.0
 * Date               : 2026-02-14
 * Description        : Door status UI for 128x160 ST7735S LCD (ST7735S).
 *                      Admin UI uses per-step full-screen screens:
 *                      ONE STEP = ONE SCREEN.
 *********************************************************************************/

#include "door_status_ui.h"
#include "lcd.h"
#include "lcd_init.h"
#include "timer_config.h"
#include "auth_manager.h"
#include "user_admin.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ================= Layout constants ================= */

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 160

#define TITLE_Y_START   0
#define TITLE_Y_END     16

#define CONTENT_Y_START 16
#define CONTENT_Y_END   116

#define PROMPT_Y_START  116
#define PROMPT_Y_END    160

#define FONT_SIZE_12    12
#define FONT_SIZE_16    16

/* ================= Global UI state ================= */

static door_status_ui_t s_ui_state = {
	.current_state = UI_STATE_IDLE,
	.previous_state = UI_STATE_IDLE,
	.user_name = "",
	.password_display = "",
	.password_length = 0,
	.lockout_seconds = 0,
	.last_update_time = 0,
	.state_enter_time = 0,
	.blink_state = 0,
};

static struct
{
	door_lock_state_t last_lock_state;
	uint8_t last_servo_angle;
	char last_user_name[USER_NAME_LEN + 1];
	uint8_t last_fail_count;
	char last_method_str[16];
} s_idle_cache;

/* ================= Internal admin UI tracking ================= */

typedef struct
{
	admin_state_t last_state;
	add_user_step_t last_add_step;

	uint16_t add_user_db_count_before;
	uint8_t add_pwd_set;

	uint8_t show_add_done;
	uint32_t add_done_enter_time;
	uint16_t last_added_id;
	char last_added_name[USER_NAME_LEN + 1];
} admin_ui_state_t;

static admin_ui_state_t s_admin_ui = {
	.last_state = ADMIN_STATE_LOCKED,
	.last_add_step = ADD_STEP_NAME,
	.add_user_db_count_before = 0,
	.add_pwd_set = 0,
	.show_add_done = 0,
	.add_done_enter_time = 0,
	.last_added_id = 0,
	.last_added_name = "",
};

/* ================= Internal helpers ================= */

static uint8_t ui_char_width(uint8_t sizey)
{
	return (sizey == FONT_SIZE_16) ? 8 : 6;
}

static uint16_t ui_center_x(uint8_t sizey, const char *text)
{
	uint16_t w = (uint16_t)strlen(text) * ui_char_width(sizey);
	if (w >= SCREEN_WIDTH)
	{
		return 0;
	}
	return (SCREEN_WIDTH - w) / 2;
}

static void ui_show_center(uint16_t y, const char *text, uint16_t fc, uint16_t bc, uint8_t sizey)
{
	uint16_t x = ui_center_x(sizey, text);
	LCD_ShowString(x, y, (const uint8_t *)text, fc, bc, sizey, 0);
}

static void ui_clear_content(void)
{
	LCD_Fill(0, CONTENT_Y_START, SCREEN_WIDTH, CONTENT_Y_END, BLACK);
}

static void ui_clear_prompt(void)
{
	LCD_Fill(0, PROMPT_Y_START, SCREEN_WIDTH, PROMPT_Y_END, BLACK);
}

static void ui_clear_content_and_prompt(void)
{
	LCD_Fill(0, CONTENT_Y_START, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
}

static uint8_t ui_str_contains_ci(const char *haystack, const char *needle)
{
	size_t needle_len;

	if (haystack == NULL || needle == NULL)
	{
		return 0;
	}

	needle_len = strlen(needle);
	if (needle_len == 0)
	{
		return 1;
	}

	for (; *haystack != '\0'; haystack++)
	{
		size_t i;
		for (i = 0; i < needle_len; i++)
		{
			char hc = haystack[i];
			char nc = needle[i];
			if (hc == '\0')
			{
				break;
			}
			if (tolower((unsigned char)hc) != tolower((unsigned char)nc))
			{
				break;
			}
		}
		if (i == needle_len)
		{
			return 1;
		}
	}
	return 0;
}

static void ui_truncate_chars(char *s, size_t max_chars)
{
	if (s == NULL)
	{
		return;
	}
	if (strlen(s) > max_chars)
	{
		s[max_chars] = '\0';
	}
}

static uint8_t ui_get_unlock_remaining_sec(void)
{
	if (g_door_status.lock_state != DOOR_UNLOCKED)
	{
		return 0;
	}
	if (g_door_status.unlock_duration == 0)
	{
		return 0;
	}

	{
		uint32_t now = TIM_Get_MillisCounter();
		uint32_t elapsed = now - g_door_status.unlock_timestamp;
		if (elapsed >= g_door_status.unlock_duration)
		{
			return 0;
		}
		{
			uint32_t rem_ms = g_door_status.unlock_duration - elapsed;
			return (uint8_t)((rem_ms + 999) / 1000);
		}
	}
}

/* Draws "[ * * _ _ ]" centered at Y using 16pt font */
static void draw_password_boxes(uint16_t y, uint8_t filled_count)
{
	char s[12];
	uint8_t i;

	if (filled_count > 4)
	{
		filled_count = 4;
	}

	s[0] = '[';
	s[1] = ' ';
	for (i = 0; i < 4; i++)
	{
		s[2 + i * 2] = (i < filled_count) ? '*' : '_';
		s[3 + i * 2] = ' ';
	}
	s[10] = ']';
	s[11] = '\0';

	ui_show_center(y, s, YELLOW, BLACK, FONT_SIZE_16);
}

static uint8_t ui_find_user_with_max_id(user_entry_t *out)
{
	uint16_t count = user_db_get_count();
	uint8_t found = 0;
	user_entry_t user;
	uint16_t max_id = 0;

	if (out == NULL)
	{
		return 0;
	}

	for (uint16_t i = 0; i < count; i++)
	{
		if (user_db_get_by_index(i, &user) == USER_DB_OK)
		{
			if (!found || user.id > max_id)
			{
				*out = user;
				max_id = user.id;
				found = 1;
			}
		}
	}

	return found;
}

static void admin_ui_make_pending_user_name(char *out, size_t out_len)
{
	unsigned int seq;

	if (out == NULL || out_len == 0)
	{
		return;
	}

	/* user_admin uses "User%d" based on current user_db_get_count()+1 at start_add_user() */
	seq = (unsigned int)(s_admin_ui.add_user_db_count_before + 1);
	snprintf(out, out_len, "User%u", seq);
	out[out_len - 1] = '\0';
}

static void admin_ui_capture_last_added_user(void)
{
	user_entry_t user;
	if (ui_find_user_with_max_id(&user))
	{
		s_admin_ui.last_added_id = user.id;
		strncpy(s_admin_ui.last_added_name, user.name, USER_NAME_LEN);
		s_admin_ui.last_added_name[USER_NAME_LEN] = '\0';
	}
	else
	{
		s_admin_ui.last_added_id = 0;
		s_admin_ui.last_added_name[0] = '\0';
	}
}

/* ================= Screen drawing: shared title bars ================= */

static void draw_title_bar(void)
{
	const char *title = "DOOR ACCESS SYS";
	LCD_Fill(0, TITLE_Y_START, SCREEN_WIDTH, TITLE_Y_END, DARKBLUE);
	ui_show_center(2, title, WHITE, DARKBLUE, FONT_SIZE_12);
}

static void draw_admin_title_bar(void)
{
	const char *title = "ADMIN MODE";
	LCD_Fill(0, TITLE_Y_START, SCREEN_WIDTH, TITLE_Y_END, RED);
	ui_show_center(2, title, WHITE, RED, FONT_SIZE_12);
}

/* ================= Screen drawing: Normal mode (7 screens) ================= */

static void draw_normal_idle_screen(void)
{
	char buf[32];
	uint8_t fail_count;

	ui_clear_content_and_prompt();

	/* Status line */
	if (g_door_status.lock_state == DOOR_LOCKED)
	{
		LCD_ShowString(2, 20, (const uint8_t *)"Status: LOCKED", RED, BLACK, FONT_SIZE_12, 0);
	}
	else
	{
		LCD_ShowString(2, 20, (const uint8_t *)"Status: UNLOCKED", GREEN, BLACK, FONT_SIZE_12, 0);
	}

	/* Method */
	snprintf(buf, sizeof(buf), "Method: %s", door_control_get_auth_method_str());
	buf[21] = '\0';
	LCD_ShowString(2, 35, (const uint8_t *)buf, WHITE, BLACK, FONT_SIZE_12, 0);

	/* Angle */
	snprintf(buf, sizeof(buf), "Angle:  %3d deg", g_door_status.servo_angle);
	buf[21] = '\0';
	LCD_ShowString(2, 50, (const uint8_t *)buf, YELLOW, BLACK, FONT_SIZE_12, 0);

	/* User */
	if (s_ui_state.user_name[0] != '\0')
	{
		snprintf(buf, sizeof(buf), "User: %s", s_ui_state.user_name);
		buf[21] = '\0';
		LCD_ShowString(2, 68, (const uint8_t *)buf, GRAY, BLACK, FONT_SIZE_12, 0);
	}
	else
	{
		LCD_ShowString(2, 68, (const uint8_t *)"User: ---", GRAY, BLACK, FONT_SIZE_12, 0);
	}

	/* Fails */
	fail_count = auth_manager_get_fail_count();
	snprintf(buf, sizeof(buf), "Fails: %d/5", fail_count);
	LCD_ShowString(2, 83, (const uint8_t *)buf, (fail_count >= 3) ? RED : WHITE, BLACK, FONT_SIZE_12, 0);

	/* Prompt */
	ui_show_center(132, ">>> Waiting Auth <<<", CYAN, BLACK, FONT_SIZE_12);

	s_idle_cache.last_lock_state = g_door_status.lock_state;
	s_idle_cache.last_servo_angle = g_door_status.servo_angle;
	strncpy(s_idle_cache.last_user_name, s_ui_state.user_name, USER_NAME_LEN);
	s_idle_cache.last_user_name[USER_NAME_LEN] = '\0';
	strncpy(s_idle_cache.last_method_str, door_control_get_auth_method_str(), sizeof(s_idle_cache.last_method_str) - 1);
	s_idle_cache.last_method_str[sizeof(s_idle_cache.last_method_str) - 1] = '\0';
	s_idle_cache.last_fail_count = fail_count;
}

static void refresh_normal_idle_data(void)
{
	char buf[32];
	uint8_t fail_count;
	const char *method_str;
	const char *user_name;

	/* Status line */
	if (g_door_status.lock_state != s_idle_cache.last_lock_state)
	{
		LCD_Fill(0, 20, SCREEN_WIDTH, 20 + FONT_SIZE_12, BLACK);
		if (g_door_status.lock_state == DOOR_LOCKED)
		{
			LCD_ShowString(2, 20, (const uint8_t *)"Status: LOCKED", RED, BLACK, FONT_SIZE_12, 0);
		}
		else
		{
			LCD_ShowString(2, 20, (const uint8_t *)"Status: UNLOCKED", GREEN, BLACK, FONT_SIZE_12, 0);
		}
		s_idle_cache.last_lock_state = g_door_status.lock_state;
	}

	/* Method */
	method_str = door_control_get_auth_method_str();
	if (strcmp(method_str, s_idle_cache.last_method_str) != 0)
	{
		LCD_Fill(0, 35, SCREEN_WIDTH, 35 + FONT_SIZE_12, BLACK);
		snprintf(buf, sizeof(buf), "Method: %s", method_str);
		buf[21] = '\0';
		LCD_ShowString(2, 35, (const uint8_t *)buf, WHITE, BLACK, FONT_SIZE_12, 0);
		strncpy(s_idle_cache.last_method_str, method_str, sizeof(s_idle_cache.last_method_str) - 1);
		s_idle_cache.last_method_str[sizeof(s_idle_cache.last_method_str) - 1] = '\0';
	}

	/* Angle */
	if (g_door_status.servo_angle != s_idle_cache.last_servo_angle)
	{
		LCD_Fill(0, 50, SCREEN_WIDTH, 50 + FONT_SIZE_12, BLACK);
		snprintf(buf, sizeof(buf), "Angle:  %3d deg", g_door_status.servo_angle);
		buf[21] = '\0';
		LCD_ShowString(2, 50, (const uint8_t *)buf, YELLOW, BLACK, FONT_SIZE_12, 0);
		s_idle_cache.last_servo_angle = g_door_status.servo_angle;
	}

	/* User */
	user_name = s_ui_state.user_name;
	if (strcmp(user_name, s_idle_cache.last_user_name) != 0)
	{
		LCD_Fill(0, 68, SCREEN_WIDTH, 68 + FONT_SIZE_12, BLACK);
		if (user_name[0] != '\0')
		{
			snprintf(buf, sizeof(buf), "User: %s", user_name);
			buf[21] = '\0';
			LCD_ShowString(2, 68, (const uint8_t *)buf, GRAY, BLACK, FONT_SIZE_12, 0);
		}
		else
		{
			LCD_ShowString(2, 68, (const uint8_t *)"User: ---", GRAY, BLACK, FONT_SIZE_12, 0);
		}
		strncpy(s_idle_cache.last_user_name, user_name, USER_NAME_LEN);
		s_idle_cache.last_user_name[USER_NAME_LEN] = '\0';
	}

	/* Fails */
	fail_count = auth_manager_get_fail_count();
	if (fail_count != s_idle_cache.last_fail_count)
	{
		LCD_Fill(0, 83, SCREEN_WIDTH, 83 + FONT_SIZE_12, BLACK);
		snprintf(buf, sizeof(buf), "Fails: %d/5", fail_count);
		LCD_ShowString(2, 83, (const uint8_t *)buf, (fail_count >= 3) ? RED : WHITE, BLACK, FONT_SIZE_12, 0);
		s_idle_cache.last_fail_count = fail_count;
	}
}

static void draw_normal_reading_card_screen(void)
{
	ui_clear_content_and_prompt();

	ui_show_center(22, ">> RFID <<", CYAN, BLACK, FONT_SIZE_16);
	ui_show_center(48, "Reading Card...", YELLOW, BLACK, FONT_SIZE_12);
	ui_show_center(68, "[...scanning...]", GRAY, BLACK, FONT_SIZE_12);

	ui_show_center(132, "Please wait...", YELLOW, BLACK, FONT_SIZE_12);
}

static void draw_normal_scanning_finger_screen(void)
{
	ui_clear_content_and_prompt();

	ui_show_center(22, ">> FINGER <<", CYAN, BLACK, FONT_SIZE_16);
	ui_show_center(48, "Scanning...", YELLOW, BLACK, FONT_SIZE_12);
	ui_show_center(68, "[...scanning...]", GRAY, BLACK, FONT_SIZE_12);

	ui_show_center(132, "Please wait...", YELLOW, BLACK, FONT_SIZE_12);
}

static void draw_normal_input_password_screen(void)
{
	ui_clear_content_and_prompt();

	ui_show_center(22, ">> PASSWORD <<", CYAN, BLACK, FONT_SIZE_16);
	ui_show_center(48, "Enter Password:", WHITE, BLACK, FONT_SIZE_12);
	draw_password_boxes(68, s_ui_state.password_length);
	ui_show_center(90, "0-9=Input", GRAY, BLACK, FONT_SIZE_12);

	ui_show_center(125, "15=Cancel", GRAY, BLACK, FONT_SIZE_12);
	ui_show_center(142, "Auto-confirm at 4", GRAY, BLACK, FONT_SIZE_12);
}

static void draw_normal_auth_success_screen(void)
{
	char buf[32];

	ui_clear_content_and_prompt();

	ui_show_center(22, "[OK] GRANTED!", GREEN, BLACK, FONT_SIZE_16);

	snprintf(buf, sizeof(buf), "User: %s", (s_ui_state.user_name[0] != '\0') ? s_ui_state.user_name : "---");
	buf[21] = '\0';
	ui_show_center(48, buf, WHITE, BLACK, FONT_SIZE_12);

	if (g_door_status.lock_state == DOOR_UNLOCKED)
	{
		ui_show_center(68, "Door: UNLOCKED", GREEN, BLACK, FONT_SIZE_12);
	}
	else
	{
		ui_show_center(68, "Door: LOCKED", RED, BLACK, FONT_SIZE_12);
	}

	{
		uint8_t rem = ui_get_unlock_remaining_sec();
		snprintf(buf, sizeof(buf), "Auto-lock: %ds", rem);
		buf[21] = '\0';
		ui_show_center(88, buf, YELLOW, BLACK, FONT_SIZE_12);
	}

	ui_show_center(132, "Welcome!", GREEN, BLACK, FONT_SIZE_12);
}

static void draw_normal_auth_failed_screen(void)
{
	char buf[32];
	uint8_t fail_count = auth_manager_get_fail_count();

	ui_clear_content_and_prompt();

	ui_show_center(22, "[X] DENIED!", RED, BLACK, FONT_SIZE_16);
	ui_show_center(50, "Invalid credential", WHITE, BLACK, FONT_SIZE_12);
	ui_show_center(70, "Try again or", GRAY, BLACK, FONT_SIZE_12);
	ui_show_center(84, "use another method", GRAY, BLACK, FONT_SIZE_12);

	snprintf(buf, sizeof(buf), "Fails: %d/5", fail_count);
	ui_show_center(100, buf, YELLOW, BLACK, FONT_SIZE_12);

	ui_show_center(132, ">>> Waiting <<<", CYAN, BLACK, FONT_SIZE_12);
}

static void draw_normal_locked_screen(void)
{
	char buf[32];

	ui_clear_content_and_prompt();

	if (s_ui_state.blink_state)
	{
		ui_show_center(22, "!! LOCKED !!", RED, BLACK, FONT_SIZE_16);
	}

	ui_show_center(50, "Too many failures", WHITE, BLACK, FONT_SIZE_12);
	ui_show_center(70, "System locked", RED, BLACK, FONT_SIZE_12);

	snprintf(buf, sizeof(buf), "Wait: %ds", s_ui_state.lockout_seconds);
	ui_show_center(90, buf, YELLOW, BLACK, FONT_SIZE_12);

	if (s_ui_state.blink_state)
	{
		ui_show_center(132, "Please wait...", RED, BLACK, FONT_SIZE_12);
	}
}

static void draw_normal_screen(ui_state_t state)
{
	switch (state)
	{
	case UI_STATE_IDLE:
		draw_normal_idle_screen();
		break;
	case UI_STATE_READING_CARD:
		draw_normal_reading_card_screen();
		break;
	case UI_STATE_SCANNING_FINGER:
		draw_normal_scanning_finger_screen();
		break;
	case UI_STATE_INPUT_PASSWORD:
		draw_normal_input_password_screen();
		break;
	case UI_STATE_AUTH_SUCCESS:
		draw_normal_auth_success_screen();
		break;
	case UI_STATE_AUTH_FAILED:
		draw_normal_auth_failed_screen();
		break;
	case UI_STATE_LOCKED:
		draw_normal_locked_screen();
		break;
	default:
		draw_normal_idle_screen();
		break;
	}
}

/* ================= Screen drawing: Admin mode (13 screens) ================= */

static void draw_admin_menu_screen(void)
{
	char buf[32];
	uint16_t count = user_db_get_count();

	ui_clear_content_and_prompt();

	ui_show_center(20, "-- Admin Menu --", CYAN, BLACK, FONT_SIZE_12);
	LCD_ShowString(8, 38, (const uint8_t *)"1. Add User", WHITE, BLACK, FONT_SIZE_12, 0);
	LCD_ShowString(8, 53, (const uint8_t *)"2. Delete User", WHITE, BLACK, FONT_SIZE_12, 0);
	LCD_ShowString(8, 68, (const uint8_t *)"3. List Users", WHITE, BLACK, FONT_SIZE_12, 0);
	LCD_ShowString(8, 83, (const uint8_t *)"4. Change MasterPwd", WHITE, BLACK, FONT_SIZE_12, 0);

	snprintf(buf, sizeof(buf), "Users: %d/%d", count, USER_DB_MAX_USERS);
	buf[21] = '\0';
	LCD_ShowString(8, 100, (const uint8_t *)buf, GRAY, BLACK, FONT_SIZE_12, 0);
}

static void draw_add_step_name(void)
{
	char name[USER_NAME_LEN + 1];

	ui_clear_content_and_prompt();

	LCD_ShowString(2, 20, (const uint8_t *)"+ Add User [1/5]", CYAN, BLACK, FONT_SIZE_12, 0);
	LCD_ShowString(2, 42, (const uint8_t *)"User Name:", WHITE, BLACK, FONT_SIZE_12, 0);

	admin_ui_make_pending_user_name(name, sizeof(name));
	ui_show_center(62, name, YELLOW, BLACK, FONT_SIZE_16);
	ui_show_center(86, "(Auto-assigned)", GRAY, BLACK, FONT_SIZE_12);
}

static void draw_add_step_rfid(void)
{
	ui_clear_content_and_prompt();

	LCD_ShowString(2, 20, (const uint8_t *)"+ Add User [2/5]", CYAN, BLACK, FONT_SIZE_12, 0);
	ui_show_center(42, ">> Swipe Card <<", YELLOW, BLACK, FONT_SIZE_16);
	ui_show_center(66, "Place RFID card", WHITE, BLACK, FONT_SIZE_12);
	ui_show_center(82, "on the reader", WHITE, BLACK, FONT_SIZE_12);
}

static void draw_add_step_fingerprint(void)
{
	fingerprint_enroll_state_t fp_state = user_admin_get_fingerprint_enroll_state();
	const char *status = "Press 1/2";
	const char *line1 = "Place finger on";
	const char *line2 = "the sensor";
	uint16_t status_color = YELLOW;

	ui_clear_content_and_prompt();

	LCD_ShowString(2, 20, (const uint8_t *)"+ Add User [3/5]", CYAN, BLACK, FONT_SIZE_12, 0);

	switch (fp_state)
	{
	case FP_ENROLL_WAIT_FINGER_1:
	case FP_ENROLL_IDLE:
		status = "Press 1/2";
		line1 = "Place finger on";
		line2 = "the sensor";
		status_color = YELLOW;
		break;
	case FP_ENROLL_CAPTURE_1:
		status = "Capturing";
		line1 = "Keep finger still";
		line2 = "Do not move";
		status_color = YELLOW;
		break;
	case FP_ENROLL_REMOVE:
		status = "Press 2/2";
		line1 = "Remove finger";
		line2 = "then press again";
		status_color = YELLOW;
		break;
	case FP_ENROLL_WAIT_FINGER_2:
		status = "Press 2/2";
		line1 = "Place finger on";
		line2 = "the sensor";
		status_color = YELLOW;
		break;
	case FP_ENROLL_CAPTURE_2:
	case FP_ENROLL_MATCH:
	case FP_ENROLL_STORE:
		status = "Capturing";
		line1 = "Keep finger still";
		line2 = "Do not move";
		status_color = YELLOW;
		break;
	case FP_ENROLL_DONE:
		status = "Success";
		line1 = "Fingerprint saved";
		line2 = "";
		status_color = GREEN;
		break;
	case FP_ENROLL_ERROR:
	default:
		status = "Failed - Retry";
		line1 = "Try again";
		line2 = "";
		status_color = RED;
		break;
	}

	ui_show_center(42, status, status_color, BLACK, FONT_SIZE_16);
	ui_show_center(66, line1, WHITE, BLACK, FONT_SIZE_12);
	ui_show_center(82, line2, WHITE, BLACK, FONT_SIZE_12);
}

static void draw_add_step_password(void)
{
	ui_clear_content_and_prompt();

	LCD_ShowString(2, 20, (const uint8_t *)"+ Add User [4/5]", CYAN, BLACK, FONT_SIZE_12, 0);
	ui_show_center(42, "Set Password:", WHITE, BLACK, FONT_SIZE_12);
	draw_password_boxes(62, s_ui_state.password_length);
	ui_show_center(86, "4 digits (0-9)", GRAY, BLACK, FONT_SIZE_12);
}

static void draw_add_step_confirm(void)
{
	char buf[32];
	char name[USER_NAME_LEN + 1];

	ui_clear_content_and_prompt();

	LCD_ShowString(2, 20, (const uint8_t *)"+ Add User [5/5]", CYAN, BLACK, FONT_SIZE_12, 0);

	admin_ui_make_pending_user_name(name, sizeof(name));
	snprintf(buf, sizeof(buf), "Name: %s", name);
	buf[21] = '\0';
	LCD_ShowString(2, 38, (const uint8_t *)buf, WHITE, BLACK, FONT_SIZE_12, 0);

	/* NOTE: user_admin does not expose in-progress new_user flags via public API.
	 * Here we render a fixed layout and show PWD as [OK]/Skip based on UI-tracked completion.
	 */
	LCD_ShowString(2, 52, (const uint8_t *)"RFID: [OK]", GREEN, BLACK, FONT_SIZE_12, 0);
	LCD_ShowString(2, 66, (const uint8_t *)"FP:   [OK]", GREEN, BLACK, FONT_SIZE_12, 0);

	if (s_admin_ui.add_pwd_set)
	{
		LCD_ShowString(2, 80, (const uint8_t *)"PWD:  [OK]", GREEN, BLACK, FONT_SIZE_12, 0);
	}
	else
	{
		LCD_ShowString(2, 80, (const uint8_t *)"PWD:  Skip", GRAY, BLACK, FONT_SIZE_12, 0);
	}

	ui_show_center(98, ">> Confirm Add? <<", GREEN, BLACK, FONT_SIZE_12);
}

static void draw_add_step_done(void)
{
	char buf[32];
	const char *name = (s_admin_ui.last_added_name[0] != '\0') ? s_admin_ui.last_added_name : "---";

	ui_clear_content_and_prompt();

	ui_show_center(22, "[OK] User Added!", GREEN, BLACK, FONT_SIZE_16);

	snprintf(buf, sizeof(buf), "ID: %d", s_admin_ui.last_added_id);
	ui_show_center(50, buf, WHITE, BLACK, FONT_SIZE_12);

	snprintf(buf, sizeof(buf), "Name: %s", name);
	buf[21] = '\0';
	ui_show_center(68, buf, WHITE, BLACK, FONT_SIZE_12);

	ui_show_center(90, "Operation OK", GREEN, BLACK, FONT_SIZE_12);
	ui_show_center(132, "Any key to back", GRAY, BLACK, FONT_SIZE_12);
}

static void draw_delete_selection_screen(void)
{
	char buf[32];
	uint16_t count = user_db_get_count();
	user_entry_t user;
	uint8_t max_show = (count > 5) ? 5 : (uint8_t)count;

	ui_clear_content_and_prompt();

	ui_show_center(20, ">> Delete User <<", CYAN, BLACK, FONT_SIZE_12);

	for (uint8_t i = 0; i < max_show; i++)
	{
		if (user_db_get_by_index(i, &user) == USER_DB_OK)
		{
			uint16_t y = 35 + i * 13;
			snprintf(buf, sizeof(buf), "%d.%-8s ID=%d", i + 1, user.name, user.id);
			buf[21] = '\0';
			LCD_ShowString(2, y, (const uint8_t *)buf, WHITE, BLACK, FONT_SIZE_12, 0);
		}
	}

	if (count == 0)
	{
		ui_show_center(60, "(No users)", GRAY, BLACK, FONT_SIZE_12);
	}
	else
	{
		ui_show_center(100, "(No selection)", GRAY, BLACK, FONT_SIZE_12);
	}
}

static void draw_delete_confirm_screen(void)
{
	uint16_t selected_id = user_admin_get_selected_user_id();
	user_entry_t user;
	char big[32];

	ui_clear_content_and_prompt();

	ui_show_center(20, ">> Delete User <<", CYAN, BLACK, FONT_SIZE_12);
	ui_show_center(42, "Delete user:", WHITE, BLACK, FONT_SIZE_12);

	if (user_db_find_by_id(selected_id, &user) == USER_DB_OK)
	{
		snprintf(big, sizeof(big), "%s (ID=%d)", user.name, user.id);
	}
	else
	{
		snprintf(big, sizeof(big), "ID=%d", selected_id);
	}

	/* Keep within 16 characters for 16pt font */
	ui_truncate_chars(big, 16);
	ui_show_center(62, big, RED, BLACK, FONT_SIZE_16);

	ui_show_center(88, "Are you sure?", YELLOW, BLACK, FONT_SIZE_12);
}

static void draw_list_users_screen(void)
{
	char buf[32];
	uint16_t count = user_db_get_count();
	user_entry_t user;
	uint8_t max_show = (count > 5) ? 5 : (uint8_t)count;

	ui_clear_content_and_prompt();

	snprintf(buf, sizeof(buf), "-- Users (%d) --", count);
	ui_show_center(20, buf, CYAN, BLACK, FONT_SIZE_12);

	if (count == 0)
	{
		ui_show_center(60, "(Empty)", GRAY, BLACK, FONT_SIZE_12);
		ui_show_center(132, "Any key to back", GRAY, BLACK, FONT_SIZE_12);
		return;
	}

	for (uint8_t i = 0; i < max_show; i++)
	{
		if (user_db_get_by_index(i, &user) == USER_DB_OK)
		{
			uint16_t y = 35 + i * 13;
			char flags[8] = "       ";
			if (user.flags & USER_FLAG_RFID_EN)
				flags[0] = 'R';
			if (user.flags & USER_FLAG_FP_EN)
				flags[2] = 'F';
			if (user.flags & USER_FLAG_PWD_EN)
				flags[4] = 'P';
			if (user.flags & USER_FLAG_DISABLED)
			{
				flags[5] = '!';
				flags[6] = '\0';
			}

			snprintf(buf, sizeof(buf), "%d.%-8s %s", i + 1, user.name, flags);
			buf[21] = '\0';
			LCD_ShowString(2, y, (const uint8_t *)buf, (user.flags & USER_FLAG_DISABLED) ? RED : WHITE, BLACK, FONT_SIZE_12, 0);
		}
	}

	ui_show_center(132, "Any key to back", GRAY, BLACK, FONT_SIZE_12);
}

static uint8_t admin_get_master_pwd_step(void)
{
	const char *prompt = user_admin_get_prompt();

	if (ui_str_contains_ci(prompt, "old"))
	{
		return 0;
	}
	if (ui_str_contains_ci(prompt, "new"))
	{
		return 1;
	}
	if (ui_str_contains_ci(prompt, "confirm"))
	{
		return 2;
	}

	/* Fallback */
	return 0;
}

static void draw_chgpwd_step_old(void)
{
	ui_clear_content_and_prompt();

	LCD_ShowString(2, 20, (const uint8_t *)"ChgPwd [1/3]", CYAN, BLACK, FONT_SIZE_12, 0);
	ui_show_center(42, "Enter OLD Password:", WHITE, BLACK, FONT_SIZE_12);
	draw_password_boxes(62, s_ui_state.password_length);
	ui_show_center(86, "4 digits (0-9)", GRAY, BLACK, FONT_SIZE_12);
}

static void draw_chgpwd_step_new(void)
{
	ui_clear_content_and_prompt();

	LCD_ShowString(2, 20, (const uint8_t *)"ChgPwd [2/3]", CYAN, BLACK, FONT_SIZE_12, 0);
	ui_show_center(42, "Enter NEW Password:", WHITE, BLACK, FONT_SIZE_12);
	draw_password_boxes(62, s_ui_state.password_length);
	ui_show_center(86, "4 digits (0-9)", GRAY, BLACK, FONT_SIZE_12);
}

static void draw_chgpwd_step_confirm(void)
{
	ui_clear_content_and_prompt();

	LCD_ShowString(2, 20, (const uint8_t *)"ChgPwd [3/3]", CYAN, BLACK, FONT_SIZE_12, 0);
	ui_show_center(42, "CONFIRM Password:", WHITE, BLACK, FONT_SIZE_12);
	draw_password_boxes(62, s_ui_state.password_length);
	ui_show_center(86, "Must match step 2", GRAY, BLACK, FONT_SIZE_12);
}

static void draw_admin_content(void)
{
	admin_state_t state = user_admin_get_state();

	if (state == ADMIN_STATE_UNLOCKED)
	{
		if (s_admin_ui.show_add_done)
		{
			draw_add_step_done();
		}
		else
		{
			draw_admin_menu_screen();
		}
		return;
	}

	switch (state)
	{
	case ADMIN_STATE_ADD_USER:
	{
		add_user_step_t step = user_admin_get_add_step();
		switch (step)
		{
		case ADD_STEP_NAME:
			draw_add_step_name();
			break;
		case ADD_STEP_RFID:
			draw_add_step_rfid();
			break;
		case ADD_STEP_FINGERPRINT:
			draw_add_step_fingerprint();
			break;
		case ADD_STEP_PASSWORD:
			draw_add_step_password();
			break;
		case ADD_STEP_CONFIRM:
			draw_add_step_confirm();
			break;
		case ADD_STEP_DONE:
			draw_add_step_done();
			break;
		default:
			draw_add_step_name();
			break;
		}
		break;
	}

	case ADMIN_STATE_DELETE_USER:
		if (user_admin_get_selected_user_id() == 0)
		{
			draw_delete_selection_screen();
		}
		else
		{
			draw_delete_confirm_screen();
		}
		break;

	case ADMIN_STATE_LIST_USERS:
		draw_list_users_screen();
		break;

	case ADMIN_STATE_CHANGE_MASTER_PWD:
		switch (admin_get_master_pwd_step())
		{
		case 0:
			draw_chgpwd_step_old();
			break;
		case 1:
			draw_chgpwd_step_new();
			break;
		case 2:
			draw_chgpwd_step_confirm();
			break;
		default:
			draw_chgpwd_step_old();
			break;
		}
		break;

	default:
		draw_admin_menu_screen();
		break;
	}
}

static void draw_admin_prompt(void)
{
	admin_state_t state = user_admin_get_state();

	/* Clear prompt area only; content is cleared by draw_admin_content() */
	ui_clear_prompt();

	/* Special: show "done" prompt override */
	if (state == ADMIN_STATE_UNLOCKED && s_admin_ui.show_add_done)
	{
		ui_show_center(132, "Any key to back", GRAY, BLACK, FONT_SIZE_12);
		return;
	}

	switch (state)
	{
	case ADMIN_STATE_UNLOCKED:
		ui_show_center(120, "1-4=Select", YELLOW, BLACK, FONT_SIZE_12);
		ui_show_center(140, "15=Exit", GRAY, BLACK, FONT_SIZE_12);
		break;

	case ADMIN_STATE_ADD_USER:
	{
		add_user_step_t step = user_admin_get_add_step();
		switch (step)
		{
		case ADD_STEP_NAME:
			ui_show_center(120, "16=Confirm name", YELLOW, BLACK, FONT_SIZE_12);
			ui_show_center(140, "15=Cancel", GRAY, BLACK, FONT_SIZE_12);
			break;
		case ADD_STEP_RFID:
			ui_show_center(120, "16=Skip", YELLOW, BLACK, FONT_SIZE_12);
			ui_show_center(140, "15=Cancel", GRAY, BLACK, FONT_SIZE_12);
			break;
		case ADD_STEP_FINGERPRINT:
			ui_show_center(120, "16=Skip", YELLOW, BLACK, FONT_SIZE_12);
			ui_show_center(140, "15=Cancel", GRAY, BLACK, FONT_SIZE_12);
			break;
		case ADD_STEP_PASSWORD:
			ui_show_center(120, "0-9=Input 16=Skip", YELLOW, BLACK, FONT_SIZE_12);
			ui_show_center(140, "15=Cancel", GRAY, BLACK, FONT_SIZE_12);
			break;
		case ADD_STEP_CONFIRM:
			ui_show_center(120, "16=Confirm", YELLOW, BLACK, FONT_SIZE_12);
			ui_show_center(140, "15=Cancel", GRAY, BLACK, FONT_SIZE_12);
			break;
		case ADD_STEP_DONE:
			ui_show_center(132, "Any key to back", GRAY, BLACK, FONT_SIZE_12);
			break;
		default:
			break;
		}
		break;
	}

	case ADMIN_STATE_DELETE_USER:
		if (user_admin_get_selected_user_id() == 0)
		{
			ui_show_center(120, "1-9=Select user", YELLOW, BLACK, FONT_SIZE_12);
			ui_show_center(140, "15=Back", GRAY, BLACK, FONT_SIZE_12);
		}
		else
		{
			ui_show_center(120, "16=DELETE", RED, BLACK, FONT_SIZE_12);
			ui_show_center(140, "15=Cancel", GRAY, BLACK, FONT_SIZE_12);
		}
		break;

	case ADMIN_STATE_LIST_USERS:
		ui_show_center(132, "Any key to back", GRAY, BLACK, FONT_SIZE_12);
		break;

	case ADMIN_STATE_CHANGE_MASTER_PWD:
		ui_show_center(132, "15=Cancel", GRAY, BLACK, FONT_SIZE_12);
		break;

	default:
		break;
	}
}

static void refresh_admin_screen(void)
{
	ui_clear_content_and_prompt();
	draw_admin_content();
	draw_admin_prompt();
}

/* ================= Public API ================= */

void door_status_ui_init(void)
{
	LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
	draw_title_bar();

	/* Ensure initial view is full-screen idle */
	draw_normal_idle_screen();

	s_ui_state.last_update_time = TIM_Get_MillisCounter();
	s_ui_state.state_enter_time = s_ui_state.last_update_time;
}

void door_status_ui_update(void)
{
	uint32_t now = TIM_Get_MillisCounter();
	uint32_t elapsed = now - s_ui_state.last_update_time;

	switch (s_ui_state.current_state)
	{
	case UI_STATE_IDLE:
		if (elapsed >= 1000)
		{
			refresh_normal_idle_data();
			s_ui_state.last_update_time = now;
		}
		break;

	case UI_STATE_AUTH_SUCCESS:
		/* Return to idle after 5 seconds */
		if (now - s_ui_state.state_enter_time >= 5000)
		{
			door_status_ui_set_state(UI_STATE_IDLE);
		}
		else if (elapsed >= 1000)
		{
			/* Refresh countdown line */
			draw_normal_auth_success_screen();
			s_ui_state.last_update_time = now;
		}
		break;

	case UI_STATE_AUTH_FAILED:
		if (now - s_ui_state.state_enter_time >= 5000)
		{
			door_status_ui_set_state(UI_STATE_IDLE);
		}
		break;

	case UI_STATE_LOCKED:
		if (elapsed >= 1000)
		{
			uint32_t lockout_ms = auth_manager_get_lockout_remaining();
			s_ui_state.lockout_seconds = (uint8_t)((lockout_ms + 999) / 1000);

			if (s_ui_state.lockout_seconds == 0)
			{
				door_status_ui_set_state(UI_STATE_IDLE);
			}
			else
			{
				s_ui_state.blink_state = !s_ui_state.blink_state;
				draw_normal_locked_screen();
				s_ui_state.last_update_time = now;
			}
		}
		break;

	case UI_STATE_ADMIN:
		/* Auto-return from Add Done screen to menu */
		if (s_admin_ui.show_add_done && (now - s_admin_ui.add_done_enter_time >= 2000))
		{
			s_admin_ui.show_add_done = 0;
			refresh_admin_screen();
		}
		break;

	case UI_STATE_INPUT_PASSWORD:
	case UI_STATE_READING_CARD:
	case UI_STATE_SCANNING_FINGER:
	default:
		break;
	}
}

void door_status_ui_set_state(ui_state_t state)
{
	if (s_ui_state.current_state == state)
	{
		return;
	}

	s_ui_state.previous_state = s_ui_state.current_state;
	s_ui_state.current_state = state;
	s_ui_state.state_enter_time = TIM_Get_MillisCounter();
	s_ui_state.last_update_time = s_ui_state.state_enter_time;

	if (state == UI_STATE_IDLE)
	{
		s_ui_state.password_length = 0;
		s_ui_state.password_display[0] = '\0';
	}

	if (state == UI_STATE_LOCKED)
	{
		uint32_t lockout_ms = auth_manager_get_lockout_remaining();
		s_ui_state.lockout_seconds = (uint8_t)((lockout_ms + 999) / 1000);
		s_ui_state.blink_state = 1;
	}

	if (state == UI_STATE_ADMIN)
	{
		LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
		draw_admin_title_bar();
		refresh_admin_screen();
		return;
	}

	/* If coming from admin unexpectedly, rebuild normal mode screen */
	if (s_ui_state.previous_state == UI_STATE_ADMIN)
	{
		LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
		draw_title_bar();
	}

	draw_normal_screen(state);
}

ui_state_t door_status_ui_get_state(void)
{
	return s_ui_state.current_state;
}

void door_status_ui_on_auth_start(auth_method_t method)
{
	switch (method)
	{
	case AUTH_RFID:
		door_status_ui_set_state(UI_STATE_READING_CARD);
		break;
	case AUTH_FINGERPRINT:
		door_status_ui_set_state(UI_STATE_SCANNING_FINGER);
		break;
	case AUTH_PASSWORD:
		door_status_ui_set_state(UI_STATE_INPUT_PASSWORD);
		break;
	default:
		break;
	}
}

void door_status_ui_on_auth_result(auth_method_t method, auth_result_t result, const user_entry_t *user)
{
	(void)method;

	if (auth_manager_is_locked())
	{
		door_status_ui_set_state(UI_STATE_LOCKED);
		return;
	}

	if (result == AUTH_RESULT_SUCCESS)
	{
		if (user != NULL)
		{
			strncpy(s_ui_state.user_name, (const char *)user->name, sizeof(s_ui_state.user_name) - 1);
			s_ui_state.user_name[sizeof(s_ui_state.user_name) - 1] = '\0';
		}
		else
		{
			strcpy(s_ui_state.user_name, "Unknown");
		}
		door_status_ui_set_state(UI_STATE_AUTH_SUCCESS);
	}
	else
	{
		door_status_ui_set_state(UI_STATE_AUTH_FAILED);
	}
}

void door_status_ui_on_password_input(uint8_t length)
{
	uint8_t i;

	s_ui_state.password_length = length;

	for (i = 0; i < length && i < 4; i++)
	{
		s_ui_state.password_display[i] = '*';
	}
	s_ui_state.password_display[i] = '\0';

	/* Update password-dependent screens immediately */
	if (s_ui_state.current_state == UI_STATE_INPUT_PASSWORD)
	{
		draw_normal_input_password_screen();
	}
	else if (s_ui_state.current_state == UI_STATE_ADMIN)
	{
		admin_state_t state = user_admin_get_state();
		if (state == ADMIN_STATE_ADD_USER && user_admin_get_add_step() == ADD_STEP_PASSWORD && length >= 4)
		{
			s_admin_ui.add_pwd_set = 1;
		}
		refresh_admin_screen();
	}
}

void door_status_ui_on_admin_enter(void)
{
	s_ui_state.previous_state = s_ui_state.current_state;
	s_ui_state.current_state = UI_STATE_ADMIN;
	s_ui_state.state_enter_time = TIM_Get_MillisCounter();
	s_ui_state.last_update_time = s_ui_state.state_enter_time;

	s_ui_state.password_length = 0;
	s_ui_state.password_display[0] = '\0';

	s_admin_ui.show_add_done = 0;
	s_admin_ui.add_pwd_set = 0;
	s_admin_ui.last_state = user_admin_get_state();
	s_admin_ui.last_add_step = user_admin_get_add_step();

	LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
	draw_admin_title_bar();
	refresh_admin_screen();
}

void door_status_ui_on_admin_exit(void)
{
	s_ui_state.password_length = 0;
	s_ui_state.password_display[0] = '\0';
	s_ui_state.user_name[0] = '\0';

	s_admin_ui.show_add_done = 0;
	s_admin_ui.add_pwd_set = 0;

	LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
	draw_title_bar();

	s_ui_state.previous_state = UI_STATE_ADMIN;
	s_ui_state.current_state = UI_STATE_IDLE;
	s_ui_state.state_enter_time = TIM_Get_MillisCounter();
	s_ui_state.last_update_time = s_ui_state.state_enter_time;

	draw_normal_idle_screen();
}

void door_status_ui_on_admin_state_change(void)
{
	admin_state_t state;
	add_user_step_t add_step;
	uint32_t now;

	if (s_ui_state.current_state != UI_STATE_ADMIN)
	{
		return;
	}

	state = user_admin_get_state();
	add_step = user_admin_get_add_step();
	now = TIM_Get_MillisCounter();

	/* Clear "done" banner when entering any non-menu state */
	if (state != ADMIN_STATE_UNLOCKED)
	{
		s_admin_ui.show_add_done = 0;
	}

	/* Track add-user flow transitions for DONE screen */
	if (state == ADMIN_STATE_ADD_USER && s_admin_ui.last_state != ADMIN_STATE_ADD_USER)
	{
		s_admin_ui.add_user_db_count_before = user_db_get_count();
		s_admin_ui.add_pwd_set = 0;
	}

	if (state != ADMIN_STATE_ADD_USER && s_admin_ui.last_state == ADMIN_STATE_ADD_USER)
	{
		uint16_t count_after = user_db_get_count();
		if (count_after > s_admin_ui.add_user_db_count_before)
		{
			admin_ui_capture_last_added_user();
			s_admin_ui.show_add_done = 1;
			s_admin_ui.add_done_enter_time = now;
		}
	}

	s_admin_ui.last_state = state;
	s_admin_ui.last_add_step = add_step;

	/* Reset password visual on step/state change (boxes are drawn from s_ui_state.password_length) */
	s_ui_state.password_length = 0;
	s_ui_state.password_display[0] = '\0';

	refresh_admin_screen();
}
