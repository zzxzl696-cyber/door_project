# User 模块 - 用户应用代码层

[根目录](../CLAUDE.md) > **User**

---

## 模块职责

User模块是门禁系统的**应用层核心**，包含所有用户业务逻辑、驱动程序、中间件和任务调度。负责：

- **应用程序入口** (`main.c`) - 系统初始化和主循环
- **任务调度器** (`scheduler.c/h`) - 5ms协作式调度系统
- **设备驱动** - RFID、指纹、LCD、UART DMA等外设驱动
- **业务逻辑** - 门禁控制、认证管理、UI交互
- **中间件** - 环形缓冲区、协议解析、状态机

---

## 入口与启动

### 主程序入口

**文件**: `main.c`

**初始化顺序**:
```c
int main(void) {
    // 1. 核心系统初始化
    led_init();                      // LED指示灯
    scheduler_init();                // 任务调度器
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();

    // 2. 通信模块初始化
    USART1_DMA_RX_FullInit(115200, &g_usart1_ringbuf);  // UART1 DMA接收

    // 3. 定时器初始化
    TIM_1ms_Init();                  // 1ms定时器（用于调度器）

    // 4. 门禁模块初始化
    door_control_init();             // 门锁控制（舵机）
    menu_init();                     // LCD菜单系统
    key_init();                      // 独立按键
    matrix_key_init();               // 矩阵键盘
    BY8301_Init();                   // 语音模块

    // 5. 认证模块初始化
    RFID_Init(115200);               // RFID读卡器（UART5）
    RFID_SetWorkMode(RFID_MODE_AUTO_ID_BLOCK, 8, 2000);

    // 6. 启动调度器
    TIM_1ms_Start();
    while (1) {
        scheduler_run();             // 5ms周期任务循环
    }
}
```

**关键全局变量**:
```c
volatile uint32_t led_toggle_count = 0;        // LED翻转计数
ringbuffer_large_t g_usart1_ringbuf;           // UART1环形缓冲区
door_control_status_t g_door_status;            // 门禁状态
```

---

## 对外接口

### 1. 调度器接口 (`scheduler.c/h`)

```c
void scheduler_init(void);      // 初始化调度器
void scheduler_run(void);       // 运行一次调度周期（由主循环调用）
```

**调度机制**:
- **周期**: 5ms（由TIM2触发）
- **任务列表**:
  - LED任务 (`led_task`)
  - UART接收任务 (`uart_rx_task_handler`)
  - RFID扫描任务 (`rfid_task_handler`)
  - LCD更新任务 (`lcd_test_task_handler`)
  - 菜单任务 (`menu_task_handler`)

### 2. UART DMA接收 (`usart1_dma_rx.c/h`)

```c
// 完整初始化（包含GPIO、USART、DMA、NVIC）
void USART1_DMA_RX_FullInit(uint32_t baudrate, ringbuffer_large_t* rb);

// 获取接收状态
usart1_dma_rx_t* USART1_DMA_RX_GetState(void);

// 内部使用的DMA处理函数
void USART1_DMA_RX_On_Idle_Or_TC(usart1_dma_rx_t* state);
```

**架构特性**:
- **三级缓冲**: USART FIFO (3B) → DMA双缓冲 (256B) → 环形缓冲 (1024B)
- **零CPU接收**: DMA循环模式 + 空闲中断触发
- **防丢失机制**: 双重保护（DMA完成中断 + UART空闲中断）

### 3. RFID读卡器 (`rfid_reader.c/h`)

```c
// 初始化RFID读卡器（使用UART5: PC12/PD2）
void RFID_Init(uint32_t baudrate);

// 设置工作模式
RFID_Result RFID_SetWorkMode(RFID_WorkMode mode, uint8_t block_num, uint32_t timeout_ms);

// 命令模式接口
RFID_Result RFID_ReadId(uint8_t *card_type, uint8_t *uid, uint32_t timeout_ms);
RFID_Result RFID_ReadBlock(uint8_t block, uint8_t *data16, uint32_t timeout_ms);
RFID_Result RFID_WriteBlock(uint8_t block, const uint8_t *data16, uint32_t timeout_ms);

// 自动模式接口
RFID_Result RFID_TryGetFrame(RFID_Frame *frame);  // 非阻塞获取帧
RFID_Result RFID_WaitFrame(RFID_Frame *frame, uint32_t timeout_ms);  // 阻塞等待
```

**工作模式**:
- `RFID_MODE_COMMAND` - 命令模式（被动响应）
- `RFID_MODE_AUTO_ID` - 自动上传卡号
- `RFID_MODE_AUTO_BLOCK` - 自动上传块数据
- `RFID_MODE_AUTO_ID_BLOCK` - 自动上传卡号+块数据

### 4. 指纹识别 (`as608.c/h`)

