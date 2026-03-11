/********************************** (C) COPYRIGHT *******************************
 * File Name          : rfid_reader.c
 * Author             :
 * Version            : V1.0.0
 * Date               : 2026-01-31
 * Description        : HF RFID reader driver (auto + command modes)
 *********************************************************************************/

#include "bsp_system.h"

// ================= Internal state =================
static volatile uint8_t s_rx_buf[RFID_MAX_FRAME_LEN];
static volatile uint8_t s_rx_len = 0;
static volatile uint8_t s_rx_expected = 0;
static volatile uint8_t s_frame_ready = 0;
static volatile uint32_t s_drop_count = 0;

static uint8_t s_addr = RFID_ADDR_DEFAULT;

// 内部配置缓存
static RFID_Config s_config = {
	.addr = RFID_ADDR_DEFAULT,
	.work_mode = RFID_MODE_COMMAND,
	.auto_block_num = 0,
	.beep = RFID_BEEP_DEFAULT,
	.password_a = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	.password_b = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	.baudrate = RFID_BAUD_9600};

// ================= Internal helpers =================
static void RFID_GPIO_Init(void);
static void RFID_USART_Init(uint32_t baudrate);
static void RFID_SendBytes(const uint8_t *data, uint8_t len);
static void RFID_ResetRxState(void);
static uint8_t RFID_CalcChecksum(const uint8_t *buf, uint8_t len);
static RFID_Result RFID_ParseFrame(const uint8_t *buf, uint8_t len, RFID_Frame *frame);
static RFID_Result RFID_WaitCmdResp(uint8_t cmd, RFID_Frame *frame, uint32_t timeout_ms);
static RFID_Result RFID_SendConfigCommand(uint8_t cmd, const uint8_t *payload, uint8_t payload_len);
static RFID_Result RFID_SendQueryCommand(uint8_t cmd, const uint8_t *payload, uint8_t payload_len);
static RFID_Result RFID_WaitConfigResp(uint8_t cmd, RFID_Frame *frame, uint32_t timeout_ms);
static RFID_Result RFID_WaitQueryResp(uint8_t cmd, RFID_Frame *frame, uint32_t timeout_ms);

void RFID_Init(uint32_t baudrate)
{
	RFID_GPIO_Init();
	RFID_USART_Init(baudrate);
	RFID_ResetRxState();
}

void RFID_SetAddr(uint8_t addr)
{
	s_addr = addr;
}

void RFID_Poll(void)
{
#if !RFID_USE_IRQ
	// 轮询模式：手动检查并接收数据
	while (USART_GetFlagStatus(RFID_USART, USART_FLAG_RXNE) == SET)
	{
		uint8_t byte = (uint8_t)USART_ReceiveData(RFID_USART);
		RFID_OnRxByte(byte);
	}
#endif
	// 中断模式下，数据在 UART5_IRQHandler 中自动接收，此处无需操作
}

void RFID_OnRxByte(uint8_t byte)
{
	if (s_frame_ready != 0)
	{
		s_drop_count++;
		return;
	}

	// Wait for a valid frame header
	if (s_rx_len == 0)
	{
		// 检查是否为有效的帧头（0x01配置, 0x02命令, 0x03查询, 0x04自动）
		if ((byte != RFID_FRAME_HEAD_CONFIG) &&
			(byte != RFID_FRAME_HEAD_CMD) &&
			(byte != RFID_FRAME_HEAD_QUERY) &&
			(byte != RFID_FRAME_HEAD_AUTO))
		{
			return; // 拒绝无效帧头
		}
	}

	if (s_rx_len < RFID_MAX_FRAME_LEN)
	{
		s_rx_buf[s_rx_len++] = byte;
	}
	else
	{
		RFID_ResetRxState();
		return;
	}

	if (s_rx_len == 2)
	{
		s_rx_expected = s_rx_buf[1];
		if (s_rx_expected < 6 || s_rx_expected > RFID_MAX_FRAME_LEN)
		{
			RFID_ResetRxState();
			return;
		}
	}

	if ((s_rx_expected != 0) && (s_rx_len >= s_rx_expected))
	{
		s_frame_ready = 1;
	}
}

