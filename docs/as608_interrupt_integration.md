# USART2中断服务函数集成说明

**文件**: ch32v30x_it.c
**功能**: 为AS608指纹模块添加USART2中断支持
**日期**: 2026-01-21

---

## 📝 需要添加的代码

### 1. 在 ch32v30x_it.h 中添加声明

找到 `User/ch32v30x_it.h` 文件，在函数声明区域添加:

```c
void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
```

### 2. 在 ch32v30x_it.c 中添加实现

找到 `User/ch32v30x_it.c` 文件，在文件末尾添加:

```c
/********************************** AS608指纹模块 **********************************/

// USART2接收缓冲区 (与as608.c共享)
extern uint8_t AS608_RX_BUF[];
extern uint16_t AS608_RX_STA;

/**
 * @brief USART2中断服务函数 (AS608指纹模块通信)
 */
void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART2_IRQHandler(void)
{
    uint8_t res;

    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        res = USART_ReceiveData(USART2);

        if((AS608_RX_STA & 0x8000) == 0)  // 接收未完成
        {
            if(AS608_RX_STA < 256)  // 缓冲区未满
            {
                AS608_RX_BUF[AS608_RX_STA & 0x7FFF] = res;
                AS608_RX_STA++;
            }
            else
            {
                AS608_RX_STA |= 0x8000;  // 接收完成标志
            }
        }
    }
}
```

---

## 🔧 修改 as608.c

为了让中断服务函数能访问接收缓冲区，需要修改 `as608.c`:

### 修改前

```c
// USART2接收缓冲区 (用于AS608通信)
#define AS608_RX_BUF_SIZE 256
static uint8_t AS608_RX_BUF[AS608_RX_BUF_SIZE];
static uint16_t AS608_RX_STA = 0;
```

### 修改后

```c
// USART2接收缓冲区 (用于AS608通信)
#define AS608_RX_BUF_SIZE 256
uint8_t AS608_RX_BUF[AS608_RX_BUF_SIZE];  // 移除static，供中断使用
uint16_t AS608_RX_STA = 0;                // 移除static，供中断使用
```

---

## 📋 集成步骤

### 步骤1: 修改 as608.c

1. 打开 `User/as608.c`
2. 找到第15-16行:
   ```c
   static uint8_t AS608_RX_BUF[AS608_RX_BUF_SIZE];
   static uint16_t AS608_RX_STA = 0;
   ```
3. 移除 `static` 关键字:
   ```c
   uint8_t AS608_RX_BUF[AS608_RX_BUF_SIZE];
   uint16_t AS608_RX_STA = 0;
   ```

### 步骤2: 修改 ch32v30x_it.h

1. 打开 `User/ch32v30x_it.h`
2. 在函数声明区域添加:
   ```c
   void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
   ```

### 步骤3: 修改 ch32v30x_it.c

1. 打开 `User/ch32v30x_it.c`
2. 在文件末尾添加上述中断服务函数代码

### 步骤4: 编译测试

```bash
make clean
make
```

检查是否有编译错误。

---

## ✅ 验证方法

### 方法1: 串口输出验证

运行程序后，串口应输出:

```
[AS608] GPIO and USART2 initialized
✓ AS608 connected! Address: 0xFFFFFFFF
```

### 方法2: 握手测试

在main函数中添加:

```c
uint32_t addr;
for(int i = 0; i < 3; i++) {
    printf("Handshake attempt %d...\r\n", i+1);
    if(PS_HandShake(&addr) == 0) {
        printf("✓ Success! Address: 0x%08X\r\n", addr);
        break;
    } else {
        printf("✗ Failed\r\n");
    }
    Delay_Ms(500);
}
```

### 方法3: 中断计数验证

添加调试代码:

```c
// 在中断服务函数中添加计数器
static uint32_t irq_count = 0;
irq_count++;

// 在main函数中定期打印
printf("USART2 IRQ count: %d\r\n", irq_count);
```

---

## ⚠️ 常见问题

### 问题1: 编译错误 "undefined reference to USART2_IRQHandler"

**原因**: 中断服务函数未正确声明或实现

**解决**:
1. 确认 ch32v30x_it.h 中有声明
2. 确认 ch32v30x_it.c 中有实现
3. 确认函数名拼写正确

### 问题2: 中断不触发

**原因**: 中断未使能或优先级配置错误

**解决**:
```c
// 检查NVIC配置
NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
NVIC_Init(&NVIC_InitStructure);

// 检查中断使能
USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
```

### 问题3: 接收数据错误

**原因**: 波特率不匹配或缓冲区溢出

**解决**:
```c
// 确认波特率
printf("USART2 Baudrate: 57600\r\n");

// 增加缓冲区大小
#define AS608_RX_BUF_SIZE 512  // 从256改为512
```

---

## 📚 参考资料

- `User/as608.c` - AS608驱动源码
- `User/ch32v30x_it.c` - 中断服务函数
- `docs/as608_porting.md` - 移植文档
- CH32V30x参考手册 - 中断章节

---

**文档版本**: V1.0.0
**最后更新**: 2026-01-21
**状态**: ✅ 待集成
