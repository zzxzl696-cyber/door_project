/********************************** (C) COPYRIGHT *******************************
 * File Name          : as608.c
 * Author             : Ported from STM32 to CH32V30x
 * Version            : V1.0.0
 * Date               : 2026-01-21
 * Description        : AS608指纹识别模块驱动实现
 *                      移植自STM32F103，适配CH32V30x
 *                      使用USART2进行通信
 *********************************************************************************/

#include <string.h>
#include <stdio.h>
#include "as608.h"
#include "debug.h"
#include "bsp_system.h"

// 全局变量
uint32_t AS608Addr = 0xFFFFFFFF; // 默认地址

// USART2接收缓冲区 (用于AS608通信)
#define AS608_RX_BUF_SIZE 256
uint8_t AS608_RX_BUF[AS608_RX_BUF_SIZE]; // 移除static，供中断使用
uint16_t AS608_RX_STA = 0;				 // 移除static，供中断使用

// 内部函数声明
static void AS608_USART_Init(uint32_t baudrate);
static void MYUSART_SendData(uint8_t data);
static uint8_t *JudgeStr(uint16_t waittime);

/**
 * @brief 初始化状态引脚GPIO
 */
void PS_StaGPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};

	// 使能GPIOB时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	// 配置PB0为上拉输入 (状态引脚)
	GPIO_InitStructure.GPIO_Pin = PS_STA_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(PS_STA_GPIO_PORT, &GPIO_InitStructure);

	// 初始化USART2 (用于AS608通信)
	AS608_USART_Init(57600); // AS608默认波特率57600

	printf("[AS608] GPIO and USART2 initialized\r\n");
}

/**
 * @brief 初始化USART2用于AS608通信
 * @param baudrate 波特率
 */
static void AS608_USART_Init(uint32_t baudrate)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};
	USART_InitTypeDef USART_InitStructure = {0};
	NVIC_InitTypeDef NVIC_InitStructure = {0};

	// 使能时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	// PA2: USART2_TX
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// PA3: USART2_RX
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// USART2配置
	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_Init(USART2, &USART_InitStructure);

	// 使能接收中断和空闲中断
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE); // 接收数据中断
	USART_ITConfig(USART2, USART_IT_IDLE, ENABLE); // 空闲中断（一帧数据接收完成）

	// NVIC配置
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	// 使能USART2
	USART_Cmd(USART2, ENABLE);
}

/**
 * @brief 串口发送一个字节
 * @note USART2中断处理函数已移至ch32v30x_it.c
 */
static void MYUSART_SendData(uint8_t data)
{
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET)
		;
	USART_SendData(USART2, data);
}

/**
 * @brief 发送包头
 */
static void SendHead(void)
{
	MYUSART_SendData(0xEF);
	MYUSART_SendData(0x01);
}

/**
 * @brief 发送地址
 */
static void SendAddr(void)
{
	MYUSART_SendData(AS608Addr >> 24);
	MYUSART_SendData(AS608Addr >> 16);
	MYUSART_SendData(AS608Addr >> 8);
	MYUSART_SendData(AS608Addr);
}

/**
 * @brief 发送包标识
 */
static void SendFlag(uint8_t flag)
{
	MYUSART_SendData(flag);
}

/**
 * @brief 发送包长度
 */
static void SendLength(int length)
{
	MYUSART_SendData(length >> 8);
	MYUSART_SendData(length);
}

/**
 * @brief 发送指令码
 */
static void Sendcmd(uint8_t cmd)
{
	MYUSART_SendData(cmd);
}

/**
 * @brief 发送校验和
 */
static void SendCheck(uint16_t check)
{
	MYUSART_SendData(check >> 8);
	MYUSART_SendData(check);
}

/**
 * @brief 在缓冲区中查找AS608应答包头
 * @param buf 缓冲区
 * @param buf_len 缓冲区长度
 * @param pattern 要查找的模式
 * @param pattern_len 模式长度
 * @return 找到返回位置指针，否则返回NULL
 */
