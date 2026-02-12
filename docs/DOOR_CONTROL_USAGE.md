# 门禁控制模块使用说明

## 概述

门禁控制模块 (`door_control.c/h`) 提供了完整的门禁状态管理和控制接口，支持多种认证方式、自动锁定、门磁检测和异常告警功能。

## 功能特性

### 1. 门锁状态管理
- **DOOR_LOCKED**: 门已锁定
- **DOOR_UNLOCKED**: 门已解锁
- **DOOR_OPENING**: 门正在开启
- **DOOR_ALARM**: 门异常告警（门锁定但门打开）

### 2. 门磁传感器检测
- **DOOR_CLOSED**: 门已关闭
- **DOOR_OPENED**: 门已打开

### 3. 多种认证方式
- **AUTH_PASSWORD**: 密码认证
- **AUTH_RFID**: RFID卡认证
- **AUTH_FINGERPRINT**: 指纹认证
- **AUTH_FACE**: 人脸认证
- **AUTH_REMOTE**: 远程开门

## 硬件配置

### GPIO引脚定义
```c
// 门锁控制引脚 (输出)
#define DOOR_LOCK_GPIO_PORT     GPIOB
#define DOOR_LOCK_GPIO_PIN      GPIO_Pin_0

// 门磁传感器引脚 (输入，上拉)
#define DOOR_SENSOR_GPIO_PORT   GPIOB
#define DOOR_SENSOR_GPIO_PIN    GPIO_Pin_1
```

**注意**: 请根据实际硬件连接修改这些定义。

### 硬件连接
- **PB0**: 连接继电器控制引脚（控制电磁锁/电插锁）
- **PB1**: 连接门磁传感器（低电平=门关闭，高电平=门打开）

## API 接口

### 初始化函数

```c
void door_control_init(void);
```
**功能**: 初始化门禁控制模块，配置GPIO，设置初始状态
**调用时机**: 系统启动时调用一次

### 控制函数

#### 解锁门禁
```c
void door_control_unlock(auth_method_t method, uint32_t duration_ms);
```
**参数**:
- `method`: 认证方式（AUTH_PASSWORD, AUTH_RFID等）
- `duration_ms`: 解锁持续时间（毫秒），0表示永久解锁

**示例**:
```c
// RFID卡认证，解锁5秒
door_control_unlock(AUTH_RFID, 5000);

// 密码认证，解锁3秒
door_control_unlock(AUTH_PASSWORD, 3000);

// 远程开门，永久解锁（需手动锁定）
door_control_unlock(AUTH_REMOTE, 0);
```

#### 锁定门禁
```c
void door_control_lock(void);
```
**功能**: 立即锁定门禁

**示例**:
```c
door_control_lock();
```

### 状态更新函数

```c
void door_control_update(void);
```
**功能**: 更新门禁状态，检测门磁变化，处理自动锁定和异常告警
**调用时机**: 在主循环或定时任务中周期调用（建议100ms周期）

**自动功能**:
- 自动锁定：解锁时间到期后自动锁定
- 异常告警：检测到门锁定但门打开时触发告警

### 查询函数

#### 检查门是否锁定
```c
uint8_t door_control_is_locked(void);
```
**返回**: 1=锁定, 0=未锁定

#### 获取状态字符串
```c
const char* door_control_get_lock_state_str(void);
const char* door_control_get_sensor_state_str(void);
const char* door_control_get_auth_method_str(void);
```
**返回**: 状态的字符串描述，用于显示

## 使用示例

### 示例1: 基本初始化和控制

```c
#include "door_control.h"

int main(void)
{
    // 系统初始化...

    // 初始化门禁控制
    door_control_init();

    // 主循环
    while(1)
    {
        // 周期更新门禁状态
        door_control_update();

        // 其他任务...
        Delay_Ms(100);
    }
}
```

### 示例2: RFID卡开门

```c
void rfid_card_detected(uint32_t card_id)
{
    // 验证卡号
    if (is_valid_card(card_id))
    {
        printf("Valid card: %08X\r\n", card_id);

        // 解锁5秒
        door_control_unlock(AUTH_RFID, 5000);

        // 播放提示音
        beep_success();
    }
    else
    {
        printf("Invalid card: %08X\r\n", card_id);
        beep_error();
    }
}
```

### 示例3: 密码开门

```c
void password_input_complete(const char* password)
{
    // 验证密码
    if (verify_password(password))
    {
        printf("Password correct\r\n");

        // 解锁3秒
        door_control_unlock(AUTH_PASSWORD, 3000);

        // 显示成功信息
        lcd_show_message("Welcome!");
    }
    else
    {
        printf("Password incorrect\r\n");
        lcd_show_message("Access Denied");
    }
}
```

### 示例4: 集成到调度器

