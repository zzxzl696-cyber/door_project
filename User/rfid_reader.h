/********************************** (C) COPYRIGHT *******************************
 * File Name          : rfid_reader.h
 * Author             :
 * Version            : V1.0.0
 * Date               : 2026-01-31
 * Description        : HF RFID reader driver (auto + command modes)
 *                      Transport: UART
 *********************************************************************************/

#ifndef __RFID_READER_H
#define __RFID_READER_H

#include "ch32v30x.h"
#include "debug.h"

// ================= Hardware configuration (default: UART5 on PC12/PD2) =================
#ifndef RFID_USART
#define RFID_USART UART5
#endif

#ifndef RFID_USART_CLK
#define RFID_USART_CLK RCC_APB1Periph_UART5
#endif

#ifndef RFID_USART_RCC_CMD
#define RFID_USART_RCC_CMD RCC_APB1PeriphClockCmd
#endif

#ifndef RFID_TX_GPIO_CLK
#define RFID_TX_GPIO_CLK RCC_APB2Periph_GPIOC
#endif

#ifndef RFID_TX_GPIO_PORT
#define RFID_TX_GPIO_PORT GPIOC
#endif

#ifndef RFID_TX_PIN
#define RFID_TX_PIN GPIO_Pin_12 // PC12: UART5_TX
#endif

#ifndef RFID_RX_GPIO_CLK
#define RFID_RX_GPIO_CLK RCC_APB2Periph_GPIOD
#endif

#ifndef RFID_RX_GPIO_PORT
#define RFID_RX_GPIO_PORT GPIOD
#endif

#ifndef RFID_RX_PIN
#define RFID_RX_PIN GPIO_Pin_2 // PD2: UART5_RX
#endif

#ifndef RFID_BAUDRATE
#define RFID_BAUDRATE 9600
#endif

// Enable RX interrupt handling inside RFID driver (0=polling, 1=IRQ)
#ifndef RFID_USE_IRQ
#define RFID_USE_IRQ 1
#endif

// ================= Protocol constants =================
// 帧头类型定义（按协议手册顺序）
#define RFID_FRAME_HEAD_CONFIG 0x01 // 配置命令响应帧
#define RFID_FRAME_HEAD_CMD 0x02	// 命令工作模式响应帧
#define RFID_FRAME_HEAD_QUERY 0x03	// 查询命令响应帧
#define RFID_FRAME_HEAD_AUTO 0x04	// 主动工作模式上传帧
// 注：0x05为复位命令，无响应帧

#define RFID_ADDR_DEFAULT 0x30

#define RFID_STATUS_OK 0x00
#define RFID_STATUS_ERR 0x01

#define RFID_CMD_READ_ID 0xB0
#define RFID_CMD_READ_BLOCK 0xB1
#define RFID_CMD_WRITE_BLOCK 0xB2
#define RFID_CMD_INIT_WALLET 0xB4
#define RFID_CMD_DEC_WALLET 0xB5
#define RFID_CMD_INC_WALLET 0xB6
#define RFID_CMD_QUERY_WALLET 0xB7

// 查询命令字节定义
#define RFID_CMD_QUERY_WORK_MODE 0xC1 // 查询工作模式
#define RFID_CMD_QUERY_VERSION 0xC5	  // 查询版本号

#define RFID_BEEP_OFF 0x00
#define RFID_BEEP_ON 0x01

#ifndef RFID_BEEP_DEFAULT
#define RFID_BEEP_DEFAULT RFID_BEEP_ON
#endif

#ifndef RFID_MAX_FRAME_LEN
#define RFID_MAX_FRAME_LEN 64
#endif

// ================= Extended enumerations =================
// 工作模式定义
typedef enum
{
	RFID_MODE_COMMAND = 0x01,	   // 命令工作模式（被动）
	RFID_MODE_AUTO_ID = 0x02,	   // 自动上传卡号
	RFID_MODE_AUTO_BLOCK = 0x03,   // 自动上传块数据
	RFID_MODE_AUTO_ID_BLOCK = 0x04 // 自动上传卡号+块数据
} RFID_WorkMode;

// 波特率定义
typedef enum
{
	RFID_BAUD_4800 = 0x00,
	RFID_BAUD_9600 = 0x01,
	RFID_BAUD_14400 = 0x02,
	RFID_BAUD_19200 = 0x03,
	RFID_BAUD_38400 = 0x04,
	RFID_BAUD_57600 = 0x05,
	RFID_BAUD_115200 = 0x06
} RFID_Baudrate;

typedef enum
{
	RFID_FRAME_NONE = 0,
	RFID_FRAME_AUTO_ID,
	RFID_FRAME_AUTO_BLOCK,
	RFID_FRAME_AUTO_ID_BLOCK,
	RFID_FRAME_CMD_RESP,
	RFID_FRAME_UNKNOWN
} RFID_FrameType;