static uint8_t *FindPattern(uint8_t *buf, uint16_t buf_len, uint8_t *pattern, uint8_t pattern_len)
{
	if (buf_len < pattern_len)
		return NULL;

	for (uint16_t i = 0; i <= buf_len - pattern_len; i++)
	{
		if (memcmp(&buf[i], pattern, pattern_len) == 0)
		{
			return &buf[i];
		}
	}
	return NULL;
}

/**
 * @brief 判断中断接收到的数据是否有应答包
 * @param waittime 等待时间(单位1ms)
 * @return 数据包首地址
 *
 * @note AS608_RX_STA格式：
 *       bit15: 接收完成标志 (由IDLE空闲中断或缓冲区满时设置)
 *       bit14~0: 接收到的数据长度
 */
static uint8_t *JudgeStr(uint16_t waittime)
{
	uint8_t *data;
	uint8_t str[7];
	uint16_t rx_len;

	// AS608应答包头: 包头(2) + 地址(4) + 包标识(1)
	str[0] = 0xef;
	str[1] = 0x01;
	str[2] = AS608Addr >> 24;
	str[3] = AS608Addr >> 16;
	str[4] = AS608Addr >> 8;
	str[5] = AS608Addr;
	str[6] = 0x07; // 应答包标识

	AS608_RX_STA = 0; // 清零，准备接收新数据

	while (--waittime)
	{
		Delay_Ms(1);

		// 检测接收完成标志（由IDLE中断或缓冲区满时设置）
		if (AS608_RX_STA & 0x8000)
		{
			rx_len = AS608_RX_STA & 0x7FFF; // 获取接收到的数据长度

			// 使用memcmp查找包头，不受0x00字节影响
			data = FindPattern(AS608_RX_BUF, rx_len, str, 7);

			AS608_RX_STA = 0; // 清零以便下次接收

			if (data)
			{
				return data;
			}
		}
	}
	return 0;
}

/**
 * @brief 录入图像 PS_GetImage
 */
uint8_t PS_GetImage(void)
{
	uint16_t temp;
	uint8_t ensure;
	uint8_t *data;

	SendHead();
	SendAddr();
	SendFlag(0x01);
	SendLength(0x03);
	Sendcmd(0x01);
	temp = 0x01 + 0x03 + 0x01;
	SendCheck(temp);

	data = JudgeStr(2000);
	if (data) // 得到正确的应答信号 否则就是失败
		ensure = data[9];
	else
		ensure = 0xff;

	return ensure;
}

/**
 * @brief 生成特征 PS_GenChar
 */
uint8_t PS_GenChar(uint8_t BufferID)
{
	uint16_t temp;
	uint8_t ensure;
	uint8_t *data;

	SendHead();
	SendAddr();
	SendFlag(0x01);
	SendLength(0x04);
	Sendcmd(0x02);
	MYUSART_SendData(BufferID);
	temp = 0x01 + 0x04 + 0x02 + BufferID;
	SendCheck(temp);

	data = JudgeStr(2000);
	if (data)
		ensure = data[9];
	else
		ensure = 0xff;

	return ensure;
}

/**
 * @brief 精确比对两枚指纹特征 PS_Match
 */
uint8_t PS_Match(void)
{
	uint16_t temp;
	uint8_t ensure;
	uint8_t *data;

	SendHead();
	SendAddr();
	SendFlag(0x01);
	SendLength(0x03);
	Sendcmd(0x03);
	temp = 0x01 + 0x03 + 0x03;
	SendCheck(temp);

	data = JudgeStr(2000);
	if (data)
		ensure = data[9];
	else
		ensure = 0xff;

	return ensure;
}

/**
 * @brief 搜索指纹 PS_Search
 */
