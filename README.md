# Mail System — SMTP & POP3 Email Client

计算机网络实践课程项目：基于 C++ 原生 Socket 编程的邮件系统（SMTP + POP3 客户端），使用 MySQL 数据库存储邮件数据，Qt 框架构建 Windows 图形界面。

## 功能

| 功能 | 描述 |
|------|------|
| 📧 **编写邮件** | 支持收件人/抄送/密送、纯文本及 HTML 正文、文件附件 |
| 📤 **发送邮件** | SMTP 协议，支持 SSL/TLS 加密（端口 465）及 STARTTLS（端口 587），AUTH LOGIN 认证 |
| 📥 **接收邮件** | POP3 协议，支持 SSL/TLS 加密（端口 995），UIDL 去重，增量获取新邮件 |
| 📖 **阅读邮件** | 邮件列表（发件人/主题/日期），正文渲染（纯文本+HTML），附件展示 |
| 🗑️ **删除邮件** | 软删除（移入废纸篓），支持 POP3 DELE 服务器端删除 |
| 📝 **草稿箱** | 保存草稿、重新编辑后发送 |
| 🔄 **回复/转发** | 引用原邮件内容，自动填充收件人和主题 |
| 📋 **多账号管理** | 支持多个邮箱账号，独立收件箱/已发送/草稿/废纸篓 |
| 🌐 **主流邮箱** | 预置 QQ邮箱、163邮箱、126邮箱、Outlook、Gmail 配置模板 |

## 技术栈

| 层面 | 技术 |
|------|------|
| 编程语言 | C++17 |
| GUI 框架 | Qt 5.15+ / Qt 6.x (Widgets) |
| 数据库 | MySQL 8.0 (MySQL C API / libmysql) |
| 加密传输 | OpenSSL 1.1.1+ |
| Socket | Winsock2（原生 Windows Socket API） |
| 构建系统 | CMake 3.16+ |
| 编译器 | MSVC 2019+ 或 MinGW-w64 |

## 项目结构

```
mail_system/
├── CMakeLists.txt                     # CMake 构建配置
├── README.md                          # 项目说明
├── .gitignore
├── sql/
│   └── schema.sql                     # 数据库建表 DDL
├── resources/
│   └── resources.qrc                  # Qt 资源文件
└── src/
    ├── main.cpp                       # 程序入口
    ├── core/                          # 核心模块
    │   ├── email.h / .cpp             # Email/Attachment/Account 数据模型
    │   ├── base64.h / .cpp            # Base64 编解码
    │   ├── mime_encoder.h / .cpp      # MIME 消息构造（发送）
    │   └── mime_decoder.h / .cpp      # MIME 消息解析（接收）
    ├── network/                       # 网络层
    │   ├── tcp_socket.h / .cpp        # Winsock TCP 封装
    │   ├── ssl_socket.h / .cpp        # OpenSSL 加密层
    │   ├── smtp_client.h / .cpp       # SMTP 协议实现
    │   └── pop3_client.h / .cpp       # POP3 协议实现
    ├── database/                      # 数据库层
    │   ├── db_connection.h / .cpp     # MySQL 连接管理
    │   └── db_manager.h / .cpp        # 数据 CRUD 操作
    └── ui/                            # 图形界面
        ├── main_window.h / .cpp       # 主窗口
        ├── compose_dialog.h / .cpp    # 编写邮件对话框
        ├── email_list_widget.h / .cpp # 邮件列表组件
        ├── email_view_widget.h / .cpp # 邮件阅读组件
        └── account_dialog.h / .cpp    # 账号配置对话框
```

## 架构

```
┌─────────────────────────────────────┐
│            UI Layer (Qt)            │  主窗口 / 编写 / 列表 / 阅读 / 账号管理
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│          Core Layer                 │  Email模型 / MIME编解码 / Base64
└──────┬────────────────┬─────────────┘
       │                │
┌──────▼──────┐  ┌──────▼──────┐
│   Network   │  │  Database   │
│ SMTP/POP3   │  │  MySQL CRUD │
│ TCP/SSL     │  │  6 Tables   │
└─────────────┘  └─────────────┘
```

## 数据库设计

6 张数据表，字符集 utf8mb4：

| 表名 | 说明 |
|------|------|
| `accounts` | 邮箱账号配置（SMTP/POP3 服务器、端口、SSL、认证信息） |
| `emails` | 邮件存储（按 folder 分类：inbox/sent/drafts/trash，含邮件头、正文、标记） |
| `attachments` | 附件元数据（文件名、MIME类型、大小、本地路径） |
| `contacts` | 地址簿（账号ID、显示名、邮箱地址） |
| `sync_state` | POP3 同步状态（UIDL 去重追踪） |