typedef enum
{
	RFID_RET_OK = 0,
	RFID_RET_ERR = 1,
	RFID_RET_TIMEOUT = 2,
	RFID_RET_NOFRAME = 3,
	RFID_RET_BAD_CHECKSUM = 4,
	RFID_RET_BAD_FRAME = 5,
	RFID_RET_OVERFLOW = 6
} RFID_Result;

typedef struct
{
	RFID_FrameType type;
	uint8_t addr;
	uint8_t cmd;
	uint8_t status;
	uint8_t card_type[2];
	uint8_t uid[4];
	uint8_t block_data[16];
	uint8_t pvalue[4];
	uint8_t raw[RFID_MAX_FRAME_LEN];
	uint8_t raw_len;
	uint8_t data_len;
} RFID_Frame;

// 读卡器配置结构
typedef struct
{
	uint8_t addr;			 // 读卡器地址
	RFID_WorkMode work_mode; // 当前工作模式
	uint8_t auto_block_num;	 // 自动模式读取的块号
	uint8_t beep;			 // 蜂鸣器状态 (0x00=关, 0x01=开)
	uint8_t password_a[6];	 // 密码A
	uint8_t password_b[6];	 // 密码B
	RFID_Baudrate baudrate;	 // 波特率
} RFID_Config;

// ================= Public APIs =================
void RFID_Init(uint32_t baudrate);
void RFID_SetAddr(uint8_t addr);
void RFID_Poll(void);
void RFID_OnRxByte(uint8_t byte);
RFID_Result RFID_TryGetFrame(RFID_Frame *frame);
RFID_Result RFID_WaitFrame(RFID_Frame *frame, uint32_t timeout_ms);
uint32_t RFID_GetDropCount(void);
void RFID_ClearDropCount(void);

RFID_Result RFID_SendCommand(uint8_t cmd, const uint8_t *payload, uint8_t payload_len);

RFID_Result RFID_ReadId(uint8_t *card_type, uint8_t *uid, uint32_t timeout_ms);
RFID_Result RFID_ReadBlock(uint8_t block, uint8_t *data16, uint32_t timeout_ms);
RFID_Result RFID_WriteBlock(uint8_t block, const uint8_t *data16, uint32_t timeout_ms);
RFID_Result RFID_InitWallet(uint8_t block, uint32_t value, uint32_t timeout_ms);
RFID_Result RFID_IncreaseWallet(uint8_t block, uint32_t inc_value, uint32_t *new_value, uint32_t timeout_ms);
RFID_Result RFID_DecreaseWallet(uint8_t block, uint32_t dec_value, uint32_t *new_value, uint32_t timeout_ms);
RFID_Result RFID_QueryWallet(uint8_t block, uint32_t *value, uint32_t timeout_ms);

// ============== 配置命令（0x01类型）==============
RFID_Result RFID_SetAddress(uint8_t addr, uint32_t timeout_ms);
RFID_Result RFID_SetWorkMode(RFID_WorkMode mode, uint8_t block_num, uint32_t timeout_ms);
RFID_Result RFID_SetBeeper(uint8_t beep, uint32_t timeout_ms);
RFID_Result RFID_SetPasswordA(const uint8_t *pwd6, uint32_t timeout_ms);
RFID_Result RFID_SetPasswordB(const uint8_t *pwd6, uint32_t timeout_ms);
RFID_Result RFID_SetBaudrate(RFID_Baudrate baud, uint32_t timeout_ms);

// ============== 查询命令（0x03类型）==============
RFID_Result RFID_QueryAddress(uint8_t *addr, uint32_t timeout_ms);
RFID_Result RFID_QueryWorkMode(RFID_WorkMode *mode, uint8_t *block_num, uint32_t timeout_ms);
RFID_Result RFID_QueryBeeper(uint8_t *beep, uint32_t timeout_ms);
RFID_Result RFID_QueryPasswordA(uint8_t *pwd6, uint32_t timeout_ms);
RFID_Result RFID_QueryPasswordB(uint8_t *pwd6, uint32_t timeout_ms);
RFID_Result RFID_QueryVersion(uint8_t *version, uint32_t timeout_ms);

// ============== 其他命令 ==============
RFID_Result RFID_SetSectorPassword(uint8_t control_block, uint32_t timeout_ms);
RFID_Result RFID_SoftReset(void);

// ============== 工作模式管理 ==============
void RFID_GetConfig(RFID_Config *config);
RFID_WorkMode RFID_GetCurrentMode(void);

// ============== 自动模式专用接口 ==============
RFID_Result RFID_WaitAutoFrame(RFID_Frame *frame, uint32_t timeout_ms); // 只是加了一个循环死等
RFID_Result RFID_PollAutoFrame(RFID_Frame *frame);						// 这个可以添加到调度器中 ，但是调度器其实有点问题 需要修改中断的接收 所以这俩个函数都不是很好用

#if RFID_USE_IRQ
void RFID_IRQHandler(void);
#endif

#endif /* __RFID_READER_H */
