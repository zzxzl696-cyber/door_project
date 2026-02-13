# 门禁状态显示界面测试指南

**文档版本**: v1.0.0
**创建日期**: 2026-02-13
**测试目标**: 验证门禁状态显示界面功能完整性和稳定性

---

## 测试环境准备

### 硬件要求

- [x] CH32V307VCT6 开发板
- [x] ST7735S LCD 屏幕（128×160 像素）
- [x] RFID 读卡器（UART5）
- [x] AS608 指纹模块（UART2，可选）
- [x] 4×4 矩阵键盘
- [x] WCH-Link 调试器
- [x] USB 转串口模块（查看调试信息）

### 软件要求

- [x] MounRiver Studio IDE
- [x] 串口调试工具（SecureCRT / PuTTY / 串口助手）
- [x] 配置：115200 bps, 8N1, 无流控

### 编译前检查

#### 1. 文件完整性检查

确认以下文件存在且无编译错误：

```bash
# 新增文件
User/door_status_ui.h
User/door_status_ui.c

# 修改文件
User/auth_manager.h
User/auth_manager.c
User/password_input.h
User/password_input.c
User/main.c
User/scheduler.c
User/bsp_system.h
```

#### 2. 头文件依赖检查

在 `User/bsp_system.h` 中确认已包含：

```c
#include "door_status_ui.h"
```

#### 3. 编译测试

```bash
# 在 MounRiver Studio 中执行
Project -> Clean Project
Project -> Build Project
```

