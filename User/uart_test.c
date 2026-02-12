/********************************** (C) COPYRIGHT *******************************
* File Name          : uart_test.c
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-30
* Description        : UART DMA接收测试模块实现
*********************************************************************************/

#include "uart_test.h"
#include "usart1_dma_rx.h"
#include "uart_rx_task.h"
#include "debug.h"

/**
 * @brief  UART接收统计任务
 * @note   每秒打印一次统计信息
 */
void uart_debug_task(void)
{
    static uint32_t last_rx_count = 0;
    uint32_t total_bytes = 0;
    uint16_t buffer_level = 0;

    // 获取统计信息
    uart_rx_get_statistics(&total_bytes, &buffer_level);
    uint32_t current_rx_count = USART1_Get_RxCount();

    // 计算接收速率
    uint32_t rx_speed = current_rx_count - last_rx_count;  // 字节/秒
    last_rx_count = current_rx_count;

    // 打印统计信息
    printf("[UART Stats] RX: %lu bytes | Speed: %lu B/s | Buffer: %u/%u bytes",
           current_rx_count, rx_speed, buffer_level, RINGBUFFER_LARGE_SIZE);

    // 检查溢出
    if (g_usart1_ringbuf.overflow_flag)
    {
        printf(" | [OVERFLOW]");
    }

    printf("\r\n");
}

/**
 * @brief  UART回环测试
 * @note   通过外部短接PA9和PA10进行测试
 */
void uart_loopback_test(void)
{
    uint8_t test_data[] = "UART DMA Test - 0123456789\r\n";
    uart_send_test_data(test_data, sizeof(test_data) - 1);
}

/**
 * @brief  发送测试数据包
 */
void uart_send_test_data(uint8_t* data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        // 轮询发送（简单实现）
        while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
        USART_SendData(USART1, data[i]);
    }
}
