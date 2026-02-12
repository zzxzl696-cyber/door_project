/********************************** (C) COPYRIGHT *******************************
 * File Name          : by8301.c
 * Author             :
 * Version            : V1.2.0
 * Date               : 2026-01-30
 * Description        : BY8301-16P语音模块驱动实现
 *                      使用UART4进行通信，波特率9600
 *                      协议格式：帧头(0x7E) + 长度 + 命令 + 参数 + 校验(XOR) + 帧尾(0xEF)
 *********************************************************************************/

#include "by8301.h"
#include "debug.h"
#include "bsp_system.h"

// ============== 内部函数声明 ==============
static void BY8301_GPIO_Init(void);
static void BY8301_USART_Init(void);
static void BY8301_SendByte(uint8_t data);
static void BY8301_SendCmd_NoParam(uint8_t cmd);
static void BY8301_SendCmd_1Param(uint8_t cmd, uint8_t param);
static void BY8301_SendCmd_2Param(uint8_t cmd, uint8_t param_h, uint8_t param_l);

/**
 * @brief 初始化BY8301语音模块
 */
void BY8301_Init(void)
{
	BY8301_GPIO_Init();
	BY8301_USART_Init();

	Delay_Ms(500); // 等待模块启动完成（官方建议等待较长时间）

	// 设置默认音量
	BY8301_SetVolume(20);

	// 设置为单曲播放后停止模式
	BY8301_SetLoopMode(BY8301_LOOP_NONE);

	printf("[BY8301] Voice module initialized\r\n");
}

/**
 * @brief GPIO初始化
 */
static void BY8301_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};

	// 使能GPIO时钟
	RCC_APB2PeriphClockCmd(BY8301_GPIO_CLK, ENABLE);

	// PC10: UART4_TX - 复用推挽输出
	GPIO_InitStructure.GPIO_Pin = BY8301_TX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BY8301_GPIO_PORT, &GPIO_InitStructure);

	// PC11: UART4_RX - 浮空输入（用于接收查询响应）
	GPIO_InitStructure.GPIO_Pin = BY8301_RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(BY8301_GPIO_PORT, &GPIO_InitStructure);
}

/**
 * @brief UART4初始化
 */
static void BY8301_USART_Init(void)
{
	USART_InitTypeDef USART_InitStructure = {0};

	// 使能UART4时钟
	RCC_APB1PeriphClockCmd(BY8301_USART_CLK, ENABLE);

	// USART配置：9600, 8N1
	USART_InitStructure.USART_BaudRate = BY8301_BAUDRATE;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_Init(BY8301_USART, &USART_InitStructure);

	// 使能UART4
	USART_Cmd(BY8301_USART, ENABLE);
}

/**
 * @brief 发送单个字节
 */
static void BY8301_SendByte(uint8_t data)
{
	while (USART_GetFlagStatus(BY8301_USART, USART_FLAG_TXE) == RESET)
		;
	USART_SendData(BY8301_USART, data);
	while (USART_GetFlagStatus(BY8301_USART, USART_FLAG_TC) == RESET)
		;
}

/**
 * @brief 发送无参数命令
 * @note 帧格式: 7E 03 CMD XOR EF
 *       长度=3 (长度+命令+校验)
 *       校验=03 XOR CMD
 */
static void BY8301_SendCmd_NoParam(uint8_t cmd)
{
	uint8_t len = 0x03;
	uint8_t checksum = len ^ cmd;

	BY8301_SendByte(BY8301_FRAME_HEAD);  // 帧头 0x7E
	BY8301_SendByte(len);                 // 长度
	BY8301_SendByte(cmd);                 // 命令
	BY8301_SendByte(checksum);            // 校验码(XOR)
	BY8301_SendByte(BY8301_FRAME_END);   // 帧尾 0xEF
}

/**
 * @brief 发送单参数命令
 * @note 帧格式: 7E 04 CMD PARAM XOR EF
 *       长度=4 (长度+命令+参数+校验)
 *       校验=04 XOR CMD XOR PARAM
 */
static void BY8301_SendCmd_1Param(uint8_t cmd, uint8_t param)
{
	uint8_t len = 0x04;
	uint8_t checksum = len ^ cmd ^ param;

	BY8301_SendByte(BY8301_FRAME_HEAD);
	BY8301_SendByte(len);
	BY8301_SendByte(cmd);
	BY8301_SendByte(param);
	BY8301_SendByte(checksum);
	BY8301_SendByte(BY8301_FRAME_END);
}

