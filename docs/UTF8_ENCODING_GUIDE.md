# UTF-8 编码配置说明

## 📋 配置概述

本项目已配置为**强制使用UTF-8编码**，确保所有文件在VSCode和其他编辑器中以UTF-8格式打开和保存。

---

## ✅ 已完成的配置

### 1. VSCode工作区配置
**文件位置：** `.vscode/settings.json`

**核心配置项：**
```json
"files.encoding": "utf8",              // 强制UTF-8编码
"files.autoGuessEncoding": false,       // 禁用自动猜测
"files.defaultEncoding": "utf8",        // 新文件默认UTF-8
"files.eol": "\n"                       // Unix行尾符（LF）
```

**语言特定配置：**
- C文件 (.c/.h): UTF-8 + Tab缩进(4)
- C++文件 (.cpp/.hpp): UTF-8 + Tab缩进(4)
- Markdown文件 (.md): UTF-8 + 空格缩进(2)
- JSON文件 (.json): UTF-8 + 空格缩进(4)

### 2. EditorConfig配置
**文件位置：** `.editorconfig`

确保跨编辑器的编码一致性（支持VSCode、Sublime Text、Vim等）。

---

## 🔧 配置生效

### 方式一：重新打开VSCode（推荐）
1. 关闭VSCode
2. 重新打开项目文件夹
3. 所有文件将自动以UTF-8打开

### 方式二：重新加载窗口
1. 按 `Ctrl+Shift+P`（Windows）或 `Cmd+Shift+P`（Mac）
2. 输入 `Reload Window`
3. 回车执行

---

## 📁 如何检查文件编码

### 在VSCode中查看当前文件编码：
1. 查看状态栏右下角
2. 显示 `UTF-8` 表示正确
3. 如果显示其他编码（如`GBK`），点击可切换

### 手动切换编码：
1. 点击状态栏右下角的编码标识
2. 选择 `Save with Encoding`（使用编码保存）
3. 选择 `UTF-8`

---

## 🔄 已有文件编码转换

如果您有**已存在的非UTF-8文件**需要转换，可以使用以下方法：

### 方法一：VSCode批量转换
1. 打开需要转换的文件
2. 点击状态栏编码标识
3. 选择 `Save with Encoding` → `UTF-8`
4. 保存文件

### 方法二：使用命令行工具（批量转换）
```bash
# 安装iconv工具（如果没有）
# Windows: 使用Git Bash或下载iconv for Windows

# 转换单个文件（GBK → UTF-8）
iconv -f GBK -t UTF-8 input.c > output.c

# 批量转换所有.c文件
for file in User/*.c; do
    iconv -f GBK -t UTF-8 "$file" > "$file.utf8"
    mv "$file.utf8" "$file"
done
```

### 方法三：Python脚本批量转换
```python
import os
import chardet

def convert_to_utf8(file_path):
    # 检测原编码
    with open(file_path, 'rb') as f:
        raw = f.read()
        encoding = chardet.detect(raw)['encoding']

    # 转换为UTF-8
    if encoding and encoding.lower() != 'utf-8':
        try:
            content = raw.decode(encoding)
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"转换成功: {file_path} ({encoding} → UTF-8)")
        except Exception as e:
            print(f"转换失败: {file_path}, 错误: {e}")

# 批量转换User目录下的所有.c和.h文件
for root, dirs, files in os.walk('User'):
    for file in files:
        if file.endswith(('.c', '.h')):
            convert_to_utf8(os.path.join(root, file))
```

---

## ⚙️ 配置详解

### 核心配置项说明

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `files.encoding` | `utf8` | 打开和保存文件时使用UTF-8编码 |
| `files.autoGuessEncoding` | `false` | 禁用自动猜测（避免误判为GBK等） |
| `files.defaultEncoding` | `utf8` | 新建文件默认编码 |
| `files.eol` | `\n` | 行尾符（LF，Unix格式） |

### 为什么要禁用自动猜测？
- 避免VSCode将中文注释误判为GBK编码
- 确保始终使用UTF-8打开文件
- 防止编码混乱导致乱码

---

## 🚨 常见问题

### Q1: 打开旧文件出现乱码怎么办？
**原因：** 旧文件可能是GBK或其他编码

**解决方法：**
1. 点击状态栏编码标识
2. 选择 `Reopen with Encoding`（重新打开，使用编码）
3. 选择 `GBK` 或 `GB2312`
4. 正常显示后，再选择 `Save with Encoding` → `UTF-8`

### Q2: 为什么中文注释显示正常，但编译报错？
**原因：** 文件编码与编译器预期不一致

**解决方法：**
1. 确保源文件为UTF-8编码
2. 在编译参数中添加：`-finput-charset=UTF-8 -fexec-charset=GBK`（如果需要）
3. 或者统一使用UTF-8编译环境

### Q3: Git提交时显示编码问题？
**解决方法：**
```bash
# 配置Git使用UTF-8
git config --global core.quotepath false
git config --global gui.encoding utf-8
git config --global i18n.commit.encoding utf-8
git config --global i18n.logoutputencoding utf-8
```

### Q4: 如何验证文件确实是UTF-8编码？
**方法一：** 使用VSCode状态栏查看

**方法二：** 使用命令行工具
```bash
# Linux/Mac
file -bi filename.c

# Windows (PowerShell)
Get-Content -Encoding UTF8 filename.c | Out-Null
```

---

## 📊 编码对比

| 编码 | 优点 | 缺点 | 推荐度 |
|------|------|------|--------|
| **UTF-8** | 国际标准，跨平台兼容，支持所有语言 | 中文占3字节 | ⭐⭐⭐⭐⭐ |
| GBK | 中文占2字节，节省空间 | 仅支持中文，跨平台差 | ⭐ |
| GB2312 | 中文占2字节 | 字符集受限 | ⭐ |
| ANSI | 系统默认 | 不同系统不同编码 | ❌ |

---

## 🎯 最佳实践

✅ **推荐做法：**
1. 所有新建文件使用UTF-8编码
2. 中文注释使用UTF-8编码
3. 统一团队编码标准
4. 使用版本控制（Git）时注意编码一致性

❌ **避免做法：**
1. 混用UTF-8和GBK编码
2. 使用系统默认编码（ANSI）
3. 手动修改编码后不保存
4. 复制粘贴时不注意编码转换

---

## 📖 相关文件

- [.vscode/settings.json](.vscode/settings.json) - VSCode工作区配置
- [.editorconfig](.editorconfig) - 跨编辑器配置

---

## 🔗 参考资料

- [UTF-8 - 维基百科](https://zh.wikipedia.org/wiki/UTF-8)
- [EditorConfig 官网](https://editorconfig.org/)
- [VSCode 编码设置](https://code.visualstudio.com/docs/editor/codebasics#_file-encoding-support)

---

**配置完成日期：** 2025-12-29
**维护者：** 项目开发团队
