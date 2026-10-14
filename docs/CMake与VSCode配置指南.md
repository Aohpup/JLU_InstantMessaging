# CMake 与 VS Code 配置指南

更新日期：2026 年 9 月 30 日。P1/P2/P3 已验收完成，SQLite 启动加载、保存菜单和正常退出保存可用。

## 1 工具各自做什么

| 工具 | 本机版本 | 作用 |
| --- | --- | --- |
| VS Code | 使用本机已安装版本 | 写代码、显示错误、提供操作入口 |
| C/C++ 扩展 | 1.34.3 | 代码提示、跳转、连接调试器 |
| CMake Tools 扩展 | 1.23.52 | 在 VS Code 中选择预设、配置和构建 |
| CMake | 4.2.1 | 根据 CMakeLists.txt 生成构建规则 |
| Ninja | 1.13.2 | 按规则调用编译器，并只重建有变化的部分 |
| g++ | GCC 15.2.0 | 把 C++ 源文件编译并链接成程序 |
| GDB | 17.1 | 设置断点、单步执行、查看变量 |

这些工具和两个必要扩展均已检查到，无需重新安装。CMake、Ninja、g++、GDB 均位于 C:/mingw64/bin。

构建过程是：VS Code 或终端调用 CMake → CMake 生成 Ninja 规则 → Ninja 调用 g++ → 生成 jlu_im.exe。调试时由 VS Code 调用 GDB 启动该程序。

## 2 本次创建的文件

| 文件 | 用途 |
| --- | --- |
| CMakeLists.txt | 定义项目、可执行目标、源文件、C++17 和警告选项 |
| CMakePresets.json | 项目通用 Debug 构建预设 |
| CMakeUserPresets.json | 本机 local-debug 预设，指定 C:/mingw64 的编译器与 Ninja |
| src/main.cpp | 处理启动模式、读库、创建会话及菜单并传入保存动作，设置中文输入输出 |
| .vscode/settings.json | CMake 路径、编译命令清单、备用包含路径、悬浮提示和 UTF-8 |
| .vscode/tasks.json | Ctrl+Shift+B 先配置再构建 |
| .vscode/launch.json | F5 调用 GDB，调试前自动构建 |
| .vscode/extensions.json | 推荐两个必要扩展 |
| .gitignore | 忽略构建输出和本机预设；本次没有初始化 Git |

生成结果在 build/debug，程序在 build/debug/bin/jlu_im.exe。build/debug/compile_commands.json 是 CMake 自动生成的编译命令清单；不手动修改它，也不手改 CMakeCache.txt 或 build.ninja。

本机预设继承通用 debug 预设。项目搬到另一台电脑时可重新建立 CMakeUserPresets.json，并调整 .vscode 中的工具路径。两个预设目前共享 build/debug，只能用于同一套 GCC/Ninja 工具链；以后切换 MSVC 或 Release，应另用构建目录。

## 3 在 VS Code 中打开

1. 在 VS Code 选择“文件 → 打开文件夹”，打开 D:/C_code/Cpp_test/JLU_InstantMessaging，不能只打开 src 或旧的“新建文件夹”。
2. 若出现工作区信任提示，确认是自己的这个项目后启用信任。
3. 按 Ctrl+Shift+X 搜索并确认 Microsoft 的 C/C++ 和 CMake Tools 已启用。它们已经安装，通常无需再次操作。
4. 按 Ctrl+Shift+P，运行 CMake: Select Configure Preset，选择 Local Debug - MinGW C:/mingw64（内部名字 local-debug）。
5. 运行 CMake: Configure。看到 Configuring done 和 Generating done 代表配置完成。
6. 运行 CMake: Select Build Preset，选择 local-debug，再运行 CMake: Build；若询问目标，选择 jlu_im。

这里已经提供完整配置文件，无需再运行 CMake: Quick Start，以免生成第二套工程。选择的是 Preset（预设），不是旧教程的 Kit（工具包）。界面命令可能显示中文，可搜索 CMake 关键词找到对应操作。