RFID_Result RFID_TryGetFrame(RFID_Frame *frame)
{
	RFID_Result ret;
	uint8_t len;
	uint8_t i;

	if (frame == 0)
	{
		return RFID_RET_ERR;
	}

	if (s_frame_ready == 0)
	{
		return RFID_RET_NOFRAME;
	}

	len = s_rx_len;
	if (len > RFID_MAX_FRAME_LEN)
	{
		RFID_ResetRxState();
		return RFID_RET_BAD_FRAME;
	}

	for (i = 0; i < len; i++)
	{
		frame->raw[i] = s_rx_buf[i];
	}
	frame->raw_len = len;

	ret = RFID_ParseFrame(frame->raw, frame->raw_len, frame);

	RFID_ResetRxState();
	return ret;
}

RFID_Result RFID_WaitFrame(RFID_Frame *frame, uint32_t timeout_ms)
{
	uint32_t waited = 0;
	RFID_Result ret;

	if (frame == 0)
	{
		return RFID_RET_ERR;
	}

	while (waited < timeout_ms)
	{
		RFID_Poll();
		ret = RFID_TryGetFrame(frame);
		if (ret != RFID_RET_NOFRAME)
		{
			return ret;
		}
		Delay_Ms(1);
		waited++;
	}

	return RFID_RET_TIMEOUT;
}

uint32_t RFID_GetDropCount(void)
{
	return (uint32_t)s_drop_count;
}

void RFID_ClearDropCount(void)
{
	s_drop_count = 0;
}

RFID_Result RFID_SendCommand(uint8_t cmd, const uint8_t *payload, uint8_t payload_len)
{
	uint8_t frame[RFID_MAX_FRAME_LEN];
	uint8_t len = (uint8_t)(payload_len + 5); // type+len+cmd+addr+payload+checksum

	if (len > RFID_MAX_FRAME_LEN)
	{
		return RFID_RET_BAD_FRAME;
	}

	frame[0] = RFID_FRAME_HEAD_CMD;
	frame[1] = len;
	frame[2] = cmd;
	frame[3] = s_addr;

	if (payload_len > 0)
	{
		uint8_t i;
		if (payload != 0)
		{
			for (i = 0; i < payload_len; i++)
			{
				frame[4 + i] = payload[i];
			}
		}
		else
		{
			for (i = 0; i < payload_len; i++)
			{
				frame[4 + i] = 0x00;
			}
		}
	}

	frame[len - 1] = RFID_CalcChecksum(frame, len);

	RFID_ResetRxState();
	RFID_SendBytes(frame, len);
	return RFID_RET_OK;
}