```c
#include "scheduler.h"
#include "door_control.h"
#include "menu_task.h"

static task_t scheduler_task[] =
{
    {door_control_update, 100, 0},      // 门禁状态更新：100ms
    {menu_update_door_status, 500, 0},  // 菜单刷新：500ms
    // 其他任务...
};
```

### 示例5: 菜单显示集成

```c
#include "menu_task.h"
#include "door_control.h"

// 门禁状态显示缓冲区
static char door_lock_status_buf[16];
static char door_sensor_status_buf[16];

// 菜单项
static menu_item_t item_door_lock = {
    "Lock:",
    MENU_ITEM_STRING,
    door_lock_status_buf,
    NULL,
    NULL
};

void menu_update_door_status(void)
{
    // 更新门锁状态
    const char* lock_str = door_control_get_lock_state_str();
    snprintf(door_lock_status_buf, sizeof(door_lock_status_buf), "%s", lock_str);

    // 重绘菜单
    menu_c_draw(&menu);
}
```

## 全局状态访问

可以直接访问全局状态结构体获取详细信息：

```c
extern door_control_status_t g_door_status;

void print_door_status(void)
{
    printf("Lock State: %d\r\n", g_door_status.lock_state);
    printf("Sensor State: %d\r\n", g_door_status.sensor_state);
    printf("Last Auth: %d\r\n", g_door_status.last_auth_method);
    printf("Fail Count: %d\r\n", g_door_status.auth_fail_count);
    printf("Alarm: %d\r\n", g_door_status.alarm_active);
}
```

## 状态机流程

```
[初始化] --> [LOCKED]
              |
              | unlock()
              v
          [UNLOCKED] --timeout--> [LOCKED]
              |
              | lock()
              v
          [LOCKED]
              |
              | (门打开 && 锁定)
              v
          [ALARM] --门关闭--> [LOCKED]
```

## 注意事项

### 1. 时间戳功能
当前 `get_system_tick_ms()` 函数返回0，需要集成实际的系统定时器：

```c
// TODO: 实现实际的系统时间戳
static uint32_t get_system_tick_ms(void)
{
    return TIM_Get_MillisCounter();  // 使用实际的定时器
}
```

### 2. GPIO配置
根据实际硬件修改GPIO引脚定义：
- 门锁控制引脚
- 门磁传感器引脚

### 3. 继电器极性
如果继电器是低电平触发，需要修改 `door_lock_hardware_control()` 函数的逻辑。

### 4. 自动锁定
- `duration_ms = 0`: 永久解锁，需手动调用 `door_control_lock()`
- `duration_ms > 0`: 自动锁定，时间到期后自动锁定

### 5. 异常告警
当检测到"门锁定但门打开"时，会自动触发告警状态。可以在此基础上添加：
- 蜂鸣器报警
- LED闪烁
- 发送通知

## 扩展功能建议

### 1. 添加开门记录
```c
typedef struct {
    uint32_t timestamp;
    auth_method_t method;
    uint32_t user_id;
} door_log_entry_t;

void door_control_log_access(auth_method_t method, uint32_t user_id);
```

### 2. 添加防撬检测
```c
void door_control_set_tamper_alarm(uint8_t enable);
uint8_t door_control_is_tampered(void);
```

### 3. 添加远程控制
```c
void door_control_remote_unlock(uint32_t duration_ms);
void door_control_remote_lock(void);
```

### 4. 添加权限管理
```c
uint8_t door_control_check_permission(uint32_t user_id, uint8_t time_slot);
```

## 调试输出

模块会输出详细的调试信息：
```
[DOOR] Initializing door control module...
[DOOR] Door control initialized
[DOOR] Unlock request: method=2, duration=5000ms
[DOOR] Hardware: Lock OPENED
[DOOR] Door unlocked successfully
[DOOR] Sensor state changed: OPENED
[DOOR] Auto-lock timeout
[DOOR] Hardware: Lock CLOSED
[DOOR] Door locked
[DOOR] ALARM: Door opened while locked!
```

## 常见问题

### Q1: 门禁无法解锁？
- 检查GPIO配置是否正确
- 检查继电器连接和电源
- 查看调试输出确认函数是否被调用

### Q2: 自动锁定不工作？
- 确认 `door_control_update()` 被周期调用
- 确认 `get_system_tick_ms()` 返回正确的时间戳

### Q3: 门磁状态不准确？
- 检查门磁传感器连接
- 确认GPIO输入模式配置正确（上拉/下拉）
- 调整门磁传感器的安装位置

### Q4: 告警误触发？
- 检查门磁传感器灵敏度
- 调整告警触发条件
- 添加防抖动处理

## 版本历史

- **V1.0.0** (2026-01-20)
  - 初始版本
  - 基本的门禁控制功能
  - 多种认证方式支持
  - 自动锁定和异常告警
