# 门禁控制系统 (Door Access Control System)

基于 CH32V307VCT6 RISC-V 平台的智能门禁系统，支持 RFID 卡、指纹、密码多种认证方式，配备 LCD 彩屏 UI、语音提示和 WiFi/MQTT 远程控制。

## 硬件平台

| 项目 | 参数 |
|------|------|
| 主控芯片 | CH32V307VCT6 (RISC-V 32-bit) |
| 主频 | 144 MHz |
| Flash | 256 KB |
| RAM | 64 KB |
| 开发工具 | MounRiver Studio + RISC-V GCC |

## 功能特性

### 认证方式
- **RFID 卡识别** — 13.56 MHz 高频卡（Mifare S50/S70）
- **指纹识别** — AS608 光学/电容式模块
- **密码输入** — 矩阵键盘输入

### 门禁控制
- 舵机驱动开关门（TIM3 PWM 输出）
- 状态机管理锁定/解锁状态
- 可配置超时自动关门

### 人机交互
- ST7735S 彩色 LCD（128×160 像素）
- 纯 C 实现的多级菜单系统
- BY8301 语音提示模块
- LED 状态指示灯

### 网络通信
- ESP8266 WiFi 模块（AT 指令驱动）
- MQTT 协议远程控制与状态上报

## 项目结构

```
door_project/
├── Core/           # RISC-V 内核文件
├── Peripheral/     # CH32V30x 外设库
├── User/           # 用户应用代码（主要开发目录）
│   ├── main.c              # 程序入口
│   ├── scheduler.c/h       # 5ms 协作式任务调度器
│   ├── bsp_system.h        # 全项目统一头文件入口
│   ├── door_control.c/h    # 门禁控制状态机
│   ├── door_status_ui.c/h  # 门禁状态 UI 显示
│   ├── menu_task.c/h       # LCD 菜单任务
│   ├── rfid_reader.c/h     # RFID 驱动
│   ├── rfid_task.c/h       # RFID 任务
│   ├── as608.c/h           # 指纹识别驱动
│   ├── lcd.c/h             # LCD 绘图接口
│   ├── lcd_init.c/h        # LCD 初始化驱动
│   ├── usart1_dma_rx.c/h   # UART DMA 三级缓冲接收
│   ├── ringbuffer_large.c/h# 大容量环形缓冲区
│   ├── user_database.c/h   # 用户数据库
│   ├── access_log.c/h      # 开门记录
│   ├── auth_manager.c/h    # 认证管理器
│   ├── esp8266.c/h         # ESP8266 WiFi 驱动
│   ├── esp_at.c/h          # AT 指令层
│   ├── esp8266_mqtt.c/h    # MQTT 协议封装
│   └── mqtt_app.c/h        # MQTT 应用层
├── Debug/          # 串口调试模块
├── Startup/        # RISC-V 启动文件
├── Ld/             # 链接脚本
└── docs/           # 技术文档
```

## 快速开始

### 环境准备

1. 安装 [MounRiver Studio](http://www.wch.cn/downloads/MounRiver_Studio_Setup_exe.html)（内置 RISC-V GCC 工具链）
2. 连接 WCH-Link 调试器到开发板

### 编译与烧录

1. 在 MounRiver Studio 中打开项目根目录
2. 点击 **Build** 编译
3. 连接开发板后点击 **Download** 烧录

### 调试输出

通过 USART1 查看串口日志：
- **引脚**: PA9 (Tx) / PA10 (Rx)
- **波特率**: 115200 bps，8N1
- **工具**: SecureCRT / PuTTY / 串口助手

## 架构说明

系统采用**裸机协作式调度**，核心为 5ms 时间片任务调度器：

```c
while (1) {
    scheduler_run();  // 5ms 周期轮询所有任务
}
```

通信采用 **UART DMA 三级缓冲**，实现零 CPU 占用接收：
```
硬件DMA → DMA缓冲(256B) → 环形缓冲(1024B) → 应用解析
```

## 重要资源约束

| 资源 | 占用情况 |
|------|----------|
| TIM2 | 调度器 5ms 基准时钟（禁止占用） |
| TIM3 | 舵机 PWM 输出（禁止占用） |
| DMA1_CH5 | USART1 DMA 接收（禁止占用） |
| RAM | 总计 64KB，谨慎使用大数组 |

## 文档

详细技术文档请查阅 [docs/README.md](docs/README.md)

## 开发计划

- [x] RFID 卡识别
- [x] 指纹识别（AS608）
- [x] 密码输入
- [x] LCD 彩屏 UI 与菜单系统
- [x] 用户数据库与权限管理
- [x] WiFi/MQTT 远程通信（ESP8266）
- [ ] 外部 Flash 存储（W25Q128）
- [ ] 云平台对接
- [ ] 人脸识别模块

## 参考资料

- [CH32V307 数据手册](http://www.wch.cn/downloads/CH32V307DS0_PDF.html)
- [MounRiver Studio](http://www.wch.cn/downloads/MounRiver_Studio_Setup_exe.html)
- [硬件引脚文档](docs/hardware_pinout.md)
