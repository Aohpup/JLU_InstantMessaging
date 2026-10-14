# 数据库与 MySQL 安装指南

> 历史资料：2026-09-29 项目已改用 SQLite，本文不再作为项目配置任务。下述安装与运行状态是此前检查记录，本次未重新检查，也未停用、卸载 MySQL84 或删除数据库。后续使用 [SQLite 清单](P3_SQLite临时任务清单.md)。

适用场景：Windows 本机学习。本文只提供手动操作说明，没有自动下载安装、修改系统配置或创建项目数据库。

当前进度：用户已安装 MySQL Community Server 8.4.11（64 位）。复查确认 MySQL84 服务正在运行，启动类型为自动，MySQL 进程监听 3306 和 33060。安装阶段已完成，可以直接阅读“第四步 登录验证”；以下下载与安装步骤保留供参考，不需要重新执行。

已确认客户端位置为 `C:/Program Files/MySQL/MySQL Server 8.4/bin/mysql.exe`，但当前终端 PATH 中还找不到 mysql。使用下文完整路径命令即可。账号密码登录、SQL 查询和 C++ 项目连接尚未验证；本次检查没有创建或修改数据库。

## 1 数据库是什么

数据库是按一定结构组织并长期保存的数据集合。用这个课设举例，用户资料是一类数据，好友关系是一类，群成员又是一类；它们既需要保存，也需要能互相查找和保持一致。

关系型数据库通常把数据放在表中。表像有固定列的表格：用户表的一行代表一个用户，列可能是用户编号、昵称、生日。主键用于唯一识别一行，外键用于引用另一张表的记录；事务让一组操作一起成功或一起撤销，例如加好友不能只保存了其中一边。

| 名词 | 含义 | 本课设中的例子 |
| --- | --- | --- |
| 数据库 | 实际保存的一套相关数据 | 以后创建的 jlu_im |
| 数据库管理系统 | 管理数据的软件 | MySQL Server |
| SQL | 向数据库发命令的语言 | SELECT 查询、INSERT 增加、UPDATE 修改、DELETE 删除 |
| 客户端 | 输入命令、查看结果的工具 | mysql.exe 命令行 |
| C++ 数据库连接库 | 程序访问 MySQL 的接口 | 后续封装进 MySqlStorage 的客户端库 |

所以“安装 SQL”通常实际指“安装 MySQL 等数据库软件”。SQL 本身是语言，不是一个必须单独安装的应用。Microsoft SQL Server 是另一种数据库产品，也不等于 SQL 语言。

程序运行时的内存数据随进程退出而消失；保存到 JSON 文件或数据库后，才可以下次启动继续读取。数据库提供查询、约束和事务，但你的 C++ 程序仍要实现账号、好友、群权限等业务。

## 2 这次是否需要数据库

课程文件允许文件保存，数据库编程出现在课后扩展部分。MySQL 安装已完成；核心项目仍先用 JSON 完成，批准数据库增强阶段后复用现有 MySQL 实例接入。

