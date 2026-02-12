# ST7735S LCD驱动 - 快速参考卡片 (硬件SPI版本)

## 📌 引脚连接（CH32V30x）

| LCD | CH32V | 说明 | 备注 |
|-----|-------|------|------|
| VCC | 3.3V | 供电 | - |
| GND | GND | 接地 | - |
| **SCL** | **PB13** | SPI2时钟 | SPI2_SCK (硬件SPI) |
| **SDA** | **PB15** | SPI2数据 | SPI2_MOSI (硬件SPI) |
| **RES** | **PB10** | 复位 | GPIO控制 |
| **DC** | **PB14** | 数据/命令选择 | GPIO控制 |
| **CS** | **PB12** | 片选 | GPIO控制 |
| **BLK** | **PB11** | 背光 | GPIO控制 |

**⚡ 性能提升**: 使用硬件SPI2，速度从~500kHz提升至24MHz，刷新速度提升约**48倍**!

---

## 🚀 快速开始（3步）

### 1. 添加文件到编译

```
User/lcd_init.c
User/lcd.c
User/lcd_test_task.c
```

### 2. 初始化（main.c）

```c
#include "lcd_init.h"

LCD_Init();  // 初始化LCD
```

### 3. 绘图

```c
#include "lcd.h"

// 清屏为红色
LCD_Fill(0, 0, LCD_W, LCD_H, RED);

// 画圆
Draw_Circle(64, 80, 30, BLUE);
```

---

## 🎨 常用函数速查

### 填充与清屏

```c
LCD_Fill(x1, y1, x2, y2, color);        // 填充矩形
LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);    // 清屏为白色
```

### 绘制图形

```c
LCD_DrawPoint(x, y, color);                      // 画点
LCD_DrawLine(x1, y1, x2, y2, color);             // 画线
LCD_DrawRectangle(x1, y1, x2, y2, color);        // 画矩形框
Draw_Circle(x0, y0, radius, color);              // 画圆
```

### 显示文字/数字

```c
LCD_ShowIntNum(x, y, num, len, fc, bc, size);     // 显示整数
LCD_ShowFloatNum1(x, y, num, len, fc, bc, size);  // 显示浮点数
LCD_ShowString(x, y, "text", fc, bc, size, mode); // 显示字符串
```

---

## 🌈 颜色代码（RGB565）

```c
WHITE    0xFFFF    BLACK    0x0000
RED      0xF800    GREEN    0x07E0
BLUE     0x001F    YELLOW   0xFFE0
CYAN     0x7FFF    MAGENTA  0xF81F
GRAY     0X8430    BROWN    0XBC40
```

---

## 📏 屏幕参数

- **分辨率**：128×160像素
- **宽度**：`LCD_W` = 128
- **高度**：`LCD_H` = 160
- **颜色**：RGB565（65K色）

---

## 🔧 配置选项

### 显示方向（lcd_init.h）

```c
#define USE_HORIZONTAL 0  // 0=竖屏, 2=横屏
```

### 调试开关（lcd_test_task.c）

```c
// 禁用测试任务（scheduler.c）
// {lcd_test_task, 1000, 0},  // 注释掉这行
```

---

## 📖 示例代码

### 绘制界面

```c
void my_screen(void)
{
    LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);           // 清屏
    LCD_Fill(0, 0, LCD_W, 30, BLUE);               // 蓝色标题栏
    LCD_DrawRectangle(2, 32, 125, 157, BLUE);      // 边框
    LCD_ShowIntNum(20, 40, 12345, 5, RED, WHITE, 16); // 数字
}
```

### 进度条

```c
void progress(uint8_t percent)
{
    uint16_t w = (LCD_W - 20) * percent / 100;
    LCD_DrawRectangle(10, 140, LCD_W-10, 155, BLUE);
    LCD_Fill(11, 141, 11 + w, 154, GREEN);
}
```

---

## 🐛 快速诊断

### LCD无显示

```c
// 1. 检查背光
GPIO_SetBits(GPIOB, GPIO_Pin_11);  // 手动点亮背光

// 2. 测试填充
LCD_Fill(0, 0, LCD_W, LCD_H, RED);  // 应显示红屏
```

### 显示异常

```c
// 降低SPI速度（在LCD_Writ_Bus中添加延时）
Delay_Us(1);
```

---

## 📞 更多信息

详细文档：[docs/LCD驱动使用说明.md](LCD驱动使用说明.md)

---

**版本**：v1.0 | **日期**：2025-12-30
