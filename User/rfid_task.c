/********************************** (C) COPYRIGHT *******************************
 * File Name          : rfid_task.c
 * Author             : Custom Implementation
 * Version            : V1.1.0
 * Date               : 2026-02-05
 * Description        : RFID读卡器周期性轮询任务实现
 *                      集成认证管理器和用户管理功能
 *********************************************************************************/

#include "rfid_task.h"
#include "rfid_reader.h"
#include "auth_manager.h"
#include "user_admin.h"

/* 调试开关 */
#define RFID_TASK_DEBUG 0 // 1=开启调试输出, 0=关闭

/* 统计变量 */
static uint32_t s_poll_count = 0;  // 累计Poll调用次数
static uint32_t s_frame_count = 0; // 累计接收帧数

/* 上次刷卡时间，用于防抖 */
static uint32_t s_last_card_time = 0;
#define RFID_DEBOUNCE_MS 2000  /* 2秒内不重复处理同一张卡 */

/**
 * @brief  RFID读卡器轮询任务
 * @note   由调度器周期性调用（推荐5ms周期）
 *         负责调用RFID_Poll()处理UART接收和帧解析
 */
void rfid_task(void)
{
	RFID_Frame frame;
	RFID_Result ret;

	// 调用RFID驱动的轮询函数，处理UART接收
	ret = RFID_PollAutoFrame(&frame);
	if (ret == RFID_RET_OK)
	{
		uint32_t current_time = TIM_Get_MillisCounter();

		/* 防抖：检查是否在短时间内重复刷卡 */
		if (current_time - s_last_card_time < RFID_DEBOUNCE_MS) {
			return;
		}
		s_last_card_time = current_time;

		s_frame_count++;

		printf("\r\n[RFID] Card detected: %02X %02X %02X %02X\r\n",
			   frame.uid[0], frame.uid[1], frame.uid[2], frame.uid[3]);

		/* 检查是否在用户管理模式 */
		if (user_admin_is_active()) {
			/* 传递给用户管理模块（用于录入RFID） */
			user_admin_on_rfid(frame.uid);
		} else {
			/* 正常认证流程 */
			auth_process_rfid(&frame);
		}
	}
// RFID_Poll();
// s_poll_count++;

// 可选：检查是否有新帧可用（用于调试或统计）
#if RFID_TASK_DEBUG
	ret = RFID_TryGetFrame(&frame);
	if (ret == RFID_RET_OK)
	{
		s_frame_count++;

		// 根据帧类型输出调试信息
		switch (frame.type)
		{
		case RFID_FRAME_CMD_RESP:
			printf("[RFID Task] CMD Response: 0x%02X, Status: 0x%02X\r\n",
				   frame.cmd, frame.status);
			break;

		case RFID_FRAME_AUTO_ID:
			printf("[RFID Task] Auto ID: %02X %02X %02X %02X\r\n",
				   frame.uid[0], frame.uid[1], frame.uid[2], frame.uid[3]);
			break;

		case RFID_FRAME_AUTO_BLOCK:
			printf("[RFID Task] Auto Block %d received\r\n", frame.block_num);
			break;

		case RFID_FRAME_AUTO_ID_BLOCK:
			printf("[RFID Task] Auto ID+Block: UID=%02X%02X%02X%02X, Block=%d\r\n",
				   frame.uid[0], frame.uid[1], frame.uid[2], frame.uid[3],
				   frame.block_num);
			break;

		default:
			printf("[RFID Task] Unknown frame type: 0x%02X\r\n", frame.type);
			break;
		}
	}
	else if (ret == RFID_RET_BAD_FRAME)
	{
		printf("[RFID Task] Bad frame detected!\r\n");
	}
#endif
}

/**
 * @brief  获取RFID任务统计信息
 */
void rfid_task_get_statistics(uint32_t *poll_count, uint32_t *frame_count)
{
	if (poll_count != NULL)
	{
		*poll_count = s_poll_count;
	}

	if (frame_count != NULL)
	{
		*frame_count = s_frame_count;
	}
}

/**
 * @brief  重置RFID任务统计信息
 */
void rfid_task_reset_statistics(void)
{
	s_poll_count = 0;
	s_frame_count = 0;
}
