# UTF-8 编码配置 - 快速参考

## ✅ 配置状态

🎉 **项目已配置为强制UTF-8编码！**

所有文件将自动以UTF-8格式打开和保存。

---

## 🚀 立即生效

### 方法一：重启VSCode（推荐）
```
关闭VSCode → 重新打开项目
```

### 方法二：重新加载窗口
```
Ctrl+Shift+P → 输入 "Reload Window" → 回车
```

---

## 📁 检查文件编码

### 查看当前文件编码：
- 看VSCode右下角状态栏
- 显示 `UTF-8` = 正确 ✅
- 显示其他（如 `GBK`）= 需要转换 ⚠️

### 手动切换编码：
1. 点击右下角编码标识
2. 选择 `Save with Encoding`
3. 选择 `UTF-8`

---

## 🔄 旧文件转换（如果出现乱码）

### 步骤：
1. 点击右下角编码标识
2. 选择 `Reopen with Encoding` → `GBK`（或`GB2312`）
3. 正常显示后，再选择 `Save with Encoding` → `UTF-8`
4. 保存文件 ✅

---

## 📋 配置文件位置

| 文件 | 作用 |
|------|------|
| `.vscode/settings.json` | VSCode工作区配置 |
| `.editorconfig` | 跨编辑器配置 |
| `UTF8_ENCODING_GUIDE.md` | 完整使用说明 |

---

## ⚙️ 核心配置

```json
{
    "files.encoding": "utf8",           // 强制UTF-8
    "files.autoGuessEncoding": false,   // 禁用自动猜测
    "files.defaultEncoding": "utf8",    // 新文件UTF-8
    "files.eol": "\n"                   // Unix行尾符
}
```

---

## 🎯 语言特定配置

| 文件类型 | 编码 | 缩进 | Tab大小 |
|---------|------|------|---------|
| `.c/.h` | UTF-8 | Tab | 4 |
| `.cpp/.hpp` | UTF-8 | Tab | 4 |
| `.md` | UTF-8 | Space | 2 |
| `.json` | UTF-8 | Space | 4 |

---

## 🚨 常见问题速查

### Q: 打开文件显示乱码？
**A:** 文件可能是GBK编码
1. 右下角点击编码
2. `Reopen with Encoding` → `GBK`
3. `Save with Encoding` → `UTF-8`

### Q: 中文注释正常但编译报错？
**A:** 添加编译参数：`-finput-charset=UTF-8`

### Q: 如何批量转换现有文件？
**A:** 参考 `UTF8_ENCODING_GUIDE.md` 中的批量转换方法

---

## 📖 详细说明

查看完整文档：[UTF8_ENCODING_GUIDE.md](UTF8_ENCODING_GUIDE.md)

---

**配置日期：** 2025-12-29