uint8_t PS_Search(uint8_t BufferID, uint16_t StartPage, uint16_t PageNum, SearchResult *p)
{
	uint16_t temp;
	uint8_t ensure;
	uint8_t *data;

	SendHead();
	SendAddr();
	SendFlag(0x01);
	SendLength(0x08);
	Sendcmd(0x04);
	MYUSART_SendData(BufferID);
	MYUSART_SendData(StartPage >> 8);
	MYUSART_SendData(StartPage);
	MYUSART_SendData(PageNum >> 8);
	MYUSART_SendData(PageNum);
	temp = 0x01 + 0x08 + 0x04 + BufferID + (StartPage >> 8) + (uint8_t)StartPage + (PageNum >> 8) + (uint8_t)PageNum;
	SendCheck(temp);

	data = JudgeStr(2000);
	if (data)
	{
		ensure = data[9];
		p->pageID = (data[10] << 8) + data[11];
		p->mathscore = (data[12] << 8) + data[13];
	}
	else
		ensure = 0xff;

	return ensure;
}

/**
 * @brief 合并特征生成模板 PS_RegModel
 */
uint8_t PS_RegModel(void)
{
	uint16_t temp;
	uint8_t ensure;
	uint8_t *data;

	SendHead();
	SendAddr();
	SendFlag(0x01);
	SendLength(0x03);
	Sendcmd(0x05);
	temp = 0x01 + 0x03 + 0x05;
	SendCheck(temp);

	data = JudgeStr(2000);
	if (data)
		ensure = data[9];
	else
		ensure = 0xff;

	return ensure;
}

/**
 * @brief 储存模板 PS_StoreChar
 */
uint8_t PS_StoreChar(uint8_t BufferID, uint16_t PageID)
{
	uint16_t temp;
	uint8_t ensure;
	uint8_t *data;

	SendHead();
	SendAddr();
	SendFlag(0x01);
	SendLength(0x06);
	Sendcmd(0x06);
	MYUSART_SendData(BufferID);
	MYUSART_SendData(PageID >> 8);
	MYUSART_SendData(PageID);
	temp = 0x01 + 0x06 + 0x06 + BufferID + (PageID >> 8) + (uint8_t)PageID;
	SendCheck(temp);

	data = JudgeStr(2000);
	if (data)
		ensure = data[9];
	else
		ensure = 0xff;

	return ensure;
}

/**
 * @brief 删除模板 PS_DeletChar
 */
uint8_t PS_DeletChar(uint16_t PageID, uint16_t N)
{
	uint16_t temp;
	uint8_t ensure;
	uint8_t *data;

	SendHead();
	SendAddr();
	SendFlag(0x01);
	SendLength(0x07);
	Sendcmd(0x0C);
	MYUSART_SendData(PageID >> 8);
	MYUSART_SendData(PageID);
	MYUSART_SendData(N >> 8);
	MYUSART_SendData(N);
	temp = 0x01 + 0x07 + 0x0C + (PageID >> 8) + (uint8_t)PageID + (N >> 8) + (uint8_t)N;
	SendCheck(temp);

	data = JudgeStr(2000);
	if (data)
		ensure = data[9];
	else
		ensure = 0xff;

	return ensure;
}

/**
 * @brief 清空指纹库 PS_Empty
 */
uint8_t PS_Empty(void)
{
	uint16_t temp;
	uint8_t ensure;
	uint8_t *data;

	SendHead();
	SendAddr();
	SendFlag(0x01);
	SendLength(0x03);
	Sendcmd(0x0D);
	temp = 0x01 + 0x03 + 0x0D;
	SendCheck(temp);

	data = JudgeStr(2000);
	if (data)
		ensure = data[9];
	else
		ensure = 0xff;

	return ensure;
}

/**
 * @brief 读系统基本参数 PS_ReadSysPara
 */