RFID_Result RFID_ReadId(uint8_t *card_type, uint8_t *uid, uint32_t timeout_ms)
{
	uint8_t payload[3] = {0x00, RFID_BEEP_DEFAULT, 0x00};
	RFID_Frame frame;
	RFID_Result ret;

	ret = RFID_SendCommand(RFID_CMD_READ_ID, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitCmdResp(RFID_CMD_READ_ID, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.type != RFID_FRAME_CMD_RESP || frame.cmd != RFID_CMD_READ_ID || frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	if (card_type != 0)
	{
		card_type[0] = frame.card_type[0];
		card_type[1] = frame.card_type[1];
	}
	if (uid != 0)
	{
		uid[0] = frame.uid[0];
		uid[1] = frame.uid[1];
		uid[2] = frame.uid[2];
		uid[3] = frame.uid[3];
	}
	return RFID_RET_OK;
}

RFID_Result RFID_ReadBlock(uint8_t block, uint8_t *data16, uint32_t timeout_ms)
{
	uint8_t payload[3] = {block, RFID_BEEP_DEFAULT, 0x00};
	RFID_Frame frame;
	RFID_Result ret;

	ret = RFID_SendCommand(RFID_CMD_READ_BLOCK, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitCmdResp(RFID_CMD_READ_BLOCK, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.type != RFID_FRAME_CMD_RESP || frame.cmd != RFID_CMD_READ_BLOCK || frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	if (data16 != 0)
	{
		uint8_t i;
		for (i = 0; i < 16; i++)
		{
			data16[i] = frame.block_data[i];
		}
	}
	return RFID_RET_OK;
}

RFID_Result RFID_WriteBlock(uint8_t block, const uint8_t *data16, uint32_t timeout_ms)
{
	uint8_t payload[18];
	RFID_Frame frame;
	RFID_Result ret;
	uint8_t i;

	payload[0] = block;
	payload[1] = RFID_BEEP_DEFAULT;
	for (i = 0; i < 16; i++)
	{
		payload[2 + i] = (data16 != 0) ? data16[i] : 0x00;
	}

	ret = RFID_SendCommand(RFID_CMD_WRITE_BLOCK, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitCmdResp(RFID_CMD_WRITE_BLOCK, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.type != RFID_FRAME_CMD_RESP || frame.cmd != RFID_CMD_WRITE_BLOCK || frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	return RFID_RET_OK;
}

RFID_Result RFID_InitWallet(uint8_t block, uint32_t value, uint32_t timeout_ms)
{
	uint8_t payload[6];
	RFID_Frame frame;
	RFID_Result ret;

	payload[0] = block;
	payload[1] = RFID_BEEP_DEFAULT;
	payload[2] = (uint8_t)(value & 0xFF);
	payload[3] = (uint8_t)((value >> 8) & 0xFF);
	payload[4] = (uint8_t)((value >> 16) & 0xFF);
	payload[5] = (uint8_t)((value >> 24) & 0xFF);

	ret = RFID_SendCommand(RFID_CMD_INIT_WALLET, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitCmdResp(RFID_CMD_INIT_WALLET, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.type != RFID_FRAME_CMD_RESP || frame.cmd != RFID_CMD_INIT_WALLET || frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}
	return RFID_RET_OK;
}

RFID_Result RFID_IncreaseWallet(uint8_t block, uint32_t inc_value, uint32_t *new_value, uint32_t timeout_ms)
{
	uint8_t payload[6];
	RFID_Frame frame;
	RFID_Result ret;

	payload[0] = block;
	payload[1] = RFID_BEEP_DEFAULT;
	payload[2] = (uint8_t)(inc_value & 0xFF);
	payload[3] = (uint8_t)((inc_value >> 8) & 0xFF);
	payload[4] = (uint8_t)((inc_value >> 16) & 0xFF);
	payload[5] = (uint8_t)((inc_value >> 24) & 0xFF);

	ret = RFID_SendCommand(RFID_CMD_INC_WALLET, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitCmdResp(RFID_CMD_INC_WALLET, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.type != RFID_FRAME_CMD_RESP || frame.cmd != RFID_CMD_INC_WALLET || frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	if (new_value != 0)
	{
		*new_value = ((uint32_t)frame.pvalue[3] << 24) |
					 ((uint32_t)frame.pvalue[2] << 16) |
					 ((uint32_t)frame.pvalue[1] << 8) |
					 ((uint32_t)frame.pvalue[0]);
	}
	return RFID_RET_OK;
}

RFID_Result RFID_DecreaseWallet(uint8_t block, uint32_t dec_value, uint32_t *new_value, uint32_t timeout_ms)
{
	uint8_t payload[6];
	RFID_Frame frame;
	RFID_Result ret;

	payload[0] = block;
	payload[1] = RFID_BEEP_DEFAULT;
	payload[2] = (uint8_t)(dec_value & 0xFF);
	payload[3] = (uint8_t)((dec_value >> 8) & 0xFF);
	payload[4] = (uint8_t)((dec_value >> 16) & 0xFF);
	payload[5] = (uint8_t)((dec_value >> 24) & 0xFF);

	ret = RFID_SendCommand(RFID_CMD_DEC_WALLET, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitCmdResp(RFID_CMD_DEC_WALLET, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.type != RFID_FRAME_CMD_RESP || frame.cmd != RFID_CMD_DEC_WALLET || frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	if (new_value != 0)
	{
		*new_value = ((uint32_t)frame.pvalue[3] << 24) |
					 ((uint32_t)frame.pvalue[2] << 16) |
					 ((uint32_t)frame.pvalue[1] << 8) |
					 ((uint32_t)frame.pvalue[0]);
	}
	return RFID_RET_OK;
}

RFID_Result RFID_QueryWallet(uint8_t block, uint32_t *value, uint32_t timeout_ms)
{
	uint8_t payload[6];
	RFID_Frame frame;
	RFID_Result ret;

	payload[0] = block;
	payload[1] = RFID_BEEP_DEFAULT;
	payload[2] = 0x00;
	payload[3] = 0x00;
	payload[4] = 0x00;
	payload[5] = 0x00;

	ret = RFID_SendCommand(RFID_CMD_QUERY_WALLET, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitCmdResp(RFID_CMD_QUERY_WALLET, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.type != RFID_FRAME_CMD_RESP || frame.cmd != RFID_CMD_QUERY_WALLET || frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	if (value != 0)
	{
		*value = ((uint32_t)frame.pvalue[3] << 24) |
				 ((uint32_t)frame.pvalue[2] << 16) |
				 ((uint32_t)frame.pvalue[1] << 8) |
				 ((uint32_t)frame.pvalue[0]);
	}
	return RFID_RET_OK;
}

#if RFID_USE_IRQ
void RFID_IRQHandler(void)
{
	uint8_t res;
	if (USART_GetITStatus(RFID_USART, USART_IT_RXNE) != RESET)
	{
		res = (uint8_t)USART_ReceiveData(RFID_USART);
		RFID_OnRxByte(res);
	}
}

// // UART5 中断入口函数
// void UART5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
// void UART5_IRQHandler(void)
// {
// 	RFID_IRQHandler();
// }
#endif

// ================= Internal helpers =================
static void RFID_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};

	RCC_APB2PeriphClockCmd(RFID_TX_GPIO_CLK, ENABLE);
	RCC_APB2PeriphClockCmd(RFID_RX_GPIO_CLK, ENABLE);

	GPIO_InitStructure.GPIO_Pin = RFID_TX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(RFID_TX_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = RFID_RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(RFID_RX_GPIO_PORT, &GPIO_InitStructure);
}

static void RFID_USART_Init(uint32_t baudrate)
{
	USART_InitTypeDef USART_InitStructure = {0};

	RFID_USART_RCC_CMD(RFID_USART_CLK, ENABLE);

	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_Init(RFID_USART, &USART_InitStructure);

#if RFID_USE_IRQ
	{
		NVIC_InitTypeDef NVIC_InitStructure = {0};
		NVIC_InitStructure.NVIC_IRQChannel = UART5_IRQn;
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&NVIC_InitStructure);

		USART_ITConfig(RFID_USART, USART_IT_RXNE, ENABLE);
	}
#endif

	USART_Cmd(RFID_USART, ENABLE);
}

static void RFID_SendBytes(const uint8_t *data, uint8_t len)
{
	uint8_t i;
	for (i = 0; i < len; i++)
	{
		while (USART_GetFlagStatus(RFID_USART, USART_FLAG_TXE) == RESET)
		{
		}
		USART_SendData(RFID_USART, data[i]);
	}
	while (USART_GetFlagStatus(RFID_USART, USART_FLAG_TC) == RESET)
	{
	}
}

static void RFID_ResetRxState(void)
{
	s_rx_len = 0;
	s_rx_expected = 0;
	s_frame_ready = 0;
}

static uint8_t RFID_CalcChecksum(const uint8_t *buf, uint8_t len)
{
	uint8_t i;
	uint8_t check = 0;
	if (len < 2)
	{
		return 0;
	}
	for (i = 0; i < (uint8_t)(len - 1); i++)
	{
		check ^= buf[i];
	}
	return (uint8_t)(~check);
}

static RFID_Result RFID_ParseFrame(const uint8_t *buf, uint8_t len, RFID_Frame *frame)
{
	uint8_t i;
	uint8_t checksum;

	if (buf == 0 || frame == 0 || len < 6)
	{
		return RFID_RET_BAD_FRAME;
	}

	checksum = RFID_CalcChecksum(buf, len);
	if (checksum != buf[len - 1])
	{
		return RFID_RET_BAD_CHECKSUM;
	}

	frame->type = RFID_FRAME_UNKNOWN;
	frame->addr = buf[3];
	frame->cmd = 0x00;
	frame->status = buf[4];
	frame->data_len = (uint8_t)(len - 6); // remove head/len/cmd/addr/status/checksum

	for (i = 0; i < 2; i++)
	{
		frame->card_type[i] = 0x00;
	}
	for (i = 0; i < 4; i++)
	{
		frame->uid[i] = 0x00;
		frame->pvalue[i] = 0x00;
	}
	for (i = 0; i < 16; i++)
	{
		frame->block_data[i] = 0x00;
	}

	if (buf[0] == RFID_FRAME_HEAD_AUTO)
	{
		if (buf[2] == 0x02 && len >= 12)
		{
			frame->type = RFID_FRAME_AUTO_ID;
			frame->card_type[0] = buf[5];
			frame->card_type[1] = buf[6];
			for (i = 0; i < 4; i++)
			{
				frame->uid[i] = buf[7 + i];
			}
		}
		else if (buf[2] == 0x03 && len >= 22)
		{
			frame->type = RFID_FRAME_AUTO_BLOCK;
			for (i = 0; i < 16; i++)
			{
				frame->block_data[i] = buf[5 + i];
			}
		}
		else if (buf[2] == 0x04 && len >= 28)
		{
			frame->type = RFID_FRAME_AUTO_ID_BLOCK;
			frame->card_type[0] = buf[5];
			frame->card_type[1] = buf[6];
			for (i = 0; i < 4; i++)
			{
				frame->uid[i] = buf[7 + i];
			}
			for (i = 0; i < 16; i++)
			{
				frame->block_data[i] = buf[11 + i];
			}
		}
		else
		{
			frame->type = RFID_FRAME_UNKNOWN;
		}
	}
	else if (buf[0] == RFID_FRAME_HEAD_CMD)
	{
		frame->type = RFID_FRAME_CMD_RESP;
		frame->cmd = buf[2];

		if (buf[2] == RFID_CMD_READ_ID && len >= 12)
		{
			frame->card_type[0] = buf[5];
			frame->card_type[1] = buf[6];
			for (i = 0; i < 4; i++)
			{
				frame->uid[i] = buf[7 + i];
			}
		}
		else if (buf[2] == RFID_CMD_READ_BLOCK && len >= 22)
		{
			for (i = 0; i < 16; i++)
			{
				frame->block_data[i] = buf[5 + i];
			}
		}
		else if ((buf[2] == RFID_CMD_INIT_WALLET ||
				  buf[2] == RFID_CMD_DEC_WALLET ||
				  buf[2] == RFID_CMD_INC_WALLET ||
				  buf[2] == RFID_CMD_QUERY_WALLET) &&
				 len >= 10)
		{
			for (i = 0; i < 4; i++)
			{
				frame->pvalue[i] = buf[5 + i];
			}
		}
	}
	else if (buf[0] == RFID_FRAME_HEAD_CONFIG)
	{
		// 配置命令响应帧（格式与0x02相同）
		frame->type = RFID_FRAME_CMD_RESP;
		frame->cmd = buf[2];
		// 配置响应通常只返回状态字节，无额外数据
	}
	else if (buf[0] == RFID_FRAME_HEAD_QUERY)
	{
		// 查询命令响应帧
		frame->type = RFID_FRAME_CMD_RESP;
		frame->cmd = buf[2];

		// 根据具体查询命令解析数据
		if (buf[2] == 0xC1 && len >= 7) // RFID_CMD_QUERY_WORK_MODE
		{
			// 查询工作模式：响应格式 03 09 C1 30 00 [模式] [块号] 00 [校验]
			frame->block_data[0] = buf[5]; // 工作模式
			frame->block_data[1] = buf[6]; // 块号
		}
		else if (buf[2] == 0xC5 && len >= 6) // RFID_CMD_QUERY_VERSION
		{
			// 查询版本：响应格式 03 08 C5 30 00 [版本] 00 00 [校验]
			frame->block_data[0] = buf[5]; // 版本号
		}
		// 其他查询命令（地址、蜂鸣器、密码等）只返回状态字节，无需特殊解析
	}
	else
	{
		frame->type = RFID_FRAME_UNKNOWN;
	}

	return RFID_RET_OK;
}

static RFID_Result RFID_WaitCmdResp(uint8_t cmd, RFID_Frame *frame, uint32_t timeout_ms)
{
	uint32_t waited = 0;

	if (frame == 0)
	{
		return RFID_RET_ERR;
	}

	while (waited < timeout_ms)
	{
		RFID_Poll();
		RFID_Result ret = RFID_TryGetFrame(frame);
		if (ret == RFID_RET_OK)
		{
			// 增强过滤：忽略自动帧，只接收命令响应帧
			if (frame->type == RFID_FRAME_CMD_RESP && frame->cmd == cmd)
			{
				return RFID_RET_OK;
			}
			// Ignore non-target frames (auto frames, other commands)
		}
		else if (ret != RFID_RET_NOFRAME)
		{
			// Bad frame or checksum: keep waiting until timeout
		}

		Delay_Ms(1);
		waited++;
	}

	return RFID_RET_TIMEOUT;
}

// ============== 辅助函数：发送配置命令（0x01帧头）==============
static RFID_Result RFID_SendConfigCommand(uint8_t cmd, const uint8_t *payload, uint8_t payload_len)
{
	uint8_t frame[RFID_MAX_FRAME_LEN];
	uint8_t len = (uint8_t)(payload_len + 5); // 0x01+len+cmd+addr+payload+checksum

	if (len > RFID_MAX_FRAME_LEN)
	{
		return RFID_RET_BAD_FRAME;
	}

	frame[0] = 0x01; // 配置命令帧头
	frame[1] = len;
	frame[2] = cmd;
	frame[3] = s_addr;

	if (payload_len > 0 && payload != 0)
	{
		uint8_t i;
		for (i = 0; i < payload_len; i++)
		{
			frame[4 + i] = payload[i];
		}
	}

	frame[len - 1] = RFID_CalcChecksum(frame, len);

	RFID_ResetRxState();
	RFID_SendBytes(frame, len);
	return RFID_RET_OK;
}

// ============== 辅助函数：发送查询命令（0x03帧头）==============
static RFID_Result RFID_SendQueryCommand(uint8_t cmd, const uint8_t *payload, uint8_t payload_len)
{
	uint8_t frame[RFID_MAX_FRAME_LEN];
	uint8_t len = (uint8_t)(payload_len + 5); // 0x03+len+cmd+addr+payload+checksum

	if (len > RFID_MAX_FRAME_LEN)
	{
		return RFID_RET_BAD_FRAME;
	}

	frame[0] = 0x03; // 查询命令帧头
	frame[1] = len;
	frame[2] = cmd;
	frame[3] = s_addr;

	if (payload_len > 0 && payload != 0)
	{
		uint8_t i;
		for (i = 0; i < payload_len; i++)
		{
			frame[4 + i] = payload[i];
		}
	}

	frame[len - 1] = RFID_CalcChecksum(frame, len);

	RFID_ResetRxState();
	RFID_SendBytes(frame, len);
	return RFID_RET_OK;
}

// ============== 辅助函数：等待配置命令响应==============
static RFID_Result RFID_WaitConfigResp(uint8_t cmd, RFID_Frame *frame, uint32_t timeout_ms)
{
	uint32_t waited = 0;

	if (frame == 0)
	{
		return RFID_RET_ERR;
	}

	while (waited < timeout_ms)
	{
		RFID_Result ret = RFID_TryGetFrame(frame);
		if (ret == RFID_RET_OK)
		{
			// 配置响应：帧头0x01，命令匹配
			if (frame->raw[0] == 0x01 && frame->cmd == cmd)
			{
				return RFID_RET_OK;
			}
		}
		else if (ret != RFID_RET_NOFRAME)
		{
			// Bad frame or checksum
		}
		Delay_Ms(1);
		waited++;
	}

	return RFID_RET_TIMEOUT;
}

// ============== 辅助函数：等待查询命令响应==============
static RFID_Result RFID_WaitQueryResp(uint8_t cmd, RFID_Frame *frame, uint32_t timeout_ms)
{
	uint32_t waited = 0;

	if (frame == 0)
	{
		return RFID_RET_ERR;
	}

	while (waited < timeout_ms)
	{
		RFID_Poll();
		RFID_Result ret = RFID_TryGetFrame(frame);
		if (ret == RFID_RET_OK)
		{
			// 查询响应：帧头0x03，命令匹配
			if (frame->raw[0] == 0x03 && frame->cmd == cmd)
			{
				return RFID_RET_OK;
			}
		}
		else if (ret != RFID_RET_NOFRAME)
		{
			// Bad frame or checksum
		}

		Delay_Ms(1);
		waited++;
	}

	return RFID_RET_TIMEOUT;
}

// ============== 配置命令实现 ==============

RFID_Result RFID_SetAddress(uint8_t addr, uint32_t timeout_ms)
{
	uint8_t payload[3];
	RFID_Frame frame;
	RFID_Result ret;

	payload[0] = addr;
	payload[1] = 0x00;
	payload[2] = 0x00;

	ret = RFID_SendConfigCommand(0xA0, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitConfigResp(0xA0, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 更新内部缓存
	s_config.addr = addr;
	s_addr = addr;

	return RFID_RET_OK;
}

RFID_Result RFID_SetWorkMode(RFID_WorkMode mode, uint8_t block_num, uint32_t timeout_ms)
{
	uint8_t payload[3];
	RFID_Frame frame;
	RFID_Result ret;

	payload[0] = (uint8_t)mode;
	payload[1] = block_num;
	payload[2] = 0x00;

	ret = RFID_SendConfigCommand(0xA1, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitConfigResp(0xA1, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 更新内部缓存
	s_config.work_mode = mode;
	s_config.auto_block_num = block_num;

	return RFID_RET_OK;
}

RFID_Result RFID_SetBeeper(uint8_t beep, uint32_t timeout_ms)
{
	uint8_t payload[3];
	RFID_Frame frame;
	RFID_Result ret;

	payload[0] = beep;
	payload[1] = 0x00;
	payload[2] = 0x00;

	ret = RFID_SendConfigCommand(0xA2, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitConfigResp(0xA2, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 更新内部缓存
	s_config.beep = beep;

	return RFID_RET_OK;
}

RFID_Result RFID_SetPasswordA(const uint8_t *pwd6, uint32_t timeout_ms)
{
	RFID_Frame frame;
	RFID_Result ret;
	uint8_t i;

	if (pwd6 == 0)
	{
		return RFID_RET_ERR;
	}

	ret = RFID_SendConfigCommand(0xA3, pwd6, 6);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitConfigResp(0xA3, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 更新内部缓存
	for (i = 0; i < 6; i++)
	{
		s_config.password_a[i] = pwd6[i];
	}

	return RFID_RET_OK;
}

RFID_Result RFID_SetPasswordB(const uint8_t *pwd6, uint32_t timeout_ms)
{
	RFID_Frame frame;
	RFID_Result ret;
	uint8_t i;

	if (pwd6 == 0)
	{
		return RFID_RET_ERR;
	}

	ret = RFID_SendConfigCommand(0xA4, pwd6, 6);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitConfigResp(0xA4, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 更新内部缓存
	for (i = 0; i < 6; i++)
	{
		s_config.password_b[i] = pwd6[i];
	}

	return RFID_RET_OK;
}

RFID_Result RFID_SetBaudrate(RFID_Baudrate baud, uint32_t timeout_ms)
{
	uint8_t payload[3];
	RFID_Frame frame;
	RFID_Result ret;

	payload[0] = (uint8_t)baud;
	payload[1] = 0x00;
	payload[2] = 0x00;

	ret = RFID_SendConfigCommand(0xA5, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitConfigResp(0xA5, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 更新内部缓存
	s_config.baudrate = baud;

	return RFID_RET_OK;
}

// ============== 查询命令实现 ==============

RFID_Result RFID_QueryAddress(uint8_t *addr, uint32_t timeout_ms)
{
	uint8_t payload[3] = {0x00, 0x00, 0x00};
	RFID_Frame frame;
	RFID_Result ret;

	ret = RFID_SendQueryCommand(0xC0, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitQueryResp(0xC0, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 响应格式：03 08 C0 00 00 [地址] 00 [校验]
	if (addr != 0)
	{
		*addr = frame.raw[5];
	}

	return RFID_RET_OK;
}

RFID_Result RFID_QueryWorkMode(RFID_WorkMode *mode, uint8_t *block_num, uint32_t timeout_ms)
{
	uint8_t payload[3] = {0x00, 0x00, 0x00};
	RFID_Frame frame;
	RFID_Result ret;

	ret = RFID_SendQueryCommand(0xC1, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitQueryResp(0xC1, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 响应格式：03 09 C1 30 00 [模式] [块号] 00 [校验]
	if (mode != 0)
	{
		*mode = (RFID_WorkMode)frame.raw[5];
	}
	if (block_num != 0)
	{
		*block_num = frame.raw[6];
	}

	return RFID_RET_OK;
}

RFID_Result RFID_QueryBeeper(uint8_t *beep, uint32_t timeout_ms)
{
	uint8_t payload[3] = {0x00, 0x00, 0x00};
	RFID_Frame frame;
	RFID_Result ret;

	ret = RFID_SendQueryCommand(0xC2, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitQueryResp(0xC2, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 响应格式：03 08 C2 30 00 [蜂鸣器] 00 00 [校验]
	if (beep != 0)
	{
		*beep = frame.raw[5];
	}

	return RFID_RET_OK;
}

RFID_Result RFID_QueryPasswordA(uint8_t *pwd6, uint32_t timeout_ms)
{
	uint8_t payload[3] = {0x00, 0x00, 0x00};
	RFID_Frame frame;
	RFID_Result ret;
	uint8_t i;

	if (pwd6 == 0)
	{
		return RFID_RET_ERR;
	}

	ret = RFID_SendQueryCommand(0xC3, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitQueryResp(0xC3, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 响应格式：03 0C C3 30 00 [6字节密码] [校验]
	for (i = 0; i < 6; i++)
	{
		pwd6[i] = frame.raw[5 + i];
	}

	return RFID_RET_OK;
}

RFID_Result RFID_QueryPasswordB(uint8_t *pwd6, uint32_t timeout_ms)
{
	uint8_t payload[3] = {0x00, 0x00, 0x00};
	RFID_Frame frame;
	RFID_Result ret;
	uint8_t i;

	if (pwd6 == 0)
	{
		return RFID_RET_ERR;
	}

	ret = RFID_SendQueryCommand(0xC4, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitQueryResp(0xC4, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 响应格式：03 0C C4 30 00 [6字节密码] [校验]
	for (i = 0; i < 6; i++)
	{
		pwd6[i] = frame.raw[5 + i];
	}

	return RFID_RET_OK;
}

RFID_Result RFID_QueryVersion(uint8_t *version, uint32_t timeout_ms)
{
	uint8_t payload[3] = {0x00, 0x00, 0x00};
	RFID_Frame frame;
	RFID_Result ret;

	if (version == 0)
	{
		return RFID_RET_ERR;
	}

	ret = RFID_SendQueryCommand(0xC5, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitQueryResp(0xC5, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	// 响应格式：03 08 C5 30 00 [版本] 00 00 [校验]
	*version = frame.raw[5];

	return RFID_RET_OK;
}

// ============== 其他命令实现 ==============

RFID_Result RFID_SetSectorPassword(uint8_t control_block, uint32_t timeout_ms)
{
	uint8_t payload[3];
	RFID_Frame frame;
	RFID_Result ret;

	payload[0] = control_block;
	payload[1] = RFID_BEEP_DEFAULT;
	payload[2] = 0x00;

	ret = RFID_SendCommand(0xB3, payload, sizeof(payload));
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	ret = RFID_WaitCmdResp(0xB3, &frame, timeout_ms);
	if (ret != RFID_RET_OK)
	{
		return ret;
	}

	if (frame.type != RFID_FRAME_CMD_RESP || frame.cmd != 0xB3 || frame.status != RFID_STATUS_OK)
	{
		return RFID_RET_ERR;
	}

	return RFID_RET_OK;
}

RFID_Result RFID_SoftReset(void)
{
	uint8_t frame[8];

	frame[0] = 0x05; // 复位命令帧头
	frame[1] = 0x08; // 包长度
	frame[2] = 0xD0; // 复位命令
	frame[3] = s_addr;
	frame[4] = 0x00;
	frame[5] = 0x00;
	frame[6] = 0x00;
	frame[7] = RFID_CalcChecksum(frame, 8);

	RFID_ResetRxState();
	RFID_SendBytes(frame, 8);

	// 复位命令无响应，等待模块重启（约2秒）
	return RFID_RET_OK;
}

// ============== 工作模式管理 ==============

void RFID_GetConfig(RFID_Config *config)
{
	uint8_t i;

	if (config == 0)
	{
		return;
	}

	config->addr = s_config.addr;
	config->work_mode = s_config.work_mode;
	config->auto_block_num = s_config.auto_block_num;
	config->beep = s_config.beep;
	config->baudrate = s_config.baudrate;

	for (i = 0; i < 6; i++)
	{
		config->password_a[i] = s_config.password_a[i];
		config->password_b[i] = s_config.password_b[i];
	}
}

RFID_WorkMode RFID_GetCurrentMode(void)
{
	return s_config.work_mode;
}

// ============== 自动模式增强接口 ==============

RFID_Result RFID_WaitAutoFrame(RFID_Frame *frame, uint32_t timeout_ms)
{
	uint32_t waited = 0;
	RFID_Result ret;

	if (frame == 0)
	{
		return RFID_RET_ERR;
	}

	while (waited < timeout_ms)
	{
		RFID_Poll();
		ret = RFID_TryGetFrame(frame);

		if (ret == RFID_RET_OK)
		{
			// 检查是否为自动模式帧（帧头0x04）
			if (frame->type == RFID_FRAME_AUTO_ID ||
				frame->type == RFID_FRAME_AUTO_BLOCK ||
				frame->type == RFID_FRAME_AUTO_ID_BLOCK)
			{
				return RFID_RET_OK;
			}
			// 非自动帧：忽略，继续等待
		}
		else if (ret != RFID_RET_NOFRAME)
		{
			// 帧错误或校验错误
			return ret;
		}

		Delay_Ms(1);
		waited++;
	}

	return RFID_RET_TIMEOUT;
}

RFID_Result RFID_PollAutoFrame(RFID_Frame *frame)
{
	RFID_Result ret;

	if (frame == 0)
	{
		return RFID_RET_ERR;
	}
	ret = RFID_TryGetFrame(frame);

	if (ret == RFID_RET_OK)
	{
		// 检查是否为自动模式帧
		if (frame->type == RFID_FRAME_AUTO_ID ||
			frame->type == RFID_FRAME_AUTO_BLOCK ||
			frame->type == RFID_FRAME_AUTO_ID_BLOCK)
		{
			return RFID_RET_OK;
		}
		return RFID_RET_BAD_FRAME; // 不是自动帧
	}

	return ret;
}
