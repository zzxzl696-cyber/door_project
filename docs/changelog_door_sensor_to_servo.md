# 门磁传感器改为舵机控制 - 修改记录

**修改日期**: 2026-01-21
**修改类型**: 硬件架构变更
**影响范围**: 门禁控制模块、菜单显示模块

---

## 修改概述

将门禁系统从"门磁传感器检测门状态"改为"舵机PWM控制门锁"，完全移除门磁传感器相关代码。

### 修改原因

1. 用户需求：使用舵机直接控制门锁机构，不再需要门磁传感器检测门开关状态
2. 简化硬件：减少传感器数量，降低系统复杂度
3. 解决冲突：原TIM2被系统定时器和舵机PWM同时占用，需要分离

---

## 硬件变更

### 变更前

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| 门锁控制 | PB0 | GPIO输出 | 电磁锁控制 |
| 门磁传感器 | PB1 | GPIO输入 | 检测门开关状态 |

### 变更后

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| 舵机PWM | PA6 | TIM3_CH1 | 舵机角度控制 (0-180度) |

**舵机参数**:
- PWM频率: 50Hz (20ms周期)
- 锁定角度: 0度 (脉宽500μs)
- 解锁角度: 90度 (脉宽1500μs)
- 定时器: TIM3 (避免与TIM2系统定时器冲突)

---

## 代码修改详情

### 1. door_control.h

**移除内容**:
```c
// 门磁状态枚举
typedef enum {
    DOOR_CLOSED = 0,
    DOOR_OPENED = 1
} door_sensor_state_t;
```

**新增内容**:
```c
// 舵机角度定义
#define SERVO_ANGLE_LOCKED   0      // 锁定角度(0度)
#define SERVO_ANGLE_UNLOCKED 90     // 解锁角度(90度)
```

**结构体修改**:
```c
// 修改前
typedef struct {
    door_lock_state_t lock_state;
    door_sensor_state_t sensor_state;  // 门磁状态
    ...
} door_control_status_t;

// 修改后
typedef struct {
    door_lock_state_t lock_state;
    uint8_t servo_angle;               // 舵机当前角度
    ...
} door_control_status_t;
```

**函数接口变更**:
- ❌ 移除: `const char* door_control_get_sensor_state_str(void)`
- ✅ 保留: 其他接口不变

---

### 2. door_control.c

**GPIO配置变更**:
```c
// 修改前 (门磁GPIO)
#define DOOR_LOCK_GPIO_PORT     GPIOB
#define DOOR_LOCK_GPIO_PIN      GPIO_Pin_0
#define DOOR_SENSOR_GPIO_PORT   GPIOB
#define DOOR_SENSOR_GPIO_PIN    GPIO_Pin_1

// 修改后 (舵机PWM)
#define SERVO_TIM               TIM3
#define SERVO_TIM_CHANNEL       TIM_Channel_1
#define SERVO_GPIO_PORT         GPIOA
#define SERVO_GPIO_PIN          GPIO_Pin_6
#define SERVO_TIM_RCC           RCC_APB1Periph_TIM3
#define SERVO_GPIO_RCC          RCC_APB2Periph_GPIOA
```

**新增函数**:
- `static void servo_pwm_init(void)` - 初始化TIM3 PWM
- `static void servo_set_angle(uint8_t angle)` - 设置舵机角度

**移除函数**:
- `static void door_gpio_init(void)` - 门磁GPIO初始化
- `static void door_lock_hardware_control(uint8_t unlock)` - 电磁锁控制
- `static door_sensor_state_t door_sensor_read(void)` - 门磁读取
- `const char* door_control_get_sensor_state_str(void)` - 门磁状态字符串

**修改函数**:

#### door_control_init()
```c
// 修改前
door_gpio_init();
door_lock_hardware_control(0);

// 修改后
servo_pwm_init();
servo_set_angle(SERVO_ANGLE_LOCKED);
```

#### door_control_unlock()
```c
// 修改前
door_lock_hardware_control(1);

// 修改后
servo_set_angle(SERVO_ANGLE_UNLOCKED);
```

#### door_control_lock()
```c
// 修改前
door_lock_hardware_control(0);

// 修改后
servo_set_angle(SERVO_ANGLE_LOCKED);
```

#### door_control_update()
```c
// 移除门磁检测逻辑
// 移除异常告警逻辑 (门锁定但门打开)
// 保留自动锁定逻辑
```

---

### 3. menu_task.c

**显示变量修改**:
```c
// 修改前
static char door_sensor_status_buf[16] = "Closed";

// 修改后
static char door_servo_angle_buf[16] = "0";
```