学长示例使用 MySQL 8.0.34；本方案为新项目推荐 MySQL Community Server 8.4 LTS，并使用该分支下载页当时提供的维护版本，不要求复现学长的旧小版本。Windows 官方推荐 MSI 安装后运行 MySQL Configurator。[官方安装说明](https://dev.mysql.com/doc/refman/8.4/en/windows-installation.html)

安装 Server 不等于 C++ 工程已能连接数据库；头文件、连接库、位数与编译器匹配的验证留在批准后的接入阶段。当前也不要求安装 Navicat 等额外付费工具。

## 3 手动安装

### 第一步 检查是否已有 MySQL

在开始菜单搜索 MySQL，或按 Win+R 输入 services.msc 查看是否已有 MySQL 服务。如果已有实例，先记录版本和服务名；不要为了本教程卸载、覆盖或选择原地升级。本机现在已经有运行中的 MySQL84 服务，无需重复安装；mysql 未加入当前终端 PATH 不代表服务未安装。

### 第二步 下载官方安装包

打开 [MySQL Community Server 8.4 下载页](https://dev.mysql.com/downloads/mysql/8.4.html)，版本选择 8.4 LTS 分支，操作系统选择 Microsoft Windows，选择 Windows x86 64-bit 的 MSI Installer，文件名类似 mysql-8.4.x-winx64.msi。

本教程面向 x64 Windows；如果系统为 ARM64，先核实支持情况再选择安装包。不要选择 ZIP Debug Binaries & Test Suite。下载页若出现账号入口并提供 No thanks, just start my download，可用该入口直接下载。

双击 MSI，按向导安装。若提示缺少 Visual C++ 运行库，按安装器要求补齐；官方文档说明 Windows 版需要 Visual C++ 2019 Redistributable。可参考 [微软最新受支持的 Visual C++ 运行库说明](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)，选择与 x64 匹配的包。

### 第三步 运行配置向导

安装完成后运行 MySQL Configurator。MySQL 8.4 使用 Server MSI 加 Configurator，旧教程中的 MySQL Installer 和 Developer Default 页面不一定出现。

建议配置如下。名称会随版本略有差异，以安装界面为准。[官方 Configurator 说明](https://dev.mysql.com/doc/refman/8.4/en/mysql-configurator-workflow-server.html)

| 页面或项目 | 建议选择 | 为什么 |
| --- | --- | --- |
| Configuration Type | Development | 用于个人开发电脑 |
| TCP/IP | 开启 | 后续本机客户端通过 TCP 连接 |
| Port | 3306 | MySQL 常用默认端口 |
| Open Windows Firewall port for network access | 本机练习取消勾选 | 本次只需要在自己电脑上访问 |
| Root Password | 设置自己的密码并妥善保存 | root 是数据库管理员 |
| Additional Users | 可暂时不添加 | 项目专用账号在批准接入后建立 |
| Windows Service | 启用，记下实际服务名 | 方便启动和检查状态 |
| Start at System Startup | 可保留默认 | 重启后自动运行 |
| Service Account 与 File Permissions | 保留向导推荐值 | 初学阶段无需自定义账户 |
| Sample Databases | 不必安装 | 安装验证用不到示例库 |
| Apply Configuration | 点击 Execute | 等待各项完成 |

不要将 root 密码发送给我、写在聊天截图中或放进项目文档。之后程序会使用仅限项目数据库的专用账号。

如果 3306 被占用，先确认是否已经运行 MySQL；不要直接关闭不认识的服务。确需新实例时可用 3307 并记录，后面连接命令也要相应改成 3307。如果向导询问升级现有实例，先停在该页面核对现有数据与用途。

### 第四步 登录验证

打开 PowerShell。如果安装路径使用默认值，可以执行：

```powershell
& "C:\Program Files\MySQL\MySQL Server 8.4\bin\mysql.exe" --host=127.0.0.1 --port=3306 --user=root --password
```

这里的 & 是 PowerShell 的运行运算符，允许执行带空格路径的程序。安装路径或端口不同则改成你自己的值。出现密码提示后再输入，输入时通常不显示字符。不要把密码直接写在命令行末尾。

成功后应看到 mysql> 提示符。在这个提示符下逐条输入：

```sql
SELECT VERSION();
SELECT CURRENT_USER();
SHOW DATABASES;
SELECT 1 + 1 AS result;
```

能显示版本、当前用户、数据库列表以及结果 2，就说明服务运行且客户端可以登录。SELECT 是查询，SHOW DATABASES 是查看数据库列表。这些语句不会创建项目表。每条 SQL 末尾的分号表示语句结束；返回 PowerShell 用 exit;。

## 4 可选 SQL 小练习

确认安装后可以自己手动执行下列练习。它只在当前连接创建临时表，断开后自动消失，不是正式项目建库。先通过 USE mysql 选中已有数据库，再操作名字明确的临时表；不会改动 MySQL 的系统表。

```sql
USE mysql;
CREATE TEMPORARY TABLE jlu_learning_users (
    id INT PRIMARY KEY,
    nickname VARCHAR(32) NOT NULL
);
INSERT INTO jlu_learning_users VALUES (1, 'Alice'), (2, 'Bob');
SELECT * FROM jlu_learning_users;
UPDATE jlu_learning_users SET nickname = 'Alice2' WHERE id = 1;
SELECT * FROM jlu_learning_users WHERE id = 1;
DELETE FROM jlu_learning_users WHERE id = 2;
SELECT * FROM jlu_learning_users;
```

顺序对应建临时表、添加、查询、修改、删除。WHERE 限定操作对象；以后操作真实表时，遗漏 WHERE 可能影响整张表。这个练习用英文昵称，先排除终端编码影响；正式项目统一使用 utf8mb4 保存中文。

## 5 常见问题

| 现象 | 含义和处理 |
| --- | --- |
| mysql 不是可识别命令 | 通常只是没加入 PATH；先用上面的完整路径，不必重装 |
| Can't connect 或错误 2003 | 检查服务是否启动、地址及端口是否正确 |
| Access denied 或错误 1045 | 核对用户名和密码；它不是“没安装成功”的同义词 |
| 输入后出现 -> | SQL 可能缺少分号或引号未闭合；用 \c 取消当前输入后重输 |
| 配置停在 Starting Server | 查看 Configurator 的 Log，检查端口和数据目录错误；保存具体报错，不要先删除数据目录 |
| Workbench 提示版本不兼容 | 可以直接使用 mysql.exe；官方兼容表对 Workbench 8.0 连接 Server 8.4 标有限制 |
| SQL 在 PowerShell 报错 | SQL 应输入 mysql>，连接启动命令才输入 PowerShell |

Workbench 不是本教程的必装项。其 8.0 版本面向 MySQL 8.0，不能把它与 8.4 的所有管理功能视为完全兼容。[官方兼容表](https://www.mysql.com/support/supportedplatforms/compatibility.html)

## 6 安装后告诉我什么

版本、服务状态和监听端口已核实。下一步可在本机手动登录并执行 SELECT VERSION()，告诉我查询是否成功；若失败，提供不含密码的错误文本或截图即可，无需再次报告已验证的安装信息。安装成功不会自动触发项目开发，仍须你明确批准方案。