## 环境配置与编译

### 前置依赖

编译前请确保已安装：

| 依赖 | 本机确认命令 |
|------|-------------|
| Qt 6.5+（MinGW 版本） | `ls D:/huawei/qt/6.5.3/mingw_64/` |
| MySQL Server 8.0 | `ls "C:/Program Files/MySQL/MySQL Server 8.0/"` |
| OpenSSL（conda 自带） | `ls D:/ProgramData/miniconda3/Library/include/openssl/ssl.h` |
| CMake 3.16+（Qt 自带） | `D:/huawei/qt/Tools/CMake_64/bin/cmake.exe --version` |
| MinGW-w64（Qt 自带） | `D:/huawei/qt/Tools/mingw1310_64/bin/g++.exe --version` |

### 初始化数据库

#### ① 启动 MySQL 服务

以**管理员身份**打开终端，执行以下任一方式：

```powershell
# 方式 A：net 命令（以管理员身份运行）
net start MySQL

# 方式 B：sc 命令（以管理员身份运行）
sc start MySQL

# 方式 C：图形界面
Win+R → 输入 services.msc → 找到 MySQL → 右键 → 启动
```

> **必须以管理员身份运行终端**（右键 → 以管理员身份运行），否则会报"拒绝访问"。
>
> MySQL 服务名常见为 `MySQL`、`MySQL80`、`MYSQL`。不确定时运行 `sc query state= all | findstr MySQL` 查看。

#### ② 执行建表脚本

启动 MySQL 命令行并执行 `sql/schema.sql`：

```sql
-- 在 mysql> 命令行中使用 source（注意用正斜杠）
source D:/HuaweiMoveData/Users/huawei/Desktop/mail_system/mail_system/sql/schema.sql;
```

> 首次进入 MySQL 命令行：`mysql -u root -p`，输入密码即可。如果从未设置过 root 密码，命令行可能会提示或用空密码尝试。

### 编译前需修改

**只需修改一个文件：`src/main.cpp` 第 18 行** — 把 `DB_PASSWORD` 改成你的 MySQL root 密码：

```cpp
static const char* DB_PASSWORD = "你的MySQL密码";   // 第 18 行，只改这里
```

其他参数（主机 `localhost`、端口 `3306`、用户名 `root`、数据库名 `mail_system`）均使用默认值，无需改动。

> **`CMakeLists.txt` 通常不需要改。** 仅当你把 MySQL 或 OpenSSL 装在非默认路径时才需要修改以下两行：
> - 第 21 行 `MYSQL_ROOT` — MySQL 安装根目录
> - 第 33 行 `OPENSSL_ROOT` — OpenSSL 安装根目录
>
> 项目中 `lib_mingw/` 目录已包含预生成的 MinGW 导入库（`libmysql.dll.a` / `libssl.dll.a` / `libcrypto.dll.a`），开箱即用。

### 编译（三种方式，选其一）

**方式一：Qt 自带终端（最简单，推荐）**

开始菜单搜索 **"Qt 6.5.3 (MinGW 13.1.0 64-bit)"**，打开后执行：

```bash
cd D:/HuaweiMoveData/Users/huawei/Desktop/mail_system/mail_system
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="D:/huawei/qt/6.5.3/mingw_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**方式二：PowerShell 手动指定**

```powershell
cd D:\HuaweiMoveData\Users\huawei\Desktop\mail_system\mail_system

# 设置 PATH
$env:PATH = "D:\huawei\qt\Tools\mingw1310_64\bin;D:\huawei\qt\Tools\CMake_64\bin;$env:PATH"

# 配置
cmake -B build -G "MinGW Makefiles" `
    -DCMAKE_PREFIX_PATH="D:/huawei/qt/6.5.3/mingw_64" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_CXX_COMPILER="D:/huawei/qt/Tools/mingw1310_64/bin/g++.exe" `
    -DCMAKE_MAKE_PROGRAM="D:/huawei/qt/Tools/mingw1310_64/bin/mingw32-make.exe"