**菜单项修改**:
```c
// 修改前
static menu_item_t item_door_sensor = {
    "Door:", MENU_ITEM_STRING, door_sensor_status_buf, NULL, NULL
};

// 修改后
static menu_item_t item_door_servo = {
    "Servo:", MENU_ITEM_STRING, door_servo_angle_buf, NULL, NULL
};
```

**状态更新函数修改**:
```c
// menu_update_door_status() 函数

// 修改前
const char* sensor_str = door_control_get_sensor_state_str();
snprintf(door_sensor_status_buf, sizeof(door_sensor_status_buf), "%s", sensor_str);

// 修改后
snprintf(door_servo_angle_buf, sizeof(door_servo_angle_buf), "%d deg", g_door_status.servo_angle);
```

---

## 定时器资源分配

### 修改前 (冲突)
```
TIM2 → 系统定时器 (1ms) + 舵机PWM (50Hz)  ⚠️ 冲突
```

### 修改后 (已解决)
```
TIM2 → 系统定时器 (1ms)
TIM3 → 舵机PWM (50Hz)
```

---

## 功能对比

| 功能 | 修改前 | 修改后 |
|------|--------|--------|
| 门锁控制 | GPIO控制电磁锁 | PWM控制舵机角度 |
| 门状态检测 | 门磁传感器 (开/关) | 舵机角度反馈 (0-180度) |
| 异常检测 | 门锁定但门打开告警 | 无 (舵机直接控制) |
| 自动锁定 | ✅ 支持 | ✅ 支持 |
| 菜单显示 | 门磁状态 (Open/Closed) | 舵机角度 (0-180 deg) |

---

## 测试要点

### 硬件测试

1. **舵机接线检查**
   ```
   舵机信号线(黄/白) → PA6
   舵机VCC(红)       → 5V独立电源
   舵机GND(黑/棕)    → GND (与主控共地)
   ```

2. **PWM信号测试**
   - 使用示波器测量PA6引脚PWM信号
   - 频率应为50Hz (20ms周期)
   - 锁定时脉宽: 500μs
   - 解锁时脉宽: 1500μs

3. **舵机动作测试**
   - 上电后舵机应转到0度 (锁定位置)
   - 调用解锁函数，舵机应转到90度
   - 调用锁定函数，舵机应转回0度

### 软件测试

1. **初始化测试**
   ```c
   door_control_init();
   // 检查串口输出: "[SERVO] PWM initialized (50Hz, PA6/TIM3_CH1)"
   ```

2. **解锁/锁定测试**
   ```c
   door_control_unlock(AUTH_PASSWORD, 5000);  // 解锁5秒
   Delay_Ms(6000);
   // 应自动锁定
   ```

3. **菜单显示测试**
   ```c
   menu_update_door_status();
   // LCD应显示: "Servo: 0 deg" 或 "Servo: 90 deg"
   ```

---

## 注意事项

### 1. 电源要求
- ⚠️ 舵机需要独立5V电源，电流需求500mA-1A
- ⚠️ 不要直接从MCU的3.3V或5V引脚供电，会导致电压不稳

### 2. 角度调整
如果舵机锁定/解锁角度不合适，修改以下宏定义：
```c
// door_control.h
#define SERVO_ANGLE_LOCKED   0      // 可改为其他角度
#define SERVO_ANGLE_UNLOCKED 90     // 可改为其他角度
```

### 3. 脉宽校准
不同品牌舵机脉宽范围可能不同，如需调整：
```c
// door_control.c
#define SERVO_PULSE_MIN         500     // 最小脉宽(us)
#define SERVO_PULSE_MAX         2500    // 最大脉宽(us)
```

### 4. 定时器冲突
- ✅ TIM2已分配给系统定时器
- ✅ TIM3已分配给舵机PWM
- ⚠️ 如需使用TIM3的其他通道，注意不要影响CH1

---

## 回滚方案

如需恢复门磁传感器方案，参考以下步骤：

1. 恢复 `door_control.h` 中的 `door_sensor_state_t` 枚举
2. 恢复 `door_control.c` 中的门磁GPIO初始化代码
3. 恢复 `door_control_get_sensor_state_str()` 函数
4. 恢复 `menu_task.c` 中的门磁显示变量
5. 恢复硬件连接: PB0(门锁), PB1(门磁)

---

## 相关文档

- [硬件引脚连接文档](hardware_pinout.md)
- `User/door_control.h` - 门禁控制接口
- `User/door_control.c` - 门禁控制实现
- `User/menu_task.c` - 菜单显示任务

---

## 修改人员

- AI Assistant
- 修改日期: 2026-01-21
- 审核状态: 待测试验证