**预期结果**：
- ✅ 编译成功，0 errors, 0 warnings（或仅有少量无关警告）
- ✅ 生成 `.hex` 和 `.bin` 固件文件
- ❌ 如有错误，参考 [常见编译错误](#常见编译错误) 章节

---

## 阶段 1: 基本显示测试

### 测试目标
验证 LCD 初始化和基本布局显示是否正常。

### 测试步骤

1. **烧录固件**
   - 使用 WCH-Link 烧录固件到开发板
   - 等待烧录完成（约 5-10 秒）

2. **系统复位**
   - 按下开发板复位键
   - 观察串口输出

3. **验证串口输出**

   **预期串口日志**：
   ```
   =================================
   System Clock: 96000000 Hz
   Chip ID: 30700518
   CH32V30x LCD + UART DMA Demo
   =================================

   [INFO] Initializing door control...
   [DOOR] Initializing door control module...
   [DOOR] Door control initialized
   [INFO] Door control initialized!

   [INFO] Initializing user database...
   [INFO] Initializing access log...
   [INFO] Initializing auth manager...
   [AuthMgr] Initialized
   [PwdInput] Initialized
   [INFO] Auth system initialized!

   [INFO] Initializing ST7735S LCD...
   [INFO] LCD initialized successfully!

   [INFO] System initialized successfully!
   ```

4. **验证 LCD 显示**

   观察 LCD 屏幕，应看到以下布局：

   ```
   ┌──────────────────────────────┐
   │ [深蓝色背景]                  │
   │ DOOR ACCESS SYS              │  ← 标题栏（白色文字）
   ├──────────────────────────────┤
   │                              │
   │ Status: LOCKED               │  ← 红色文字
   │ Method: None                 │  ← 白色文字
   │ Angle:  45 deg               │  ← 黄色文字
   │                              │
   ├──────────────────────────────┤
   │                              │
   │ User: ---                    │  ← 灰色文字
   │ Time: ---                    │  ← 灰色文字
   │ Fails: 0/5                   │  ← 白色文字
   │                              │
   ├──────────────────────────────┤
   │                              │
   │                              │
   │ >>> Waiting Auth <<<         │  ← 青色文字，居中
   │                              │
   └──────────────────────────────┘
   ```

### 验收标准

- [x] 标题栏显示 "DOOR ACCESS SYS"，背景深蓝色
- [x] 状态区显示锁定状态（红色 "LOCKED"）
- [x] 信息区显示默认值（"---"）
- [x] 提示区显示 ">>> Waiting Auth <<<"（青色）
- [x] 无花屏、乱码或闪烁现象
- [x] 文字清晰可读

### ❌ 常见问题

| 问题 | 可能原因 | 解决方案 |
|------|---------|---------|
| LCD 无显示 | 供电不足或连线错误 | 检查 SPI2 连线和电源 |
| 显示花屏 | 复位时序错误 | 重新上电或复位 |
| 文字乱码 | 字库未正确烧录 | 检查 `lcd.c` 中的字库数组 |
| 布局错乱 | 坐标计算错误 | 检查 `door_status_ui.c` 中的 Y 坐标常量 |

---

## 阶段 2: RFID 认证测试

### 测试目标
验证 RFID 刷卡时的状态显示和认证流程。

### 前置条件

- ✅ 用户数据库中已有测试用户（默认用户 ID=1, name="Admin"）
- ✅ RFID 读卡器已连接并初始化（UART5: PC12-Tx, PD2-Rx）

### 测试步骤

#### 2.1 刷卡触发测试

1. **使用已注册的 RFID 卡刷卡**

2. **观察 LCD 变化**（按时间顺序）

   **步骤 1** - 读卡中状态（瞬间，约 100ms）：
   ```
   提示区变化：
   >>> Waiting Auth <<< → Reading Card...（青色 → 黄色）
   ```

   **步骤 2** - 认证成功状态（持续 3 秒）：
   ```
   状态区：Status: UNLOCKED（绿色）
          Method: RFID（白色）
          Angle:  180 deg（黄色）

   信息区：User: Admin（青色，显示实际用户名）
          Time: 5 sec left（黄色，倒计时）
          Fails: 0/5（白色）

   提示区：Access Granted!（绿色，居中）
   ```

   **步骤 3** - 倒计时更新（每秒更新）：
   ```
   Time: 5 sec left
   Time: 4 sec left
   Time: 3 sec left
   Time: 2 sec left
   Time: 1 sec left
   Time: Expired（灰色）
   ```

   **步骤 4** - 自动锁定（倒计时结束后）：
   ```
   状态区：Status: LOCKED（红色）
          Method: RFID（白色）
          Angle:  45 deg（黄色）

   信息区：User: ---（灰色）
          Time: ---（灰色）
          Fails: 0/5（白色）

   提示区：>>> Waiting Auth <<<（青色）
   ```

3. **观察串口日志**

   **预期日志**：
   ```
   [RFID] Auto mode frame received: ID + Block
   [RFID] Card Type: Mifare S50
   [RFID] UID: 12 34 56 78
   [AuthMgr] Processing RFID: 12 34 56 78
   [AuthMgr] Auth SUCCESS: method=2, user_id=1, name=Admin
   [DOOR] Unlocking door, method=RFID, duration=5000ms
   [DOOR] Door unlocked
   ```

### 验收标准

- [x] 刷卡瞬间提示区显示 "Reading Card..."
- [x] 认证成功后状态变为 "UNLOCKED"（绿色）
- [x] 用户名正确显示
- [x] 倒计时每秒准确更新（5→4→3→2→1→0）
- [x] 5 秒后自动锁定并返回待机状态
- [x] 舵机角度正确显示（45° ↔ 180°）

#### 2.2 未注册卡测试

1. **使用未注册的 RFID 卡刷卡**

2. **观察 LCD 变化**

   **步骤 1** - 读卡中：
   ```
   Reading Card...（黄色）
   ```

   **步骤 2** - 认证失败（持续 2 秒）：
   ```
   状态区：Status: LOCKED（保持红色）

   信息区：User: ---（灰色）
          Time: ---（灰色）
          Fails: 1/5（白色）← 失败次数增加

   提示区：Access Denied!（红色）
   ```

   **步骤 3** - 返回待机（2 秒后）：
   ```
   提示区：>>> Waiting Auth <<<（青色）
   ```

3. **观察串口日志**

   **预期日志**：
   ```
   [RFID] UID: AA BB CC DD
   [AuthMgr] Processing RFID: AA BB CC DD
   [AuthMgr] Auth FAILED: method=2, result=2
   [AuthMgr] Fail count: 1/5
   ```

### 验收标准

- [x] 失败次数正确增加（0 → 1）
- [x] 提示区显示 "Access Denied!"（红色）
- [x] 门锁保持锁定状态
- [x] 2 秒后返回待机状态

---

## 阶段 3: 密码输入测试

### 测试目标
验证密码输入时的实时显示和星号反馈。

### 前置条件

- ✅ 矩阵键盘已连接并初始化
- ✅ 用户数据库中已有密码用户（例如：密码 1234）

### 测试步骤

#### 3.1 密码输入显示测试

1. **触发密码输入模式**（通过特定按键或菜单）

   > **注意**：根据当前代码，需要调用 `auth_start_password()` 触发。如果没有对外接口，可以通过矩阵键盘的特定键（如键 13）触发。

2. **观察 LCD 初始状态**

   ```
   提示区：Password: （黄色，无星号）
   ```

3. **按下矩阵键盘数字键 "1"**

   **LCD 变化**：
   ```
   提示区：Password: *（黄色，1个星号）
   ```

   **串口日志**：
   ```
   [PwdInput] Input digit 1, count=1
   ```

4. **继续按下 "2", "3", "4"**

   **LCD 变化**（按顺序）：
   ```
   Password: **
   Password: ***
   Password: ****（4个星号，输入完成）
   ```

   **串口日志**：
   ```
   [PwdInput] Input digit 2, count=2
   [PwdInput] Input digit 3, count=3
   [PwdInput] Input digit 4, count=4
   [PwdInput] Complete: 1234
   [AuthMgr] Processing password: 1234
   [AuthMgr] Auth SUCCESS: method=1, user_id=2, name=User01
   ```

5. **观察认证成功后的显示**（与 RFID 测试相同）

   ```
   状态区：Status: UNLOCKED（绿色）
   信息区：User: User01（青色）
          Time: 5 sec left（黄色倒计时）
   提示区：Access Granted!（绿色）
   ```

### 验收标准

- [x] 每次按键后星号数量正确增加（* → ** → *** → ****）
- [x] 输入完成（4位）后自动触发认证
- [x] 认证成功后正确显示用户信息和倒计时
- [x] UI 更新实时无延迟

#### 3.2 密码错误测试

1. **输入错误密码（例如：5678）**

2. **观察 LCD 显示**

   ```
   提示区：Password: **** → Access Denied!（红色）
   信息区：Fails: 1/5（白色，失败次数增加）
   ```

3. **串口日志**

   ```
   [PwdInput] Complete: 5678
   [AuthMgr] Processing password: 5678
   [AuthMgr] Auth FAILED: method=1, result=2
   ```

### 验收标准

- [x] 错误密码导致认证失败
- [x] 失败次数正确增加
- [x] 提示区显示 "Access Denied!"

#### 3.3 退格功能测试

1. **输入 "1", "2", "3"**（3个星号）

2. **按下退格键（键 11）**

   **LCD 变化**：
   ```
   Password: *** → Password: **（星号减少）
   ```

   **串口日志**：
   ```
   [PwdInput] Backspace, count=2
   pwd = 1200
   ```

3. **继续输入 "4", "5"**

   **LCD 变化**：
   ```
   Password: ** → Password: *** → Password: ****
   ```

   **最终密码**：1245（而非 1234）

### 验收标准

- [x] 退格键正确删除最后一位
- [x] 星号数量正确减少
- [x] 删除后可继续输入

---

## 阶段 4: 系统锁定测试

### 测试目标
验证连续认证失败后的系统锁定机制和倒计时显示。

### 测试步骤

1. **连续刷 5 次未注册的 RFID 卡**

2. **观察 LCD 变化**

   **第 1-4 次失败**：
   ```
   信息区：Fails: 1/5
          Fails: 2/5
          Fails: 3/5（变红色）
          Fails: 4/5（红色）
   ```

   **第 5 次失败后**：
   ```
   状态区：保持 LOCKED

   信息区：Fails: 5/5（红色）

   提示区：!!! LOCKED 60s !!!（红色，闪烁）
   ```

3. **观察倒计时闪烁效果**（每秒更新）

   **闪烁规律**：
   ```
   秒数    显示内容
   60s     !!! LOCKED 60s !!!（显示）
   59s     （空白）
   58s     !!! LOCKED 58s !!!（显示）
   57s     （空白）
   ...
   3s      !!! LOCKED 3s !!!（显示）
   2s      （空白）
   1s      !!! LOCKED 1s !!!（显示）
   0s      >>> Waiting Auth <<<（解除锁定）
   ```

4. **观察串口日志**

   ```
   [AuthMgr] Auth FAILED: method=2, result=2
   [AuthMgr] Fail count: 5/5
   [AuthMgr] LOCKED! Too many failures
   [BY8301] Playing index: 3（警告语音）
   ```

5. **尝试刷卡（锁定期间）**

   **LCD 显示**：无变化（仍显示锁定倒计时）

   **串口日志**：
   ```
   [AuthMgr] System locked, rejecting RFID
   ```

6. **等待 60 秒后**

   **LCD 变化**：
   ```
   提示区：!!! LOCKED 1s !!! → >>> Waiting Auth <<<
   信息区：Fails: 0/5（失败次数重置）
   ```

   **串口日志**：
   ```
   [AuthMgr] Lockout expired, resetting
   ```

7. **验证解锁后可正常认证**

   - 刷注册卡 → 认证成功 ✅

### 验收标准

- [x] 第 5 次失败后进入锁定状态
- [x] 锁定倒计时从 60 秒开始
- [x] 每秒正确更新倒计时
- [x] 闪烁效果正常（1 秒显示，1 秒空白）
- [x] 锁定期间拒绝所有认证
- [x] 60 秒后自动解除锁定并重置失败计数
- [x] 解锁后可正常使用

---

## 阶段 5: 性能与稳定性测试

### 测试目标
验证系统长时间运行的稳定性和响应速度。

### 测试步骤

#### 5.1 响应速度测试

1. **刷卡响应时间**

   - 刷卡 → 观察提示区变化时间
   - **预期**: < 100ms（几乎即时）

2. **倒计时更新精度**

   - 使用秒表对比 LCD 倒计时和实际时间
   - **预期**: 误差 < ±0.5 秒/分钟

3. **UI 刷新性能**

   - 观察区域刷新时是否有撕裂或闪烁
   - **预期**: 无明显闪烁，刷新流畅

#### 5.2 长时间运行测试

1. **连续运行测试（至少 30 分钟）**

   - 每隔 2-3 分钟刷卡一次
   - 观察 LCD 显示是否正常
   - 检查串口是否有错误日志

2. **内存泄漏检查**

   - 观察系统是否出现卡顿或死机
   - 串口日志是否有栈溢出警告

3. **温度稳定性**

   - 检查开发板和 LCD 温度（不应过热）
   - 显示是否因温度变化而异常

### 验收标准

- [x] 响应时间 < 100ms
- [x] 倒计时误差 < ±0.5s/min
- [x] 无闪烁、撕裂现象
- [x] 连续运行 30 分钟无死机
- [x] 无内存泄漏或栈溢出
- [x] 温度正常（< 60°C）

---

## 阶段 6: 边界条件测试

### 测试用例

#### 6.1 快速连续刷卡

**操作**: 快速连续刷卡 10 次（< 1 秒间隔）

**预期**:
- ✅ 每次刷卡都正确处理
- ✅ UI 正确更新，无丢失
- ✅ 无崩溃或异常

#### 6.2 倒计时临界值

**操作**: 在倒计时剩余 1 秒时观察显示

**预期**:
- ✅ 显示 "Time: 1 sec left"
- ✅ 1 秒后正确切换到 "Time: Expired"
- ✅ 自动锁定

#### 6.3 锁定倒计时临界值

**操作**: 在锁定剩余 1 秒时观察

**预期**:
- ✅ 显示 "!!! LOCKED 1s !!!"
- ✅ 1 秒后解除锁定
- ✅ 失败次数重置为 0

#### 6.4 超长用户名

**操作**: 添加用户名长度为 15 字符的测试用户

**预期**:
- ✅ 用户名显示不超出屏幕
- ✅ 可能截断但不乱码
- ✅ 不影响其他区域显示

---

## 常见问题排查

### 编译错误

#### 错误 1: 找不到 `door_status_ui.h`

**原因**: 头文件路径未添加到编译器搜索路径

**解决**:
1. 检查 `User/bsp_system.h` 是否包含 `#include "door_status_ui.h"`
2. 确认 `door_status_ui.h` 文件在 `User/` 目录下
3. 清理工程后重新编译

#### 错误 2: 未定义的引用 `door_status_ui_init`

**原因**: 链接器找不到实现文件

**解决**:
1. 确认 `door_status_ui.c` 已添加到工程
2. 检查 MounRiver Studio 项目树中是否可见
3. 右键项目 → Refresh → 重新编译

#### 错误 3: 重复定义回调类型

**原因**: 头文件多次包含

**解决**:
1. 检查所有头文件是否有 `#ifndef` 包含保护
2. 移除重复的 `#include`

### 运行时错误

#### 问题 1: LCD 显示空白

**排查步骤**:
1. 检查 LCD 供电（3.3V）
2. 检查 SPI2 连线（PB13-SCK, PB15-MOSI）
3. 检查控制引脚（RES, DC, CS, BLK）
4. 在 `door_status_ui_init()` 中添加调试日志

**调试日志**:
```c
void door_status_ui_init(void)
{
    printf("[DEBUG] Clearing screen...\r\n");
    LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
    printf("[DEBUG] Drawing title bar...\r\n");
    draw_title_bar();
    printf("[DEBUG] UI init complete\r\n");
}
```

#### 问题 2: 倒计时不更新

**排查步骤**:
1. 检查调度器是否正常运行
2. 检查 `TIM_Get_MillisCounter()` 返回值
3. 检查 `door_status_ui_update()` 是否被调用

**调试日志**:
```c
void door_status_ui_update(void)
{
    static uint32_t debug_count = 0;
    debug_count++;
    if (debug_count % 100 == 0) { // 每 10 秒打印一次
        printf("[DEBUG] UI update called %d times\r\n", debug_count);
    }
    // ... 原有代码 ...
}
```

#### 问题 3: 刷卡无反应

**排查步骤**:
1. 检查 RFID 读卡器连接（UART5）
2. 检查 `auth_manager_set_start_callback()` 是否调用
3. 检查回调函数是否被触发

**调试日志**:
```c
void door_status_ui_on_auth_start(auth_method_t method)
{
    printf("[DEBUG] Auth start callback: method=%d\r\n", method);
    // ... 原有代码 ...
}
```

#### 问题 4: 密码输入无星号

**排查步骤**:
1. 检查 `pwd_input_set_ui_callback()` 是否调用
2. 检查矩阵键盘是否正常
3. 检查 `pwd_input_on_key()` 是否被调用

**调试日志**:
```c
void door_status_ui_on_password_input(uint8_t length)
{
    printf("[DEBUG] Password input: length=%d\r\n", length);
    // ... 原有代码 ...
}
```

---

## 测试清单

### 功能完整性测试

- [ ] 1.1 基本显示 - 标题栏正确显示
- [ ] 1.2 基本显示 - 4 个区域布局正确
- [ ] 1.3 基本显示 - 初始状态为待机

- [ ] 2.1 RFID 认证 - 已注册卡认证成功
- [ ] 2.2 RFID 认证 - 未注册卡认证失败
- [ ] 2.3 RFID 认证 - 倒计时正确更新
- [ ] 2.4 RFID 认证 - 自动锁定功能

- [ ] 3.1 密码输入 - 星号实时显示
- [ ] 3.2 密码输入 - 正确密码认证成功
- [ ] 3.3 密码输入 - 错误密码认证失败
- [ ] 3.4 密码输入 - 退格键正常工作

- [ ] 4.1 系统锁定 - 5 次失败触发锁定
- [ ] 4.2 系统锁定 - 倒计时闪烁显示
- [ ] 4.3 系统锁定 - 锁定期间拒绝认证
- [ ] 4.4 系统锁定 - 60 秒后自动解锁

- [ ] 5.1 性能测试 - 响应速度 < 100ms
- [ ] 5.2 性能测试 - 倒计时精度 < ±0.5s
- [ ] 5.3 性能测试 - 无闪烁撕裂
- [ ] 5.4 性能测试 - 长时间运行稳定

### 代码质量检查

- [ ] 6.1 无编译警告（或仅无关警告）
- [ ] 6.2 无内存泄漏
- [ ] 6.3 无全局变量滥用
- [ ] 6.4 函数职责单一
- [ ] 6.5 代码注释完整
- [ ] 6.6 命名规范一致

---

## 测试报告模板

```markdown
# 门禁状态显示界面测试报告

**测试日期**: YYYY-MM-DD
**测试人员**: [姓名]
**固件版本**: vX.X.X
**硬件版本**: CH32V307VCT6 + ST7735S

## 测试环境

- 开发板型号: _______________
- LCD 屏幕型号: _______________
- 编译器版本: _______________
- 烧录工具: _______________

## 测试结果

### 阶段 1: 基本显示测试
- 测试结果: ✅ 通过 / ❌ 失败
- 问题描述: _______________

### 阶段 2: RFID 认证测试
- 测试结果: ✅ 通过 / ❌ 失败
- 问题描述: _______________

### 阶段 3: 密码输入测试
- 测试结果: ✅ 通过 / ❌ 失败
- 问题描述: _______________

### 阶段 4: 系统锁定测试
- 测试结果: ✅ 通过 / ❌ 失败
- 问题描述: _______________

### 阶段 5: 性能测试
- 响应速度: _____ ms
- 倒计时误差: _____ s/min
- 长时间运行: ✅ 通过 / ❌ 失败

## 发现的问题

| 编号 | 问题描述 | 严重性 | 状态 |
|------|---------|--------|------|
| 1    |         | 高/中/低 | 待修复/已修复 |

## 总体评价

- 功能完整性: ____%
- 稳定性: ____%
- 用户体验: ____%

## 建议

1. _______________
2. _______________

## 签名

测试人员: ___________  日期: __________
审核人员: ___________  日期: __________
```

---

## 附录：快速验证脚本

如果只想快速验证核心功能，按以下简化步骤执行：

### 5 分钟快速测试

1. **编译烧录** (1 分钟)
   - 编译 → 烧录 → 复位

2. **基本显示** (30 秒)
   - 观察 LCD 是否显示标题和待机状态

3. **刷卡测试** (1 分钟)
   - 刷已注册卡 → 观察是否显示用户名和倒计时
   - 刷未注册卡 → 观察失败次数是否增加

4. **密码测试** (1 分钟)
   - 输入密码 → 观察星号显示
   - 认证成功 → 观察倒计时

5. **锁定测试** (1.5 分钟)
   - 连续刷 5 次未注册卡
   - 观察是否进入锁定并显示倒计时

**预期**: 全部通过 → ✅ 代码正常，可进入详细测试
**如有失败** → ❌ 查看串口日志，参考 [常见问题排查](#常见问题排查)

---

## 联系支持

如遇到无法解决的问题，请提供以下信息：

1. 完整的串口日志（从启动到错误发生）
2. LCD 显示的照片或视频
3. 使用的硬件型号和连线图
4. 代码修改记录（如果有）

---

**文档结束**
