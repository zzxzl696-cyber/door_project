/********************************** (C) COPYRIGHT *******************************
* File Name          : usart1_dma_rx.c
* Author             : Custom Implementation
* Version            : V1.0.0
* Date               : 2025-12-30
* Description        : USART1 DMA接收模块实现
*********************************************************************************/

#include "bsp_system.h"

/* 全局变量定义 */
usart1_dma_rx_t g_usart1_dma_rx = {0};
ringbuffer_large_t g_usart1_ringbuf = {0};

/**
 * @brief  USART1 GPIO初始化
 * @note   PA9  - USART1_Tx (复用推挽输出)
 *         PA10 - USART1_Rx (浮空输入)
 */
void USART1_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    // 使能GPIOA时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 配置PA9 (Tx) - 复用推挽输出
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置PA10 (Rx) - 浮空输入
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
 * @brief  USART1基础配置
 * @param  baudrate: 波特率 (例如: 115200)
 */
void USART1_Config(uint32_t baudrate)
{
    USART_InitTypeDef USART_InitStructure = {0};

    // 使能USART1时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    // 配置USART参数
    USART_InitStructure.USART_BaudRate   = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits   = USART_StopBits_1;
    USART_InitStructure.USART_Parity     = USART_Parity_No;
    USART_InitStructure.USART_Mode       = USART_Mode_Rx | USART_Mode_Tx; // 收发模式
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

    USART_Init(USART1, &USART_InitStructure);

    // 使能空闲中断 (用于处理不定长数据包)
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);

    // 使能USART1的DMA接收请求
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);

    // 使能USART1
    USART_Cmd(USART1, ENABLE);
}

/**
 * @brief  USART1 DMA接收初始化
 * @note   使用DMA1 Channel5，循环模式，半满+全满中断
 */
void USART1_DMA_RX_Init(void)
{
    DMA_InitTypeDef DMA_InitStructure = {0};

    // 使能DMA1时钟
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // 复位DMA1 Channel5
    DMA_DeInit(DMA1_Channel5);

    // 配置DMA参数
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DATAR;  // 外设地址：USART1数据寄存器
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)g_usart1_dma_rx.dma_buffer;  // 内存地址：DMA缓冲区
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;     // 方向：外设→内存
    DMA_InitStructure.DMA_BufferSize         = USART1_DMA_RX_BUFFER_SIZE; // 缓冲区大小：256
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable; // 外设地址不自增
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;      // 内存地址自增
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 外设数据宽度：8位
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;     // 内存数据宽度：8位
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular;           // 循环模式 ✓
    DMA_InitStructure.DMA_Priority           = DMA_Priority_VeryHigh;       // 优先级：最高
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;             // 非内存到内存

    DMA_Init(DMA1_Channel5, &DMA_InitStructure);

    // 使能DMA中断：半满(HT) + 全满(TC)
    DMA_ITConfig(DMA1_Channel5, DMA_IT_HT, ENABLE);  // 半满中断（128字节）
    DMA_ITConfig(DMA1_Channel5, DMA_IT_TC, ENABLE);  // 全满中断（256字节）

    // 使能DMA1 Channel5
    DMA_Cmd(DMA1_Channel5, ENABLE);
}

/**
 * @brief  USART1和DMA中断优先级配置
 * @note   优先级分组：NVIC_PriorityGroup_2 (2位抢占，2位子优先级)
 *         DMA优先级高于TIM2（确保数据不丢失）
 */
void USART1_NVIC_Config(void)
{
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    // 配置DMA1 Channel5中断
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  // 抢占优先级1 (高于TIM2)
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;         // 子优先级0
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 配置USART1中断（空闲中断）
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  // 抢占优先级1
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;         // 子优先级1
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/**
 * @brief  USART1 DMA接收完整初始化（一键配置）
 * @param  baudrate: 波特率
 * @param  ringbuf: 用户环形缓冲区指针
 */
void USART1_DMA_RX_FullInit(uint32_t baudrate, ringbuffer_large_t* ringbuf)
{
    // 初始化环形缓冲区
    ringbuffer_large_init(ringbuf);

    // 绑定环形缓冲区
    g_usart1_dma_rx.user_ringbuf = ringbuf;
    g_usart1_dma_rx.rx_count = 0;
    g_usart1_dma_rx.idle_detected = 0;
    g_usart1_dma_rx.error_flag = 0;
    g_usart1_dma_rx.dma_last_pos = 0;

    // 初始化GPIO
    USART1_GPIO_Init();

    // 配置USART1
    USART1_Config(baudrate);

    // 配置DMA
    USART1_DMA_RX_Init();

    // 配置NVIC
    USART1_NVIC_Config();
}

/**
 * @brief  获取USART1累计接收字节数
 */
uint32_t USART1_Get_RxCount(void)
{
    return g_usart1_dma_rx.rx_count;
}

/**
 * @brief  获取并清除空闲中断标志
 */
uint8_t USART1_Get_IdleFlag(void)
{
    uint8_t flag = g_usart1_dma_rx.idle_detected;
    g_usart1_dma_rx.idle_detected = 0;
    return flag;
}

/**
 * @brief  处理DMA双缓冲区数据（内部函数，中断调用）
 * @param  data_ptr: 数据指针
 * @param  data_len: 数据长度
 */
void USART1_DMA_Process_Data(uint8_t* data_ptr, uint16_t data_len)
{
    if (g_usart1_dma_rx.user_ringbuf == NULL)
    {
        return;  // 环形缓冲区未初始化
    }

    // 批量写入环形缓冲区
    uint16_t written = ringbuffer_large_write_multi(g_usart1_dma_rx.user_ringbuf,
                                                     data_ptr,
                                                     data_len);

    // 统计接收字节数
    g_usart1_dma_rx.rx_count += written;

    // 检查是否发生溢出
    if (written < data_len)
    {
        g_usart1_dma_rx.user_ringbuf->overflow_flag = 1;
    }
}

/**
 * @brief  从USART1环形缓冲区读取1字节
 * @param  data: 输出字节指针
 * @retval 0=成功, -1=无数据
 */
int USART1_RX_ReadByte(uint8_t *data)
{
    if (g_usart1_dma_rx.user_ringbuf == NULL)
    {
        return -1;
    }

    return ringbuffer_large_read(g_usart1_dma_rx.user_ringbuf, data);
}

/**
 * @brief  阻塞式写入USART1（逐字节等待TXE）
 * @param  data: 数据指针
 * @param  len: 数据长度
 * @retval 实际写入字节数
 */
uint16_t USART1_TX_Write(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++)
    {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        {
            /* 等待发送寄存器空 */
        }
        USART_SendData(USART1, data[i]);
    }

    return len;
}


