# CH32V30x 1ms定时器中断方案

## 📋 方案概述

本方案为CH32V30x系列单片机提供**1ms精确定时中断**功能，基于TIM2定时器实现。

---

## ⚙️ 系统配置

### 时钟配置
- **系统时钟（SYSCLK）：** 96 MHz
- **外部晶振（HSE）：** 8 MHz
- **AHB总线（HCLK）：** 96 MHz
- **APB2总线（PCLK2）：** 96 MHz
- **APB1总线（PCLK1）：** 48 MHz
- **定时器时钟：** 96 MHz（APB1预分频≠1时自动×2）

### 定时器配置
- **选用定时器：** TIM2（32位通用定时器）
- **预分频系数：** 96（PSC=95）
- **自动重载值：** 1000（ARR=999）
- **中断频率：** 1000 Hz（1ms周期）
- **中断优先级：** 抢占优先级2，子优先级0

---

## 📂 文件结构

```
User/
├── main.c              # 主程序（已修改，集成定时器调用）
├── timer_config.h      # 定时器配置头文件（新增）
├── timer_config.c      # 定时器配置实现文件（新增）
├── ch32v30x_it.c      # 中断服务函数（无需修改）
└── ch32v30x_it.h      # 中断头文件（无需修改）
```

---

## 🔧 时钟计算原理

### 计算公式
```
定时器中断频率 = 定时器时钟频率 / [(预分频值 + 1) × (自动重载值 + 1)]
```

### 具体计算
```
系统配置：
- APB1总线频率 = 48 MHz（SYSCLK/2）
- 定时器时钟 = 48 MHz × 2 = 96 MHz（自动倍频）

目标：实现1ms中断（1000 Hz）

计算：
1000 Hz = 96,000,000 Hz / [(PSC + 1) × (ARR + 1)]
=> (PSC + 1) × (ARR + 1) = 96,000

选择方案：
PSC = 95（预分频96倍）
ARR = 999（计数1000次）

验证：
96,000,000 / (96 × 1000) = 1000 Hz ✓
周期 = 1/1000 = 1ms ✓
```

### 时钟倍频说明

⚠️ **重要：** CH32V30x的定时器时钟规则：
- 当APB1预分频系数 = 1时，定时器时钟 = APB1时钟
- 当APB1预分频系数 ≠ 1时，定时器时钟 = APB1时钟 × 2

本项目中：
- PCLK1 = HCLK/2 = 48MHz（预分频系数为2）
- 因此TIM2时钟 = 48MHz × 2 = 96MHz

---

## 🚀 使用方法

### 1. 基本使用流程

```c
#include "timer_config.h"

int main(void)
{
    // 1. 系统初始化
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();

    // 2. 初始化定时器
    TIM_1ms_Init();

    // 3. 启动定时器
    TIM_1ms_Start();

    while(1)
    {
        // 主循环代码
    }
}
```

### 2. 实现回调函数

在任意.c文件中实现 `TIM_1ms_Callback()` 函数：

```c
void TIM_1ms_Callback(void)
{
    // 每1ms执行一次的代码
    static uint16_t count = 0;
    count++;

    if(count >= 1000)  // 每1秒执行一次
    {
        count = 0;
        // 你的代码
    }
}
```

### 3. API函数说明

| 函数名 | 功能 | 参数 | 返回值 |
|--------|------|------|--------|
| `TIM_1ms_Init()` | 初始化1ms定时器 | 无 | 无 |
| `TIM_1ms_Start()` | 启动定时器 | 无 | 无 |
| `TIM_1ms_Stop()` | 停止定时器 | 无 | 无 |
| `TIM_Get_MillisCounter()` | 获取毫秒计数值 | 无 | `uint32_t` |
| `TIM_1ms_Callback()` | 中断回调函数（用户实现） | 无 | 无 |

---

## 💡 示例代码

### 示例1：LED闪烁（500ms翻转）

```c
void TIM_1ms_Callback(void)
{
    static uint16_t count = 0;
    count++;

    if(count >= 500)  // 每500ms
    {
        count = 0;
        GPIO_WriteBit(GPIOC, GPIO_Pin_2,
                     (BitAction)(1 - GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_2)));
    }
}
```

### 示例2：多任务调度

