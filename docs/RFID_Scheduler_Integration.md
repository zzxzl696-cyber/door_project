# RFID驱动调度器集成说明

**版本：** V1.1.0
**更新日期：** 2026-02-02
**集成状态：** ✅ 已完成

---

## 📋 更新概述

RFID读卡器驱动已成功集成到项目的时间片轮询调度器框架中，实现了与现有任务的协同工作。

---

## 🔄 架构变更

### 变更前（原始架构）

```
主循环中需要手动调用：
while(1) {
    RFID_Poll();  // ❌ 需要用户手动调用
    // 其他代码
}
```

### 变更后（调度器架构）

```
调度器自动管理：
while(1) {
    scheduler_run();  // ✅ 调度器自动调用所有任务
}

任务列表：
├── uart_rx_task (5ms)      - UART接收处理
├── rfid_task (5ms)         - RFID轮询处理 ← 新增
├── matrix_key_scan (10ms)  - 按键扫描
└── door_control_update (100ms) - 门禁控制
```

---

## 📁 新增文件

### 1. rfid_task.h
**路径：** `User/rfid_task.h`
**功能：** RFID任务模块头文件

**提供的API：**
```c
void rfid_task(void);  // 调度器调用的任务函数
void rfid_task_get_statistics(uint32_t* poll_count, uint32_t* frame_count);
void rfid_task_reset_statistics(void);
```

### 2. rfid_task.c
**路径：** `User/rfid_task.c`
**功能：** RFID任务模块实现

**核心功能：**
- 每5ms调用一次`RFID_Poll()`处理UART接收
- 可选的调试信息输出（通过RFID_TASK_DEBUG控制）
- 统计信息收集（Poll调用次数、帧接收次数）

### 3. rfid_application_example.c
**路径：** `User/rfid_application_example.c`
**功能：** 完整的应用层示例代码

**包含示例：**
- 示例1：调度器框架下的命令模式使用
- 示例2：调度器框架下的自动模式使用
- 示例3：创建应用级RFID处理任务
- 主程序完整示例

---

## 🔧 修改文件

### 1. scheduler.c
**修改内容：**

```c
// 1. 添加头文件
#include "rfid_task.h"

// 2. 在任务数组中添加RFID任务
static task_t scheduler_task[] = {
    {uart_rx_task, 5, 0},      // UART接收任务：5ms周期
    {rfid_task, 5, 0},         // RFID轮询任务：5ms周期 ← 新增
    {matrix_key_scan, 10, 0},  // 矩阵按键扫描：10ms周期
    {door_control_update, 100, 0}, // 门禁状态更新：100ms周期
};
```

### 2. docs/RFID_Quick_Start.md
**修改内容：**
- 在目录中添加"调度器集成"章节
- 在"软件配置"和"第一个程序"之间插入完整的调度器集成指南
- 提供了详细的集成步骤和代码示例

---

## 🎯 使用方式

### 方式1：直接使用RFID API（简单）

适用于偶尔读卡的场景：

```c
int main(void)
{
    // 初始化
    RFID_Init();
    scheduler_init();

    // 进入调度器主循环
    while(1)
    {
        scheduler_run();

        // 在主循环中直接调用RFID API
        if (需要读卡)
        {
            RFID_ReadId(card_type, uid, 1000);
        }
    }
}
```

### 方式2：创建RFID应用任务（推荐）

适用于持续监控刷卡的场景：

```c
// 1. 创建应用任务函数
void rfid_application_task(void)
{
    RFID_Frame frame;

    // 非阻塞检查自动帧
    if (RFID_PollAutoFrame(&frame) == RFID_RET_OK)
    {
        // 处理刷卡事件
        process_card(frame.uid);
    }
}

// 2. 在scheduler.c中注册
static task_t scheduler_task[] = {
    {uart_rx_task, 5, 0},
    {rfid_task, 5, 0},
    {rfid_application_task, 100, 0},  // ← 添加应用任务
    {matrix_key_scan, 10, 0},
    {door_control_update, 100, 0},
};
```

---

## ⚡ 性能特点

### 任务周期分配

| 任务 | 周期 | CPU占用 | 说明 |
|------|------|---------|------|
| uart_rx_task | 5ms | 低 | 处理UART1接收 |
| rfid_task | 5ms | 低 | 处理RFID UART5接收 |
| rfid_application | 100ms | 低-中 | 业务逻辑处理 |
| matrix_key_scan | 10ms | 低 | 按键扫描 |
| door_control_update | 100ms | 低 | 门禁控制 |