/**
 * @brief 发送双参数命令
 * @note 帧格式: 7E 05 CMD PARAM_H PARAM_L XOR EF
 *       长度=5 (长度+命令+参数高+参数低+校验)
 *       校验=05 XOR CMD XOR PARAM_H XOR PARAM_L
 */
static void BY8301_SendCmd_2Param(uint8_t cmd, uint8_t param_h, uint8_t param_l)
{
	uint8_t len = 0x05;
	uint8_t checksum = len ^ cmd ^ param_h ^ param_l;

	BY8301_SendByte(BY8301_FRAME_HEAD);
	BY8301_SendByte(len);
	BY8301_SendByte(cmd);
	BY8301_SendByte(param_h);
	BY8301_SendByte(param_l);
	BY8301_SendByte(checksum);
	BY8301_SendByte(BY8301_FRAME_END);
}

/**
 * @brief 播放指定曲目
 * @note 命令: 7E 05 41 HH LL XOR EF
 */
void BY8301_PlayIndex(uint16_t index)
{
	if (index == 0)
		index = 1; // 曲目从1开始
	if (index > 255)
		index = 255; // FLASH最多255首

	BY8301_SendCmd_2Param(BY8301_CMD_PLAY_INDEX,
						  (index >> 8) & 0xFF,
						  index & 0xFF);

	Delay_Ms(20); // 命令间隔至少20ms
}

/**
 * @brief 设置音量
 * @note 命令: 7E 04 31 VOL XOR EF
 */
void BY8301_SetVolume(uint8_t volume)
{
	if (volume > 30)
		volume = 30;

	BY8301_SendCmd_1Param(BY8301_CMD_SET_VOLUME, volume);
	Delay_Ms(20);
}

/**
 * @brief 播放
 * @note 命令: 7E 03 01 02 EF
 */
void BY8301_Play(void)
{
	BY8301_SendCmd_NoParam(BY8301_CMD_PLAY);
	Delay_Ms(20);
}

/**
 * @brief 暂停
 * @note 命令: 7E 03 02 01 EF
 */
void BY8301_Pause(void)
{
	BY8301_SendCmd_NoParam(BY8301_CMD_PAUSE);
	Delay_Ms(20);
}

/**
 * @brief 停止
 * @note 命令: 7E 03 0E 0D EF
 */
void BY8301_Stop(void)
{
	BY8301_SendCmd_NoParam(BY8301_CMD_STOP);
	Delay_Ms(20);
}

/**
 * @brief 上一曲
 * @note 命令: 7E 03 04 07 EF
 */
void BY8301_Prev(void)
{
	BY8301_SendCmd_NoParam(BY8301_CMD_PREV);
	Delay_Ms(20);
}

/**
 * @brief 下一曲
 * @note 命令: 7E 03 03 00 EF
 */
void BY8301_Next(void)
{
	BY8301_SendCmd_NoParam(BY8301_CMD_NEXT);
	Delay_Ms(20);
}

/**
 * @brief 音量加
 * @note 命令: 7E 03 05 06 EF
 */
void BY8301_VolumeUp(void)
{
	BY8301_SendCmd_NoParam(BY8301_CMD_VOL_UP);
	Delay_Ms(20);
}

/**
 * @brief 音量减
 * @note 命令: 7E 03 06 05 EF
 */
void BY8301_VolumeDown(void)
{
	BY8301_SendCmd_NoParam(BY8301_CMD_VOL_DOWN);
	Delay_Ms(20);
}

/**
 * @brief 设置循环模式
 * @note 命令: 7E 04 33 MODE XOR EF
 */
void BY8301_SetLoopMode(uint8_t mode)
{
	if (mode > 4)
		mode = BY8301_LOOP_NONE;

	BY8301_SendCmd_1Param(BY8301_CMD_SET_LOOP_MODE, mode);
	Delay_Ms(20);
}

/**
 * @brief 设置EQ音效
 * @note 命令: 7E 04 32 EQ XOR EF
 */
void BY8301_SetEQ(uint8_t eq)
{
	if (eq > 5)
		eq = BY8301_EQ_NORMAL;

	BY8301_SendCmd_1Param(BY8301_CMD_SET_EQ, eq);
	Delay_Ms(20);
}

/**
 * @brief 插播功能
 * @note 命令: 7E 05 43 HH LL XOR EF
 *       播放完插播曲目后，自动返回原曲目继续播放
 */
void BY8301_InsertPlay(uint16_t index)
{
	if (index == 0)
		index = 1;
	if (index > 255)
		index = 255;

	BY8301_SendCmd_2Param(BY8301_CMD_INSERT_PLAY,
						  (index >> 8) & 0xFF,
						  index & 0xFF);
	Delay_Ms(20);
}

