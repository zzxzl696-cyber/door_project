# 门禁控制模块快速参考

## 快速开始

### 1. 初始化（在main.c中）
```c
#include "door_control.h"

int main(void)
{
    // ... 其他初始化 ...

    door_control_init();  // 初始化门禁控制

    while(1) {
        door_control_update();  // 周期调用（100ms）
    }
}
```

### 2. 解锁门禁
```c
// RFID卡开门（5秒自动锁定）
door_control_unlock(AUTH_RFID, 5000);

// 密码开门（3秒自动锁定）
door_control_unlock(AUTH_PASSWORD, 3000);

// 远程开门（永久解锁）
door_control_unlock(AUTH_REMOTE, 0);
```

### 3. 锁定门禁
```c
door_control_lock();
```

### 4. 查询状态
```c
// 检查是否锁定
if (door_control_is_locked()) {
    printf("Door is locked\r\n");
}

// 获取状态字符串
printf("Lock: %s\r\n", door_control_get_lock_state_str());
printf("Door: %s\r\n", door_control_get_sensor_state_str());
printf("Auth: %s\r\n", door_control_get_auth_method_str());
```

## 硬件连接

| 引脚 | 功能 | 说明 |
|------|------|------|
| PB0 | 门锁控制 | 输出，控制继电器 |
| PB1 | 门磁传感器 | 输入，检测门开关 |

**注意**: 根据实际硬件修改 `door_control.c` 中的GPIO定义。

## 认证方式

| 枚举值 | 说明 |
|--------|------|
| AUTH_NONE | 未认证 |
| AUTH_PASSWORD | 密码认证 |
| AUTH_RFID | RFID卡认证 |
| AUTH_FINGERPRINT | 指纹认证 |
| AUTH_FACE | 人脸认证 |
| AUTH_REMOTE | 远程开门 |

## 门锁状态

| 状态 | 说明 |
|------|------|
| DOOR_LOCKED | 门已锁定 |
| DOOR_UNLOCKED | 门已解锁 |
| DOOR_OPENING | 门正在开启 |
| DOOR_ALARM | 异常告警 |

## 菜单集成

菜单系统会自动显示以下门禁状态：
- **Lock**: 门锁状态（Locked/Unlocked/ALARM）
- **Door**: 门磁状态（Open/Closed）
- **Auth**: 最后认证方式（Password/RFID/Finger等）

状态每500ms自动刷新。

## 调度器配置

```c
// scheduler.c
static task_t scheduler_task[] = {
    {door_control_update, 100, 0},      // 门禁状态更新：100ms
    {menu_update_door_status, 500, 0},  // 菜单刷新：500ms
};
```

## 常用代码片段

### RFID卡验证开门
```c
void on_rfid_card_detected(uint32_t card_id)
{
    if (is_valid_card(card_id)) {
        door_control_unlock(AUTH_RFID, 5000);
        lcd_show_message("Welcome!");
    } else {
        lcd_show_message("Invalid Card");
    }
}
```

### 密码验证开门
```c
void on_password_entered(const char* password)
{
    if (verify_password(password)) {
        door_control_unlock(AUTH_PASSWORD, 3000);
        beep_success();
    } else {
        beep_error();
    }
}
```

### 远程开门命令
```c
void on_remote_unlock_command(void)
{
    door_control_unlock(AUTH_REMOTE, 10000);
    send_notification("Door unlocked remotely");
}
```

### 检查告警状态
```c
void check_alarm(void)
{
    if (g_door_status.alarm_active) {
        // 触发蜂鸣器
        beep_alarm();
        // 发送通知
        send_alert("Door alarm triggered!");
    }
}
```

## 文件清单

| 文件 | 说明 |
|------|------|
| `door_control.h` | 门禁控制头文件 |
| `door_control.c` | 门禁控制实现 |
| `menu_task.c` | 菜单任务（已集成门禁状态显示） |
| `scheduler.c` | 调度器（已添加门禁更新任务） |
| `main.c` | 主程序（已添加门禁初始化） |

## 调试输出示例

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
```

## 待完成事项

1. **时间戳集成**: 修改 `get_system_tick_ms()` 使用实际定时器
2. **GPIO配置**: 根据实际硬件调整引脚定义
3. **继电器极性**: 根据继电器类型调整控制逻辑
4. **扩展功能**: 添加开门记录、权限管理等

## 更多信息

详细文档请参考：[DOOR_CONTROL_USAGE.md](DOOR_CONTROL_USAGE.md)