### 响应时间

- **UART接收延迟：** ≤5ms（任务周期）
- **帧解析延迟：** ≤5ms
- **总响应时间：** ≤10ms（足够快）

---

## ✅ 兼容性

### 向后兼容性

✅ **完全兼容原有RFID驱动API**
- 所有现有的`RFID_ReadId()`、`RFID_WriteBlock()`等函数无需修改
- 原有的命令模式和自动模式功能完整保留
- 配置和查询API全部正常工作

### 与其他模块的兼容性

✅ **与UART1任务并行**
- RFID使用UART5，UART1任务使用UART1
- 两者独立运行，互不干扰

✅ **与按键和门禁任务协同**
- 调度器统一管理，资源合理分配
- 无阻塞冲突

---

## 🔍 调试技巧

### 1. 查看任务统计

```c
uint32_t poll_count, frame_count;
rfid_task_get_statistics(&poll_count, &frame_count);
printf("Poll次数: %lu, 帧数: %lu\r\n", poll_count, frame_count);
```

### 2. 开启调试输出

在`rfid_task.c`中：
```c
#define RFID_TASK_DEBUG   1  // 开启调试
```

重新编译后会输出每一帧的详细信息。

### 3. 验证任务运行

```c
// 在main.c中
scheduler_init();
printf("任务数量: %d\r\n", task_num);  // 应该输出4或5
```

---

## 📚 参考示例

完整的使用示例请查看：
- [rfid_application_example.c](../User/rfid_application_example.c) - 3个完整示例
- [RFID_Quick_Start.md](RFID_Quick_Start.md) - 快速入门指南
- [RFID_API_Reference.md](RFID_API_Reference.md) - API详细说明

---

## 🎓 最佳实践

### ✅ 推荐做法

1. **分层设计**
   - `rfid_task` (5ms) - 负责底层UART接收和帧解析
   - `rfid_application_task` (100ms) - 负责业务逻辑

2. **非阻塞操作**
   - 在应用任务中使用`RFID_PollAutoFrame()`
   - 避免使用`RFID_WaitAutoFrame()`阻塞其他任务

3. **错误处理**
   - 检查所有API的返回值
   - 处理超时和通信错误

### ❌ 避免的错误

1. ❌ **不要在主循环中手动调用`RFID_Poll()`**
   - 调度器已经自动调用

2. ❌ **不要在中断中调用RFID API**
   - 所有RFID操作应在任务中进行

3. ❌ **不要使用过长的超时时间**
   - 会阻塞调度器，影响其他任务

---

## 🚀 快速开始

```bash
# 1. 确认文件已添加到工程
User/rfid_task.h
User/rfid_task.c

# 2. 确认scheduler.c已更新
- 包含rfid_task.h
- 任务数组中包含rfid_task

# 3. 编译下载
make clean
make
make download

# 4. 运行测试
- 上电后应该能正常读卡
- 如果不工作，检查调试输出
```

---

## 💡 常见问题

### Q1: RFID不读卡？
**A:** 检查以下几点：
1. `scheduler_init()`是否被调用
2. `RFID_Init()`是否被调用
3. 调度器主循环是否在运行
4. 使用万用表检查硬件连接

### Q2: 如何确认rfid_task在运行？
**A:**
```c
rfid_task_get_statistics(&poll_count, &frame_count);
Delay_Ms(1000);
rfid_task_get_statistics(&poll_count2, &frame_count2);
// poll_count2应该比poll_count大约增加200（1秒/5ms = 200次）
```

### Q3: 可以修改rfid_task的周期吗？
**A:** 可以，但建议保持5ms。如果修改：
- 增大周期：响应变慢，但CPU占用降低
- 减小周期：响应更快，但CPU占用增加
- 不建议小于5ms或大于20ms

---

## 📞 技术支持

如有问题，请参考：
1. [RFID_API_Reference.md](RFID_API_Reference.md) - 完整API文档
2. [rfid_application_example.c](../User/rfid_application_example.c) - 示例代码
3. 项目Issue跟踪系统

---

**集成完成！现在您可以在调度器框架下无缝使用RFID驱动了！** 🎉
