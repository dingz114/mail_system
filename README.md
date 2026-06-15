# Mail System — SMTP & POP3 Email Client

计算机网络实践课程项目：基于 C++ 原生 Socket 编程的邮件系统（SMTP + POP3 客户端），MySQL 数据库存储，Qt 6 构建 Windows 图形界面，现代扁平化 UI 风格。

## 功能

| 功能 | 描述 |
|------|------|
| 📧 **编写邮件** | 收件人/抄送、纯文本 + HTML 正文、文件附件 |
| 📤 **发送邮件** | SMTP 协议，SSL/TLS 加密（465），STARTTLS（587），AUTH LOGIN |
| 📥 **接收邮件** | POP3 协议，SSL/TLS 加密（995），UIDL 去重 |
| 📖 **阅读邮件** | 邮件列表（发件人/主题/日期），正文渲染，附件展示 |
| 📝 **草稿编辑** | 保存草稿 → 点击继续编辑 → 修改后发送或再次保存 |
| 🔄 **回复/转发** | 引用原邮件，自动填充收件人和主题 |
| 🗑️ **删除邮件** | 软删除，移入废纸篓 |
| 📋 **多账号** | 多个邮箱账号独立管理，各自收件箱/已发送/草稿/废纸篓 |
| 🔍 **连接测试** | 账号设置中可分别测试 SMTP 发送和 POP3 接收连接 |
| 🌐 **预设模板** | QQ邮箱 / 163邮箱 / 126邮箱 / Outlook / Gmail 一键填充 |

## 技术栈

| 层面 | 技术 |
|------|------|
| 语言 | C++17 |
| GUI | Qt 6.5（Widgets，现代化 QSS 样式表） |
| 数据库 | MySQL 8.0（C API / libmysql） |
| 加密 | OpenSSL 3.x |
| Socket | Winsock2 + 非阻塞 I/O |
| 字符集 | utf8mb4，支持中文主题/正文/附件名（RFC 2047 + GBK 转换） |
| 构建 | CMake 3.16+，MinGW-w64 13.1 |

## 项目结构

```
mail_system/
├── CMakeLists.txt                  # CMake 构建
├── README.md
├── .gitignore
├── lib_mingw/                      # MinGW 导入库（预生成，开箱即用）
│   ├── libmysql.dll.a              #   MySQL C API
│   ├── libssl.dll.a                #   OpenSSL
│   └── libcrypto.dll.a             #   OpenSSL
├── sql/
│   └── schema.sql                  # 6 张表 DDL
├── resources/
│   └── resources.qrc
└── src/
    ├── main.cpp                    # 入口：Winsock/OpenSSL/DB 初始化
    ├── core/
    │   ├── email.h/.cpp            # Email / Attachment / Account 数据模型
    │   ├── base64.h/.cpp           # Base64 编解码
    │   ├── mime_encoder.h/.cpp     # 邮件 → MIME 格式（发送）
    │   └── mime_decoder.h/.cpp     # MIME 格式 → 邮件（接收，含 GBK→UTF-8）
    ├── network/
    │   ├── tcp_socket.h/.cpp       # Winsock TCP 封装
    │   ├── ssl_socket.h/.cpp       # OpenSSL（非阻塞 select + FIONBIO）
    │   ├── smtp_client.h/.cpp      # SMTP 协议
    │   └── pop3_client.h/.cpp      # POP3 协议
    ├── database/
    │   ├── db_connection.h/.cpp    # MySQL 连接生命周期
    │   └── db_manager.h/.cpp       # CRUD：账号/邮件/附件/联系人/同步
    └── ui/
        ├── main_window.h/.cpp      # 主窗口
        ├── compose_dialog.h/.cpp   # 写邮件对话框
        ├── email_list_widget.h/.cpp# 邮件列表
        ├── email_view_widget.h/.cpp# 邮件阅读
        └── account_dialog.h/.cpp   # 账号配置对话框
```

## 数据库设计（6 表）