# 编译
cmake --build build
```

**方式三：Qt Creator**

打开 Qt Creator → **File → Open File or Project** → 选择 `CMakeLists.txt` → 选择 Kit **"Qt 6.5.3 MinGW 64-bit"** → 点击左下角锤子图标 Build。

### 运行

```powershell
# 确保 MySQL 服务已启动
.\build\MailSystem.exe
```

## 使用说明

### 1. 首次启动 — 添加邮箱账号

程序启动后，点击菜单 **File → Add Account**，填写以下信息：

| 字段 | 说明 |
|------|------|
| Account Name | 自定义名称，如 "我的QQ邮箱" |
| Email Address | 完整邮箱地址 |
| Username | 通常与邮箱地址相同 |
| Password | **授权码**（非邮箱登录密码，见下方说明） |
| SMTP Server | 发送邮件服务器 |
| SMTP Port | 465（SSL）或 587（STARTTLS） |
| POP3 Server | 接收邮件服务器 |
| POP3 Port | 995（SSL）或 110 |

也可以通过 **Quick Setup** 下拉框选择预设模板自动填充服务器信息。

> ⚠️ **重要**：QQ邮箱、163邮箱等需要使用**授权码**而非登录密码。请在邮箱网页版设置中开启 SMTP/POP3 服务并生成授权码。

### 2. 获取邮箱授权码

- **QQ邮箱**：设置 → 账户 → POP3/SMTP服务 → 开启 → 生成授权码
- **163邮箱**：设置 → POP3/SMTP/IMAP → 开启 → 新增授权码
- **126邮箱**：同 163邮箱操作

### 3. 收发邮件

- 点击 **Receive** 按钮从服务器拉取新邮件
- 点击 **Compose** 按钮编写新邮件
- 选中邮件后点击 **Reply** 回复
- 选中邮件后点击 **Delete** 删除

## SMTP 协议实现要点

```
客户端                              服务器
  │──── TCP 连接 (端口 465/587) ────→│
  │←──── 220 服务就绪 ───────────────│
  │──── EHLO localhost ─────────────→│
  │←──── 250 扩展列表 ───────────────│
  │  (端口587: STARTTLS → 升级SSL)   │
  │──── AUTH LOGIN ─────────────────→│
  │←──── 334 VXNlcm5hbWU6 ──────────│
  │──── <Base64用户名> ─────────────→│
  │←──── 334 UGFzc3dvcmQ6 ──────────│
  │──── <Base64密码> ───────────────→│
  │←──── 235 认证成功 ───────────────│
  │──── MAIL FROM:<sender> ─────────→│
  │←──── 250 OK ────────────────────│
  │──── RCPT TO:<recipient> ────────→│
  │←──── 250 OK ────────────────────│
  │──── DATA ───────────────────────→│
  │←──── 354 开始邮件输入 ───────────│
  │──── <MIME邮件内容> ─────────────→│
  │──── \r\n.\r\n ──────────────────→│
  │←──── 250 OK ────────────────────│
  │──── QUIT ───────────────────────→│
```

## POP3 协议实现要点

```
客户端                              服务器
  │──── TCP 连接 (端口 995/110) ────→│
  │←──── +OK 服务就绪 ───────────────│
  │──── USER <username> ────────────→│
  │←──── +OK ───────────────────────│
  │──── PASS <password> ────────────→│
  │←──── +OK ───────────────────────│
  │──── STAT ───────────────────────→│
  │←──── +OK <邮件数> <总大小> ─────│
  │──── UIDL ───────────────────────→│
  │←──── +OK <编号> <UID>列表 ──────│
  │──── RETR <n> ───────────────────→│
  │←──── +OK <原始邮件内容> ────────│
  │──── DELE <n> ───────────────────→│  (可选)
  │──── QUIT ───────────────────────→│
```

## 任务分工建议（4-6人组）

| 角色 | 负责模块 | 文件 |
|------|----------|------|
| A | 网络基础层 + SMTP | `tcp_socket.*`, `ssl_socket.*`, `smtp_client.*` |
| B | POP3 + MIME 解码 | `pop3_client.*`, `mime_decoder.*` |
| C | 数据库 + MIME 编码 | `db_connection.*`, `db_manager.*`, `mime_encoder.*` |
| D | UI 全部 | `main_window.*`, `compose_dialog.*`, `email_list_widget.*`, `email_view_widget.*`, `account_dialog.*` |
| E (可选) | Base64 + 集成测试 | `base64.*`, `email.*`, 端到端测试 |
| F (可选) | 文档 + 实验报告 | README、实验报告撰写 |

## 许可证

本项目为计算机网络实践课程作业，仅供学习交流使用。