```c
// 握手验证
uint8_t PS_HandShake(uint32_t *PS_Addr);

// 录入指纹流程
uint8_t Add_FR(uint16_t PageID);

// 验证指纹流程
uint8_t Press_FR(SearchResult *result);

// 删除指纹
uint8_t Del_FR(uint16_t PageID);

// 底层操作
uint8_t PS_GetImage(void);                   // 录入图像
uint8_t PS_GenChar(uint8_t BufferID);        // 生成特征
uint8_t PS_Match(void);                       // 精确比对
uint8_t PS_Search(uint8_t BufferID, uint16_t StartPage, uint16_t PageNum, SearchResult *p);
uint8_t PS_RegModel(void);                    // 合并特征
uint8_t PS_StoreChar(uint8_t BufferID, uint16_t PageID);  // 储存模板
```

**连接**: 使用UART2 (PA2-Tx, PA3-Rx), 115200 bps

### 5. 门禁控制 (`door_control.c/h`)

```c
// 初始化门禁控制模块
void door_control_init(void);

// 解锁门禁（指定认证方式和持续时间）
void door_control_unlock(auth_method_t method, uint32_t duration_ms);

// 手动锁门
void door_control_lock(void);

// 更新门禁状态（由调度器周期调用）
void door_control_update(void);

// 查询状态
uint8_t door_control_is_locked(void);
const char* door_control_get_lock_state_str(void);
const char* door_control_get_auth_method_str(void);
```

**状态机**:
```
DOOR_LOCKED → DOOR_OPENING → DOOR_UNLOCKED → DOOR_ALARM
     ↑                                           |
     └───────────────────────────────────────────┘
```

### 6. LCD显示 (`lcd.c/h`, `lcd_init.c/h`)

```c
// 初始化LCD（ST7735S）
void LCD_Init(void);

// 绘图函数
void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

// 文字显示
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);
void LCD_ShowString(uint16_t x, uint16_t y, const uint8_t *p, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);
void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey);
```

**连接**: SPI2 (PB13-SCK, PB15-MOSI), 分辨率: 128x160

---

## 关键依赖与配置

### 外部依赖

- **CH32V30x外设库**: `../Peripheral/` - GPIO, UART, SPI, TIM, DMA
- **RISC-V核心库**: `../Core/core_riscv.c/h`
- **调试库**: `../Debug/debug.c/h` - printf重定向

### 配置文件

- **系统配置**: `ch32v30x_conf.h` - 外设使能配置
- **系统初始化**: `system_ch32v30x.c/h` - 时钟配置
- **中断向量**: `ch32v30x_it.c/h` - 中断服务函数

### 定时器资源分配

| 定时器 | 用途 | 配置 | 优先级 |
|--------|------|------|--------|
| **TIM2** | 调度器1ms基准 | 72MHz预分频, 1ms中断 | 0 (最高) |
| **TIM3** | 舵机PWM输出 | 50Hz, 0.5-2.5ms脉宽 | - |
| **TIM4** | 预留 | 未使用 | - |

### 中断优先级（NVIC_PriorityGroup_2）

| 中断源 | 抢占优先级 | 响应优先级 | 说明 |
|--------|-----------|-----------|------|
| TIM2 | 0 | 0 | 调度器定时器 |
| USART1空闲 | 1 | 0 | UART1空闲中断 |
| DMA1_Channel5 | 1 | 1 | UART1 DMA |
| UART5 | 2 | 0 | RFID读卡器 |

---

## 数据模型

### 环形缓冲区结构

```c
typedef struct {
    uint8_t* buffer;       // 缓冲区指针
    uint32_t size;         // 缓冲区大小
    volatile uint32_t head;  // 写指针
    volatile uint32_t tail;  // 读指针
    volatile uint32_t count; // 当前数据量
} ringbuffer_large_t;
```

### UART DMA状态

```c
typedef struct {
    uint8_t dma_buffer[USART1_DMA_RX_BUFFER_SIZE];  // DMA缓冲（256字节）
    ringbuffer_large_t* user_ringbuf;  // 用户环形缓冲（1024字节）
    volatile uint32_t last_dma_ndtr;   // 上次DMA剩余计数
    volatile uint32_t rx_count;        // 总接收字节数
} usart1_dma_rx_t;
```

### 门禁状态结构

```c
typedef struct {
    door_lock_state_t lock_state;     // 门锁状态
    uint8_t servo_angle;              // 舵机角度
    auth_method_t last_auth_method;   // 最后认证方式
    uint32_t unlock_timestamp;        // 解锁时间戳(ms)
    uint32_t unlock_duration;         // 解锁持续时间(ms)
    uint8_t auth_fail_count;          // 认证失败次数
    uint8_t alarm_active;             // 告警激活标志
} door_control_status_t;
```

### RFID帧结构

```c
typedef struct {
    RFID_FrameType type;      // 帧类型
    uint8_t addr;             // 读卡器地址
    uint8_t cmd;              // 命令字节
    uint8_t status;           // 状态字节
    uint8_t card_type[2];     // 卡类型
    uint8_t uid[4];           // 卡号UID
    uint8_t block_data[16];   // 块数据
    uint8_t raw[RFID_MAX_FRAME_LEN];  // 原始帧
    uint8_t raw_len;          // 原始帧长度
} RFID_Frame;
```

---

## 测试与质量

### 单元测试文件