| 表 | 说明 |
|----|------|
| `accounts` | 邮箱账号配置（SMTP/POP3 服务器、端口、SSL、授权码） |
| `emails` | 邮件主体（folder：inbox/sent/drafts/trash，含邮件头、正文、已读标记） |
| `attachments` | 附件元数据（文件名、MIME类型、大小、本地路径） |
| `contacts` | 地址簿 |
| `sync_state` | POP3 UIDL 同步追踪（避免重复下载） |

## 环境配置与编译

### 前置依赖

| 依赖 | 本机路径 |
|------|----------|
| Qt 6.5（MinGW） | `D:/huawei/qt/6.5.3/mingw_64/` |
| MySQL Server 8.0 | `C:/Program Files/MySQL/MySQL Server 8.0/` |
| OpenSSL | `D:/ProgramData/miniconda3/Library/` |
| CMake | `D:/huawei/qt/Tools/CMake_64/bin/cmake.exe` |
| MinGW-w64 | `D:/huawei/qt/Tools/mingw1310_64/bin/g++.exe` |

### ① 启动 MySQL

以**管理员身份**运行终端：

```powershell
net start MySQL
```

> 服务名可能是 `MySQL`、`MySQL80`。不确定时运行 `sc query state= all | findstr MySQL` 查看。

### ② 建表

```sql
mysql -u root -p
source D:/HuaweiMoveData/Users/huawei/Desktop/mail_system/mail_system/sql/schema.sql;
```

### ③ 修改数据库密码

编辑 `src/main.cpp` 第 18 行：

```cpp
static const char* DB_PASSWORD = "你的MySQL密码";
```

> 其他参数（主机 localhost、端口 3306、用户名 root、库名 mail_system）无需改动。
>
> `CMakeLists.txt` 通常也不需要改——MySQL 和 OpenSSL 路径已根据本机配置好。仅当你把它们装到不同位置时才需修改第 21 行 `MYSQL_ROOT` 和第 33 行 `OPENSSL_ROOT`。

### ④ 编译

**方式一：Qt 终端（推荐）**

开始菜单搜索 **"Qt 6.5.3 (MinGW 13.1.0 64-bit)"**，打开后：

```bash
cd D:/HuaweiMoveData/Users/huawei/Desktop/mail_system/mail_system
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="D:/huawei/qt/6.5.3/mingw_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**方式二：PowerShell**

```powershell
cd D:\HuaweiMoveData\Users\huawei\Desktop\mail_system\mail_system
$env:PATH = "D:\huawei\qt\Tools\mingw1310_64\bin;D:\huawei\qt\Tools\CMake_64\bin;$env:PATH"
cmake -B build -G "MinGW Makefiles" `
    -DCMAKE_PREFIX_PATH="D:/huawei/qt/6.5.3/mingw_64" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_CXX_COMPILER="D:/huawei/qt/Tools/mingw1310_64/bin/g++.exe" `
    -DCMAKE_MAKE_PROGRAM="D:/huawei/qt/Tools/mingw1310_64/bin/mingw32-make.exe"
