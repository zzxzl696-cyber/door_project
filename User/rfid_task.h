/********************************** (C) COPYRIGHT *******************************
* File Name          : rfid_task.h
* Author             : Custom Implementation
* Version            : V1.1.0
* Date               : 2026-02-05
* Description        : RFID读卡器周期性轮询任务模块
*                      集成认证管理器
*********************************************************************************/

#ifndef __RFID_TASK_H
#define __RFID_TASK_H

#include "ch32v30x.h"
#include "rfid_reader.h"
#include "timer_config.h"

/**
 * @brief  RFID读卡器轮询任务（调度器调用）
 * @note   周期：5ms (推荐)
 *         功能：周期性调用RFID_Poll()处理UART接收和帧解析
 *         说明：此任务必须添加到调度器中才能使RFID驱动正常工作
 * @retval None
 */
void rfid_task(void);

/**
 * @brief  获取RFID任务统计信息
 * @param  poll_count: 累计Poll调用次数输出
 * @param  frame_count: 累计接收帧数输出
 * @retval None
 */
void rfid_task_get_statistics(uint32_t* poll_count, uint32_t* frame_count);

/**
 * @brief  重置RFID任务统计信息
 * @retval None
 */
void rfid_task_reset_statistics(void);

#endif /* __RFID_TASK_H */