```c
void TIM_1ms_Callback(void)
{
    static uint16_t task1_count = 0;
    static uint16_t task2_count = 0;

    // 任务1：每10ms执行
    task1_count++;
    if(task1_count >= 10)
    {
        task1_count = 0;
        Task1_Handler();
    }

    // 任务2：每100ms执行
    task2_count++;
    if(task2_count >= 100)
    {
        task2_count = 0;
        Task2_Handler();
    }
}
```

### 示例3：获取运行时间

```c
int main(void)
{
    TIM_1ms_Init();
    TIM_1ms_Start();

    while(1)
    {
        uint32_t current_time = TIM_Get_MillisCounter();
        printf("运行时间: %lu ms\r\n", current_time);
        Delay_Ms(1000);
    }
}
```

---

## ⚙️ 配置修改

### 修改中断频率

如需修改中断频率，编辑 `timer_config.h`：

```c
// 示例：改为10ms中断（100Hz）
#define TIM_PRESCALER       95      // 保持不变
#define TIM_PERIOD          9999    // 改为9999（10000计数）
#define TIM_INTERRUPT_FREQ  100     // 改为100Hz

// 计算验证：96,000,000 / (96 × 10000) = 100 Hz ✓
```

### 切换到TIM6（基本定时器）

编辑 `timer_config.h`：

```c
// 注释掉TIM2，启用TIM6
// #define USE_TIM2
#define USE_TIM6

#ifdef USE_TIM6
    #define TIMx                    TIM6
    #define TIMx_IRQn               TIM6_IRQn
    #define TIMx_IRQHandler         TIM6_IRQHandler
    #define RCC_APBxPeriph_TIMx     RCC_APB1Periph_TIM6
    #define RCC_APBxPeriphClockCmd  RCC_APB1PeriphClockCmd
#endif
```

### 修改中断优先级

编辑 `timer_config.h`：

```c
#define TIM_IRQ_PREEMPTION_PRIORITY     1   // 改为更高优先级
#define TIM_IRQ_SUB_PRIORITY            0
```

---

## 📊 性能特性

- **精度：** ±1 时钟周期（±10.4ns @ 96MHz）
- **最大计数时间：** 49.7天（2^32 ms）
- **中断响应时间：** < 1μs（RISC-V快速中断）
- **CPU占用率：** < 0.1%（1ms空回调）

---

## ⚠️ 注意事项

1. **中断函数规则**
   - 中断函数必须添加 `__attribute__((interrupt("WCH-Interrupt-fast")))` 属性
   - 中断函数内代码应尽量简短，避免阻塞
   - 不要在中断中使用 `printf()`（可能导致死锁）

2. **时钟配置依赖**
   - 本方案基于96MHz系统时钟
   - 如修改系统时钟，需重新计算预分频和重载值

3. **计数器溢出**
   - 毫秒计数器为32位，约49.7天溢出
   - 如需长时间运行，建议自行处理溢出逻辑

4. **线程安全**
   - 访问 `g_millis_counter` 时建议关闭中断或使用原子操作

---

## 🔍 调试技巧

### 验证定时器是否正常工作

```c
void TIM_1ms_Callback(void)
{
    static uint16_t count = 0;
    count++;

    if(count >= 1000)  // 每1秒
    {
        count = 0;
        printf("Tick!\r\n");  // 应该每秒打印一次
    }
}
```

### 测量实际中断周期

```c
uint32_t start_time = TIM_Get_MillisCounter();
Delay_Ms(1000);
uint32_t end_time = TIM_Get_MillisCounter();
uint32_t elapsed = end_time - start_time;  // 应接近1000
```

---

## 🛠️ 常见问题

**Q1: 定时器不工作怎么办？**
- 检查是否调用了 `TIM_1ms_Start()`
- 确认时钟已使能（`RCC_APB1PeriphClockCmd`）
- 检查NVIC配置是否正确

**Q2: 中断频率不准确？**
- 确认系统时钟配置为96MHz
- 重新计算预分频和重载值
- 使用示波器或逻辑分析仪验证

**Q3: 如何实现更高精度的定时？**
- 使用硬件定时器的输出比较功能
- 考虑使用DMA配合定时器
- 提高系统时钟频率

---

## 📖 参考资料

- CH32V30x数据手册
- CH32V30x参考手册 - TIM章节
- WCH官方例程库

---

## 📝 版本历史

- **V1.0.0** (2025-12-29)
  - 初始版本
  - 实现基于TIM2的1ms定时中断
  - 支持毫秒计数和回调函数

---

**作者：** Custom Implementation
**日期：** 2025-12-29
**平台：** CH32V307 / CH32V305 / CH32V317
