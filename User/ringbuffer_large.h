/********************************** (C) COPYRIGHT *******************************
 * File Name          : ringbuffer_large.h
 * Author             : Custom Implementation
 * Version            : V1.0.0
 * Date               : 2025-12-30
 * Description        : 大容量环形缓冲区模块 - 用于USART DMA接收
 *                      支持1024字节缓冲区，适配高速串口数据接收
 *********************************************************************************/

#ifndef __RINGBUFFER_LARGE_H
#define __RINGBUFFER_LARGE_H

#include "ch32v30x.h"

/* 配置参数 */
#define RINGBUFFER_LARGE_SIZE 1024 // 缓冲区大小：1024字节

/* 环形缓冲区结构体 */
typedef struct
{
	uint16_t w;							   // 写指针 (0-1023)
	uint16_t r;							   // 读指针 (0-1023)
	uint8_t buffer[RINGBUFFER_LARGE_SIZE]; // 数据缓冲区
	uint16_t itemCount;					   // 当前数据量
	uint8_t overflow_flag;				   // 溢出标志位 (1=溢出, 0=正常)
} ringbuffer_large_t;

/* API函数声明 */

/**
 * @brief  初始化环形缓冲区
 * @param  rb: 环形缓冲区指针
 * @retval None
 */
void ringbuffer_large_init(ringbuffer_large_t *rb);

/**
 * @brief  向环形缓冲区写入单字节数据
 * @param  rb: 环形缓冲区指针
 * @param  data: 要写入的数据
 * @retval 0=成功, -1=缓冲区满(写入失败)
 */
int ringbuffer_large_write(ringbuffer_large_t *rb, uint8_t data);

/**
 * @brief  从环形缓冲区读取单字节数据
 * @param  rb: 环形缓冲区指针
 * @param  data: 读取数据的存储指针
 * @retval 0=成功, -1=缓冲区空(读取失败)
 */
int ringbuffer_large_read(ringbuffer_large_t *rb, uint8_t *data);

/**
 * @brief  批量写入数据到环形缓冲区
 * @param  rb: 环形缓冲区指针
 * @param  data: 数据源指针
 * @param  len: 数据长度
 * @retval 实际写入的字节数
 */
uint16_t ringbuffer_large_write_multi(ringbuffer_large_t *rb, uint8_t *data, uint16_t len);

/**
 * @brief  检查环形缓冲区是否为空
 * @param  rb: 环形缓冲区指针
 * @retval 1=空, 0=非空
 */
uint8_t ringbuffer_large_is_empty(ringbuffer_large_t *rb);

/**
 * @brief  检查环形缓冲区是否已满
 * @param  rb: 环形缓冲区指针
 * @retval 1=满, 0=非满
 */
uint8_t ringbuffer_large_is_full(ringbuffer_large_t *rb);

/**
 * @brief  获取当前缓冲区数据量
 * @param  rb: 环形缓冲区指针
 * @retval 当前数据字节数
 */
uint16_t ringbuffer_large_available(ringbuffer_large_t *rb);

/**
 * @brief  清空环形缓冲区
 * @param  rb: 环形缓冲区指针
 * @retval None
 */
void ringbuffer_large_clear(ringbuffer_large_t *rb);

#endif /* __RINGBUFFER_LARGE_H */
