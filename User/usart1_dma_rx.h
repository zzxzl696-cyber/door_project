/********************************** (C) COPYRIGHT *******************************
* File Name          : usart1_dma_rx.h
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-30
* Description        : USART1 DMA接收模块 - 支持循环模式+环形缓冲区
*                      适配CH32V30x系列微控制器
*********************************************************************************/

#ifndef __USART1_DMA_RX_H
#define __USART1_DMA_RX_H

#include "ch32v30x.h"
#include "ringbuffer_large.h"

/* DMA配置参数 */
#define USART1_DMA_RX_BUFFER_SIZE   256  // DMA双缓冲区大小

/* USART1 DMA接收控制结构体 */
typedef struct {
    uint8_t  dma_buffer[USART1_DMA_RX_BUFFER_SIZE];  // DMA硬件缓冲区
    ringbuffer_large_t* user_ringbuf;                // 用户环形缓冲区指针
    uint32_t rx_count;                               // 累计接收字节数
    uint8_t  idle_detected;                          // 空闲中断标志
    uint8_t  error_flag;                             // 错误标志
    uint16_t dma_last_pos;                           // DMA上次已搬运位置（用于IDLE补齐，避免重复）
} usart1_dma_rx_t;

/* 全局变量声明 */
extern usart1_dma_rx_t g_usart1_dma_rx;
extern ringbuffer_large_t g_usart1_ringbuf;

/* API函数声明 */

/**
 * @brief  USART1 GPIO初始化（PA9-Tx, PA10-Rx）
 * @retval None
 */
void USART1_GPIO_Init(void);

/**
 * @brief  USART1基础配置（波特率、数据位、停止位等）
 * @param  baudrate: 波特率 (例如: 115200)
 * @retval None
 */
void USART1_Config(uint32_t baudrate);

/**
 * @brief  USART1 DMA接收初始化（DMA1 Channel5，循环模式）
 * @retval None
 */
void USART1_DMA_RX_Init(void);

/**
 * @brief  USART1和DMA中断优先级配置
 * @retval None
 */
void USART1_NVIC_Config(void);

/**
 * @brief  USART1 DMA接收完整初始化（一键配置）
 * @param  baudrate: 波特率
 * @param  ringbuf: 用户环形缓冲区指针
 * @retval None
 */
void USART1_DMA_RX_FullInit(uint32_t baudrate, ringbuffer_large_t* ringbuf);

/**
 * @brief  获取USART1累计接收字节数
 * @retval 接收字节总数
 */
uint32_t USART1_Get_RxCount(void);

/**
 * @brief  获取并清除空闲中断标志
 * @retval 1=检测到空闲, 0=无空闲
 */
uint8_t USART1_Get_IdleFlag(void);

/**
 * @brief  处理DMA双缓冲区数据（内部函数，中断调用）
 * @param  data_ptr: 数据指针
 * @param  data_len: 数据长度
 * @retval None
 */
void USART1_DMA_Process_Data(uint8_t* data_ptr, uint16_t data_len);

/**
 * @brief  从USART1环形缓冲区读取1字节（供esp_at使用）
 * @param  data: 输出字节指针
 * @retval 0=成功, -1=无数据
 */
int USART1_RX_ReadByte(uint8_t *data);

/**
 * @brief  阻塞式写入USART1（供esp_at使用）
 * @param  data: 数据指针
 * @param  len: 数据长度
 * @retval 实际写入字节数
 */
uint16_t USART1_TX_Write(const uint8_t *data, uint16_t len);

#endif /* __USART1_DMA_RX_H */
