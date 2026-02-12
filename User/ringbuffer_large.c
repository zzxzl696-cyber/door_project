/********************************** (C) COPYRIGHT *******************************
* File Name          : ringbuffer_large.c
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-30
* Description        : 大容量环形缓冲区模块实现
*********************************************************************************/

#include "ringbuffer_large.h"

/**
 * @brief  初始化环形缓冲区
 */
void ringbuffer_large_init(ringbuffer_large_t* rb)
{
    rb->w = 0;
    rb->r = 0;
    rb->itemCount = 0;
    rb->overflow_flag = 0;

    // 清空缓冲区（可选）
    for (uint16_t i = 0; i < RINGBUFFER_LARGE_SIZE; i++)
    {
        rb->buffer[i] = 0;
    }
}

/**
 * @brief  向环形缓冲区写入单字节数据
 * @note   写入策略：丢弃新数据（保护旧数据完整性）
 */
int ringbuffer_large_write(ringbuffer_large_t* rb, uint8_t data)
{
    // 检查缓冲区是否已满
    if (rb->itemCount >= RINGBUFFER_LARGE_SIZE)
    {
        rb->overflow_flag = 1;  // 设置溢出标志
        return -1;              // 拒绝写入
    }

    // 写入数据
    rb->buffer[rb->w] = data;

    // 更新写指针（循环）
    rb->w = (rb->w + 1) % RINGBUFFER_LARGE_SIZE;

    // 更新数据计数
    rb->itemCount++;

    return 0;
}

/**
 * @brief  从环形缓冲区读取单字节数据
 */
int ringbuffer_large_read(ringbuffer_large_t* rb, uint8_t* data)
{
    // 检查缓冲区是否为空
    if (rb->itemCount == 0)
    {
        return -1;  // 无数据可读
    }

    // 读取数据
    *data = rb->buffer[rb->r];

    // 更新读指针（循环）
    rb->r = (rb->r + 1) % RINGBUFFER_LARGE_SIZE;

    // 更新数据计数
    rb->itemCount--;

    return 0;
}

/**
 * @brief  批量写入数据到环形缓冲区
 * @note   适用于DMA中断中批量写入数据
 */
uint16_t ringbuffer_large_write_multi(ringbuffer_large_t* rb, uint8_t* data, uint16_t len)
{
    uint16_t written = 0;

    for (uint16_t i = 0; i < len; i++)
    {
        if (ringbuffer_large_write(rb, data[i]) == 0)
        {
            written++;
        }
        else
        {
            // 缓冲区满，停止写入
            break;
        }
    }

    return written;
}

/**
 * @brief  检查环形缓冲区是否为空
 */
uint8_t ringbuffer_large_is_empty(ringbuffer_large_t* rb)
{
    return (rb->itemCount == 0) ? 1 : 0;
}

/**
 * @brief  检查环形缓冲区是否已满
 */
uint8_t ringbuffer_large_is_full(ringbuffer_large_t* rb)
{
    return (rb->itemCount >= RINGBUFFER_LARGE_SIZE) ? 1 : 0;
}

/**
 * @brief  获取当前缓冲区数据量
 */
uint16_t ringbuffer_large_available(ringbuffer_large_t* rb)
{
    return rb->itemCount;
}

/**
 * @brief  清空环形缓冲区
 */
void ringbuffer_large_clear(ringbuffer_large_t* rb)
{
    rb->w = 0;
    rb->r = 0;
    rb->itemCount = 0;
    rb->overflow_flag = 0;
}
