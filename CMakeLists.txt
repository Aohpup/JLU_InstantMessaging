cmake_minimum_required(VERSION 3.21)

project(JLU_InstantMessaging VERSION 0.1.0 LANGUAGES C CXX)
if(NOT WIN32)
    message(FATAL_ERROR "Console input currently requires Windows.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# SQLite 官方 C 源码独立编译，不使用项目自写代码的告警选项。
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite")
set_target_properties(sqlite3 PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)

# 当新项目增加时，需要手动添加新的源文件。
# 业务代码编译成静态库，演示程序和测试程序共同使用，避免维护两份实现。
add_library(jlu_core STATIC
    src/services/ChatIdentityLJY.cpp
    src/storage/SqliteStorageLJY.cpp
    src/storage/ChatHistoryLJY.cpp
    src/domain/FriendRequestLJY.cpp
    src/domain/GroupMemberLJY.cpp
    src/domain/GroupRequestLJY.cpp
    src/domain/GroupLJY.cpp
    src/services/GroupPolicyLJY.cpp
    src/services/QQGroupPolicyLJY.cpp
    src/services/WeChatGroupPolicyLJY.cpp
    src/services/GroupManagerLJY.cpp
    src/services/DemoDataLJY.cpp
    src/domain/FriendRelationLJY.cpp
    src/domain/FriendQueryResultLJY.cpp
    src/services/FriendManagerLJY.cpp
    src/domain/PersonLJY.cpp
    src/domain/ServiceAccountLJY.cpp
    src/domain/DateLJY.cpp
    src/domain/PasswordCredentialLJY.cpp
    src/services/SocialServiceLJY.cpp
    src/services/QQServiceLJY.cpp
    src/services/WeChatServiceLJY.cpp
    src/services/WeiboServiceLJY.cpp
    src/services/PlatformLJY.cpp
    src/services/AccountServiceLJY.cpp
    src/services/SessionManagerLJY.cpp
) # 在这里添加源文件
target_include_directories(jlu_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(jlu_core PUBLIC cxx_std_17)
target_link_libraries(jlu_core PUBLIC sqlite3)

# 建表脚本只有一份，配置时嵌入私有头文件，改动脚本会触发重新配置。
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/schema/create_tables.sql")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/schema/create_tables.sql" JLU_SCHEMA_SQL)
configure_file(cmake/SqliteSchemaLJY.h.in generated/SqliteSchemaLJY.h @ONLY)
target_include_directories(jlu_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")


# 菜单独立于业务库，便于注入输入输出流测试；密码演示不依赖额外系统库。
add_library(jlu_console STATIC src/ui/ConsoleAppLJY.cpp)
target_link_libraries(jlu_console PUBLIC jlu_core)

add_executable(jlu_im src/main.cpp)
target_link_libraries(jlu_im PRIVATE jlu_console)
# Release 可选便携库；默认开发配置仍使用项目库。
option(JLU_PORTABLE_DATA "Store database beside the executable" OFF)
if(JLU_PORTABLE_DATA)
    target_compile_definitions(jlu_im PRIVATE JLU_PORTABLE_DATA)
endif()
install(TARGETS jlu_im RUNTIME DESTINATION .)
# 默认库位置在配置时固定，切换工作目录不会生成另一份库。
target_compile_definitions(jlu_im PRIVATE JLU_DEFAULT_DATABASE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/jlu_im.db")
# Windows 宽字符入口支持 --database 后的中文文件名。
if(MINGW)
    target_link_options(jlu_im PRIVATE -municode)
endif()
# Socket 模块已接入 GUI；独立演示程序仍可用于网络验证。
find_package(Threads REQUIRED)
add_library(jlu_network STATIC
    src/domain/ChatMessageLJY.cpp
    src/network/ChatProtocolLJY.cpp
    src/network/TcpPeerLJY.cpp
    src/network/TcpPeerChatLJY.cpp)
target_include_directories(jlu_network PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(jlu_network PUBLIC ws2_32 Threads::Threads)
set(JLU_TARGETS jlu_core jlu_console jlu_im jlu_network)

include(CTest)
if(BUILD_TESTING)
    add_executable(p5_history_tests tests/p5_history_tests.cpp)
    target_link_libraries(p5_history_tests PRIVATE jlu_core)
    add_test(NAME p5_history COMMAND p5_history_tests "${CMAKE_BINARY_DIR}/p5_history_test.db")
    list(APPEND JLU_TARGETS p5_history_tests)
    add_executable(p5_chat_demo tests/p5_chat_demo.cpp)
    target_link_libraries(p5_chat_demo PRIVATE jlu_network jlu_core)
    add_executable(p5_protocol_tests tests/p5_protocol_tests.cpp)
    target_link_libraries(p5_protocol_tests PRIVATE jlu_network jlu_core)
    add_test(NAME p5_protocol COMMAND p5_protocol_tests)
    set_tests_properties(p5_protocol PROPERTIES TIMEOUT 15)
    list(APPEND JLU_TARGETS p5_chat_demo p5_protocol_tests)
    add_executable(p5_socket_demo tests/p5_socket_demo.cpp)
    target_link_libraries(p5_socket_demo PRIVATE jlu_network)
    if(MINGW)
        target_link_options(p5_socket_demo PRIVATE -municode)
    endif()
    list(APPEND JLU_TARGETS p5_socket_demo)
    # Python 只用于双进程自动验收，缺少它仍能编译并手动运行演示。
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME p5_chat COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/p5_chat_acceptance.py"
            --exe "$<TARGET_FILE:p5_chat_demo>"
            --output "${CMAKE_BINARY_DIR}/p5_batch2/chat_results.json")
        set_tests_properties(p5_chat PROPERTIES TIMEOUT 100)
        add_test(NAME p5_socket COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/p5_socket_acceptance.py"
            --exe "$<TARGET_FILE:p5_socket_demo>"
            --output "${CMAKE_BINARY_DIR}/p5_batch1/socket_results.json")
        set_tests_properties(p5_socket PROPERTIES TIMEOUT 45)
    endif()
    add_executable(p3_console_tests tests/p3_console_tests.cpp)
    target_link_libraries(p3_console_tests PRIVATE jlu_console)
    add_test(NAME p3_console COMMAND p3_console_tests)
    list(APPEND JLU_TARGETS p3_console_tests)
    add_executable(p3_storage_load_tests tests/p3_storage_load_tests.cpp)
    target_link_libraries(p3_storage_load_tests PRIVATE jlu_core)
    target_compile_definitions(p3_storage_load_tests PRIVATE
        JLU_STORAGE_TEST_DIR="${CMAKE_BINARY_DIR}/p3_batch3")
    add_test(NAME p3_storage_load COMMAND p3_storage_load_tests)
    list(APPEND JLU_TARGETS p3_storage_load_tests)
    add_executable(p3_storage_save_tests tests/p3_storage_save_tests.cpp)
    target_link_libraries(p3_storage_save_tests PRIVATE jlu_core)
    target_compile_definitions(p3_storage_save_tests PRIVATE
        JLU_STORAGE_TEST_DIR="${CMAKE_BINARY_DIR}/p3_batch2")
    add_test(NAME p3_storage_save COMMAND p3_storage_save_tests)
    list(APPEND JLU_TARGETS p3_storage_save_tests)
    # 独立验证库始终在 build 下，不接触主程序正式数据库。
    add_executable(p3_sqlite_smoke tests/p3_sqlite_smoke.cpp)
    target_link_libraries(p3_sqlite_smoke PRIVATE sqlite3)
    target_compile_definitions(p3_sqlite_smoke PRIVATE
        JLU_SCHEMA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/schema/create_tables.sql"
        JLU_SQLITE_SMOKE_DB="${CMAKE_BINARY_DIR}/p3_batch1/smoke.db")
    add_test(NAME p3_sqlite_smoke COMMAND p3_sqlite_smoke)
    list(APPEND JLU_TARGETS p3_sqlite_smoke)
    add_executable(p1_tests tests/p1_steps34_tests.cpp)
    target_link_libraries(p1_tests PRIVATE jlu_core)
    add_test(NAME p1_steps34 COMMAND p1_tests)
    add_executable(p1_session_tests tests/p1_steps56_tests.cpp)
    target_link_libraries(p1_session_tests PRIVATE jlu_console)
    add_test(NAME p1_steps56 COMMAND p1_session_tests)
    add_executable(p1_binding_tests tests/p1_binding_history_tests.cpp)
    target_link_libraries(p1_binding_tests PRIVATE jlu_core)
    add_test(NAME p1_binding_history COMMAND p1_binding_tests)
    add_executable(p2_friend_tests tests/p2_friend_tests.cpp)
    target_link_libraries(p2_friend_tests PRIVATE jlu_console)
    add_test(NAME p2_friends COMMAND p2_friend_tests)
    add_executable(p2_recommendation_tests tests/p2_recommendation_tests.cpp)
    target_link_libraries(p2_recommendation_tests PRIVATE jlu_console)
    add_test(NAME p2_recommendations COMMAND p2_recommendation_tests)
    add_executable(p2_group_tests tests/p2_group_tests.cpp)
    target_link_libraries(p2_group_tests PRIVATE jlu_console)
    add_test(NAME p2_groups COMMAND p2_group_tests)
    add_executable(p2_group_operations_tests tests/p2_group_operations_tests.cpp)
    target_link_libraries(p2_group_operations_tests PRIVATE jlu_console)
    add_test(NAME p2_group_operations COMMAND p2_group_operations_tests)
    add_executable(p2_interaction_tests tests/p2_interaction_tests.cpp)
    target_link_libraries(p2_interaction_tests PRIVATE jlu_console)
    add_test(NAME p2_interaction COMMAND p2_interaction_tests)
    add_executable(p2_subgroup_mode_tests tests/p2_subgroup_mode_tests.cpp)
    target_link_libraries(p2_subgroup_mode_tests PRIVATE jlu_console)
    add_test(NAME p2_subgroup_mode COMMAND p2_subgroup_mode_tests)
    list(APPEND JLU_TARGETS p2_subgroup_mode_tests p2_interaction_tests p2_group_operations_tests p2_group_tests p1_tests p1_session_tests p1_binding_tests p2_friend_tests p2_recommendation_tests)
endif()

foreach(target IN LISTS JLU_TARGETS)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

# 图形入口独立可选；未启用时不查找或依赖 Qt。
option(JLU_BUILD_GUI "Build the Qt desktop application" OFF)
if(JLU_BUILD_GUI)
    add_subdirectory(src/gui)
endif()