uint8_t PS_ReadSysPara(SysPara *p)
{
	uint16_t temp;
	uint8_t ensure;
	uint8_t *data;

	SendHead();
	SendAddr();
	SendFlag(0x01);
	SendLength(0x03);
	Sendcmd(0x0F);
	temp = 0x01 + 0x03 + 0x0F;
	SendCheck(temp);

	data = JudgeStr(1000);
	if (data)
	{
		ensure = data[9];
		p->PS_max = (data[14] << 8) + data[15];
		p->PS_level = data[17];
		p->PS_addr = (data[18] << 24) + (data[19] << 16) + (data[20] << 8) + data[21];
		p->PS_size = data[23];
		p->PS_N = data[25];
	}
	else
		ensure = 0xff;

	if (ensure == 0x00)
	{
		printf("[AS608] Max fingerprints: %d\r\n", p->PS_max);
		printf("[AS608] Security level: %d\r\n", p->PS_level);
		printf("[AS608] Address: 0x%08X\r\n", p->PS_addr);
		printf("[AS608] Baudrate: %d\r\n", p->PS_N * 9600);
	}

	return ensure;
}

/**
 * @brief 高速搜索 PS_HighSpeedSearch
 */
uint8_t PS_HighSpeedSearch(uint8_t BufferID, uint16_t StartPage, uint16_t PageNum, SearchResult *p)
{
	uint16_t temp;
	uint8_t ensure;
	uint8_t *data;

	SendHead();
	SendAddr();
	SendFlag(0x01);
	SendLength(0x08);
	Sendcmd(0x1b);
	MYUSART_SendData(BufferID);
	MYUSART_SendData(StartPage >> 8);
	MYUSART_SendData(StartPage);
	MYUSART_SendData(PageNum >> 8);
	MYUSART_SendData(PageNum);
	temp = 0x01 + 0x08 + 0x1b + BufferID + (StartPage >> 8) + (uint8_t)StartPage + (PageNum >> 8) + (uint8_t)PageNum;
	SendCheck(temp);

	data = JudgeStr(2000);
	if (data)
	{
		ensure = data[9];
		p->pageID = (data[10] << 8) + data[11];
		p->mathscore = (data[12] << 8) + data[13];
	}
	else
		ensure = 0xff;

	return ensure;
}

/**
 * @brief 读有效模板个数 PS_ValidTempleteNum
 */
uint8_t PS_ValidTempleteNum(uint16_t *ValidN)
{
	uint16_t temp;
	uint8_t ensure;
	uint8_t *data;

	SendHead();
	SendAddr();
	SendFlag(0x01);
	SendLength(0x03);
	Sendcmd(0x1d);
	temp = 0x01 + 0x03 + 0x1d;
	SendCheck(temp);

	data = JudgeStr(2000);
	if (data)
	{
		ensure = data[9];
		*ValidN = (data[10] << 8) + data[11];
	}
	else
		ensure = 0xff;

	if (ensure == 0x00)
	{
		printf("[AS608] Valid fingerprints: %d\r\n", *ValidN);
	}

	return ensure;
}

/**
 * @brief 与AS608握手 PS_HandShake
 */
uint8_t PS_HandShake(uint32_t *PS_Addr)
{
	SendHead();
	SendAddr();
	MYUSART_SendData(0x01);
	MYUSART_SendData(0x00);
	MYUSART_SendData(0x00);
	Delay_Ms(200);

	if (AS608_RX_STA & 0x8000) // 接收到数据
	{
		if (AS608_RX_BUF[0] == 0xEF && AS608_RX_BUF[1] == 0x01 && AS608_RX_BUF[6] == 0x07)
		{
			*PS_Addr = (AS608_RX_BUF[2] << 24) + (AS608_RX_BUF[3] << 16) + (AS608_RX_BUF[4] << 8) + (AS608_RX_BUF[5]);
			AS608_RX_STA = 0;
			return 0;
		}
		AS608_RX_STA = 0;
	}
	return 1;
}

/**
 * @brief 确认码转错误信息
 */
