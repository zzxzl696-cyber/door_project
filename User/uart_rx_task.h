/********************************** (C) COPYRIGHT *******************************
* File Name          : uart_rx_task.h
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-30
* Description        : UART接收数据处理任务模块
*********************************************************************************/

#ifndef __UART_RX_TASK_H
#define __UART_RX_TASK_H

#include "ch32v30x.h"

/**
 * @brief  UART接收数据处理任务（调度器调用）
 * @note   周期：5ms (可根据数据量调整)
 *         功能：从环形缓冲区读取数据并处理
 * @retval None
 */
void uart_rx_task(void);

/**
 * @brief  获取UART接收统计信息
 * @param  total_bytes: 累计接收字节数输出
 * @param  buffer_level: 当前缓冲区数据量输出
 * @retval None
 */
void uart_rx_get_statistics(uint32_t* total_bytes, uint16_t* buffer_level);

#endif /* __UART_RX_TASK_H */
