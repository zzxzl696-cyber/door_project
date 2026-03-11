# 项目文档索引

**项目名称**: 门禁控制系统
**主控芯片**: CH32V307VCT6 (RISC-V)
**文档版本**: V2.0.0
**更新日期**: 2026-03-11

---

## 文档目录

```
docs/
├── README.md                                  # 本文件 - 文档索引
│
├── 硬件
│   └── hardware_pinout.md                     # 引脚分配与资源总表
│
├── 认证与门禁
│   ├── 门禁认证系统_代码实现逻辑_2026-02-05.md  # 认证系统核心设计
│   ├── DOOR_CONTROL_USAGE.md                  # 门禁舵机控制 API
│   ├── 用户数据库模块说明.md                   # 用户数据库 API
│   └── changelog_door_sensor_to_servo.md      # 架构变更：门磁→舵机
│
├── 外设驱动
│   ├── RFID_API_Reference.md                  # RFID 驱动 API 参考
│   ├── RFID_Quick_Start.md                    # RFID 快速入门
│   ├── as608_quickstart.md                    # 指纹模块快速入门
│   ├── LCD驱动使用说明.md                      # LCD 绘图接口说明
│   ├── LED_USAGE_GUIDE.md                     # LED 驱动说明
│   └── USART1_DMA使用说明.md                  # UART DMA 三级缓冲接收
│
├── 菜单与 UI
│   ├── SimpleMenu使用说明.md                  # 纯 C 菜单框架说明
│   └── door_status_ui_test_guide.md           # 门禁状态 UI 测试指南
│
└── 网络通信
    ├── esp8266_framework.md                   # ESP8266 WiFi 框架架构
    └── mqtt_app_test_guide.md                 # MQTT 应用测试指南
```

---

## 快速查找

### 按任务查找

| 我想做什么 | 查阅文档 |
|------------|----------|
| 查询引脚占用 / 添加新硬件 | [hardware_pinout.md](hardware_pinout.md) |
| 了解认证系统设计 | [门禁认证系统_代码实现逻辑](门禁认证系统_代码实现逻辑_2026-02-05.md) |
| 控制门锁开关 / 舵机 | [DOOR_CONTROL_USAGE.md](DOOR_CONTROL_USAGE.md) |
| 用户增删查改 | [用户数据库模块说明.md](用户数据库模块说明.md) |
| 使用 RFID 读卡 | [RFID_Quick_Start.md](RFID_Quick_Start.md) |
| RFID API 详情 | [RFID_API_Reference.md](RFID_API_Reference.md) |
| 使用指纹模块 | [as608_quickstart.md](as608_quickstart.md) |
| LCD 绘图 / 显示字符 | [LCD驱动使用说明.md](LCD驱动使用说明.md) |
| 控制 LED | [LED_USAGE_GUIDE.md](LED_USAGE_GUIDE.md) |
| UART 串口接收数据 | [USART1_DMA使用说明.md](USART1_DMA使用说明.md) |
| 添加菜单项 / 子菜单 | [SimpleMenu使用说明.md](SimpleMenu使用说明.md) |
| 测试 LCD UI 显示 | [door_status_ui_test_guide.md](door_status_ui_test_guide.md) |
| 配置 WiFi / MQTT | [esp8266_framework.md](esp8266_framework.md) |
| 测试远程控制 | [mqtt_app_test_guide.md](mqtt_app_test_guide.md) |
| 了解门控历史设计变更 | [changelog_door_sensor_to_servo.md](changelog_door_sensor_to_servo.md) |

### 按模块查找

| 模块 | 代码文件 | 文档 |
|------|----------|------|
| 硬件引脚 | — | [hardware_pinout.md](hardware_pinout.md) |
| 门禁控制 | `door_control.c/h` | [DOOR_CONTROL_USAGE.md](DOOR_CONTROL_USAGE.md) |
| 认证管理 | `auth_manager.c/h` | [门禁认证系统_代码实现逻辑](门禁认证系统_代码实现逻辑_2026-02-05.md) |
| 用户数据库 | `user_database.c/h` | [用户数据库模块说明.md](用户数据库模块说明.md) |
| RFID | `rfid_reader.c/h`, `rfid_task.c/h` | [RFID_API_Reference.md](RFID_API_Reference.md) |
| 指纹识别 | `as608.c/h` | [as608_quickstart.md](as608_quickstart.md) |
| LCD 显示 | `lcd.c/h`, `lcd_init.c/h` | [LCD驱动使用说明.md](LCD驱动使用说明.md) |
| 门禁 UI | `door_status_ui.c/h` | [door_status_ui_test_guide.md](door_status_ui_test_guide.md) |
| 菜单系统 | `simple_menu_c.c/h`, `menu_task.c/h` | [SimpleMenu使用说明.md](SimpleMenu使用说明.md) |
| LED | `led.c/h` | [LED_USAGE_GUIDE.md](LED_USAGE_GUIDE.md) |
| UART DMA | `usart1_dma_rx.c/h` | [USART1_DMA使用说明.md](USART1_DMA使用说明.md) |
| WiFi | `esp8266.c/h`, `esp_at.c/h` | [esp8266_framework.md](esp8266_framework.md) |
| MQTT | `esp8266_mqtt.c/h`, `mqtt_app.c/h` | [mqtt_app_test_guide.md](mqtt_app_test_guide.md) |

---

## 重要约束（开发必读）

| 资源 | 占用情况 | 违反后果 |
|------|----------|----------|
| **TIM2** | 调度器 5ms 基准时钟 | 调度器停止运行 |
| **TIM3** | 舵机 PWM 输出 (50Hz) | 门锁失控 |
| **DMA1_CH5** | USART1 DMA 接收 | 串口数据丢失 |
| **RAM 64KB** | 当前约用 45KB | 谨慎分配大数组 |

---

## 文档更新记录

| 日期 | 版本 | 变更内容 |
|------|------|----------|
| 2026-01-21 | V1.0.0 | 创建文档索引，整理初始文档 |
| 2026-03-11 | V2.0.0 | 清理冗余文档（删除 27 个），重写索引，补充 ESP8266/MQTT/UI 文档 |
