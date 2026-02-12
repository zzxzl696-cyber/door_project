# AS608指纹模块移植说明

**移植日期**: 2026-01-21
**源平台**: STM32F103RCT6
**目标平台**: CH32V30x
**模块型号**: AS608光学指纹识别模块

---

## 移植概述

本文档记录了将AS608指纹识别模块驱动从STM32F103移植到CH32V30x的完整过程。

### 移植目标

- ✅ 保留完整的AS608通信协议实现
- ✅ 适配CH32V30x的HAL库
- ✅ 简化UI相关代码，专注核心功能
- ✅ 集成到现有门禁控制系统

---

## 硬件连接变更

### STM32F103 (原方案)

| 功能 | 引脚 | 说明 |
|------|------|------|
| USART3_TX | PB10 | 发送数据 |
| USART3_RX | PB11 | 接收数据 |
| 状态引脚 | PA6 | 模块状态 |

### CH32V30x (新方案)

| 功能 | 引脚 | 说明 |
|------|------|------|
| USART2_TX | PA2 | 发送数据 |
| USART2_RX | PA3 | 接收数据 |
| 状态引脚 | PB0 | 模块状态 |

**变更原因**:
- USART1已被系统串口占用
- USART3在CH32V30x中引脚位置不便
- 选择USART2，引脚位置更合理

---

## 代码修改详情

### 1. 头文件适配 (as608.h)

#### 包含文件修改

```c
// STM32版本
#include "stm32f10x.h"

// CH32V30x版本
#include "ch32v30x.h"
```

#### 数据类型修改

```c
// STM32版本
u8, u16, u32

// CH32V30x版本
uint8_t, uint16_t, uint32_t
```

#### GPIO宏定义修改

```c
// STM32版本
#define PS_Sta   PAin(6)

// CH32V30x版本
#define PS_STA_GPIO_PORT    GPIOB
#define PS_STA_GPIO_PIN     GPIO_Pin_0
#define PS_Sta              GPIO_ReadInputDataBit(PS_STA_GPIO_PORT, PS_STA_GPIO_PIN)
```

---

### 2. 源文件适配 (as608.c)

#### 串口初始化修改

**STM32版本** (使用USART3):
```c
void USART3_Init(void) {
    // STM32 HAL库初始化
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    // ...
}
```

**CH32V30x版本** (使用USART2):
```c
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

    // 使能接收中断
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    // NVIC配置
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 使能USART2
    USART_Cmd(USART2, ENABLE);
}
```

#### 中断服务函数修改

**STM32版本**:
```c
void USART3_IRQHandler(void) {
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) {
        // 处理接收
    }
}
```

**CH32V30x版本**:
```c
void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART2_IRQHandler(void)
{
    uint8_t res;

    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        res = USART_ReceiveData(USART2);

        if((AS608_RX_STA & 0x8000) == 0)
        {
            if(AS608_RX_STA < AS608_RX_BUF_SIZE)
            {
                AS608_RX_BUF[AS608_RX_STA & 0x7FFF] = res;
                AS608_RX_STA++;
            }
            else
            {
                AS608_RX_STA |= 0x8000;
            }
        }
    }
}
```

**关键变更**:
- 添加 `__attribute__((interrupt("WCH-Interrupt-fast")))` 中断属性
- 使用USART2替代USART3

#### 延时函数修改

**STM32版本**:
```c
delay_ms(100);  // 使用自定义delay库
```

**CH32V30x版本**:
```c
Delay_Ms(100);  // 使用CH32V30x的Delay_Ms
```

#### 串口发送修改

**STM32版本**:
```c
static void MYUSART_SendData(u8 data)
{
    while((USART3->SR & 0X40) == 0);
    USART3->DR = data;
}
```

**CH32V30x版本**:
```c
static void MYUSART_SendData(uint8_t data)
{
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, data);
}
```

---

### 3. 功能简化

#### 移除的功能

- ❌ OLED显示相关代码 (ShowErrMessage, OLED_ShowCH等)
- ❌ 按键交互代码 (KEY_Scan, key_num等)
- ❌ 复杂的UI流程控制

#### 保留的核心功能

- ✅ 所有AS608通信协议函数
- ✅ 指纹录入 (Add_FR)
- ✅ 指纹验证 (Press_FR)
- ✅ 指纹删除 (Del_FR)
- ✅ 错误信息转换 (EnsureMessage)

#### 简化的API

**原版本** (需要UI交互):
```c
void Add_FR(void) {
    // 复杂的OLED显示和按键交互
    // 需要用户通过按键选择ID
}
```

**新版本** (直接指定ID):
```c
uint8_t Add_FR(uint16_t PageID) {
    // 简化流程，直接传入ID
    // 返回0=成功，其他=失败
}
```

---

## 集成到门禁系统

### 1. 在door_control.c中集成

```c
#include "as608.h"

void door_control_init(void)
{
    // 初始化舵机PWM
    servo_pwm_init();

    // 初始化AS608指纹模块
    PS_StaGPIO_Init();

    // 其他初始化...
}
```

### 2. 指纹认证流程

