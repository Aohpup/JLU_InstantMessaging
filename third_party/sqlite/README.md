# SQLite 第三方源码

固定版本：3.53.4。官方合并源码，原样保存 sqlite3.c、sqlite3.h、sqlite3ext.h。

来源：https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip

下载页：https://www.sqlite.org/download.html

官方压缩包 SHA3-256：`628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e`，下载后已核对。

源文件逐项 SHA-256 见 source.json。版本随项目固定，不在每次构建时联网更新。

SQLite 原文件中的版权/公有领域说明原样保留；许可说明：https://www.sqlite.org/copyright.html 。第三方代码不适用自写类名 LJY 和中文注释约定。

本项目将 sqlite3.c 编译为独立静态库；sqlite3ext.h 保留供完整接口配套，未开启项目扩展开发。未安装系统数据库服务。