- **UART测试**: `uart_test.c/h` - 串口DMA接收测试
- **LCD测试**: `lcd_test_task.c/h` - LCD显示功能测试

### 测试命令

通过UART1发送以下字符串测试：

```bash
# 测试UART接收
echo "Hello CH32V30x" > /dev/ttyUSB0

# 测试RFID自动模式
# 直接刷卡，系统自动识别并打印卡号+块数据
```

### 调试输出

系统启动后通过UART1输出：

```
=================================
System Clock: 144000000 Hz
Chip ID: 30700518
CH32V30x LCD + UART DMA Demo
=================================

[INFO] Initializing door control...
[INFO] Door control initialized!
[INFO] Initializing ST7735S LCD...
[INFO] LCD initialized successfully!
[INFO] System initialized successfully!
[INFO] USART1 DMA RX enabled, waiting for data...
[INFO] Door control system running...
```

---

## 常见问题 (FAQ)

### Q1: UART接收丢数据怎么办？

**A**: 检查以下几点：
1. DMA缓冲区大小（256字节）是否足够
2. 环形缓冲区（1024字节）是否溢出
3. 调度器周期（5ms）是否及时处理数据
4. 空闲中断和DMA中断是否正常触发

参考文档：[USART1_DMA使用说明](../docs/USART1_DMA使用说明.md)

### Q2: RFID读卡器无响应？

**A**: 检查：
1. UART5引脚连接（PC12-TX, PD2-RX）
2. 波特率配置（默认9600，可配置115200）
3. 读卡器电源是否正常
4. 使用 `RFID_QueryVersion()` 测试通信

参考文档：[RFID硬件连接](../docs/RFID_Hardware_Connection.md)

### Q3: 指纹模块握手失败？

**A**: 排查步骤：
1. 检查UART2连接（PA2-TX, PA3-RX）
2. 确认波特率115200
3. 模块上电延迟（至少500ms）
4. 检查状态引脚（PB0）

参考文档：[AS608快速开始](../docs/as608_quickstart.md)

### Q4: LCD显示花屏或无显示？

**A**: 可能原因：
1. SPI2配置错误
2. 控制引脚（RES, DC, CS, BLK）未正确初始化
3. 电源电压不足
4. 屏幕复位时序错误

参考文档：[LCD驱动使用说明](../docs/LCD驱动使用说明.md)

---

## 相关文件清单

### 核心文件

| 文件 | 行数 | 功能 |
|------|------|------|
| `main.c` | 134 | 主程序入口 |
| `scheduler.c/h` | ~100 | 任务调度器 |
| `ch32v30x_it.c/h` | ~200 | 中断服务函数 |
| `system_ch32v30x.c/h` | ~150 | 系统初始化 |

### 驱动文件

| 文件 | 行数 | 功能 |
|------|------|------|
| `usart1_dma_rx.c/h` | ~300 | UART1 DMA接收 |
| `rfid_reader.c/h` | ~800 | RFID读卡器驱动 |
| `as608.c/h` | ~600 | AS608指纹识别 |
| `lcd.c/h` | ~500 | LCD绘图函数 |
| `lcd_init.c/h` | ~200 | LCD初始化 |
| `door_control.c/h` | ~300 | 门禁控制 |
| `led.c/h` | ~100 | LED驱动 |
| `key_app.c/h` | ~200 | 按键驱动 |
| `by8301.c/h` | ~150 | 语音模块 |

### 中间件文件

| 文件 | 行数 | 功能 |
|------|------|------|
| `ringbuffer.c/h` | ~150 | 小环形缓冲区 |
| `ringbuffer_large.c/h` | ~200 | 大环形缓冲区 |
| `simple_menu_c.c/h` | ~400 | 纯C菜单系统 |
| `menu_task.c/h` | ~300 | 菜单任务 |

### 任务文件

| 文件 | 行数 | 功能 |
|------|------|------|
| `uart_rx_task.c/h` | ~150 | UART接收任务 |
| `lcd_test_task.c/h` | ~200 | LCD测试任务 |
| `rfid_task.c/h` | ~150 | RFID扫描任务 |

### 配置文件

| 文件 | 行数 | 功能 |
|------|------|------|
| `ch32v30x_conf.h` | ~100 | 外设使能配置 |
| `bsp_system.h` | ~50 | 系统配置头文件 |
| `timer_config.c/h` | ~200 | 定时器配置 |
| `func_num.c/h` | ~50 | 功能编号定义 |

---

## 变更记录 (Changelog)

| 日期 | 版本 | 修改内容 | 文件 |
|------|------|----------|------|
| 2026-02-05 | v1.0.0 | 创建User模块文档 | - |
| 2026-01-31 | - | 添加RFID读卡器驱动 | rfid_reader.c/h |
| 2026-01-21 | - | 移植AS608指纹模块 | as608.c/h |
| 2026-01-20 | - | 实现门禁控制模块 | door_control.c/h |
| 2025-12-30 | - | 移植LCD驱动 | lcd.c/h, lcd_init.c/h |

---

**提示**: 本模块是项目的核心应用层，修改时注意调度器时序和中断优先级配置。
