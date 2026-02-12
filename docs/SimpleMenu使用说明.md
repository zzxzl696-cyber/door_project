# 精简菜单系统使用说明

## 概述

这是一个专门为CH32V30x平台优化的精简菜单系统，基于Adafruit GFX兼容层实现。相比完整的GEM库，这个系统更轻量、更易于集成。

## 特性

- ✅ 支持int和bool类型变量编辑
- ✅ 基于Adafruit GFX，与现有LCD驱动完美兼容
- ✅ 精简设计，Flash占用小（约5-8KB）
- ✅ 简单易用的API
- ✅ 支持上下导航和值编辑

## 文件结构

```
User/
├── SimpleMenu.h          # 菜单系统头文件
├── SimpleMenu.cpp        # 菜单系统实现
├── menu_task.h           # 菜单任务头文件
├── menu_task.cpp         # 菜单任务实现
├── CH32_ST7735.h         # LCD驱动包装类
├── CH32_ST7735.cpp
├── Adafruit_GFX.h        # GFX基类
└── Adafruit_GFX.cpp
```

## 快速开始

### 1. 初始化菜单

在你的main函数中调用：

```c
#include "menu_task.h"

int main(void)
{
    // ... 其他初始化代码 ...

    menu_init();  // 初始化菜单系统

    // ... 主循环 ...
}
```

### 2. 添加到调度器

在你的调度器中添加菜单任务：

```c
static task_t scheduler_task[] = {
    {uart_rx_task, 5, 0},          // UART: 5ms周期
    {menu_input_task, 20, 0},      // 菜单输入: 20ms周期
    {menu_update_task, 100, 0}     // 菜单更新: 100ms周期
};
```

### 3. 实现按键检测

在 `menu_task.cpp` 的 `menu_input_task()` 函数中实现按键检测：

```cpp
void menu_input_task(void)
{
    if (!menu) return;

    // 检测按键
    uint8_t key = KEY_NONE;

    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_5) == 0) {
        key = KEY_UP;
    } else if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_2) == 0) {
        key = KEY_DOWN;
    } else if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7) == 0) {
        key = KEY_OK;
    } else if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_6) == 0) {
        key = KEY_CANCEL;
    }

    if (key != KEY_NONE) {
        menu->handleKey(key);
    }
}
```

## API参考

### 按键定义

```cpp
#define KEY_NONE   0  // 无按键
#define KEY_UP     1  // 上键（向上导航/增加值）
#define KEY_DOWN   2  // 下键（向下导航/减少值）
#define KEY_OK     3  // 确认键（进入编辑/切换bool）
#define KEY_CANCEL 4  // 取消键（退出编辑）
```

### 菜单项类型

```cpp
#define ITEM_INT   0  // 整数类型
#define ITEM_BOOL  1  // 布尔类型
#define ITEM_LINK  2  // 链接类型（预留）
```

### 创建菜单项

```cpp
// 整数类型
static int my_value = 100;
static MenuItem item1 = {"Value:", ITEM_INT, &my_value, NULL, NULL};

// 布尔类型
static bool my_bool = false;
static MenuItem item2 = {"Enable:", ITEM_BOOL, &my_bool, NULL, NULL};
```

### SimpleMenu类方法

```cpp
// 初始化菜单
void init();

// 绘制菜单
void draw();

// 处理按键
void handleKey(uint8_t key);

// 添加菜单项
void addItem(MenuItem* item);
```

## 操作说明

### 导航模式（默认）

- **KEY_UP**: 向上移动光标
- **KEY_DOWN**: 向下移动光标
- **KEY_OK**: 进入编辑模式

### 编辑模式

**整数类型 (ITEM_INT)**:
- **KEY_UP**: 增加值
- **KEY_DOWN**: 减少值
- **KEY_CANCEL**: 退出编辑

**布尔类型 (ITEM_BOOL)**:
- **KEY_OK**: 切换ON/OFF
- **KEY_CANCEL**: 退出编辑

## 自定义

### 修改显示样式

在 `SimpleMenu.cpp` 中修改 `drawItem()` 函数：

```cpp
void SimpleMenu::drawItem(MenuItem* item, uint8_t y, bool selected) {
    if (selected) {
        _gfx.fillRect(0, y-2, 128, 12, GFX_BLUE);  // 修改选中颜色
        _gfx.setTextColor(GFX_WHITE);
    } else {
        _gfx.setTextColor(GFX_WHITE);
    }

    // ... 其他绘制代码 ...
}
```

### 添加更多菜单项

在 `menu_task.cpp` 中添加：

```cpp
// 添加新变量
static int new_value = 50;

// 创建新菜单项
static MenuItem item4 = {"New:", ITEM_INT, &new_value, NULL, NULL};

// 在menu_init()中添加
void menu_init(void)
{
    // ... 现有代码 ...

    menu->addItem(&item4);  // 添加新项

    // ... 其他代码 ...
}
```

## 资源占用

- **Flash**: 约5-8KB（包含Adafruit GFX基类）
- **RAM**: 约1-2KB（运行时）
- **屏幕**: 128x160 RGB565

## 下一步

1. 实现按键检测函数
2. 根据需要添加更多菜单项
3. 自定义显示样式
4. 添加多级菜单支持（可选）

## 注意事项

- 确保在 `main()` 之后调用 `menu_init()`
- 按键检测需要防抖处理
- LCD操作较慢，建议在中断中禁用LCD刷新
- 菜单变量必须是静态或全局变量

## 故障排除

**问题**: 菜单不显示
- 检查LCD是否正确初始化
- 检查 `menu_init()` 是否被调用
- 检查串口输出是否有错误信息

**问题**: 按键无响应
- 检查按键检测函数是否正确实现
- 检查GPIO配置是否正确
- 添加printf调试按键值

**问题**: 编译错误
- 确保所有文件都添加到工程中
- 检查C++编译器是否启用
- 检查头文件路径是否正确
