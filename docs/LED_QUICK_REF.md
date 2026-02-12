# LED驱动 - 快速参考

## 🎯 硬件配置

| LED | 引脚 | 极性 |
|-----|------|------|
| LED1 | PC2 | 高电平点亮 |
| LED2 | PC3 | 高电平点亮 |

---

## 🚀 快速开始

```c
#include "led.h"

int main(void)
{
    LED_Init();         // 初始化
    LED_On(LED1);       // 打开LED1
    LED_Off(LED2);      // 关闭LED2
    LED_Toggle(LED1);   // 翻转LED1
}
```

---

## 📋 常用函数速查

| 函数 | 功能 | 示例 |
|------|------|------|
| `LED_Init()` | 初始化所有LED | `LED_Init();` |
| `LED_On(led)` | 打开指定LED | `LED_On(LED1);` |
| `LED_Off(led)` | 关闭指定LED | `LED_Off(LED1);` |
| `LED_Toggle(led)` | 翻转LED状态 | `LED_Toggle(LED1);` |
| `LED_Set(led, state)` | 设置LED状态 | `LED_Set(LED1, LED_ON);` |
| `LED_GetState(led)` | 查询LED状态 | `if(LED_GetState(LED1) == LED_ON)` |
| `LED_AllOn()` | 打开所有LED | `LED_AllOn();` |
| `LED_AllOff()` | 关闭所有LED | `LED_AllOff();` |
| `LED_AllToggle()` | 翻转所有LED | `LED_AllToggle();` |

---

## 💡 典型示例

### 闪烁
```c
while(1) {
    LED_Toggle(LED1);
    Delay_Ms(500);
}
```

### 流水灯
```c
while(1) {
    LED_On(LED1);  LED_Off(LED2);  Delay_Ms(300);
    LED_Off(LED1); LED_On(LED2);   Delay_Ms(300);
}
```

### 定时器控制
```c
void TIM_1ms_Callback(void) {
    static uint16_t count = 0;
    if(++count >= 500) {
        count = 0;
        LED_Toggle(LED1);
    }
}
```

---

## 🎯 枚举类型

```c
LED_TypeDef:    LED1, LED2
LED_StateTypeDef: LED_ON, LED_OFF
```

---

## 📖 详细文档

[LED_USAGE_GUIDE.md](LED_USAGE_GUIDE.md) - 完整使用说明

---

**更新：** 2025-12-29
