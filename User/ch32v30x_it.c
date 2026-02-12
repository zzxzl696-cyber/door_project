/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v30x_it.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2024/03/06
 * Description        : Main Interrupt Service Routines.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#include "ch32v30x_it.h"
#include "usart1_dma_rx.h"
#include "bsp_system.h"

// AS608指纹模块接收缓冲区 (定义在as608.c)
extern uint8_t AS608_RX_BUF[];
extern uint16_t AS608_RX_STA;
#define AS608_RX_BUF_SIZE 256

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
// UART5 中断入口函数
void UART5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
/*********************************************************************
 * @fn      NMI_Handler
 *
 * @brief   This function handles NMI exception.
 *
 * @return  none
 */
void NMI_Handler(void)
{
	while (1)
	{
	}
}

/*********************************************************************
 * @fn      HardFault_Handler
 *
 * @brief   This function handles Hard Fault exception.
 *
 * @return  none
 */
void HardFault_Handler(void)
{
	while (1)
	{
	}
}

/*********************************************************************
 * @fn      DMA1_Channel5_IRQHandler
 *
 * @brief   DMA1 Channel5中断处理函数 (USART1接收)
 *          触发条件：半满(HT) / 全满(TC)
 *          功能：将DMA缓冲区数据搬运到环形缓冲区
 *
 * @return  none
 */
void DMA1_Channel5_IRQHandler(void)
{
	uint16_t data_len = 0;
	uint8_t *data_ptr = NULL;

	// 判断中断类型：半满中断
	if (DMA_GetITStatus(DMA1_IT_HT5) != RESET)
	{
		DMA_ClearITPendingBit(DMA1_IT_HT5);

		// 处理前半区数据 [0-127]
		data_ptr = &g_usart1_dma_rx.dma_buffer[0];
		data_len = 128;
	}
	// 判断中断类型：全满中断
	else if (DMA_GetITStatus(DMA1_IT_TC5) != RESET)
	{
		DMA_ClearITPendingBit(DMA1_IT_TC5);

		// 处理后半区数据 [128-255]
		data_ptr = &g_usart1_dma_rx.dma_buffer[128];
		data_len = 128;
	}

	// 将数据写入环形缓冲区
	if (data_ptr != NULL)
	{
		USART1_DMA_Process_Data(data_ptr, data_len);
	}
}

/*********************************************************************
 * @fn      USART1_IRQHandler
 *
 * @brief   USART1中断处理函数 (空闲中断)
 *          触发条件：接收线空闲时间 > 1字节传输时间
 *          功能：处理不定长数据包
 *
 * @return  none
 */
void USART1_IRQHandler(void)
{
	uint16_t recv_len = 0;
	uint16_t dma_remain = 0;

	// 检查空闲中断标志
	if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)
	{
		// 清除空闲中断标志 (读SR + 读DR)
		(void)USART1->STATR;
		(void)USART1->DATAR;

		// 停止DMA传输
		DMA_Cmd(DMA1_Channel5, DISABLE);

		// 计算实际接收字节数
		dma_remain = DMA_GetCurrDataCounter(DMA1_Channel5);
		recv_len = USART1_DMA_RX_BUFFER_SIZE - dma_remain;

		// 处理数据
		if (recv_len > 0)
		{
			USART1_DMA_Process_Data(g_usart1_dma_rx.dma_buffer, recv_len);
		}

		// 重新启动DMA
		DMA_SetCurrDataCounter(DMA1_Channel5, USART1_DMA_RX_BUFFER_SIZE);
		DMA_Cmd(DMA1_Channel5, ENABLE);

		// 设置空闲标志位
		g_usart1_dma_rx.idle_detected = 1;
	}
}

/*********************************************************************
 * @fn      USART2_IRQHandler
 *
 * @brief   USART2中断处理函数 (AS608指纹模块通信)
 *          触发条件：
 *          1. RXNE - 接收数据寄存器非空
 *          2. IDLE - 空闲线路检测（一帧数据接收完成）
 *
 *          AS608_RX_STA 格式：
 *          bit15: 接收完成标志 (1=完成, 0=未完成)
 *          bit14~0: 接收到的数据长度
 *
 * @return  none
 */
void USART2_IRQHandler(void)
{
	uint8_t res;

	// 接收数据中断
	if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
	{
		res = USART_ReceiveData(USART2);

		if ((AS608_RX_STA & 0x8000) == 0) // 接收未完成
		{
			if ((AS608_RX_STA & 0x7FFF) < AS608_RX_BUF_SIZE)
			{
				AS608_RX_BUF[AS608_RX_STA & 0x7FFF] = res;
				AS608_RX_STA++;
			}
			else
			{
				AS608_RX_STA |= 0x8000; // 缓冲区满，设置接收完成标志
			}
		}
	}

	// 空闲中断 - 一帧数据接收完成
	if (USART_GetITStatus(USART2, USART_IT_IDLE) != RESET)
	{
		// 清除空闲中断标志：先读SR再读DR
		res = USART2->STATR;
		res = USART2->DATAR;
		(void)res; // 避免编译器警告

		// 如果接收到了数据，设置接收完成标志
		if ((AS608_RX_STA & 0x7FFF) > 0)
		{
			AS608_RX_STA |= 0x8000; // 设置接收完成标志
		}
	}
}

void UART5_IRQHandler(void)
{
	RFID_IRQHandler();
}