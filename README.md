# 门禁控制系统

基于 **CH32V307VCT6 RISC-V** 平台的嵌入式智能门禁系统。支持 RFID 卡、指纹、密码三种本地认证方式，配备 ST7735S 彩色 LCD UI、BY8301 语音提示，并集成 ESP8266 WiFi 模块实现 MQTT 远程控制。

---

## 硬件平台

| 项目 | 参数 |
|------|------|
| 主控芯片 | CH32V307VCT6 (RISC-V 32-bit) |
| 主频 | 144 MHz |
| Flash / RAM | 256 KB / 64 KB |
| 开发工具 | MounRiver Studio + RISC-V GCC + WCH-Link |

---

## 功能特性

### 认证方式（5 种）

| 方式 | 模块 | 说明 |
|------|------|------|
| RFID 卡 | rfid_reader | 13.56 MHz 高频卡，Mifare S50/S70 |
| 指纹识别 | AS608 | 光学模块，最多存储 300 枚指纹 |
| 密码输入 | password_input | 4×4 矩阵键盘，支持 4~16 位密码 |
| MQTT 远程 | mqtt_app | 通过 OneNET 云平台下发开锁指令 |
| 预留接口 | auth_manager | 人脸识别接口已定义，待硬件接入 |

### 门禁执行

- 舵机控制开关门（TIM3 PWM 50Hz，PA6）
- 状态机管理：`LOCKED` → `OPENING` → `UNLOCKED` → `LOCKED`
- 解锁后 5 秒自动关锁
- 连续认证失败 5 次触发 60 秒告警锁定

### 人机交互

- **LCD 彩屏**：ST7735S，SPI2，128×160 像素，实时显示门锁状态/认证方式/用户名/倒计时
- **菜单系统**：纯 C 实现多级菜单（simple_menu_c）
- **语音提示**：BY8301 模块播报认证结果
- **LED 指示**：2 个状态指示灯（PC2/PC3）
- **按键输入**：1 个独立按键 + 4×4 矩阵键盘

### 网络通信

- **WiFi 模块**：ESP8266（AT 固件，UART 115200）
- **MQTT Broker**：OneNET 云平台（mqtts.heclouds.com:1883）
- **上报**：门锁状态、认证事件、访问日志、用户列表
- **订阅**：远程开锁/关锁/查询命令
- **心跳**：30 秒周期自动上报，断线自动重连

### 底层架构

- **调度器**：5ms 时间片协作式调度（非抢占，基于 TIM2）
- **UART 接收**：DMA 三级缓冲（DMA 256B → 环形缓冲 1024B），零 CPU 占用
- **统一头文件**：`bsp_system.h` 作为全项目唯一 include 入口

---

## 项目结构

```
door_project/
├── Core/               # RISC-V 内核文件
├── Peripheral/         # CH32V30x 外设库
├── User/               # 用户应用代码（主要开发目录）
│   ├── main.c                  # 程序入口与初始化流程
│   ├── bsp_system.h            # 全项目统一头文件
│   ├── scheduler.c/h           # 5ms 协作式任务调度器
│   ├── timer_config.c/h        # 定时器配置
│   │
│   ├── door_control.c/h        # 门禁舵机控制状态机
│   ├── door_status_ui.c/h      # 门禁状态 LCD 界面
│   ├── auth_manager.c/h        # 多方式认证管理器
│   ├── password_input.c/h      # 密码输入处理
│   ├── user_database.c/h       # 用户数据库（增删改查）
│   ├── user_admin.c/h          # 用户管理命令接口
│   ├── access_log.c/h          # 访问日志记录
│   │
│   ├── rfid_reader.c/h         # RFID 驱动（UART5）
│   ├── rfid_task.c/h           # RFID 扫描任务
│   ├── as608.c/h               # AS608 指纹识别（UART2）
│   ├── by8301.c/h              # BY8301 语音模块（UART4）
│   │
│   ├── lcd.c/h                 # LCD 绘图接口
│   ├── lcd_init.c/h            # LCD 初始化（SPI2）
│   ├── lcdfont.h               # LCD 字库数据
│   ├── led.c/h                 # LED 驱动
│   ├── key_app.c/h             # 按键扫描
│   │
│   ├── simple_menu_c.c/h       # 纯 C 菜单框架
│   ├── menu_task.c/h           # LCD 菜单任务
│   │
│   ├── usart1_dma_rx.c/h       # UART1 DMA 三级缓冲接收
│   ├── ringbuffer_large.c/h    # 1024B 环形缓冲区
│   │
│   ├── esp8266_config.h        # WiFi/MQTT 配置（SSID/Broker）
│   ├── esp_at.c/h              # AT 指令引擎（命令队列+状态机）
│   ├── esp8266.c/h             # ESP8266 WiFi 驱动
│   ├── esp8266_mqtt.c/h        # MQTT 协议封装
│   └── mqtt_app.c/h            # MQTT 业务应用层
│
├── Debug/              # 串口调试模块
├── Startup/            # RISC-V 启动文件（汇编）
├── Ld/                 # 链接脚本
└── docs/               # 技术文档（16 个核心文档）
```

