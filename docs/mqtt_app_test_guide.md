# MQTT 应用层 (mqtt_app) 测试操作指南

## 测试环境

- **硬件**: CH32V307VCT6 开发板 + ESP8266 WiFi 模块
- **调试串口**: USART3 (PB10-TX)，115200 bps, 8N1
- **串口工具**: SecureCRT / PuTTY / SSCOM
- **云平台**: OneNET (mqtts.heclouds.com:1883)
- **产品ID**: `3H2GSwZ0fO`
- **设备名称**: `ch32`

---

## 前置准备：OneNET 平台配置

### 步骤 1：确认产品和设备已创建

1. 登录 [OneNET 控制台](https://open.iot.10086.cn/)
2. 进入 **产品管理**，确认产品 `3H2GSwZ0fO` 存在
3. 进入该产品 → **设备管理**，确认设备 `ch32` 存在
4. 如果设备不存在，点击 **添加设备**：
   - 设备名称：`ch32`
   - 鉴权信息：与 `esp8266_config.h` 中的 token 对应

### 步骤 2：创建物模型属性

进入产品 → **物模型** → **自定义功能** → 逐个添加以下属性：

#### 上报属性（设备 → 云端，只读）

| 序号 | 标识符 | 名称 | 数据类型 | 读写类型 | 说明 |
|------|--------|------|---------|---------|------|
| 1 | `door_locked` | 门锁状态 | int32 | 只读 | 0=解锁, 1=锁定 |
| 2 | `user_count` | 用户数量 | int32 | 只读 | 当前注册用户数 |
| 3 | `log_count` | 日志数量 | int32 | 只读 | 当前日志记录数 |
| 4 | `uptime_s` | 运行时间 | int32 | 只读 | 设备运行秒数 |
| 5 | `auth_event` | 认证事件 | string | 只读 | success/fail/not_found/disabled/timeout |
| 6 | `auth_method` | 认证方式 | string | 只读 | RFID/Password/Finger/Face/Remote/None |
| 7 | `user_id` | 用户ID | int32 | 只读 | 认证对应的用户ID |
| 8 | `user_name` | 用户名 | string | 只读 | 认证对应的用户名 |
| 9 | `alarm` | 告警 | string | 只读 | lockout（连续认证失败锁定） |
| 10 | `fail_count` | 失败次数 | int32 | 只读 | 认证失败计数 |
| 11 | `cmd_result` | 命令结果 | string | 只读 | ok/fail |
| 12 | `logs` | 日志数据 | string | 只读 | 日志查询返回的数据 |
| 13 | `users` | 用户列表 | string | 只读 | 用户查询返回的数据 |

#### 下行属性（云端 → 设备，读写）

这些属性用于从云端向设备下发命令：

| 序号 | 标识符 | 名称 | 数据类型 | 读写类型 | 说明 |
|------|--------|------|---------|---------|------|
| 14 | `cmd` | 命令 | string | 读写 | unlock/lock/status/get_logs/get_users/add_user/del_user |
| 15 | `duration` | 开锁时长 | int32 | 读写 | 开锁持续毫秒数（unlock 命令用，默认 5000） |
| 16 | `count` | 查询条数 | int32 | 读写 | 日志查询条数（get_logs 命令用，默认 5） |
| 17 | `name` | 用户名参数 | string | 读写 | 添加用户名（add_user 命令用） |

> **注意**：`cmd` 属性类型为 string。如果 OneNET 物模型不支持 string 类型的读写属性，可以改用「枚举」类型定义 cmd，枚举值设为 unlock=1, lock=2, status=3 等。但此时代码中的命令解析需要相应修改。本文档假设使用 string 类型。

**添加属性操作步骤**（以 `door_locked` 为例）：

1. 点击 **添加自定义功能**
2. 功能类型：**属性**
3. 标识符：`door_locked`
4. 名称：`门锁状态`
5. 数据类型：`int32`
6. 读写类型：`只读`
7. 点击 **确定**

重复以上步骤添加全部 17 个属性。

### 步骤 3：发布物模型

所有属性添加完毕后，点击页面上方的 **发布** 按钮，使物模型生效。

> **重要**：物模型未发布前，设备上报的属性不会显示在控制台中。

### 步骤 4：确认 MQTT 接入协议

进入产品 → **产品详情**，确认：

- 接入协议：**MQTT**
- 数据格式：**OneNET 物模型**（非透传）
- Broker 地址：`mqtts.heclouds.com`
- 端口：`1883`

### 步骤 5：了解命令下发方式

OneNET 提供两种方式向设备下发命令：

#### 方式 A：OneNET 控制台在线调试（推荐新手）

1. 进入 **设备管理** → 选择设备 `ch32` → 点击 **设备调试**
2. 确认设备显示 **在线** 状态
3. 在 **属性设置** 区域，选择要设置的属性
4. 同时设置多个属性来组合命令

**示例 - 远程开锁**：
- 选择属性 `cmd`，输入值 `unlock`
- 选择属性 `duration`，输入值 `5000`
- 点击 **发送**

OneNET 会自动将这些属性打包为标准 property/set 消息发送到设备：
```json
{"id":"xxx","params":{"cmd":"unlock","duration":5000}}
```

#### 方式 B：MQTTX 客户端直连（推荐调试）

使用 [MQTTX](https://mqttx.app/) 桌面客户端直接连接 OneNET Broker 发送消息，更灵活：

1. **下载安装 MQTTX**

2. **创建连接**：
   - Name: `OneNET-Test`
   - Host: `mqtts.heclouds.com`
   - Port: `1883`
   - Client ID: `test_client`（任意，不要和设备同名）
   - Username: `3H2GSwZ0fO`（产品 ID）
   - Password: 使用产品级别的 token（在产品详情页获取）

3. **订阅设备上报主题**（查看设备上报数据）：
   ```
   $sys/3H2GSwZ0fO/ch32/thing/property/post
   ```

4. **发布命令到设备**：
   - Topic: `$sys/3H2GSwZ0fO/ch32/thing/property/set`
   - QoS: 1
   - Payload: 按测试用例填写 JSON

> **注意**：MQTTX 连接需要产品级 token，与设备级 token 不同。在 OneNET 控制台 → 产品详情 → 获取产品 key 或使用 token 生成工具。

#### 方式 C：OneNET API（适合自动化）

使用 HTTP API 调用属性设置：

```bash
curl -X POST \
  "https://iot-api.heclouds.com/thingmodel/set-device-desired-property" \
  -H "Authorization: <产品token>" \
  -H "Content-Type: application/json" \
  -d '{
    "product_id": "3H2GSwZ0fO",
    "device_name": "ch32",
    "params": {
      "cmd": "unlock",
      "duration": 5000
    }
  }'
```

---

## 测试 1：编译与启动验证

**目的**: 确认新增 mqtt_app 模块编译无误，初始化正常。

**操作**:
1. MounRiver Studio 中编译项目（Build Project）
2. 将 `mqtt_app.c` 添加到项目源文件列表（如未自动包含）
3. 烧录固件到开发板
4. 打开串口工具连接调试串口

**MounRiver 添加源文件**（如果编译报 undefined reference）：
1. 右键项目 → Properties → C/C++ Build → Settings
2. 确认 `User/mqtt_app.c` 在源文件列表中
3. 或直接右键 `User` 文件夹 → Refresh

**预期串口输出**:
```
=================================
System Clock: 144000000 Hz
Chip ID: xxxxxxxx
CH32V30x Door Access Control
=================================

[INIT] Door control...
[DOOR] Initializing door control module...
...
[INIT] MQTT app layer...
[MQTT-APP] Init
[INIT] ESP8266 WiFi module...
[INIT] ESP8266 configured: SSID=TP-LINK_E6E4, MQTT=mqtts.heclouds.com:1883
...
[INFO] System ready. Users=X, Logs=X
[INFO] Scheduler running...
```

**检查点**:
- [ ] 编译 0 errors, 0 warnings（或仅有已知 warnings）
- [ ] 串口输出 `[MQTT-APP] Init`
- [ ] 串口输出 `[INIT] ESP8266 configured`
- [ ] 系统正常运行，无死机

---

## 测试 2：WiFi/MQTT 连接验证

**目的**: 确认 ESP8266 能连接 WiFi 和 MQTT Broker，mqtt_app 收到连接通知。

**操作**:
1. 确保 WiFi 路由器已开启（SSID: TP-LINK_E6E4）
2. 上电后等待自动连接（约 10-30 秒）

**OneNET 操作**:
1. 登录 OneNET 控制台 → **设备管理**
2. 观察设备 `ch32` 的在线状态变化

**预期串口输出**:
```
[WiFi] state -> BOOT
[WiFi] state -> BASIC_SETUP
[WiFi] state -> JOIN_AP
[WiFi] WiFi connected
[WiFi] state -> MQTT_DISCONN
[WiFi] state -> MQTT_CFG
[WiFi] state -> MQTT_CONN
[WiFi] MQTT connected
[MQTT-APP] MQTT connected
[WiFi] MQTT subscribed to: $sys/3H2GSwZ0fO/ch32/#
[MQTT-APP] MQTT connected
```

**检查点**:
- [ ] WiFi 连接成功
- [ ] MQTT 连接成功
- [ ] 串口输出 `[MQTT-APP] MQTT connected`
- [ ] OneNET 控制台设备 `ch32` 显示 **在线**（绿色图标）

---

## 测试 3：心跳上报验证

**目的**: 确认每 30 秒自动上报系统状态到 OneNET。

**操作**:
1. 等待 MQTT 连接成功后
2. 观察串口输出（首次连接后会立即发送，之后每 30 秒）

**预期串口输出**:
```
[MQTT-APP] Heartbeat published
```

**OneNET 查看上报数据**:
1. 进入 **设备管理** → 选择 `ch32` → **设备详情**
2. 点击 **属性** 标签页
3. 查看以下属性的最新值和更新时间：

| 属性名 | 预期类型 | 示例值 | 含义 |
|--------|---------|--------|------|
| door_locked | int | 1 | 门锁状态（1=锁定） |
| user_count | int | 3 | 注册用户数 |
| log_count | int | 10 | 日志记录数 |
| uptime_s | int | 60 | 运行时间（秒） |

4. 每次刷新页面，`uptime_s` 应该增加约 30

> **如果属性不显示**：检查物模型是否已发布（前置准备步骤 3）。未发布的物模型不会解析上报数据。

**检查点**:
- [ ] 串口每 30 秒输出 `[MQTT-APP] Heartbeat published`
- [ ] OneNET 属性页面显示 `door_locked` / `user_count` / `log_count` / `uptime_s`
- [ ] 属性更新时间每 30 秒刷新一次

---

## 测试 4：RFID 刷卡认证事件上报

**目的**: 刷卡认证后，事件上报到云端。

### 4a: 已注册卡（认证成功）

**操作**:
1. 确保数据库中有已注册的 RFID 用户
2. 用已注册卡刷卡

**预期串口输出**:
```
[AuthMgr] Auth SUCCESS: method=2, user_id=1, name=张三
[MQTT-APP] Auth event queued: RFID success
[MQTT-APP] Door event queued: unlocked
...（500ms 后）
[MQTT-APP] Auth event published
...（再 500ms 后）
[MQTT-APP] Door event published
```

**OneNET 查看**:
1. 进入设备 → **属性** 页面，查看以下属性更新：
   - `auth_event` = `success`
   - `auth_method` = `RFID`
   - `user_id` = 用户ID（如 1）
   - `user_name` = 用户名（如 张三）
   - `door_locked` = `0`（解锁）

2. 等 5 秒后刷新：
   - `door_locked` = `1`（自动关锁）

### 4b: 未注册卡（认证失败）

**操作**:
1. 用一张未注册的卡刷卡

**预期串口输出**:
```
[AuthMgr] Auth FAILED: method=2, result=2
[MQTT-APP] Auth event queued: RFID not_found
...
[MQTT-APP] Auth event published
```

**OneNET 查看**:
- `auth_event` = `not_found`
- `auth_method` = `RFID`
- `door_locked` 不变（仍为 1）

**检查点**:
- [ ] 成功认证：OneNET 显示 `auth_event=success` + `user_name`
- [ ] 失败认证：OneNET 显示 `auth_event=not_found`
- [ ] 成功时 `door_locked` 先变 0 再变 1

---

## 测试 5：门禁状态变化上报

**目的**: 开锁/关锁时自动上报状态到云端。

**操作**:
1. 通过任意方式解锁（刷卡/密码/指纹）
2. 等待自动锁门（5 秒超时）

**预期串口输出**:
```
[DOOR] Door unlocked successfully
[MQTT-APP] Door event queued: unlocked
...
[MQTT-APP] Door event published
...（5 秒后）
[DOOR] Auto-lock timeout
[DOOR] Door locked
[MQTT-APP] Door event queued: locked
...
[MQTT-APP] Door event published
```

**OneNET 查看**:
1. 解锁瞬间刷新属性页面：`door_locked` = `0`
2. 5 秒后再次刷新：`door_locked` = `1`

**检查点**:
- [ ] OneNET 上 `door_locked` 值正确切换

---

## 测试 6：远程开锁命令

**目的**: 从 OneNET 平台下发开锁命令，验证设备执行。

### 方式 A：OneNET 控制台操作

1. 进入 **设备管理** → `ch32` → **设备调试**
2. 确认设备在线
3. 切换到 **属性设置** 功能
4. 设置以下属性并发送：
   - `cmd` = `unlock`
   - `duration` = `5000`
5. 点击 **发送**

### 方式 B：MQTTX 客户端操作

1. 在 MQTTX 中发布消息：
   - Topic: `$sys/3H2GSwZ0fO/ch32/thing/property/set`
   - QoS: 1
   - Payload:
   ```json
   {"id":"1","params":{"cmd":"unlock","duration":5000}}
   ```
2. 点击发送

**预期串口输出**:
```
[MQTT-APP] Recv topic=$sys/3H2GSwZ0fO/ch32/thing/property/set len=XX
[MQTT-APP] CMD: unlock
[DOOR] Unlock request: method=5, duration=5000ms
[DOOR] Door unlocked successfully
[MQTT-APP] Door event queued: unlocked
```

**预期物理现象**:
- 舵机转动到解锁角度
- 5 秒后舵机自动回到锁定角度

**OneNET 查看**:
- 属性页 `door_locked` 先变 0，5 秒后变 1
- `auth_method` 显示 `Remote`

**检查点**:
- [ ] 串口输出 `CMD: unlock`
- [ ] 舵机转动，门锁解锁
- [ ] 5 秒后自动关锁
- [ ] OneNET 上 `door_locked` 值变化正确

---

## 测试 7：远程关锁命令

### OneNET 控制台操作

**属性设置**:
- `cmd` = `lock`

### MQTTX 操作

```json
{"id":"2","params":{"cmd":"lock"}}
```

**预期串口输出**:
```
[MQTT-APP] CMD: lock
[DOOR] Lock request
[DOOR] Door locked
```

**检查点**:
- [ ] 门锁关闭，舵机回位
- [ ] OneNET 上 `door_locked` = 1

---

## 测试 8：远程查询状态

### OneNET 控制台操作

**属性设置**:
- `cmd` = `status`

### MQTTX 操作

```json
{"id":"3","params":{"cmd":"status"}}
```

**预期串口输出**:
```
[MQTT-APP] CMD: status
[MQTT-APP] Status published
```

**OneNET 查看** - 以下属性应全部更新：

| 属性 | 示例值 |
|------|--------|
| door_locked | 1 |
| auth_method | None |
| user_count | 3 |
| log_count | 10 |
| fail_count | 0 |
| uptime_s | 120 |

**检查点**:
- [ ] OneNET 显示全部 6 个属性值
- [ ] 值与串口显示的系统实际状态一致

---

## 测试 9：远程查询日志

### OneNET 控制台操作

**属性设置**:
- `cmd` = `get_logs`
- `count` = `3`

### MQTTX 操作

```json
{"id":"4","params":{"cmd":"get_logs","count":3}}
```

**预期串口输出**:
```
[MQTT-APP] CMD: get_logs
[MQTT-APP] Logs published (3 records)
```

**OneNET 查看**:
- `log_count` = `3`
- `logs` = `RFID:success:u1;Password:not_found:u0;RFID:success:u2`

格式说明：`认证方式:结果:u用户ID`，分号分隔。

**检查点**:
- [ ] `logs` 属性有数据
- [ ] 日志条数与请求的 count 一致

---

## 测试 10：远程查询用户列表

### OneNET 控制台操作

**属性设置**:
- `cmd` = `get_users`

### MQTTX 操作

```json
{"id":"5","params":{"cmd":"get_users"}}
```

**预期串口输出**:
```
[MQTT-APP] CMD: get_users
[MQTT-APP] Users published (N entries)
```

**OneNET 查看**:
- `user_count` = 实际用户数
- `users` = `1:张三;2:李四;3:王五`

格式说明：`用户ID:用户名`，分号分隔。

**检查点**:
- [ ] `users` 属性有数据
- [ ] 用户数和名称与本地一致

---

## 测试 11：远程添加/删除用户

### 11a: 添加用户

**OneNET 属性设置**:
- `cmd` = `add_user`
- `name` = `RemoteUser`

**MQTTX**:
```json
{"id":"6","params":{"cmd":"add_user","name":"RemoteUser"}}
```

**预期串口输出**:
```
[MQTT-APP] CMD: add_user
[MQTT-APP] CMD add_user: OK (id=N)
```

**OneNET 查看**:
- `cmd_result` = `ok`
- `user_id` = 新分配的用户 ID

**验证**: 再发一次 `get_users` 命令，确认新用户在列表中。

### 11b: 删除用户

**OneNET 属性设置**:
- `cmd` = `del_user`
- `user_id` = 上一步返回的用户 ID

> **注意**：`user_id` 属性用于 del_user 命令时是入参，但物模型中定义为只读。如果 OneNET 控制台不允许设置只读属性，请使用 MQTTX 方式发送。

**MQTTX**:
```json
{"id":"7","params":{"cmd":"del_user","user_id":4}}
```

**预期串口输出**:
```
[MQTT-APP] CMD: del_user
[MQTT-APP] CMD del_user: OK (id=4)
```

**检查点**:
- [ ] 添加用户成功，`cmd_result=ok`
- [ ] 删除用户成功
- [ ] 用 `get_users` 验证用户列表变化

---

## 测试 12：告警事件（认证锁定）

**目的**: 连续认证失败 5 次触发锁定告警。

**操作**:
1. 用 5 张未注册的卡连续刷卡（或输入 5 次错误密码）

**预期串口输出**:
```
[AuthMgr] Auth FAILED: method=2, result=2
[MQTT-APP] Auth event queued: RFID not_found
...（重复 5 次后）
[AuthMgr] LOCKED! Too many failures
[MQTT-APP] Auth event queued: RFID not_found
```

**OneNET 查看**:
- `alarm` = `lockout`
- `fail_count` = `5`

**检查点**:
- [ ] 第 5 次失败后触发锁定
- [ ] OneNET 显示 `alarm=lockout`
- [ ] 60 秒后自动解锁（串口观察 `[AuthMgr] Lockout expired, resetting`）

---

## 测试 13：MQTT 断线重连

**目的**: WiFi/MQTT 断连后重连，mqtt_app 恢复正常。

**操作**:
1. 关闭 WiFi 路由器（或拔掉 ESP8266 天线）
2. 等待 10 秒，观察断连日志
3. 恢复 WiFi
4. 等待重连

**预期串口输出**:
```
[WiFi] MQTT disconnected
[MQTT-APP] MQTT disconnected
...（WiFi 恢复后）
[WiFi] MQTT connected
[MQTT-APP] MQTT connected
[MQTT-APP] Heartbeat published   <- 重连后立即发送心跳
```

**OneNET 查看**:
1. 断连期间设备状态变为 **离线**（灰色图标）
2. 重连后变为 **在线**（绿色图标）
3. 心跳属性立即更新

**检查点**:
- [ ] 断连时串口输出 `[MQTT-APP] MQTT disconnected`
- [ ] 重连后串口输出 `[MQTT-APP] MQTT connected`
- [ ] 重连后立即发送心跳（不用等 30 秒）
- [ ] 断连期间缓冲的事件在重连后发送

---

## 故障排查

| 现象 | 可能原因 | 排查方法 |
|------|---------|---------|
| 无 `[MQTT-APP] Init` 输出 | mqtt_app.c 未加入编译 | MounRiver 项目右键 → Refresh，确认文件在源码列表中 |
| WiFi 连接失败 | SSID/密码错误或 ESP8266 未接 | 检查 `esp8266_config.h` 中的 WiFi 配置 |
| MQTT 连接失败 | token 过期 | 重新生成设备 token 并更新 `ESP8266_MQTT_PASSWORD` |
| OneNET 属性不显示 | 物模型未发布 | 进入产品 → 物模型 → 点击「发布」 |
| OneNET 属性不显示 | 标识符不匹配 | 确认物模型中标识符与代码中 JSON 的 key 完全一致（区分大小写） |
| 心跳不上报 | MQTT 未连接 | 检查串口是否有 `[MQTT-APP] MQTT connected` |
| 命令无响应 | 订阅主题不匹配 | 确认下发主题在 `$sys/3H2GSwZ0fO/ch32/#` 范围内 |
| 命令无响应 | JSON 格式错误 | 确认 JSON 中有 `"cmd":"xxx"` 字段，引号为英文双引号 |
| `cmd_result` 不更新 | 命令执行了但上报失败 | 检查 MQTT publish 是否 busy（串口观察） |
| MQTTX 连接失败 | token 类型错误 | MQTTX 需要产品级 token，不是设备级 token |
| OneNET 属性设置发不出 | 属性定义为只读 | 将 `cmd`/`duration`/`count`/`name` 设为读写类型 |

---

## 完整测试流程建议

按以下顺序执行，前一步通过后再进行下一步：

```
步骤 0：OneNET 平台配置（物模型 17 个属性 + 发布）
  ↓
测试 1：编译烧录，确认 [MQTT-APP] Init
  ↓
测试 2：WiFi/MQTT 连接，OneNET 显示在线
  ↓
测试 3：心跳上报，OneNET 属性值更新
  ↓
测试 4：刷卡认证上报（成功 + 失败）
  ↓
测试 5：门禁状态自动上报
  ↓
测试 6-7：远程开锁/关锁
  ↓
测试 8：远程查询状态
  ↓
测试 9-10：远程查询日志/用户
  ↓
测试 11：远程添加/删除用户
  ↓
测试 12：告警事件
  ↓
测试 13：断线重连
```

---

## 测试结果记录表

| 测试项 | 通过 | 备注 |
|--------|------|------|
| 0. OneNET 物模型配置 | | 17 个属性 + 发布 |
| 1. 编译与启动 | | |
| 2. WiFi/MQTT 连接 | | |
| 3. 心跳上报 | | |
| 4a. RFID 成功认证上报 | | |
| 4b. RFID 失败认证上报 | | |
| 5. 门禁状态变化上报 | | |
| 6. 远程开锁 | | |
| 7. 远程关锁 | | |
| 8. 远程查询状态 | | |
| 9. 远程查询日志 | | |
| 10. 远程查询用户 | | |
| 11a. 远程添加用户 | | |
| 11b. 远程删除用户 | | |
| 12. 告警事件 | | |
| 13. 断线重连 | | |