const char *EnsureMessage(uint8_t ensure)
{
	const char *p;
	switch (ensure)
	{
	case 0x00:
		p = "OK";
		break;
	case 0x01:
		p = "Packet receive error";
		break;
	case 0x02:
		p = "No finger detected";
		break;
	case 0x03:
		p = "Failed to enroll finger";
		break;
	case 0x06:
		p = "Image too messy";
		break;
	case 0x07:
		p = "Too few feature points";
		break;
	case 0x08:
		p = "Finger mismatch";
		break;
	case 0x09:
		p = "No matching finger";
		break;
	case 0x0a:
		p = "Feature combine failed";
		break;
	case 0x0b:
		p = "Address out of range";
		break;
	case 0x10:
		p = "Delete template failed";
		break;
	case 0x11:
		p = "Clear database failed";
		break;
	default:
		p = "Unknown error";
		break;
	}
	return p;
}

/**
 * @brief 简化的录入指纹流程
 * @param PageID 指纹ID
 * @return 0=成功, 其他=失败
 */
uint8_t Add_FR(uint16_t PageID)
{
	uint8_t ensure;

	printf("[AS608] Please place finger (1st time)...\r\n");

	// 第一次录入
	while (PS_GetImage() != 0x00)
	{
		Delay_Ms(100);
	}

	ensure = PS_GenChar(CharBuffer1);
	if (ensure != 0x00)
	{
		printf("[AS608] Generate char1 failed: %s\r\n", EnsureMessage(ensure));
		return ensure;
	}

	printf("[AS608] Remove finger and place again...\r\n");
	Delay_Ms(1000);

	// 第二次录入
	while (PS_GetImage() != 0x00)
	{
		Delay_Ms(100);
	}

	ensure = PS_GenChar(CharBuffer2);
	if (ensure != 0x00)
	{
		printf("[AS608] Generate char2 failed: %s\r\n", EnsureMessage(ensure));
		return ensure;
	}

	// 比对
	ensure = PS_Match();
	if (ensure != 0x00)
	{
		printf("[AS608] Match failed: %s\r\n", EnsureMessage(ensure));
		return ensure;
	}

	// 生成模板
	ensure = PS_RegModel();
	if (ensure != 0x00)
	{
		printf("[AS608] Generate model failed: %s\r\n", EnsureMessage(ensure));
		return ensure;
	}

	// 存储模板
	ensure = PS_StoreChar(CharBuffer2, PageID);
	if (ensure != 0x00)
	{
		printf("[AS608] Store failed: %s\r\n", EnsureMessage(ensure));
		return ensure;
	}

	printf("[AS608] Fingerprint enrolled successfully! ID=%d\r\n", PageID);
	return 0;
}

/**
 * @brief 简化的验证指纹流程
 * @param result 搜索结果指针
 * @return 0=成功, 其他=失败
 */
uint8_t Press_FR(SearchResult *result)
{
	uint8_t ensure;

	// 录入图像
	ensure = PS_GetImage();
	if (ensure != 0x00)
	{
		return ensure;
	}

	// 生成特征
	ensure = PS_GenChar(CharBuffer1);
	if (ensure != 0x00)
	{
		return ensure;
	}

	// 高速搜索
	ensure = PS_HighSpeedSearch(CharBuffer1, 0, 99, result);
	if (ensure != 0x00)
	{
		printf("[AS608] Fingerprint not found\r\n");
		return ensure;
	}

	printf("[AS608] Fingerprint matched! ID=%d, Score=%d\r\n",
		   result->pageID, result->mathscore);
	return 0;
}

/**
 * @brief 删除指纹
 * @param PageID 指纹ID
 * @return 0=成功, 其他=失败
 */
uint8_t Del_FR(uint16_t PageID)
{
	uint8_t ensure;

	ensure = PS_DeletChar(PageID, 1);
	if (ensure == 0x00)
	{
		printf("[AS608] Fingerprint deleted! ID=%d\r\n", PageID);
	}
	else
	{
		printf("[AS608] Delete failed: %s\r\n", EnsureMessage(ensure));
	}

	return ensure;
}