/**
 * @brief 复位模块
 * @note 命令: 7E 03 09 0A EF
 *       所有参数恢复出厂设置（音量最大，回到第一首）
 */
void BY8301_Reset(void)
{
	BY8301_SendCmd_NoParam(BY8301_CMD_RESET);
	Delay_Ms(500); // 复位需要较长等待时间
}

/**
 * @brief 进入/退出待机模式
 * @note 命令: 7E 03 07 04 EF
 *       工作状态发送进入待机，待机状态发送唤醒
 */
void BY8301_Standby(void)
{
	BY8301_SendCmd_NoParam(BY8301_CMD_STANDBY);
	Delay_Ms(20);
}

/**
 * @brief 组合播放多首曲目
 * @note 连续发送选曲命令，间隔小于6ms
 *       播放完所有曲目后停止
 */
void BY8301_PlayCombine(uint16_t *indexes, uint8_t count)
{
	if (count > 10)
		count = 10; // 最多10首

	for (uint8_t i = 0; i < count; i++)
	{
		uint16_t idx = indexes[i];
		if (idx == 0)
			idx = 1;
		if (idx > 255)
			idx = 255;

		BY8301_SendCmd_2Param(BY8301_CMD_PLAY_INDEX,
							  (idx >> 8) & 0xFF,
							  idx & 0xFF);

		// 组合播放命令间隔需要小于6ms
		Delay_Ms(5);
	}
}

/**
 * @brief 查询播放状态
 * @return 0=停止, 1=播放, 2=暂停, 0xFF=超时
 * @note 命令: 7E 03 10 13 EF
 *       返回: OK00XX
 */
uint8_t BY8301_QueryStatus(void)
{
	uint8_t rx_buf[10] = {0};
	uint8_t rx_cnt = 0;
	uint32_t timeout;

	// 清空接收缓冲区
	while (USART_GetFlagStatus(BY8301_USART, USART_FLAG_RXNE) == SET)
	{
		(void)USART_ReceiveData(BY8301_USART);
	}

	// 发送查询命令
	BY8301_SendCmd_NoParam(BY8301_CMD_QUERY_STATUS);

	// 等待响应（超时100ms）
	timeout = 100000;
	while (timeout--)
	{
		if (USART_GetFlagStatus(BY8301_USART, USART_FLAG_RXNE) == SET)
		{
			rx_buf[rx_cnt++] = USART_ReceiveData(BY8301_USART);
			if (rx_cnt >= 6) // "OK00XX" 6字节
				break;
		}
		Delay_Us(1);
	}

	// 解析响应 "OK00XX"
	if (rx_cnt >= 6 && rx_buf[0] == 'O' && rx_buf[1] == 'K')
	{
		// 返回状态值（最后两位十六进制）
		return rx_buf[5] - '0';
	}

	return 0xFF; // 超时或错误
}

/**
 * @brief 查询当前音量
 * @return 音量值 (0-30), 0xFF=超时
 * @note 命令: 7E 03 11 12 EF
 */
uint8_t BY8301_QueryVolume(void)
{
	uint8_t rx_buf[10] = {0};
	uint8_t rx_cnt = 0;
	uint32_t timeout;

	// 清空接收缓冲区
	while (USART_GetFlagStatus(BY8301_USART, USART_FLAG_RXNE) == SET)
	{
		(void)USART_ReceiveData(BY8301_USART);
	}

	// 发送查询命令
	BY8301_SendCmd_NoParam(BY8301_CMD_QUERY_VOLUME);

	// 等待响应
	timeout = 100000;
	while (timeout--)
	{
		if (USART_GetFlagStatus(BY8301_USART, USART_FLAG_RXNE) == SET)
		{
			rx_buf[rx_cnt++] = USART_ReceiveData(BY8301_USART);
			if (rx_cnt >= 6)
				break;
		}
		Delay_Us(1);
	}

	// 解析响应
	if (rx_cnt >= 6 && rx_buf[0] == 'O' && rx_buf[1] == 'K')
	{
		// 将十六进制字符串转换为数值
		uint8_t vol = 0;
		for (uint8_t i = 2; i < 6; i++)
		{
			vol *= 16;
			if (rx_buf[i] >= '0' && rx_buf[i] <= '9')
				vol += rx_buf[i] - '0';
			else if (rx_buf[i] >= 'A' && rx_buf[i] <= 'F')
				vol += rx_buf[i] - 'A' + 10;
			else if (rx_buf[i] >= 'a' && rx_buf[i] <= 'f')
				vol += rx_buf[i] - 'a' + 10;
		}
		return vol;
	}

	return 0xFF;
}
