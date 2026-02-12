# RFID读卡器API参考手册

**文档版本：** V1.0.0
**更新日期：** 2026-01-31
**适用芯片：** CH32V30x系列
**适用模块：** XH3650-A1/A2/B1/B2/C2系列高频RFID读写器

---

## 📋 目录

- [快速导航](#快速导航)
- [数据类型](#数据类型)
- [初始化函数](#初始化函数)
- [命令模式API](#命令模式api)
- [配置命令API](#配置命令api)
- [查询命令API](#查询命令api)
- [自动模式API](#自动模式api)
- [工作模式管理API](#工作模式管理api)
- [其他命令API](#其他命令api)
- [错误处理](#错误处理)
- [使用示例](#使用示例)

---

## 🚀 快速导航

### 常用函数速查

| 函数 | 功能 | 适用场景 |
|------|------|---------|
| `RFID_Init()` | 初始化RFID驱动 | 系统启动时调用 |
| `RFID_ReadId()` | 命令模式读卡号 | 按需读卡 |
| `RFID_SetWorkMode()` | 设置工作模式 | 切换命令/自动模式 |
| `RFID_WaitAutoFrame()` | 等待自动帧 | 自动模式读卡 |
| `RFID_QueryVersion()` | 查询固件版本 | 硬件测试 |

### API分类索引

**基础操作：**
- [初始化函数](#初始化函数)：`RFID_Init()`
- [轮询函数](#轮询函数)：`RFID_Poll()`, `RFID_TryGetFrame()`

**命令模式（主动控制）：**
- [读卡操作](#读卡操作)：`RFID_ReadId()`, `RFID_ReadBlock()`, `RFID_WriteBlock()`
- [钱包操作](#钱包操作)：`RFID_InitWallet()`, `RFID_IncreaseWallet()`, `RFID_DecreaseWallet()`, `RFID_QueryWallet()`

**配置管理：**
- [配置命令](#配置命令api)：`RFID_SetWorkMode()`, `RFID_SetBeeper()`, `RFID_SetAddress()`等
- [查询命令](#查询命令api)：`RFID_QueryWorkMode()`, `RFID_QueryBeeper()`等

**自动模式（被动接收）：**
- [自动帧接收](#自动模式api)：`RFID_WaitAutoFrame()`, `RFID_PollAutoFrame()`

---

## 📊 数据类型

### 枚举类型

#### RFID_Result - 函数返回值

```c
typedef enum
{
	RFID_RET_OK = 0x00,         // 成功
	RFID_RET_ERR = 0x01,        // 通用错误
	RFID_RET_TIMEOUT = 0x02,    // 超时
	RFID_RET_BAD_FRAME = 0x03,  // 帧格式错误
	RFID_RET_BAD_CHECKSUM = 0x04, // 校验和错误
	RFID_RET_NOFRAME = 0x05,    // 无数据帧
	RFID_RET_BUSY = 0x06        // 忙碌中
} RFID_Result;
```

#### RFID_WorkMode - 工作模式

```c
typedef enum
{
	RFID_MODE_COMMAND = 0x01,      // 命令工作模式（被动）
	RFID_MODE_AUTO_ID = 0x02,      // 自动上传卡号
	RFID_MODE_AUTO_BLOCK = 0x03,   // 自动上传块数据
	RFID_MODE_AUTO_ID_BLOCK = 0x04 // 自动上传卡号+块数据
} RFID_WorkMode;
```

#### RFID_Baudrate - 波特率

```c
typedef enum
{
	RFID_BAUD_4800 = 0x00,
	RFID_BAUD_9600 = 0x01,      // 默认波特率
	RFID_BAUD_14400 = 0x02,
	RFID_BAUD_19200 = 0x03,
	RFID_BAUD_38400 = 0x04,
	RFID_BAUD_57600 = 0x05,
	RFID_BAUD_115200 = 0x06
} RFID_Baudrate;
```

#### RFID_FrameType - 帧类型

```c
typedef enum
{
	RFID_FRAME_CMD_RESP = 0x02,    // 命令响应帧
	RFID_FRAME_AUTO_ID = 0x04,     // 自动上传卡号帧
	RFID_FRAME_AUTO_BLOCK = 0x04,  // 自动上传块数据帧
	RFID_FRAME_AUTO_ID_BLOCK = 0x04 // 自动上传卡号+块数据帧
} RFID_FrameType;
```

### 结构体类型

#### RFID_Frame - 接收帧数据

```c
typedef struct
{
	RFID_FrameType type;        // 帧类型
	uint8_t addr;               // 读卡器地址
	uint8_t cmd;                // 命令码
	uint8_t status;             // 状态码
	uint8_t card_type[2];       // 卡类型（2字节）
	uint8_t uid[4];             // 卡号（4字节）
	uint8_t block_data[16];     // 块数据（16字节）
	uint8_t pvalue[4];          // 钱包值（4字节）
	uint8_t raw[RFID_MAX_FRAME_LEN]; // 原始帧数据
	uint8_t raw_len;            // 原始帧长度
	uint8_t data_len;           // 有效数据长度
} RFID_Frame;
```

#### RFID_Config - 配置结构

```c
typedef struct
{
	uint8_t addr;               // 读卡器地址
	RFID_WorkMode work_mode;    // 当前工作模式
	uint8_t auto_block_num;     // 自动模式读取的块号
	uint8_t beep;               // 蜂鸣器状态 (0x00=关, 0x01=开)
	uint8_t password_a[6];      // 密码A
	uint8_t password_b[6];      // 密码B
	RFID_Baudrate baudrate;     // 波特率
} RFID_Config;
```

---

## 🎯 初始化函数

### RFID_Init()

**功能：** 初始化RFID读卡器驱动

**原型：**
```c
void RFID_Init(uint32_t baudrate);
```

**参数：**
- `baudrate` - 串口波特率（如：9600）

**返回值：** 无

**说明：**
- 初始化UART5硬件（PC12/PD2）
- 设置波特率、数据位、停止位、校验位
- 重置内部接收状态
- **必须在使用任何其他RFID函数之前调用**

**示例：**
```c
void main(void)
{
	Delay_Init();

	// 初始化RFID驱动（波特率9600）
	RFID_Init(9600);

	// ... 其他初始化 ...
}
```

---

### RFID_Poll()

**功能：** 轮询UART接收，处理接收到的数据

**原型：**
```c
void RFID_Poll(void);
```

**参数：** 无

**返回值：** 无

**说明：**
- 处理UART中断接收的数据
- 判断是否收到完整帧
- 在主循环中调用，或在等待响应时调用
- **自动模式必须定期调用此函数**

**示例：**
```c
void main(void)
{
	RFID_Init(9600);

	while (1)
	{
		RFID_Poll();  // 处理RFID接收数据

		// ... 其他任务 ...

		Delay_Ms(10);
	}
}
```

---

### RFID_TryGetFrame()

**功能：** 尝试获取一个完整帧

**原型：**
```c
RFID_Result RFID_TryGetFrame(RFID_Frame *frame);
```

**参数：**
- `frame` - 输出参数，存储帧数据

**返回值：**
- `RFID_RET_OK` - 成功获取帧
- `RFID_RET_NOFRAME` - 无帧可用
- `RFID_RET_BAD_FRAME` - 帧格式错误
- `RFID_RET_BAD_CHECKSUM` - 校验和错误

**说明：**
- 非阻塞函数
- 从接收缓冲区获取一帧数据
- 验证帧格式和校验和
- 解析帧数据到`RFID_Frame`结构

**示例：**
```c
RFID_Frame frame;
RFID_Result ret;

RFID_Poll();
ret = RFID_TryGetFrame(&frame);
if (ret == RFID_RET_OK)
{
	printf("收到帧：类型=0x%02X, 命令=0x%02X\n",
		   frame.type, frame.cmd);
}
```

---

## 📡 命令模式API

### 读卡操作

#### RFID_ReadId()

**功能：** 读取IC卡号（命令模式）

**原型：**
```c
RFID_Result RFID_ReadId(uint8_t *card_type, uint8_t *uid, uint32_t timeout_ms);
```

**参数：**
- `card_type` - 输出参数，卡类型（2字节）
- `uid` - 输出参数，卡号（4字节）
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 成功读取卡号
- `RFID_RET_TIMEOUT` - 超时（未检测到卡或无响应）
- `RFID_RET_ERR` - 其他错误

**说明：**
- 发送读卡号命令（0xB0）
- 等待读卡器响应（阻塞）
- 建议超时时间：1000ms~3000ms
- 卡号格式：小端序（LSB First）

**卡类型：**
- `0x0400` - Mifare S50 (1K)
- `0x0200` - Mifare S70 (4K)
- `0x4400` - Mifare Ultralight

**示例：**
```c
uint8_t card_type[2], uid[4];
RFID_Result ret;

ret = RFID_ReadId(card_type, uid, 2000);
if (ret == RFID_RET_OK)
{
	printf("卡类型: 0x%02X%02X\n", card_type[0], card_type[1]);
	printf("卡号: %02X %02X %02X %02X\n",
		   uid[0], uid[1], uid[2], uid[3]);
}
else
{
	printf("读卡失败: %d\n", ret);
}
```

---

#### RFID_ReadBlock()

**功能：** 读取指定数据块

**原型：**
```c
RFID_Result RFID_ReadBlock(uint8_t block, uint8_t *data, uint32_t timeout_ms);
```

**参数：**
- `block` - 数据块号（0-63）
- `data` - 输出参数，块数据（16字节）
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 成功读取
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 读取失败（密码错误、块号非法等）

**说明：**
- 发送读块命令（0xB1）
- 使用内部配置的密码A进行认证
- **不能读取控制块（块3/7/11/15...）**
- 块号范围：0-63（Mifare S50：0-63，S70：0-255）

**块号说明：**
```
扇区0: 块0(厂商), 块1, 块2, 块3(控制) ← 块3不可读
扇区1: 块4, 块5, 块6, 块7(控制)       ← 块7不可读
扇区2: 块8, 块9, 块10, 块11(控制)     ← 块11不可读
...
```

**示例：**
```c
uint8_t data[16];
RFID_Result ret;

// 读取块8的数据
ret = RFID_ReadBlock(8, data, 1000);
if (ret == RFID_RET_OK)
{
	printf("块8数据: ");
	for (int i = 0; i < 16; i++)
	{
		printf("%02X ", data[i]);
	}
	printf("\n");
}
```

---

#### RFID_WriteBlock()

**功能：** 写入数据到指定块

**原型：**
```c
RFID_Result RFID_WriteBlock(uint8_t block, const uint8_t *data, uint32_t timeout_ms);
```

**参数：**
- `block` - 数据块号（0-63）
- `data` - 写入数据（16字节）
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 成功写入
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 写入失败（密码错误、块号非法等）

**说明：**
- 发送写块命令（0xB2）
- 使用内部配置的密码A进行认证
- **不能写入控制块（块3/7/11/15...）**
- **不能写入块0（厂商信息）**

**⚠️ 警告：**
- 写入错误会导致数据丢失
- 建议先读取验证密码
- 避免写入控制块

**示例：**
```c
uint8_t write_data[16] = {
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF, 0x00
};
RFID_Result ret;

// 写入数据到块8
ret = RFID_WriteBlock(8, write_data, 1000);
if (ret == RFID_RET_OK)
{
	printf("写入成功\n");

	// 读取验证
	uint8_t read_data[16];
	RFID_ReadBlock(8, read_data, 1000);
	if (memcmp(write_data, read_data, 16) == 0)
	{
		printf("数据验证通过\n");
	}
}
```

---

### 钱包操作

#### RFID_InitWallet()

**功能：** 初始化钱包

**原型：**
```c
RFID_Result RFID_InitWallet(uint8_t block, uint32_t value, uint32_t timeout_ms);
```

**参数：**
- `block` - 钱包块号
- `value` - 初始值
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 初始化成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 初始化失败

**说明：**
- 发送初始化钱包命令（0xB4）
- 将指定块格式化为钱包格式
- **首次使用钱包功能必须初始化**
- 初始值范围：-2147483648 ~ 2147483647

**钱包格式：**
```
块格式：[值(4B)][~值(4B)][值(4B)][备份块号][~备份块号][备份块号][~备份块号]
```

**示例：**
```c
// 初始化块4为钱包，初始值100
RFID_Result ret = RFID_InitWallet(4, 100, 1000);
if (ret == RFID_RET_OK)
{
	printf("钱包初始化成功，余额：100\n");
}
```

---

#### RFID_IncreaseWallet()

**功能：** 钱包充值（增值）

**原型：**
```c
RFID_Result RFID_IncreaseWallet(uint8_t block, uint32_t inc_value, uint32_t *new_value, uint32_t timeout_ms);
```

**参数：**
- `block` - 钱包块号
- `inc_value` - 充值金额
- `new_value` - 输出参数，新余额
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 充值成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 充值失败（未初始化、溢出等）

**说明：**
- 发送增值钱包命令（0xB6）
- 自动读取当前余额并增加
- 防溢出检查

**示例：**
```c
uint32_t new_value;
RFID_Result ret;

// 充值30元
ret = RFID_IncreaseWallet(4, 30, &new_value, 1000);
if (ret == RFID_RET_OK)
{
	printf("充值成功，新余额：%d\n", new_value);
}
```

---

#### RFID_DecreaseWallet()

**功能：** 钱包扣款（减值）

**原型：**
```c
RFID_Result RFID_DecreaseWallet(uint8_t block, uint32_t dec_value, uint32_t *new_value, uint32_t timeout_ms);
```

**参数：**
- `block` - 钱包块号
- `dec_value` - 扣款金额
- `new_value` - 输出参数，新余额
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 扣款成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 扣款失败（余额不足、未初始化等）

**说明：**
- 发送减值钱包命令（0xB5）
- 自动读取当前余额并减少
- 防止负数（余额不足会返回错误）

**示例：**
```c
uint32_t new_value;
RFID_Result ret;

// 扣款20元
ret = RFID_DecreaseWallet(4, 20, &new_value, 1000);
if (ret == RFID_RET_OK)
{
	printf("扣款成功，剩余余额：%d\n", new_value);
}
else if (ret == RFID_RET_ERR)
{
	printf("扣款失败：余额不足\n");
}
```

---

#### RFID_QueryWallet()

**功能：** 查询钱包余额

**原型：**
```c
RFID_Result RFID_QueryWallet(uint8_t block, uint32_t *value, uint32_t timeout_ms);
```

**参数：**
- `block` - 钱包块号
- `value` - 输出参数，当前余额
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 查询成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 查询失败

**说明：**
- 发送查询钱包命令（0xB7）
- 读取当前余额
- 不修改钱包数据

**示例：**
```c
uint32_t value;
RFID_Result ret;

ret = RFID_QueryWallet(4, &value, 1000);
if (ret == RFID_RET_OK)
{
	printf("当前余额：%d 元\n", value);
}
```

---

## ⚙️ 配置命令API

### RFID_SetWorkMode()

**功能：** 设置工作模式

**原型：**
```c
RFID_Result RFID_SetWorkMode(RFID_WorkMode mode, uint8_t block_num, uint32_t timeout_ms);
```

**参数：**
- `mode` - 工作模式（见下表）
- `block_num` - 自动模式读取的块号（仅自动模式有效）
- `timeout_ms` - 超时时间（毫秒）

**工作模式：**
| 模式 | 说明 | block_num |
|------|------|-----------|
| `RFID_MODE_COMMAND` | 命令模式 | 忽略 |
| `RFID_MODE_AUTO_ID` | 自动上传卡号 | 忽略 |
| `RFID_MODE_AUTO_BLOCK` | 自动上传块数据 | 块号(0-63) |
| `RFID_MODE_AUTO_ID_BLOCK` | 自动上传卡号+块数据 | 块号(0-63) |

**返回值：**
- `RFID_RET_OK` - 设置成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 设置失败

**说明：**
- 发送设置工作模式命令（0xA1）
- **设置成功后读卡器会自动重启（约2秒）**
- **必须等待重启完成后才能继续通信**

**⚠️ 重要：**
```c
RFID_SetWorkMode(RFID_MODE_AUTO_ID, 0, 1000);
Delay_Ms(2000);  // ← 必须等待重启！
// 此后才能继续通信
```

**示例：**
```c
// 切换到命令模式
RFID_SetWorkMode(RFID_MODE_COMMAND, 0, 1000);
Delay_Ms(2000);

// 切换到自动读卡号模式
RFID_SetWorkMode(RFID_MODE_AUTO_ID, 0, 1000);
Delay_Ms(2000);

// 切换到自动读卡号+块8模式
RFID_SetWorkMode(RFID_MODE_AUTO_ID_BLOCK, 8, 1000);
Delay_Ms(2000);
```

---

### RFID_SetAddress()

**功能：** 设置读卡器地址

**原型：**
```c
RFID_Result RFID_SetAddress(uint8_t addr, uint32_t timeout_ms);
```

**参数：**
- `addr` - 新地址（0x01~0xFF）
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 设置成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 设置失败

**说明：**
- 发送设置地址命令（0xA0）
- 用于RS485组网时区分多个读卡器
- **设置后读卡器会自动重启**
- 默认地址：0x30

**⚠️ 警告：**
- RS485网络中地址必须唯一
- 设置前确保只有一个读卡器在线

**示例：**
```c
// 设置地址为0x20
RFID_SetAddress(0x20, 1000);
Delay_Ms(2000);  // 等待重启
```

---

### RFID_SetBeeper()

**功能：** 设置蜂鸣器开关

**原型：**
```c
RFID_Result RFID_SetBeeper(uint8_t beep, uint32_t timeout_ms);
```

**参数：**
- `beep` - 蜂鸣器状态（0x00=关闭，0x01=开启）
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 设置成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 设置失败

**说明：**
- 发送设置蜂鸣器命令（0xA2）
- 控制读卡时蜂鸣器是否鸣叫
- **设置后读卡器会自动重启**

**示例：**
```c
// 关闭蜂鸣器
RFID_SetBeeper(0x00, 1000);
Delay_Ms(2000);

// 开启蜂鸣器
RFID_SetBeeper(0x01, 1000);
Delay_Ms(2000);
```

---

### RFID_SetPasswordA()

**功能：** 设置密码A

**原型：**
```c
RFID_Result RFID_SetPasswordA(const uint8_t *pwd6, uint32_t timeout_ms);
```

**参数：**
- `pwd6` - 密码A（6字节）
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 设置成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 设置失败

**说明：**
- 发送设置密码A命令（0xA3）
- 用于读写卡片时的认证
- **设置后读卡器会自动重启**
- 默认密码：`FF FF FF FF FF FF`

**示例：**
```c
uint8_t pwd_a[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
RFID_SetPasswordA(pwd_a, 1000);
Delay_Ms(2000);
```

---

### RFID_SetPasswordB()

**功能：** 设置密码B

**原型：**
```c
RFID_Result RFID_SetPasswordB(const uint8_t *pwd6, uint32_t timeout_ms);
```

**参数：**
- `pwd6` - 密码B（6字节）
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 设置成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 设置失败

**说明：**
- 发送设置密码B命令（0xA4）
- 用于更改扇区密码时的旧密码验证
- **设置后读卡器会自动重启**

**示例：**
```c
uint8_t pwd_b[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
RFID_SetPasswordB(pwd_b, 1000);
Delay_Ms(2000);
```

---

### RFID_SetBaudrate()

**功能：** 设置波特率

**原型：**
```c
RFID_Result RFID_SetBaudrate(RFID_Baudrate baud, uint32_t timeout_ms);
```

**参数：**
- `baud` - 波特率（见`RFID_Baudrate`枚举）
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 设置成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 设置失败

**说明：**
- 发送设置波特率命令（0xA5）
- **设置后读卡器会自动重启**
- **设置后MCU端也要修改波特率**

**⚠️ 警告：**
- 设置后立即重新初始化UART
- 否则通信会失败

**示例：**
```c
// 设置为115200
RFID_SetBaudrate(RFID_BAUD_115200, 1000);
Delay_Ms(2000);  // 等待重启

// 重新初始化UART
RFID_Init(115200);
```

---

## 🔍 查询命令API

### RFID_QueryWorkMode()

**功能：** 查询当前工作模式

**原型：**
```c
RFID_Result RFID_QueryWorkMode(RFID_WorkMode *mode, uint8_t *block_num, uint32_t timeout_ms);
```

**参数：**
- `mode` - 输出参数，当前工作模式
- `block_num` - 输出参数，自动模式块号
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 查询成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 查询失败

**示例：**
```c
RFID_WorkMode mode;
uint8_t block_num;

RFID_QueryWorkMode(&mode, &block_num, 1000);
printf("工作模式: %d, 块号: %d\n", mode, block_num);
```

---

### RFID_QueryAddress()

**功能：** 查询读卡器地址

**原型：**
```c
RFID_Result RFID_QueryAddress(uint8_t *addr, uint32_t timeout_ms);
```

**参数：**
- `addr` - 输出参数，读卡器地址
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 查询成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 查询失败

**示例：**
```c
uint8_t addr;
RFID_QueryAddress(&addr, 1000);
printf("读卡器地址: 0x%02X\n", addr);
```

---

### RFID_QueryBeeper()

**功能：** 查询蜂鸣器状态

**原型：**
```c
RFID_Result RFID_QueryBeeper(uint8_t *beep, uint32_t timeout_ms);
```

**参数：**
- `beep` - 输出参数，蜂鸣器状态
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 查询成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 查询失败

**示例：**
```c
uint8_t beep;
RFID_QueryBeeper(&beep, 1000);
printf("蜂鸣器: %s\n", beep ? "开启" : "关闭");
```

---

### RFID_QueryVersion()

**功能：** 查询固件版本

**原型：**
```c
RFID_Result RFID_QueryVersion(uint8_t *version, uint32_t timeout_ms);
```

**参数：**
- `version` - 输出参数，固件版本
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 查询成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 查询失败

**版本格式：**
- 高4位：主版本号
- 低4位：次版本号
- 例如：`0x13` = V1.3

**示例：**
```c
uint8_t version;
RFID_QueryVersion(&version, 1000);
printf("固件版本: V%d.%d\n", version >> 4, version & 0x0F);
```

---

### RFID_QueryPasswordA()

**功能：** 查询密码A

**原型：**
```c
RFID_Result RFID_QueryPasswordA(uint8_t *pwd6, uint32_t timeout_ms);
```

**参数：**
- `pwd6` - 输出参数，密码A（6字节）
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 查询成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 查询失败

**示例：**
```c
uint8_t pwd_a[6];
RFID_QueryPasswordA(pwd_a, 1000);
printf("密码A: %02X %02X %02X %02X %02X %02X\n",
	   pwd_a[0], pwd_a[1], pwd_a[2],
	   pwd_a[3], pwd_a[4], pwd_a[5]);
```

---

### RFID_QueryPasswordB()

**功能：** 查询密码B

**原型：**
```c
RFID_Result RFID_QueryPasswordB(uint8_t *pwd6, uint32_t timeout_ms);
```

**参数：**
- `pwd6` - 输出参数，密码B（6字节）
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 查询成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 查询失败

**示例：**
```c
uint8_t pwd_b[6];
RFID_QueryPasswordB(pwd_b, 1000);
```

---

## 🤖 自动模式API

### RFID_WaitAutoFrame()

**功能：** 阻塞等待自动帧

**原型：**
```c
RFID_Result RFID_WaitAutoFrame(RFID_Frame *frame, uint32_t timeout_ms);
```

**参数：**
- `frame` - 输出参数，自动帧数据
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 成功接收自动帧
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_BAD_FRAME` - 帧格式错误

**说明：**
- **只能在自动模式下使用**
- 阻塞等待直到收到自动帧或超时
- 自动过滤命令响应帧
- 建议超时时间：3000ms~5000ms

**示例：**
```c
// 设置自动读卡号模式
RFID_SetWorkMode(RFID_MODE_AUTO_ID, 0, 1000);
Delay_Ms(2000);

// 等待卡片靠近
RFID_Frame frame;
while (1)
{
	if (RFID_WaitAutoFrame(&frame, 5000) == RFID_RET_OK)
	{
		printf("卡号: %02X %02X %02X %02X\n",
			   frame.uid[0], frame.uid[1],
			   frame.uid[2], frame.uid[3]);
	}
}
```

---

### RFID_PollAutoFrame()

**功能：** 非阻塞轮询自动帧

**原型：**
```c
RFID_Result RFID_PollAutoFrame(RFID_Frame *frame);
```

**参数：**
- `frame` - 输出参数，自动帧数据

**返回值：**
- `RFID_RET_OK` - 成功接收自动帧
- `RFID_RET_NOFRAME` - 无自动帧可用
- `RFID_RET_BAD_FRAME` - 帧格式错误

**说明：**
- **只能在自动模式下使用**
- 非阻塞，立即返回
- 适合在主循环中轮询
- 可以做其他任务

**示例：**
```c
// 设置自动读卡号模式
RFID_SetWorkMode(RFID_MODE_AUTO_ID, 0, 1000);
Delay_Ms(2000);

// 主循环中轮询
while (1)
{
	RFID_Frame frame;
	if (RFID_PollAutoFrame(&frame) == RFID_RET_OK)
	{
		printf("检测到卡片!\n");
	}

	// 可以做其他事情
	LED_Toggle();
	Delay_Ms(10);
}
```

---

## 🛠️ 工作模式管理API

### RFID_GetConfig()

**功能：** 获取内部配置缓存

**原型：**
```c
void RFID_GetConfig(RFID_Config *config);
```

**参数：**
- `config` - 输出参数，配置数据

**返回值：** 无

**说明：**
- 获取驱动内部缓存的配置
- 不发送查询命令
- 快速获取当前配置

**示例：**
```c
RFID_Config config;
RFID_GetConfig(&config);

printf("地址: 0x%02X\n", config.addr);
printf("工作模式: %d\n", config.work_mode);
printf("蜂鸣器: %s\n", config.beep ? "开启" : "关闭");
```

---

### RFID_GetCurrentMode()

**功能：** 获取当前工作模式

**原型：**
```c
RFID_WorkMode RFID_GetCurrentMode(void);
```

**参数：** 无

**返回值：** 当前工作模式

**说明：**
- 从内部缓存获取
- 不发送查询命令
- 快速获取当前模式

**示例：**
```c
RFID_WorkMode mode = RFID_GetCurrentMode();
if (mode == RFID_MODE_COMMAND)
{
	printf("当前为命令模式\n");
}
else
{
	printf("当前为自动模式\n");
}
```

---

## 🔧 其他命令API

### RFID_SetSectorPassword()

**功能：** 设置扇区密码

**原型：**
```c
RFID_Result RFID_SetSectorPassword(uint8_t control_block, uint32_t timeout_ms);
```

**参数：**
- `control_block` - 扇区控制块号（3/7/11/15...）
- `timeout_ms` - 超时时间（毫秒）

**返回值：**
- `RFID_RET_OK` - 设置成功
- `RFID_RET_TIMEOUT` - 超时
- `RFID_RET_ERR` - 设置失败

**说明：**
- 使用内部密码A设置扇区密码
- 用于修改卡片扇区的密码

**⚠️ 警告：**
- 密码设置错误会导致扇区永久锁死
- 建议使用测试卡测试

**示例：**
```c
// 设置扇区1的密码（控制块7）
RFID_SetSectorPassword(7, 1000);
```

---

### RFID_SoftReset()

**功能：** 软件复位读卡器

**原型：**
```c
RFID_Result RFID_SoftReset(void);
```

**参数：** 无

**返回值：**
- `RFID_RET_OK` - 命令发送成功

**说明：**
- 发送复位命令（0xD0）
- 读卡器将重启（约2秒）
- **无响应**，需等待重启完成

**示例：**
```c
RFID_SoftReset();
Delay_Ms(2000);  // 等待重启
```

---

## ❌ 错误处理

### 常见错误码

| 错误码 | 说明 | 可能原因 |
|-------|------|---------|
| `RFID_RET_TIMEOUT` | 超时 | 无卡、硬件未连接、波特率错误 |
| `RFID_RET_ERR` | 通用错误 | 密码错误、块号非法、操作失败 |
| `RFID_RET_BAD_FRAME` | 帧格式错误 | 干扰、通信故障 |
| `RFID_RET_BAD_CHECKSUM` | 校验和错误 | 干扰、通信故障 |
| `RFID_RET_NOFRAME` | 无数据帧 | 正常（轮询时）|

### 错误处理示例

```c
RFID_Result ret = RFID_ReadId(card_type, uid, 2000);

switch (ret)
{
	case RFID_RET_OK:
		printf("读卡成功\n");
		break;

	case RFID_RET_TIMEOUT:
		printf("超时：请靠近卡片或检查硬件连接\n");
		break;

	case RFID_RET_ERR:
		printf("读卡失败：密码错误或卡片不支持\n");
		break;

	case RFID_RET_BAD_FRAME:
	case RFID_RET_BAD_CHECKSUM:
		printf("通信错误：请检查连线和干扰\n");
		break;

	default:
		printf("未知错误\n");
		break;
}
```

---

## 💡 使用示例

### 示例1：命令模式读卡号

```c
void example_command_mode(void)
{
	uint8_t card_type[2], uid[4];
	RFID_Result ret;

	// 确保处于命令模式
	RFID_SetWorkMode(RFID_MODE_COMMAND, 0, 1000);
	Delay_Ms(2000);

	printf("请将卡片靠近读卡器...\n");

	// 读取卡号
	ret = RFID_ReadId(card_type, uid, 3000);
	if (ret == RFID_RET_OK)
	{
		printf("卡号: %02X %02X %02X %02X\n",
			   uid[0], uid[1], uid[2], uid[3]);
	}
}
```

---

### 示例2：自动模式读卡

```c
void example_auto_mode(void)
{
	RFID_Frame frame;

	// 设置自动读卡号+块8
	RFID_SetWorkMode(RFID_MODE_AUTO_ID_BLOCK, 8, 1000);
	Delay_Ms(2000);

	printf("等待卡片靠近...\n");

	while (1)
	{
		if (RFID_WaitAutoFrame(&frame, 5000) == RFID_RET_OK)
		{
			printf("卡号: %02X %02X %02X %02X\n",
				   frame.uid[0], frame.uid[1],
				   frame.uid[2], frame.uid[3]);

			printf("块8数据: ");
			for (int i = 0; i < 16; i++)
			{
				printf("%02X ", frame.block_data[i]);
			}
			printf("\n");
		}
	}
}
```

---

### 示例3：读写数据块

```c
void example_read_write_block(void)
{
	uint8_t data[16];
	uint8_t write_data[16] = {
		0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x07, 0x08,
		0x09, 0x0A, 0x0B, 0x0C,
		0x0D, 0x0E, 0x0F, 0x10
	};

	// 写入块8
	if (RFID_WriteBlock(8, write_data, 1000) == RFID_RET_OK)
	{
		printf("写入成功\n");

		// 读取验证
		if (RFID_ReadBlock(8, data, 1000) == RFID_RET_OK)
		{
			if (memcmp(data, write_data, 16) == 0)
			{
				printf("验证通过\n");
			}
		}
	}
}
```

---

### 示例4：钱包操作

```c
void example_wallet(void)
{
	uint32_t value;

	// 初始化钱包，初始值100
	RFID_InitWallet(4, 100, 1000);

	// 充值30
	RFID_IncreaseWallet(4, 30, &value, 1000);
	printf("充值后余额: %d\n", value);  // 130

	// 扣款20
	RFID_DecreaseWallet(4, 20, &value, 1000);
	printf("扣款后余额: %d\n", value);  // 110

	// 查询余额
	RFID_QueryWallet(4, &value, 1000);
	printf("当前余额: %d\n", value);    // 110
}
```

---

### 示例5：配置管理

```c
void example_config(void)
{
	uint8_t version;
	RFID_Config config;

	// 查询固件版本
	RFID_QueryVersion(&version, 1000);
	printf("固件版本: V%d.%d\n", version >> 4, version & 0x0F);

	// 关闭蜂鸣器
	RFID_SetBeeper(0x00, 1000);
	Delay_Ms(2000);

	// 获取配置
	RFID_GetConfig(&config);
	printf("蜂鸣器: %s\n", config.beep ? "开启" : "关闭");
}
```

---

## 📚 相关文档

- **硬件连接说明：** `docs/hardware_pinout.md`
- **快速入门指南：** `docs/RFID_Quick_Start.md`
- **常见问题解答：** `docs/RFID_FAQ.md`
- **协议手册：** `高频RFID读写器系列协议手册V1.3.pdf`
- **驱动源代码：** `User/rfid_reader.c`, `User/rfid_reader.h`

---

**文档维护：** 本文档将随API更新而更新
**最后更新：** 2026-01-31
