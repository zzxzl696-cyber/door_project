/********************************** (C) COPYRIGHT *******************************
* File Name          : uart_rx_task.c
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-30
* Description        : UART接收数据处理任务实现
*********************************************************************************/

#include "uart_rx_task.h"
#include "usart1_dma_rx.h"
#include "debug.h"

/* 调试开关 */
#define UART_RX_DEBUG   1  // 1=开启调试输出, 0=关闭

/* 统计变量 */
static uint32_t s_total_processed = 0;  // 累计处理字节数

/**
 * @brief  UART接收数据处理任务
 * @note   从环形缓冲区读取数据并处理
 */
void uart_rx_task(void)
{
    uint8_t  data = 0;
    uint16_t count = 0;

    // 检查缓冲区是否有数据
    if (ringbuffer_large_is_empty(&g_usart1_ringbuf))
    {
        return;  // 无数据，直接返回
    }
	
    // 逐字节读取数据
    while (!ringbuffer_large_is_empty(&g_usart1_ringbuf))
    {
        if (ringbuffer_large_read(&g_usart1_ringbuf, &data) == 0)
        {
            // ========== 数据处理逻辑 ==========
            // 方案1：简单回显（调试用）
            #if UART_RX_DEBUG
            printf("RX: 0x%02X ('%c')\r\n", data, (data >= 32 && data <= 126) ? data : '.');
            #endif

            // 方案2：协议解析（用户自定义）
            // protocol_parse_byte(data);

            // 方案3：数据转发到其他模块
            // process_received_data(data);

            s_total_processed++;
            count++;

            // 防止单次处理过多数据（避免任务饥饿）
            if (count >= 64)
            {
                break;
            }
        }
    }

    // 检查溢出标志
    if (g_usart1_ringbuf.overflow_flag)
    {
        #if UART_RX_DEBUG
        printf("[WARNING] Ring buffer overflow! Lost data.\r\n");
        #endif

        g_usart1_ringbuf.overflow_flag = 0;  // 清除标志
    }

    // 检查空闲中断标志
    if (USART1_Get_IdleFlag())
    {
        #if UART_RX_DEBUG
        // printf("[INFO] IDLE interrupt detected\r\n");
        #endif
    }
}

/**
 * @brief  获取UART接收统计信息
 */
void uart_rx_get_statistics(uint32_t* total_bytes, uint16_t* buffer_level)
{
    if (total_bytes != NULL)
    {
        *total_bytes = s_total_processed;
    }

    if (buffer_level != NULL)
    {
        *buffer_level = ringbuffer_large_available(&g_usart1_ringbuf);
    }
}
