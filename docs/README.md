# 项目文档索引

**项目名称**: 门禁控制系统
**主控芯片**: CH32V30x
**文档版本**: V1.0.0
**更新日期**: 2026-01-21

---

## 📚 文档列表

### 1. 硬件相关文档

#### [hardware_pinout.md](hardware_pinout.md)
**硬件引脚连接文档**

- 完整的GPIO引脚分配表
- 外设接口定义 (LCD, 串口, 按键, LED, 舵机)
- 定时器资源分配
- 引脚复用说明
- 硬件连接注意事项

**适用场景**:
- 查询某个引脚的功能
- 添加新硬件模块前检查引脚占用
- 硬件调试时确认接线

---

### 2. 修改记录文档

#### [changelog_door_sensor_to_servo.md](changelog_door_sensor_to_servo.md)
**门磁传感器改为舵机控制 - 完整修改记录**

- 硬件架构变更说明
- 代码修改详情 (逐文件、逐函数)
- 定时器冲突解决方案
- 功能对比表
- 测试要点和注意事项
- 回滚方案

**适用场景**:
- 了解门禁控制模块的演进历史
- 理解为什么使用舵机而不是门磁
- 需要回滚到门磁方案时参考
- 新成员了解项目架构变更

#### [as608_porting.md](as608_porting.md)
**AS608指纹模块移植文档**

- STM32到CH32V30x移植过程
- 硬件连接变更说明
- 代码适配详情
- 使用示例和测试方法
- 常见问题解决方案

**适用场景**:
- 了解AS608指纹模块的移植过程
- 学习如何使用指纹识别功能
- 排查指纹模块相关问题
- 集成其他外设时参考

#### [as608_summary.md](as608_summary.md)
**AS608指纹模块移植总结**

- 移植完成情况总览
- 核心功能清单
- 完整使用示例代码
- 测试清单和注意事项
- 性能指标和后续工作

**适用场景**:
- 快速了解AS608移植成果
- 查看完整的代码示例
- 制定测试计划
- 规划后续开发工作

#### [as608_quickstart.md](as608_quickstart.md)
**AS608指纹模块快速开始指南** ⭐ 推荐新手阅读

- 30分钟快速上手教程
- 硬件连接步骤图解
- 代码集成详细步骤
- 功能测试完整流程
- 常见问题排查方法

**适用场景**:
- 首次使用AS608模块
- 快速验证硬件连接
- 学习基本使用方法
- 排查常见问题

---

## 📂 文档目录结构

```
docs/
├── README.md                              # 本文件 - 文档索引
├── hardware_pinout.md                     # 硬件引脚连接文档
├── changelog_door_sensor_to_servo.md      # 门磁改舵机修改记录
├── as608_porting.md                       # AS608指纹模块移植文档
├── as608_summary.md                       # AS608指纹模块移植总结
└── as608_quickstart.md                    # AS608快速开始指南 ⭐
```

---

## 🔍 快速查找

### 按功能模块查找

| 模块 | 相关文档 | 章节 |
|------|----------|------|
| LCD显示 | hardware_pinout.md | 第1章 |
| 按键输入 | hardware_pinout.md | 第2章 |
| LED指示灯 | hardware_pinout.md | 第3章 |
| 串口通信 | hardware_pinout.md | 第4章 |
| 门禁控制 | hardware_pinout.md<br>changelog_door_sensor_to_servo.md | 第5章<br>完整文档 |
| 指纹识别 | hardware_pinout.md<br>as608_porting.md | 第6章<br>完整文档 |
| 定时器资源 | hardware_pinout.md | 第7章 |

### 按问题类型查找

| 问题 | 查阅文档 |
|------|----------|
| 某个引脚是什么功能？ | hardware_pinout.md - 第7章引脚汇总表 |
| 定时器TIM2/TIM3用途？ | hardware_pinout.md - 第7章定时器资源 |
| 为什么不用门磁传感器？ | changelog_door_sensor_to_servo.md - 修改概述 |
| 舵机如何接线？ | hardware_pinout.md - 第5章<br>changelog_door_sensor_to_servo.md - 测试要点 |
| 如何添加新外设？ | hardware_pinout.md - 第7章 (检查引脚占用) |
| 代码中为什么没有门磁相关函数？ | changelog_door_sensor_to_servo.md - 代码修改详情 |
| 如何使用指纹识别？ | as608_porting.md - 使用示例 |
| 指纹模块握手失败？ | as608_porting.md - 常见问题 |
| 如何录入/验证指纹？ | as608_porting.md - 使用示例 |

---

## 📝 文档维护规范

### 1. 硬件变更时

**必须更新**: `hardware_pinout.md`

**更新内容**:
- 引脚分配表
- 外设资源使用汇总
- 修改记录章节

**示例**:
```markdown
## 10. 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|------|------|----------|--------|
| V1.2.0 | 2026-01-21 | 添加RFID模块 (SPI1, PA4-PA7) | 张三 |
```

### 2. 重大架构变更时

**必须创建**: 新的 changelog 文档

**命名规范**: `changelog_<变更描述>_<日期>.md`

**包含内容**:
- 变更原因和背景
- 详细的修改记录
- 测试要点
- 回滚方案

### 3. 文档索引更新

每次添加新文档后，更新本 `README.md` 文件：
- 在"文档列表"中添加新文档
- 更新"文档目录结构"
- 更新"快速查找"表格

---

## 🛠️ 代码与文档对应关系

| 代码文件 | 相关文档 | 说明 |
|----------|----------|------|
| `User/lcd_init.h/c` | hardware_pinout.md 第1章 | LCD引脚定义 |
| `User/key_app.h/c` | hardware_pinout.md 第2章 | 按键引脚定义 |
| `User/led.h/c` | hardware_pinout.md 第3章 | LED引脚定义 |
| `User/usart1_dma_rx.h/c` | hardware_pinout.md 第4章 | 串口引脚定义 |
| `User/door_control.h/c` | hardware_pinout.md 第5章<br>changelog_door_sensor_to_servo.md | 门禁控制 |
| `User/as608.h/c` | hardware_pinout.md 第6章<br>as608_porting.md | 指纹识别 |
| `User/timer_config.h/c` | hardware_pinout.md 第7章 | 定时器配置 |
| `User/menu_task.c` | changelog_door_sensor_to_servo.md | 菜单显示 |

---

## ⚠️ 重要提醒

1. **修改硬件前必读**: `hardware_pinout.md` - 检查引脚占用情况
2. **定时器使用前必读**: `hardware_pinout.md` 第6章 - 避免资源冲突
3. **理解门禁模块必读**: `changelog_door_sensor_to_servo.md` - 了解设计决策

---

## 📞 文档反馈

如发现文档错误或需要补充内容，请：
1. 在项目issue中提出
2. 或直接修改文档并提交PR
3. 记得更新"修改记录"章节

---

## 📅 文档更新记录

| 日期 | 修改内容 | 修改人 |
|------|----------|--------|
| 2026-01-21 | 创建文档索引，整理现有文档 | AI Assistant |
| 2026-01-21 | 添加AS608指纹模块移植文档 | AI Assistant |
| 2026-01-21 | 添加AS608移植总结和快速开始指南 | AI Assistant |

---

**文档维护**: 所有项目相关文档统一存放在 `docs/` 文件夹中。
