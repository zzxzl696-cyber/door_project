/********************************** (C) COPYRIGHT *******************************
* File Name          : uart_test.h
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-30
* Description        : UART DMA接收测试模块
*********************************************************************************/

#ifndef __UART_TEST_H
#define __UART_TEST_H

#include "ch32v30x.h"

/**
 * @brief  UART接收统计任务（调度器调用）
 * @note   周期：1000ms
 *         功能：定期打印接收统计信息
 * @retval None
 */
void uart_debug_task(void);

/**
 * @brief  UART回环测试（发送数据到自身）
 * @note   测试函数，用于验证DMA接收功能
 * @retval None
 */
void uart_loopback_test(void);

/**
 * @brief  发送测试数据包
 * @param  data: 数据指针
 * @param  len: 数据长度
 * @retval None
 */
void uart_send_test_data(uint8_t* data, uint16_t len);

#endif /* __UART_TEST_H */