## 4 构建 运行与调试

日常构建：保存代码后按 Ctrl+Shift+B，运行默认 CMake Build Debug 任务。它先配置再构建，错误会显示在终端和“问题”面板。

直接运行：在项目根目录的 PowerShell 终端中执行下面三条命令。第一条准备规则，第二条编译程序，第三条才真正运行程序。

```powershell
cmake --preset local-debug
cmake --build --preset local-debug
.\build\debug\bin\jlu_im.exe
```

终端若提示找不到 cmake，打开一个新终端让工作区 PATH 设置生效，或将前两条命令中的 cmake 替换为 & "C:/mingw64/bin/cmake.exe"。不用修改系统 PATH。

程序显示中文主菜单。先选择“2 注册并开通首项服务”，再用程序返回的对外号码登录；详细演示见 [P1 第 5 和第 6 步说明](P1第5和6步说明.md)。新数据库首次写入演示账号，密码统一为 DemoOnly123!；已有库读取保存的数据。主菜单 4、登录菜单 10 可主动保存；注销后在主菜单选 0 保存并退出。--empty 和 --demo 是退出后清空的隔离内存模式。

调试练习：在 src/main.cpp 的 app.run() 处设置断点，按 F5 启动；继续运行后在终端输入菜单选项。也可在 SessionManagerLJY::login 处设断点，查看首次登录的验证过程。

VS Code 已安装 Code Runner，但它的 Run Code 按钮常按单个文件运行。这个项目使用 CMake 的构建入口和上述 F5 配置，才能保证后续多个 .cpp 一起编译。

## 5 如何理解 CMakeLists

cmake_minimum_required 要求 CMake 至少为 3.21；本机 4.2.1 满足要求。project 声明项目名与 C/C++ 语言。add_executable(jlu_im src/main.cpp) 声明生成 jlu_im 可执行文件及其源文件。

target_compile_features 声明需要 C++17 功能；CXX_STANDARD 17 与 CXX_STANDARD_REQUIRED YES 固定使用 C++17，CXX_EXTENSIONS OFF 关闭编译器专属语言扩展。RUNTIME_OUTPUT_DIRECTORY 将程序放进构建目录的 bin 子目录。警告选项帮助发现潜在错误；Debug 类型通过预设启用，便于查看源码和变量。

业务代码位于 jlu_core 静态库，菜单位于 jlu_console 静态库；jlu_im 链接菜单和业务库。p1_tests 测试底层账号，p1_session_tests 测试会话与菜单。新增业务 .cpp 加入 jlu_core，新菜单文件加入 jlu_console。密码已简化为内存字符串比较，不再链接 bcrypt。

## 6 常见情况

| 情况 | 处理 |
| --- | --- |
| CMake 命令在命令面板中不存在 | 检查 CMake Tools 是否在本工作区启用，必要时重新加载窗口 |
| 找不到 local-debug 预设 | 确认打开项目根目录，且 CMakeUserPresets.json 存在 |
| 源码红线但构建成功 | 重新配置 CMake，检查 compile_commands.json 已更新且包含对应源文件，备用包含路径中应有项目 src |
| 找不到 g++、Ninja 或 GDB | 检查本机预设与 .vscode 中 C:/mingw64/bin 路径 |
| Ninja 显示 no work to do | 没有需要重新编译的变化，是正常结果 |
| F5 找不到 exe | 先检查自动构建任务报错，不要随意改程序路径 |
| 运行提示缺少 MinGW DLL | 使用项目内新开的终端或已配置 F5，使 C:/mingw64/bin 在 PATH 中 |
| 更换编译器后报缓存冲突 | 新建构建目录和预设，不复用旧编译器的缓存 |