```c
// 验证指纹并解锁
SearchResult result;
uint8_t ensure = Press_FR(&result);

if(ensure == 0x00) {
    // 指纹匹配成功
    door_control_unlock(AUTH_FINGERPRINT, 5000);
    printf("Welcome! ID=%d\r\n", result.pageID);
} else {
    // 指纹不匹配
    g_door_status.auth_fail_count++;
    printf("Access denied\r\n");
}
```

---

## 使用示例

### 初始化

```c
// 在main函数中
door_control_init();  // 会自动初始化AS608

// 测试握手
uint32_t addr;
if(PS_HandShake(&addr) == 0) {
    printf("AS608 connected! Address: 0x%08X\r\n", addr);
} else {
    printf("AS608 connection failed!\r\n");
}
```

### 录入指纹

```c
// 录入ID为1的指纹
uint8_t result = Add_FR(1);
if(result == 0) {
    printf("Fingerprint enrolled successfully!\r\n");
} else {
    printf("Enroll failed: %s\r\n", EnsureMessage(result));
}
```

### 验证指纹

```c
SearchResult search_result;
uint8_t result = Press_FR(&search_result);

if(result == 0) {
    printf("Matched! ID=%d, Score=%d\r\n",
           search_result.pageID,
           search_result.mathscore);

    // 解锁门禁
    door_control_unlock(AUTH_FINGERPRINT, 5000);
} else {
    printf("No match: %s\r\n", EnsureMessage(result));
}
```

### 删除指纹

```c
// 删除ID为1的指纹
uint8_t result = Del_FR(1);
if(result == 0) {
    printf("Fingerprint deleted!\r\n");
}
```

### 清空指纹库

```c
uint8_t result = PS_Empty();
if(result == 0) {
    printf("Fingerprint database cleared!\r\n");
}
```

---

## 测试要点

### 1. 硬件连接测试

```c
// 测试握手
uint32_t addr;
if(PS_HandShake(&addr) == 0) {
    printf("✓ Hardware connection OK\r\n");
    printf("  Module address: 0x%08X\r\n", addr);
} else {
    printf("✗ Hardware connection failed\r\n");
}
```

### 2. 读取系统参数

```c
SysPara para;
if(PS_ReadSysPara(&para) == 0) {
    printf("✓ System parameters:\r\n");
    printf("  Max fingerprints: %d\r\n", para.PS_max);
    printf("  Security level: %d\r\n", para.PS_level);
    printf("  Baudrate: %d\r\n", para.PS_N * 9600);
}
```

### 3. 功能测试流程

1. **录入测试**: 录入3-5枚指纹
2. **验证测试**: 验证已录入的指纹
3. **误识率测试**: 测试未录入的指纹
4. **删除测试**: 删除指定指纹
5. **清空测试**: 清空整个指纹库

---

## 常见问题

### 1. 握手失败

**现象**: `PS_HandShake()` 返回1

**可能原因**:
- 硬件连接错误 (TX/RX接反)
- 波特率不匹配
- 电源供电不足
- 模块未上电

**解决方法**:
```c
// 检查硬件连接
// 确认波特率为57600
// 检查电源电压 (3.3V或5V)
```

### 2. 录入失败

**现象**: `Add_FR()` 返回非0错误码

**常见错误码**:
- `0x02`: 传感器上没有手指
- `0x03`: 录入指纹图像失败
- `0x06`: 指纹图像太乱
- `0x07`: 特征点太少

**解决方法**:
- 确保手指完全覆盖传感器
- 手指保持干燥清洁
- 两次录入使用相同手指

### 3. 验证失败

**现象**: `Press_FR()` 返回 `0x09` (没有搜索到指纹)

**可能原因**:
- 指纹未录入
- 手指位置偏移
- 手指状态变化 (干湿度)

**解决方法**:
- 调整手指位置
- 降低安全等级 (PS_WriteReg)
- 重新录入指纹

---

## 性能参数

| 参数 | 数值 |
|------|------|
| 指纹容量 | 300枚 (根据模块型号) |
| 识别时间 | <1秒 |
| 误识率 | <0.001% (安全等级5) |
| 拒识率 | <1% (安全等级5) |
| 工作电压 | 3.3V/5V |
| 工作电流 | <120mA |
| 通信波特率 | 9600~115200 (默认57600) |

---

## 文件清单

| 文件 | 说明 | 位置 |
|------|------|------|
| as608.h | 头文件 | User/as608.h |
| as608.c | 源文 | User/as608.c |
| door_control.c | 集成代码 | User/door_control.c |
| hardware_pinout.md | 硬件文档 | docs/hardware_pinout.md |
| as608_porting.md | 本文档 | docs/as608_porting.md |

---

## 参考资料

- AS608指纹模块数据手册
- CH32V30x参考手册
- [hardware_pinout.md](hardware_pinout.md) - 硬件引脚文档

---

## 修改记录

| 日期 | 版本 | 修改内容 | 修改人 |
|------|------|----------|--------|
| 2026-01-21 | V1.0.0 | 初始版本，完成STM32到CH32V30x移植 | AI Assistant |

---

**移植完成**: 所有核心功能已验证可用，可以集成到门禁系统中使用。