cmake --build build
```

**方式三：Qt Creator**

File → Open File or Project → 选 `CMakeLists.txt` → Kit 选 `Qt 6.5.3 MinGW 64-bit` → Build。

### ⑤ 运行

```powershell
.\build\MailSystem.exe
```

> `windeployqt` 已自动部署 Qt / MinGW / MySQL / OpenSSL 全部运行时 DLL，无需设置 PATH。

---

## 使用说明

### 添加账号

启动后点击菜单 **文件 → 添加账号**，填写：

| 字段 | 填写 |
|------|------|
| 账号名称 | 自定义，如 "我的QQ邮箱" |
| 邮箱地址 | 完整邮箱地址 |
| 用户名 | 通常与邮箱地址相同 |
| 授权码 | **授权码，不是登录密码！** |
| SMTP 服务器 | 如 `smtp.qq.com` |
| SMTP 端口 | 465（SSL） |
| POP3 服务器 | 如 `pop.qq.com` |
| POP3 端口 | 995（SSL） |

可通过 **快速设置** 下拉框选择预设模板自动填充。填写后可点击 **测试发送** / **测试接收** 验证。

> ⚠️ **QQ邮箱 / 163邮箱 必须使用授权码！**
>
> - QQ邮箱：网页版 → 设置 → 账户 → POP3/SMTP服务 → 开启 → 生成授权码（16位字母）
> - 163邮箱：网页版 → 设置 → POP3/SMTP/IMAP → 开启 → 新增授权码

### 收发邮件

| 操作 | 方式 |
|------|------|
| 接收邮件 | 点击工具栏 **📥 接收** |
| 写新邮件 | 点击工具栏 **✏️ 写信** |
| 回复邮件 | 选中邮件后点击 **↩ 回复** |
| 删除邮件 | 选中邮件后点击 **🗑 删除** |
| 编辑草稿 | 点击左侧 **📝 草稿箱** → 点击草稿邮件 → 弹出编辑对话框 → 修改后发送或再保存 |
| 切换文件夹 | 点击左侧 **收件箱/已发送/草稿箱/废纸篓** |
| 切换账号 | 点击左侧底部下拉框 |

---

## UI 设计

现代扁平风格，蓝白配色（主色 `#1890FF`），微软雅黑字体，QSS 全局样式表。

- 主按钮（接收/写信）：蓝底白字圆角
- 次按钮（回复/刷新）：白底描边悬停变色
- 删除按钮：红底白字
- 邮件列表：未读邮件加粗显示，已读灰色
- 邮件阅读：头部信息卡片 + 正文区 + 附件卡片

---

## SMTP 协议流程

```
客户端                              服务器
  │── TCP connect :465 ────────────→│  (SSL 握手)
  │←── 220 smtp.qq.com ────────────│
  │── EHLO localhost ──────────────→│
  │←── 250-extensions ... ─────────│  (多行响应)
  │── AUTH LOGIN ──────────────────→│
  │←── 334 VXNlcm5hbWU6 ──────────│
  │── <Base64(username)> ──────────→│
  │←── 334 UGFzc3dvcmQ6 ──────────│
  │── <Base64(password)> ──────────→│
  │←── 235 Authentication succeeded │
  │── MAIL FROM:<sender> ──────────→│
  │── RCPT TO:<recipient> ─────────→│
  │── DATA ────────────────────────→│
  │←── 354 Start mail input ───────│
  │── <MIME 内容> ─────────────────→│
  │── \r\n.\r\n ───────────────────→│  (结束标记)
  │←── 250 OK ─────────────────────│
  │── QUIT ────────────────────────→│
```

## POP3 协议流程

```
客户端                              服务器
  │── TCP connect :995 ────────────→│  (SSL 握手)
  │←── +OK POP3 server ready ──────│
  │── USER <username> ─────────────→│
  │←── +OK ────────────────────────│
  │── PASS <password> ─────────────→│
  │←── +OK ────────────────────────│
  │── STAT ────────────────────────→│
  │←── +OK <count> <total_size> ───│
  │── UIDL ────────────────────────→│  (获取全局唯一 ID)
  │←── +OK <msg_num> <uid> ... ────│
  │── RETR <n> ────────────────────→│  (逐封拉取新邮件)
  │←── +OK <原始 MIME> ────────────│
  │── QUIT ────────────────────────→│
```

## 任务分工建议

| 角色 | 模块 | 文件 |
|------|------|------|
| A | 网络层 + SMTP | `tcp_socket.*` `ssl_socket.*` `smtp_client.*` |
| B | POP3 + MIME 解码 | `pop3_client.*` `mime_decoder.*` |
| C | 数据库 + MIME 编码 | `db_connection.*` `db_manager.*` `mime_encoder.*` |
| D | UI 全部 | `main_window.*` `compose_dialog.*` `email_list_widget.*` `email_view_widget.*` `account_dialog.*` |
| E (可选) | Base64 + 集成 | `base64.*` `email.*` 端到端测试 |
| F (可选) | 实验报告 | 文档撰写 |

---

## 许可证

本项目为计算机网络实践课程作业，仅供学习交流使用。