---

## 快速开始

### 环境准备

1. 安装 [MounRiver Studio](http://www.wch.cn/downloads/MounRiver_Studio_Setup_exe.html)（内置 RISC-V GCC 工具链）
2. 连接 WCH-Link 调试器到开发板

### 编译与烧录

1. 用 MounRiver Studio 打开项目根目录
2. **Build** → **Download** 烧录到开发板

### 查看串口日志

| 项目 | 配置 |
|------|------|
| 引脚 | PA9 (Tx) / PA10 (Rx) |
| 波特率 | 115200 bps，8N1 |
| 工具 | SecureCRT / PuTTY / 串口助手 |

### WiFi/MQTT 配置

修改 `User/esp8266_config.h`：

```c
#define WIFI_SSID      "your_wifi_ssid"
#define WIFI_PASSWORD  "your_wifi_password"
#define MQTT_HOST      "mqtts.heclouds.com"
#define MQTT_PORT      1883
#define MQTT_CLIENT_ID "your_device_id"
```

---

## 重要资源约束

| 资源 | 占用模块 | 说明 |
|------|----------|------|
| TIM2 | 调度器 | 5ms 基准时钟，禁止复用 |
| TIM3 | 舵机 PWM | CH1 输出 50Hz，禁止复用 |
| DMA1_CH5 | USART1 | DMA 接收，禁止复用 |
| RAM ~45KB | 运行时 | 剩余约 19KB，避免大数组 |

---

## 开发进度

- [x] 5ms 协作式任务调度器
- [x] UART DMA 三级缓冲接收
- [x] RFID 卡认证
- [x] AS608 指纹识别
- [x] 密码输入认证
- [x] 舵机控制与自动关锁
- [x] LCD 彩屏 UI 与多级菜单
- [x] 用户数据库与权限管理
- [x] ESP8266 WiFi 驱动
- [x] MQTT 远程控制与状态上报
- [ ] 外部 Flash 持久化存储（W25Q128）
- [ ] 人脸识别模块接入
- [ ] Web 本地管理界面

---

## 技术文档

详见 [docs/README.md](docs/README.md)

| 文档 | 描述 |
|------|------|
| [hardware_pinout.md](docs/hardware_pinout.md) | 引脚分配与定时器资源总表 |
| [门禁认证系统_代码实现逻辑](docs/门禁认证系统_代码实现逻辑_2026-02-05.md) | 认证系统核心设计与算法 |
| [esp8266_framework.md](docs/esp8266_framework.md) | WiFi/MQTT 分层架构说明 |
| [mqtt_app_test_guide.md](docs/mqtt_app_test_guide.md) | MQTT 远程控制测试指南 |
| [RFID_API_Reference.md](docs/RFID_API_Reference.md) | RFID 驱动 API 参考 |
| [as608_quickstart.md](docs/as608_quickstart.md) | 指纹模块快速入门 |

---

## 参考资料

- [CH32V307 数据手册](http://www.wch.cn/downloads/CH32V307DS0_PDF.html)
- [MounRiver Studio IDE](http://www.wch.cn/downloads/MounRiver_Studio_Setup_exe.html)
