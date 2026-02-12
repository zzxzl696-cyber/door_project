# GEM图形化界面库移植方案

## 📋 项目概述

**目标**: 将 [GEM (Good Enough Menu)](https://github.com/Spirik/GEM) 图形化菜单库从Arduino移植到CH32V30x平台

**GEM库简介**:
- 专业的图形化多级菜单库
- 支持可编辑变量 (int, byte, float, double, bool, char[17])
- 支持下拉选择、数字微调器、按钮、链接等控件
- 原生支持U8g2、Adafruit GFX、AltSerialGraphicLCD三种图形库
- 可自定义回调函数、上下文切换等高级功能

---

## 🎯 移植目标

### 当前项目状态
- ✅ 硬件平台: CH32V30x (RISC-V 96MHz)
- ✅ 已有LCD驱动: ST7735S 1.8" 128×160 RGB565
- ✅ 通信接口: 硬件SPI2 (24MHz)
- ✅ 已有框架: 任务调度器 + 1ms定时器

### 移植后期望达到的效果
- ✅ 在ST7735S LCD上显示多级图形化菜单
- ✅ 通过按键(6个按键)进行菜单导航和参数编辑
- ✅ 可配置系统参数(如UART波特率、定时器周期等)
- ✅ 支持变量实时编辑和保存
- ✅ 集成到现有调度器框架中

---

## 🔍 技术分析

### 1. GEM库架构

GEM库由三个核心类组成:

```
GEM (主控制器)
 ├─ GEMPage (菜单页面)
 │   └─ GEMItem (菜单项)
 │       ├─ 变量 (int/byte/float/bool/char[])
 │       ├─ 按钮 (执行函数)
 │       ├─ 选择器 (GEMSelect)
 │       ├─ 微调器 (GEMSpinner)
 │       └─ 链接 (跳转到其他页面)
 └─ GEMContext (按钮动作上下文)
```

### 2. 支持的图形库对比

| 图形库 | 优势 | 劣势 | 适用性 |
|--------|------|------|--------|
| **Adafruit GFX** | 支持彩色LCD、API简单 | 功能相对基础 | ⭐⭐⭐⭐⭐ **推荐** |
| U8g2 | 功能强大、支持多种显示器 | 主要为单色屏设计 | ⭐⭐⭐ 可选 |
| AltSerialGraphicLCD | 专用硬件 | 需要特殊backpack | ❌ 不适用 |

**选择建议**: 使用 **Adafruit GFX** 版本

**理由**:
1. 您已经有了ST7735驱动(基于Adafruit GFX标准)
2. 支持彩色显示,可自定义前景/背景色
3. API简单易用,与现有LCD驱动兼容性好
4. GEM库原生支持,无需额外适配

### 3. 依赖关系分析

```
GEM_adafruit_gfx (核心)
 ├─ Adafruit_GFX (图形库基础类)
 ├─ Adafruit_ST7735 (LCD驱动)
 ├─ KeyDetector (可选,按键检测)
 └─ CH32V30x HAL (硬件抽象层)
```

---

## 📐 移植方案设计

### 方案一: 直接适配Adafruit GFX接口 (推荐)

**核心思路**: 创建一个兼容Adafruit GFX接口的LCD驱动包装类

#### 实现步骤

**阶段1: 基础驱动包装**

1. 创建 `CH32_Adafruit_GFX` 基类
   - 继承或实现 Adafruit GFX 接口
   - 映射到您现有的lcd.h/lcd_init.h函数

2. 实现必需的绘图函数:
   ```cpp
   class CH32_ST7735 : public Adafruit_GFX {
   public:
       void drawPixel(int16_t x, int16_t y, uint16_t color);
       void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
       void fillScreen(uint16_t color);
       void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
       void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
   };
   ```

3. 映射现有函数:
   ```cpp
   // 您现有的: LCD_Fill(x1, y1, x2, y2, color);
   // 映射为: fillRect(x, y, w, h, color);

   // 您现有的: LCD_DrawPoint(x, y, color);
   // 映射为: drawPixel(x, y, color);
   ```

**阶段2: GEM库集成**

1. 移植GEM核心文件:
   - `GEM_adafruit_gfx.h/cpp` (主控制器)
   - `GEMPage.h/cpp` (菜单页面)
   - `GEMItem.h/cpp` (菜单项)
   - `constants.h` (常量定义)

2. 修改编译配置:
   - 禁用U8g2支持
   - 禁用AltSerialGraphicLCD支持
   - 仅启用Adafruit GFX支持

3. 适配字体系统:
   - 使用GFX内置字体或自定义字体
   - 调整字体大小参数

**阶段3: 按键输入集成**

1. 按键映射:
   ```cpp
   GEM_KEY_UP      → 上键 (PB5)
   GEM_KEY_DOWN    → 下键 (PB2)
   GEM_KEY_LEFT    → 左键 (PB3)
   GEM_KEY_RIGHT   → 右键 (PB4)
   GEM_KEY_OK      → 确认键 (PB7)
   GEM_KEY_CANCEL  → 取消键 (PB6)
   ```

2. 按键检测任务:
   ```cpp
   void menu_input_task(void) {
       if (menu.readyForKey()) {
           byte keyCode = detectKeyPress();
           if (keyCode != GEM_KEY_NONE) {
               menu.registerKeyPress(keyCode);
           }
       }
   }
   ```

**阶段4: 调度器集成**

```cpp
static task_t scheduler_task[] = {
    {uart_rx_task, 5, 0},      // UART: 5ms周期
    {lcd_test_task, 1000, 0},  // LCD测试: 1000ms (可禁用)
    {menu_input_task, 20, 0},  // 菜单按键: 20ms周期
    {menu_update_task, 100, 0} // 菜单更新: 100ms周期
};
```

#### 优势
- ✅ 最小化代码改动
- ✅ 复用现有LCD驱动
- ✅ GEM功能完整保留
- ✅ 后期维护简单

#### 劣势
- ⚠️ 需要创建Adafruit GFX兼容层
- ⚠️ 增加一层抽象(性能损失<5%)

---

### 方案二: 从零实现GEM核心逻辑 (不推荐)

**核心思路**: 完全重写GEM菜单系统,直接调用您的LCD API

#### 优势
- ✅ 代码完全定制化
- ✅ 无额外抽象层

#### 劣势
- ❌ 工作量巨大(预计2000+行代码)
- ❌ 缺少社区支持
- ❌ 维护成本高
- ❌ 功能可能不完整

**结论**: 除非有特殊需求,否则不建议此方案

---

## 📦 文件结构规划

### 新增文件清单

```
User/
├── adafruit_gfx/               # Adafruit GFX兼容层
│   ├── Adafruit_GFX.h          # GFX基类
│   ├── Adafruit_GFX.cpp
│   ├── CH32_ST7735.h           # CH32的ST7735驱动包装
│   ├── CH32_ST7735.cpp
│   └── gfxfont.h               # 字体结构定义
│
├── gem/                        # GEM库核心
│   ├── GEM_adafruit_gfx.h      # GEM主类
│   ├── GEM_adafruit_gfx.cpp
│   ├── GEMPage.h               # 菜单页面
│   ├── GEMPage.cpp
│   ├── GEMItem.h               # 菜单项
│   ├── GEMItem.cpp
│   ├── GEMSelect.h             # 选择器
│   ├── GEMSelect.cpp
│   ├── constants.h             # 常量定义
│   └── config.h                # 配置文件
│
├── menu_config.h               # 菜单配置(变量、页面等)
├── menu_config.c
├── menu_task.h                 # 菜单任务
└── menu_task.c

docs/
└── GEM图形化界面使用说明.md   # 使用文档
```

---

## 🔧 关键技术点

### 1. Adafruit GFX接口适配

**最小必需接口**:

```cpp
class CH32_ST7735 : public Adafruit_GFX {
public:
    CH32_ST7735(int16_t w, int16_t h);

    // 必须实现的纯虚函数
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;

    // 优化函数(可选但推荐)
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
    void fillScreen(uint16_t color) override;

    // 初始化
    void begin(void);
};
```

**映射示例**:

```cpp
void CH32_ST7735::drawPixel(int16_t x, int16_t y, uint16_t color) {
    LCD_DrawPoint(x, y, color);  // 直接调用您的函数
}

void CH32_ST7735::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    LCD_Fill(x, y, x+w-1, y+h-1, color);  // 坐标转换
}

void CH32_ST7735::fillScreen(uint16_t color) {
    LCD_Fill(0, 0, LCD_W-1, LCD_H-1, color);
}
```

### 2. 内存优化策略

**GEM默认配置可能占用较多RAM,需要优化**:

```cpp
// config.h 配置
#define GEM_DISABLE_GLCD              // 禁用AltSerialGraphicLCD
#define GEM_DISABLE_U8G2              // 禁用U8g2
#define GEM_DISABLE_FLOAT_EDIT        // 禁用浮点编辑(可选,节省10% Flash)
#define GEM_DISABLE_SPINNER           // 禁用微调器(如不需要)
#define GEM_DISABLE_PREVIEW_CALLBACKS // 禁用预览回调(如不需要)
```

**预计资源占用**:
- Flash: 约15-20KB (完整功能)
- RAM: 约2-3KB (运行时)
- 优化后Flash: 约10-12KB

### 3. 按键防抖和长按处理

```cpp
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    uint8_t key_code;
    uint8_t state;
    uint16_t debounce_cnt;
    uint16_t long_press_cnt;
} button_t;

#define DEBOUNCE_TIME_MS  20
#define LONG_PRESS_TIME_MS 1000

byte detectKeyPress(void) {
    static button_t buttons[6] = {
        {GPIOB, GPIO_Pin_5, GEM_KEY_UP, 0, 0, 0},
        {GPIOB, GPIO_Pin_2, GEM_KEY_DOWN, 0, 0, 0},
        // ... 其他按键
    };

    // 20ms周期调用此函数
    for (int i = 0; i < 6; i++) {
        uint8_t current = GPIO_ReadInputDataBit(buttons[i].port, buttons[i].pin);

        // 防抖逻辑
        if (current != buttons[i].state) {
            buttons[i].debounce_cnt++;
            if (buttons[i].debounce_cnt > DEBOUNCE_TIME_MS / 20) {
                buttons[i].state = current;
                buttons[i].debounce_cnt = 0;
                if (current == 1) {  // 按下
                    return buttons[i].key_code;
                }
            }
        } else {
            buttons[i].debounce_cnt = 0;
        }
    }
    return GEM_KEY_NONE;
}
```

### 4. 菜单示例配置

```cpp
// 全局变量
int uart_baudrate = 115200;
bool enable_debug = false;
int timer_period = 1000;

// 创建菜单对象
CH32_ST7735 tft(LCD_W, LCD_H);
GEM_adafruit_gfx menu(tft, GEM_POINTER_ROW, GEM_ITEMS_COUNT_AUTO);

// 创建菜单项
GEMItem menuItemBaudrate("Baudrate:", uart_baudrate);
GEMItem menuItemDebug("Debug:", enable_debug);
GEMItem menuItemTimer("Timer(ms):", timer_period);

// 创建菜单页
GEMPage menuPageMain("Main Menu");
GEMPage menuPageSettings("Settings");

void setupMenu() {
    // 主菜单
    menuPageMain.addMenuItem(menuItemSettings);  // 跳转到设置页

    // 设置页
    menuPageSettings.addMenuItem(menuItemBaudrate);
    menuPageSettings.addMenuItem(menuItemDebug);
    menuPageSettings.addMenuItem(menuItemTimer);
    menuPageSettings.setParentMenuPage(menuPageMain);

    // 初始化
    menu.init();
    menu.setMenuPageCurrent(menuPageMain);
    menu.drawMenu();
}
```

---

## ⏱️ 开发时间估算

### 阶段1: Adafruit GFX兼容层 (2-3天)
- [ ] 创建CH32_ST7735类 (1天)
- [ ] 实现基础绘图函数 (1天)
- [ ] 测试和调试 (1天)

### 阶段2: GEM库移植 (3-4天)
- [ ] 移植GEM核心文件 (1天)
- [ ] 配置编译选项 (0.5天)
- [ ] 适配字体系统 (0.5天)
- [ ] 测试基础菜单显示 (2天)

### 阶段3: 功能集成 (2-3天)
- [ ] 按键输入集成 (1天)
- [ ] 调度器集成 (0.5天)
- [ ] 创建示例菜单 (1天)
- [ ] 完整功能测试 (1天)

### 阶段4: 优化和文档 (1-2天)
- [ ] 内存优化 (0.5天)
- [ ] 性能优化 (0.5天)
- [ ] 编写使用文档 (1天)

**总计: 8-12天** (根据您的熟练程度和遇到的问题而定)

---

## 📊 风险评估

| 风险 | 可能性 | 影响 | 应对方案 |
|------|--------|------|----------|
| Adafruit GFX接口不兼容 | 低 | 高 | 详细阅读GFX文档,参考Arduino实现 |
| 内存不足 | 中 | 高 | 禁用不需要的功能,优化配置 |
| 字体显示问题 | 中 | 中 | 使用内置字体,测试各种字号 |
| 按键响应慢 | 低 | 低 | 优化按键扫描周期 |
| 与现有代码冲突 | 低 | 中 | 模块化设计,独立命名空间 |

---

## 🎯 第一步行动建议

### 快速验证方案 (1天内完成)

在正式开始之前,建议先做一个快速原型验证:

1. **创建最简单的Adafruit GFX兼容类**
   ```cpp
   class CH32_ST7735_Mini : public Adafruit_GFX {
   public:
       void drawPixel(int16_t x, int16_t y, uint16_t color) {
           LCD_DrawPoint(x, y, color);
       }
   };
   ```

2. **测试GEM基础功能**
   ```cpp
   CH32_ST7735_Mini tft(LCD_W, LCD_H);
   tft.fillScreen(0x0000);
   tft.setCursor(0, 0);
   tft.setTextColor(0xFFFF);
   tft.print("GEM Test");
   ```

3. **创建单页菜单测试**
   ```cpp
   int testVar = 100;
   GEMItem item("Value:", testVar);
   GEMPage page("Test");
   page.addMenuItem(item);
   ```

**验证成功标志**:
- ✅ 编译通过
- ✅ LCD能显示文字
- ✅ GEM菜单能渲染

如果验证成功,说明方案可行,可以继续完整移植!

---

## 📚 参考资料

1. **GEM官方资源**:
   - GitHub仓库: https://github.com/Spirik/GEM
   - Wiki文档: https://github.com/Spirik/GEM/wiki
   - 示例代码: examples/AdafruitGFX/

2. **Adafruit GFX**:
   - 库文档: https://learn.adafruit.com/adafruit-gfx-graphics-library
   - API参考: https://adafruit.github.io/Adafruit-GFX-Library/

3. **您的现有代码**:
   - LCD驱动: [User/lcd.h](../User/lcd.h), [User/lcd_init.h](../User/lcd_init.h)
   - 调度器: [User/scheduler.h](../User/scheduler.h)

---

## ✅ 总结

**推荐方案**: 方案一 - 直接适配Adafruit GFX接口

**核心优势**:
1. 工作量适中(8-12天)
2. 复用现有LCD驱动
3. 保留GEM完整功能
4. 社区支持良好

**下一步**:
1. 确认是否采用此方案
2. 进行快速原型验证(1天)
3. 如果验证通过,开始完整移植

**需要您确认的问题**:
1. 是否接受8-12天的开发周期?
2. Flash和RAM资源是否充足(需要额外15-20KB Flash + 2-3KB RAM)?
3. 是否需要所有GEM功能(浮点编辑、微调器等)?
4. 菜单需要哪些功能(参数配置、数据查看、功能执行)?

请告诉我您的决定,我们可以开始实施!
