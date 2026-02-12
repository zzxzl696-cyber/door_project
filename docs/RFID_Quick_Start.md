# RFID读卡器快速入门指南

**文档版本：** V1.0.0
**更新日期：** 2026-01-31
**预计完成时间：** 30分钟

---

## 📋 目录

- [准备工作](#准备工作)
- [硬件连接](#硬件连接)
- [软件配置](#软件配置)
- [调度器集成（推荐）](#调度器集成推荐)
- [第一个程序](#第一个程序)
- [进阶应用](#进阶应用)
- [故障排除](#故障排除)

---

## 🛠️ 准备工作

### 硬件清单

| 物品 | 数量 | 说明 |
|------|------|------|
| CH32V307VCT6开发板 | 1 | 主控芯片 |
| RFID读写器模块 | 1 | XH3650系列 |
| Mifare S50卡片 | 若干 | IC卡 |
| 杜邦线 | 4根 | 连接线 |
| USB数据线 | 1根 | 供电+下载 |
| 5V电源 | 1个 | RFID模块供电（可选） |

### 软件工具

| 软件 | 用途 |
|------|------|
| MounRiver Studio | 编译+下载 |
| WCH-LinkUtility | 调试工具 |
| 串口调试助手 | 查看输出（可选） |

### 前置知识

- [ ] 了解CH32V30x的基本开发流程
- [ ] 熟悉UART串口通信原理
- [ ] 了解Mifare S50卡的基本结构

---

## 🔌 硬件连接

### 步骤1：连接RFID模块

**最小系统连接（推荐）：**

```
┌──────────────────┐         ┌──────────────────┐
│  CH32V307VCT6    │         │  RFID模块        │
│                  │         │  (XH3650-A1)     │
│                  │         │                  │
│  PC12 (RX) ●─────┼─────────┼──● TXD          │
│                  │         │                  │
│  PD2  (TX) ●─────┼─────────┼──● RXD          │
│                  │         │                  │
│  GND       ●─────┼─────────┼──● GND          │
│                  │         │                  │
│                  │         │  VCC ●           │
│                  │         │   │              │
└──────────────────┘         └───┼──────────────┘
                                 │
                             ┌───┴───┐
                             │ 5V电源 │
                             └───────┘
```

**连接步骤：**

1. **连接GND（最重要）**
   - RFID模块GND → CH32V30x GND
   - 公共地必须连接

2. **连接数据线**
   - RFID模块TXD → CH32V30x PC12 (UART5_RX)
   - RFID模块RXD → CH32V30x PD2  (UART5_TX)
   - ⚠️ **注意不要接反！**

3. **连接电源**
   - RFID模块VCC → 5V电源
   - ⚠️ **不要连接3.3V，必须5V！**

4. **检查连接**
   - 用万用表测量5V电压
   - 确认GND连接良好
   - 检查TXD/RXD是否接反

### 步骤2：硬件测试

**测试电源：**
```bash
万用表测量：
- RFID模块VCC：应为 4.75V ~ 5.25V
- CH32V30x VDD：应为 3.2V ~ 3.4V
```

**测试LED指示：**
- 上电后RFID模块LED应该点亮或闪烁
- 部分模块蜂鸣器会短鸣一声

---

## 💻 软件配置

### 步骤1：添加驱动文件

在MounRiver Studio中将以下文件添加到工程：

```
door_project/
├── User/
│   ├── rfid_reader.h     ← 添加头文件
│   └── rfid_reader.c     ← 添加源文件
```

### 步骤2：配置编译选项

在`Makefile`或IDE中添加包含路径：

```makefile
INCLUDES += -IUser
```

### 步骤3：配置UART5

驱动已内置UART5初始化，无需额外配置。

**默认配置：**
- 波特率：9600
- 数据位：8
- 停止位：1
- 校验位：无

---

## ⚙️ 调度器集成（推荐）

本项目使用时间片轮询调度器管理所有周期性任务。RFID驱动已经完美集成到调度器框架中。

### 步骤1：添加RFID任务模块

在MounRiver Studio中添加以下文件：

```
door_project/
├── User/
│   ├── rfid_task.h        ← 添加RFID任务头文件
│   └── rfid_task.c        ← 添加RFID任务实现文件
```

### 步骤2：注册RFID任务到调度器

打开`scheduler.c`文件，已自动完成以下配置：

```c
#include "rfid_task.h"  // ✅ 已添加

static task_t scheduler_task[] = {
    {uart_rx_task, 5, 0},      // UART接收任务：5ms周期
    {rfid_task, 5, 0},         // ✅ RFID轮询任务：5ms周期（已添加）
    {matrix_key_scan, 10, 0},  // 矩阵按键扫描：10ms周期
    {door_control_update, 100, 0}, // 门禁状态更新：100ms周期
};
```

### 步骤3：初始化和运行

在`main.c`中：

```c
#include "scheduler.h"
#include "rfid_reader.h"

int main(void)
{
    // 1. 系统初始化
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);

    // 2. 初始化RFID驱动
    RFID_Init();

    // 3. 初始化调度器
    scheduler_init();

    // 4. 进入调度器主循环
    while(1)
    {
        scheduler_run();  // 自动运行所有任务
    }
}
```

### 关键要点

✅ **无需手动调用RFID_Poll()** - 调度器会自动每5ms调用一次
✅ **直接使用所有RFID API** - 如RFID_ReadId()、RFID_WriteBlock()等
✅ **推荐创建应用任务** - 在100ms周期任务中处理RFID业务逻辑

完整示例代码请参考：[rfid_application_example.c](../User/rfid_application_example.c)

---

## 🚀 第一个程序

### 示例1：读取卡号（命令模式）

创建`rfid_test.c`文件：

```c
#include "debug.h"
#include "rfid_reader.h"

int main(void)
{
	uint8_t card_type[2], uid[4];
	RFID_Result ret;

	// 系统初始化
	Delay_Init();
	USART_Printf_Init(115200);  // 调试串口
	printf("\n\n=== RFID读卡器测试 ===\n");

	// 初始化RFID驱动
	RFID_Init(9600);
	printf("RFID驱动初始化完成\n");

	// 查询固件版本（测试连接）
	uint8_t version;
	ret = RFID_QueryVersion(&version, 1000);
	if (ret == RFID_RET_OK)
	{
		printf("RFID固件版本: V%d.%d\n", version >> 4, version & 0x0F);
		printf("硬件连接正常！\n\n");
	}
	else
	{
		printf("RFID连接失败！请检查接线\n");
		while (1);  // 停止运行
	}

	// 主循环：读取卡号
	printf("请将IC卡靠近读卡器...\n");
	while (1)
	{
		ret = RFID_ReadId(card_type, uid, 2000);
		if (ret == RFID_RET_OK)
		{
			printf("\n=== 读卡成功 ===\n");
			printf("卡类型: 0x%02X%02X\n", card_type[0], card_type[1]);
			printf("卡号: %02X %02X %02X %02X\n",
				   uid[0], uid[1], uid[2], uid[3]);
			printf("================\n\n");

			Delay_Ms(1000);  // 防止重复读取
		}
		else if (ret == RFID_RET_TIMEOUT)
		{
			printf(".");  // 无卡
		}

		Delay_Ms(100);
	}
}
```

**编译和下载：**
1. 点击"Build"编译项目
2. 连接WCH-Link调试器
3. 点击"Download"下载到板子

**运行结果：**
```
=== RFID读卡器测试 ===
RFID驱动初始化完成
RFID固件版本: V1.3
硬件连接正常！

请将IC卡靠近读卡器...
.....
=== 读卡成功 ===
卡类型: 0x0400
卡号: 63 EA 01 90
================
```

---

### 示例2：读写数据块

```c
void test_read_write_block(void)
{
	uint8_t data[16];
	uint8_t write_data[16] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
	};
	RFID_Result ret;

	printf("\n=== 读写块8测试 ===\n");

	// 1. 读取原始数据
	printf("读取块8原始数据...\n");
	ret = RFID_ReadBlock(8, data, 1000);
	if (ret == RFID_RET_OK)
	{
		printf("原始数据: ");
		for (int i = 0; i < 16; i++)
		{
			printf("%02X ", data[i]);
		}
		printf("\n");
	}

	// 2. 写入新数据
	printf("写入新数据...\n");
	ret = RFID_WriteBlock(8, write_data, 1000);
	if (ret == RFID_RET_OK)
	{
		printf("写入成功\n");
	}
	else
	{
		printf("写入失败: %d\n", ret);
		return;
	}

	// 3. 读取验证
	printf("读取验证...\n");
	ret = RFID_ReadBlock(8, data, 1000);
	if (ret == RFID_RET_OK)
	{
		printf("读取数据: ");
		for (int i = 0; i < 16; i++)
		{
			printf("%02X ", data[i]);
		}
		printf("\n");

		// 比较数据
		if (memcmp(data, write_data, 16) == 0)
		{
			printf("✅ 验证通过！\n");
		}
		else
		{
			printf("❌ 验证失败！\n");
		}
	}

	printf("===================\n\n");
}
```

---

### 示例3：自动模式读卡

```c
void test_auto_mode(void)
{
	RFID_Frame frame;
	RFID_Result ret;

	printf("\n=== 自动模式测试 ===\n");

	// 切换到自动读卡号模式
	printf("设置自动模式...\n");
	ret = RFID_SetWorkMode(RFID_MODE_AUTO_ID, 0, 1000);
	if (ret != RFID_RET_OK)
	{
		printf("设置失败\n");
		return;
	}

	printf("等待模块重启...\n");
	Delay_Ms(2000);  // ⚠️ 必须等待！

	// 查询确认
	RFID_WorkMode mode;
	RFID_QueryWorkMode(&mode, NULL, 1000);
	printf("当前模式: %d (2=自动读卡号)\n", mode);

	printf("请靠近卡片，自动读取...\n\n");

	// 等待自动帧
	while (1)
	{
		ret = RFID_WaitAutoFrame(&frame, 5000);
		if (ret == RFID_RET_OK)
		{
			printf("=== 自动读取成功 ===\n");
			printf("卡号: %02X %02X %02X %02X\n",
				   frame.uid[0], frame.uid[1],
				   frame.uid[2], frame.uid[3]);
			printf("====================\n\n");

			Delay_Ms(1000);
		}
		else if (ret == RFID_RET_TIMEOUT)
		{
			printf("等待超时，继续等待...\n");
		}
	}
}
```

---

## 🎓 进阶应用

### 应用1：门禁系统（白名单）

```c
// 白名单数据库
typedef struct
{
	uint8_t uid[4];
	char name[20];
} CardInfo;

CardInfo whitelist[] = {
	{{0x63, 0xEA, 0x01, 0x90}, "张三"},
	{{0x12, 0x34, 0x56, 0x78}, "李四"},
	{{0xAB, 0xCD, 0xEF, 0x01}, "王五"}
};

void door_access_control(void)
{
	uint8_t card_type[2], uid[4];
	RFID_Result ret;
	int i;

	while (1)
	{
		ret = RFID_ReadId(card_type, uid, 2000);
		if (ret == RFID_RET_OK)
		{
			// 查找白名单
			for (i = 0; i < sizeof(whitelist) / sizeof(CardInfo); i++)
			{
				if (memcmp(uid, whitelist[i].uid, 4) == 0)
				{
					printf("✅ 欢迎 %s\n", whitelist[i].name);
					// 开门
					Door_Open();
					Delay_Ms(3000);
					Door_Close();
					break;
				}
			}

			if (i >= sizeof(whitelist) / sizeof(CardInfo))
			{
				printf("❌ 无权限\n");
				Beep_Warning();
			}

			Delay_Ms(1000);
		}
	}
}
```

---

### 应用2：考勤系统

```c
void attendance_system(void)
{
	RFID_Frame frame;

	// 设置自动模式
	RFID_SetWorkMode(RFID_MODE_AUTO_ID, 0, 1000);
	Delay_Ms(2000);

	printf("=== 考勤系统 ===\n");
	printf("请刷卡打卡...\n\n");

	while (1)
	{
		if (RFID_WaitAutoFrame(&frame, 5000) == RFID_RET_OK)
		{
			// 获取当前时间
			RTC_DateTimeTypeDef datetime;
			RTC_GetDateTime(&datetime);

			// 记录打卡
			printf("[%02d:%02d:%02d] 员工卡号: %02X%02X%02X%02X 打卡成功\n",
				   datetime.hour, datetime.minute, datetime.second,
				   frame.uid[0], frame.uid[1],
				   frame.uid[2], frame.uid[3]);

			// 保存到数据库或SD卡
			SaveAttendanceRecord(frame.uid, &datetime);

			Beep_OK();
			Delay_Ms(1000);
		}
	}
}
```

---

### 应用3：电子钱包

```c
void electronic_wallet(void)
{
	uint8_t card_type[2], uid[4];
	uint32_t balance;
	RFID_Result ret;

	printf("=== 电子钱包 ===\n");
	printf("请刷卡...\n");

	// 读卡号
	ret = RFID_ReadId(card_type, uid, 3000);
	if (ret != RFID_RET_OK)
	{
		printf("读卡失败\n");
		return;
	}

	printf("卡号: %02X%02X%02X%02X\n",
		   uid[0], uid[1], uid[2], uid[3]);

	// 查询余额
	ret = RFID_QueryWallet(4, &balance, 1000);
	if (ret == RFID_RET_OK)
	{
		printf("当前余额: %d 元\n", balance);
	}
	else
	{
		printf("钱包未初始化，正在初始化...\n");
		RFID_InitWallet(4, 0, 1000);
		balance = 0;
	}

	// 菜单操作
	while (1)
	{
		printf("\n1. 充值\n");
		printf("2. 消费\n");
		printf("3. 查询余额\n");
		printf("4. 退出\n");
		printf("请选择: ");

		char choice = getchar();

		switch (choice)
		{
			case '1':  // 充值
			{
				printf("充值金额: ");
				uint32_t amount;
				scanf("%d", &amount);

				ret = RFID_IncreaseWallet(4, amount, &balance, 1000);
				if (ret == RFID_RET_OK)
				{
					printf("✅ 充值成功，新余额: %d 元\n", balance);
				}
				break;
			}

			case '2':  // 消费
			{
				printf("消费金额: ");
				uint32_t amount;
				scanf("%d", &amount);

				ret = RFID_DecreaseWallet(4, amount, &balance, 1000);
				if (ret == RFID_RET_OK)
				{
					printf("✅ 消费成功，剩余: %d 元\n", balance);
				}
				else
				{
					printf("❌ 消费失败：余额不足\n");
				}
				break;
			}

			case '3':  // 查询
			{
				RFID_QueryWallet(4, &balance, 1000);
				printf("当前余额: %d 元\n", balance);
				break;
			}

			case '4':  // 退出
				return;
		}
	}
}
```

---

## 🔧 故障排除

### 问题1：RFID连接失败

**症状：** `RFID_QueryVersion()` 返回超时

**排查步骤：**

1. **检查电源：**
   ```bash
   万用表测量：
   - RFID模块VCC：应为 4.75V ~ 5.25V
   - 如果低于4.8V，更换电源
   ```

2. **检查TXD/RXD连接：**
   ```
   正确连接：
   RFID_TXD → CH32_PC12 (UART5_RX)
   RFID_RXD ← CH32_PD2  (UART5_TX)

   如果接反会导致无法通信！
   ```

3. **检查波特率：**
   ```c
   // 确认初始化波特率为9600
   RFID_Init(9600);
   ```

4. **检查GND：**
   - 必须共地，否则电平不稳定

---

### 问题2：读不到卡

**症状：** `RFID_ReadId()` 一直超时

**可能原因：**

1. **卡片未靠近：**
   - 距离太远（>6cm）
   - 解决：卡片靠近天线（2-5cm）

2. **卡片类型不支持：**
   - 只支持ISO14443A（Mifare S50/S70）
   - 解决：使用兼容卡片

3. **电源不足：**
   - 5V电压低于4.8V影响读距
   - 解决：更换大容量电源

4. **天线干扰：**
   - 周围有金属物体
   - 解决：移开金属物体

---

### 问题3：数据读写失败

**症状：** `RFID_ReadBlock()` 返回错误

**可能原因：**

1. **密码错误：**
   ```c
   // 确认已设置正确的密码A
   uint8_t pwd_a[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
   RFID_SetPasswordA(pwd_a, 1000);
   Delay_Ms(2000);
   ```

2. **块号非法：**
   - 不能读写控制块（3/7/11/15...）
   - 不能写入块0（厂商信息）

3. **卡片损坏：**
   - 更换新卡测试

---

### 问题4：自动模式不工作

**症状：** `RFID_WaitAutoFrame()` 一直超时

**排查步骤：**

1. **确认模式切换成功：**
   ```c
   RFID_SetWorkMode(RFID_MODE_AUTO_ID, 0, 1000);
   Delay_Ms(2000);  // ⚠️ 必须等待重启！

   // 查询确认
   RFID_WorkMode mode;
   RFID_QueryWorkMode(&mode, NULL, 1000);
   printf("当前模式: %d\n", mode);  // 应为2
   ```

2. **使用正确的接收函数：**
   ```c
   // 自动模式用这个函数
   RFID_WaitAutoFrame(&frame, 5000);

   // 不要用这个（命令模式）
   // RFID_ReadId(card_type, uid, 1000);  ❌ 错误！
   ```

3. **检查帧类型：**
   - 自动帧的帧头是0x04
   - 命令响应帧的帧头是0x02

---

## 📚 下一步学习

完成快速入门后，建议继续学习：

1. **API参考手册：** `docs/RFID_API_Reference.md`
   - 所有API的详细说明
   - 参数和返回值说明

2. **常见问题解答：** `docs/RFID_FAQ.md`
   - 更多故障排除方法
   - 最佳实践建议

3. **协议手册：** `高频RFID读写器系列协议手册V1.3.pdf`
   - 底层协议细节
   - 帧格式定义

4. **进阶应用：**
   - 多卡防冲突
   - 数据加密
   - 低功耗优化

---

## 💡 小贴士

### 提高读卡成功率

1. **电源稳定：**
   - 使用独立5V电源
   - 添加滤波电容（100μF + 0.1μF）

2. **信号质量：**
   - TXD/RXD走线尽量短（<10cm）
   - 走线较长时串联22Ω电阻

3. **卡片质量：**
   - 使用质量好的原装卡片
   - 避免使用劣质复制卡

### 调试技巧

1. **串口输出：**
   ```c
   printf("RFID: %02X %02X %02X %02X\n",
          uid[0], uid[1], uid[2], uid[3]);
   ```

2. **LED指示：**
   ```c
   if (ret == RFID_RET_OK)
       LED_Green_On();
   else
       LED_Red_On();
   ```

3. **蜂鸣器提示：**
   ```c
   Beep_OK();     // 成功
   Beep_Error();  // 失败
   ```

---

## 📞 技术支持

**遇到问题？**

1. 查看 [常见问题解答](RFID_FAQ.md)
2. 检查硬件连接（参考 [hardware_pinout.md](hardware_pinout.md)）
3. 阅读 [API参考手册](RFID_API_Reference.md)
4. 查看协议手册

**示例代码位置：**
- `User/rfid_example.c` - 完整示例代码
- `docs/` - 所有文档

---

**祝你使用愉快！** 🎉

**文档维护：** 本文档将随功能更新而更新
**最后更新：** 2026-01-31