P3 第一批已接入 SQLite 3.53.4：CMake 同时启用 C/CXX，gcc 编译 sqlite3.c 为静态库；本地预设指定 C:/mingw64/bin/gcc.exe。主程序在第四批已接入持久化；独立验证目标 p3_sqlite_smoke 读写 build/debug/p3_batch1/smoke.db，不依赖 MySQL84。首次更新后运行 `cmake --preset local-debug` 再构建。详情见 [P3 第一批说明](P3第一批说明与验收.md)。

参考：[VS Code CMake 起步说明](https://code.visualstudio.com/docs/cpp/cmake-quickstart)、[CMake 预设手册](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)、[VS Code MinGW 配置说明](https://code.visualstudio.com/docs/cpp/config-mingw)。

## P1 业务测试

默认构建生成 jlu_im、p1_tests、p1_session_tests、p1_binding_tests、p2_friend_tests、p2_recommendation_tests、p2_group_tests 、p2_group_operations_tests 、p2_interaction_tests 和 p2_subgroup_mode_tests；执行 `ctest --preset local-debug` 运行原有九个业务测试程序及新增的 p3_sqlite_smoke，加上第二批 p3_storage_save 、第三批 p3_storage_load 和第四批 p3_console，共十三个程序；原业务部分为 72 组场景（12＋15＋5＋6＋8＋6＋8＋8＋4）。P1 及 P2 全阶段本地验收已通过，详见 [P2 第 10 步验收报告](P2第10步验收报告.md)。

P3 第二批新增 `p3_storage_save_tests`，包含 3 组保存、回滚及打开失败场景。可运行 `ctest --preset local-debug -R p3_storage_save -V`。建表脚本配置时生成到 build/debug/generated/SqliteSchemaLJY.h，存储类无运行时源码路径依赖。见 [第二批说明](P3第二批说明与验收.md)。

P3 第三批新增 `p3_storage_load_tests`，包含 3 组读取、往返与失败保留场景。可运行 `ctest --preset local-debug -R p3_storage_load -V`，验证库位于 build/debug/p3_batch3。主程序启动和保存菜单已在第四批整合。见 [第三批说明](P3第三批说明与验收.md)。

P3 第四批可运行 `ctest --preset local-debug -R p3_console -V` 验证保存菜单；`python tests/p3_startup_acceptance.py` 验证真实启动和重启。默认路径由 CMake 定义为项目 data/jlu_im.db；从不同工作目录启动位置相同，--database 可覆盖该位置。Windows 使用 wmain 宽字符入口，MinGW 通过 -municode 链接以支持中文路径。移动项目后重新配置构建或指定数据库路径。

VS Code 原 F5 配置进入正常持久化模式；运行和调试下拉框新增 `Debug jlu_im (演示内存模式)`，传入 --demo。完整使用方式见 [P3 第四批说明](P3第四批说明与验收.md)。

P3 第五批总体验收命令为 `python tests/p3_acceptance.py`：实际程序完成 4 次连续重启及 2 项失败场景，证据写到 build/debug/p3_acceptance。配合 `ctest --preset local-debug` 检查全部 13 个测试程序。正式数据库不参与测试；详见 [P3 阶段报告](P3第五批与阶段总体验收.md) 和 [演示备份说明](P3运行演示与备份.md)。

## 函数悬浮注释

项目已启用 `editor.hover.enabled: "on"`、`C_Cpp.hover: "default"` 和 `C_Cpp.intelliSenseEngine: "default"`。IntelliSense 直接读取 build/debug/compile_commands.json，备用 includePath 包含 `${workspaceFolder}/src`，编译器指定 g++.exe；configurationProvider 留空，CMake Tools 继续负责构建。此前实际诊断缺少项目 src 路径，修正后用户已确认悬浮可用。

函数说明保留普通简体中文 `//`，紧贴对应声明；不同函数分别写简短说明，不使用 `@brief`、`@param` 等标签。关键实现注释仍留在 `.cpp`。

日常验证及排错步骤：

1. 在 VS Code 打开整个 JLU_InstantMessaging 项目，选择 local-debug 预设并运行 `CMake: Configure`，等待代码分析结束。
2. 打开 `src/domain/PersonLJY.cpp`，将鼠标放到 `PersonLJY::birthDate` 定义处的函数名上，检查是否显示头文件中的“只读查询生日，格式为 YYYY-MM-DD。”。
3. 打开 `src/services/SessionManagerLJY.cpp`，在 `platform_.findPerson` 调用的函数名上检查查询说明；也可将光标放在函数名上，先按 Ctrl+K，再按 Ctrl+I 调出悬浮信息。
4. 如果没有显示，先确认头文件没有包含错误、能跳转到正确声明，且 C/C++ 扩展在本工作区启用。必要时运行 `C/C++: Reset IntelliSense Database`，等待索引重建后重试。

构建通过只能验证声明、定义及包含路径在编译时正确；普通注释能否在定义处和调用处显示，需要以上界面检查确认。未显示时应继续排查语言服务，不自动转换为其他注释格式。

额外验收脚本只依赖 Python 标准库：`python tests/p1_acceptance.py` 验证实际菜单与源码组织；`python tests/p1_console_native_test.py` 验证真实 Windows 控制台中的 Ctrl+C。执行前先构建，输出保存在 build/debug/acceptance。选中文本的复制仍由 VS Code 终端负责；菜单退出使用 0。

P2 第一批可单独执行 `ctest --preset local-debug -R p2_friends`。实际好友菜单流程可用 `python tests/p2_friend_acceptance.py` 验证，输出保存在 build/debug/p2_batch1_acceptance。

P2 第二批可单独执行 `ctest --preset local-debug -R p2_recommendations`。实际共同好友与推荐菜单可用 `python tests/p2_recommendation_acceptance.py` 验证，结果保存在 build/debug/p2_batch2_acceptance。

P2 第三批演示可用 `jlu_im.exe --demo` 隔离运行；`jlu_im.exe --empty` 启动空平台。单独群测试使用 `ctest --preset local-debug -R p2_groups`，实际群菜单使用 `python tests/p2_group_acceptance.py`。旧阶段实际菜单脚本已显式指定 --empty 或 --demo，不接触正式库。

第四批可单独执行 `ctest --preset local-debug -R p2_group_operations`。实际多账号群操作使用 `python tests/p2_group_operations_acceptance.py`，结果保存在 build/debug/p2_batch4_acceptance。

2026-09-29 交互修正：`ctest --preset local-debug -R p2_interaction`；原生控制台复现用 `python tests/p2_interaction_native_test.py`，结果保存到 build/debug/interaction_0929。

第五批重点验证：`ctest --preset local-debug -R p2_subgroup_mode`；实际程序主线：`python tests/p2_subgroup_mode_acceptance.py`。不需重复运行全部旧阶段的独立脚本。

完整 P2 验收可执行 `python tests/p2_acceptance.py`，它会构建并串联已有业务、实际菜单和原生控制台检查。普通修改仍按需要运行相关测试，不必每次执行全阶段验收。

## P4 Release 构建与部署（2026-10-12 已验收）

保留 local-debug 和 gui-debug，新增 gui-release。运行 `cmake --preset gui-release` 配置，再 `cmake --build --preset gui-release` 生成两个 Release 入口；该预设关闭测试并启用便携数据库路径，两个入口均由 Qt 配套 MinGW 13.1 构建。它不会改变原终端 Ctrl+Shift+B 或 F5 配置。

完整部署执行 `.\scripts\package_p4.ps1`，会构建、安装到 dist/JLU_InstantMessaging，并调用 windeployqt 复制运行库、平台插件及说明。开发版默认使用项目库；发布版使用 exe 旁 data/jlu_im.db。具体运行、数据迁移和复现命令见 [P4 使用与代码说明](P4使用与代码说明.md#6-发布版与重新打包)。
