find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)

# 页面仅链接业务核心，不依赖 ConsoleApp；测试复用同一份窗口代码。
add_library(jlu_gui_widgets STATIC
    ../network/GroupChatProtocolLJY.cpp
    ../network/GroupChatHostLJY.h ../network/GroupChatHostLJY.cpp
    ../network/GroupChatClientLJY.h ../network/GroupChatClientLJY.cpp
    ../storage/GroupChatHistoryLJY.cpp
    GroupMessagesPageLJY.h GroupMessagesPageLJY.cpp
    gui_assets.qrc
    UiFeedbackLJY.h CardSurfaceLJY.h AvatarLJY.h SidebarIconLJY.h InlineDialogLJY.h InlineDialogLJY.cpp
    LoginPageLJY.h LoginPageLJY.cpp
    AccountPageLJY.h AccountPageLJY.cpp
    MainWindowLJY.h MainWindowLJY.cpp
    MessagesPageLJY.h MessagesPageLJY.cpp ChatBubbleLJY.h
    FriendsPageLJY.h FriendsPageLJY.cpp
    GroupsPageLJY.h GroupsPageLJY.cpp
)
set_target_properties(jlu_gui_widgets PROPERTIES AUTOMOC ON AUTORCC ON)
target_link_libraries(jlu_gui_widgets PUBLIC jlu_core jlu_network Qt6::Widgets Qt6::Network)
if(MINGW)
    target_compile_options(jlu_gui_widgets PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(jlu_im_gui WIN32 main_gui.cpp app_icon.rc)
target_link_libraries(jlu_im_gui PRIVATE jlu_gui_widgets)
if(JLU_PORTABLE_DATA)
    target_compile_definitions(jlu_im_gui PRIVATE JLU_PORTABLE_DATA)
endif()
install(TARGETS jlu_im_gui RUNTIME DESTINATION .)
target_compile_definitions(jlu_im_gui PRIVATE JLU_DEFAULT_DATABASE_PATH="${PROJECT_SOURCE_DIR}/data/jlu_im.db")

# 开发运行所需的 DLL 和平台插件放入 GUI 构建目录，不污染终端目录。
get_target_property(JLU_QMAKE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(JLU_QT_BIN "${JLU_QMAKE}" DIRECTORY)
find_program(JLU_WINDEPLOYQT windeployqt HINTS "${JLU_QT_BIN}" REQUIRED NO_DEFAULT_PATH)
add_custom_command(TARGET jlu_im_gui POST_BUILD
    COMMAND "${JLU_WINDEPLOYQT}" --no-translations --no-opengl-sw "$<TARGET_FILE:jlu_im_gui>"
    VERBATIM
)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(p4_gui_tests "${PROJECT_SOURCE_DIR}/tests/p4_gui_tests.cpp")
    target_link_libraries(p4_gui_tests PRIVATE jlu_gui_widgets Qt6::Test)
    target_compile_definitions(p4_gui_tests PRIVATE JLU_GUI_TEST_DIR="${CMAKE_BINARY_DIR}/p4_batch1")
    add_test(NAME p4_gui COMMAND p4_gui_tests -platform offscreen)
    add_test(NAME p4_gui_reload COMMAND p4_gui_tests -platform offscreen --reload)
    set_tests_properties(p4_gui_reload PROPERTIES DEPENDS p4_gui)
    set_tests_properties(p4_gui PROPERTIES FIXTURES_SETUP p4_database)
    set_tests_properties(p4_gui_reload PROPERTIES FIXTURES_REQUIRED p4_database)
    # 无桌面测试需要 offscreen 插件；普通程序仍使用部署目录的 qwindows。
    set_tests_properties(p4_gui p4_gui_reload PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms"
        TIMEOUT 30)
endif()

if(BUILD_TESTING)
    add_executable(p4_friends_gui_tests "${PROJECT_SOURCE_DIR}/tests/p4_friends_gui_tests.cpp")
    target_link_libraries(p4_friends_gui_tests PRIVATE jlu_gui_widgets Qt6::Test)
    target_compile_definitions(p4_friends_gui_tests PRIVATE JLU_GUI_TEST_DIR="${CMAKE_BINARY_DIR}/p4_batch2")
    add_test(NAME p4_friends_gui COMMAND p4_friends_gui_tests -platform offscreen)
    add_test(NAME p4_friends_reload COMMAND p4_friends_gui_tests -platform offscreen --reload)
    set_tests_properties(p4_friends_gui PROPERTIES FIXTURES_SETUP p4_friends_database)
    set_tests_properties(p4_friends_reload PROPERTIES FIXTURES_REQUIRED p4_friends_database DEPENDS p4_friends_gui)
    set_tests_properties(p4_friends_gui p4_friends_reload PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms"
        TIMEOUT 30)
endif()

if(BUILD_TESTING)
    add_executable(p4_groups_gui_tests "${PROJECT_SOURCE_DIR}/tests/p4_groups_gui_tests.cpp")
    target_link_libraries(p4_groups_gui_tests PRIVATE jlu_gui_widgets Qt6::Test)
    target_compile_definitions(p4_groups_gui_tests PRIVATE JLU_GUI_TEST_DIR="${CMAKE_BINARY_DIR}/p4_batch4")
    add_test(NAME p4_groups_gui COMMAND p4_groups_gui_tests -platform offscreen)
    add_test(NAME p4_groups_reload COMMAND p4_groups_gui_tests -platform offscreen --reload)
    set_tests_properties(p4_groups_gui PROPERTIES FIXTURES_SETUP p4_groups_database)
    set_tests_properties(p4_groups_reload PROPERTIES FIXTURES_REQUIRED p4_groups_database DEPENDS p4_groups_gui)
    set_tests_properties(p4_groups_gui p4_groups_reload PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms"
        TIMEOUT 30)
endif()

if(BUILD_TESTING)
    add_executable(p4_subgroups_gui_tests "${PROJECT_SOURCE_DIR}/tests/p4_subgroups_gui_tests.cpp")
    target_link_libraries(p4_subgroups_gui_tests PRIVATE jlu_gui_widgets Qt6::Test)
    target_compile_definitions(p4_subgroups_gui_tests PRIVATE JLU_GUI_TEST_DIR="${CMAKE_BINARY_DIR}/p4_batch5")
    add_test(NAME p4_subgroups_gui COMMAND p4_subgroups_gui_tests -platform offscreen)
    add_test(NAME p4_subgroups_reload COMMAND p4_subgroups_gui_tests -platform offscreen --reload)
    set_tests_properties(p4_subgroups_gui PROPERTIES FIXTURES_SETUP p4_subgroups_database)
    set_tests_properties(p4_subgroups_reload PROPERTIES FIXTURES_REQUIRED p4_subgroups_database DEPENDS p4_subgroups_gui)
    set_tests_properties(p4_subgroups_gui p4_subgroups_reload PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms"
        TIMEOUT 30)
endif()

if(BUILD_TESTING)
    add_executable(p4_social_gui_tests "${PROJECT_SOURCE_DIR}/tests/p4_social_gui_tests.cpp")
    target_link_libraries(p4_social_gui_tests PRIVATE jlu_gui_widgets Qt6::Test)
    target_compile_definitions(p4_social_gui_tests PRIVATE JLU_GUI_TEST_DIR="${CMAKE_BINARY_DIR}/p4_social")
    add_test(NAME p4_social_gui COMMAND p4_social_gui_tests -platform offscreen)
    add_test(NAME p4_social_reload COMMAND p4_social_gui_tests -platform offscreen --reload)
    set_tests_properties(p4_social_gui PROPERTIES FIXTURES_SETUP p4_social_database)
    set_tests_properties(p4_social_reload PROPERTIES FIXTURES_REQUIRED p4_social_database DEPENDS p4_social_gui)
    set_tests_properties(p4_social_gui p4_social_reload PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms" TIMEOUT 30)
endif()

if(BUILD_TESTING)
    add_executable(p4_visual_revision_tests "${PROJECT_SOURCE_DIR}/tests/p4_visual_revision_tests.cpp")
    target_link_libraries(p4_visual_revision_tests PRIVATE jlu_gui_widgets Qt6::Test)
    target_compile_definitions(p4_visual_revision_tests PRIVATE JLU_GUI_TEST_DIR="${CMAKE_BINARY_DIR}/p4_visual_revision")
    add_test(NAME p4_visual_revision COMMAND p4_visual_revision_tests -platform offscreen)
    add_test(NAME p4_visual_reload COMMAND p4_visual_revision_tests -platform offscreen --reload)
    set_tests_properties(p4_visual_revision PROPERTIES FIXTURES_SETUP p4_visual_database)
    set_tests_properties(p4_visual_reload PROPERTIES FIXTURES_REQUIRED p4_visual_database DEPENDS p4_visual_revision)
    set_tests_properties(p4_visual_revision p4_visual_reload PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms" TIMEOUT 30)
endif()

# 第六批由 Python 编排真实终端与两次图形进程，避免把业务调用冒充双入口验收。
if(BUILD_TESTING)
    add_executable(p4_roundtrip_gui_tests "${PROJECT_SOURCE_DIR}/tests/p4_roundtrip_gui_tests.cpp")
    target_link_libraries(p4_roundtrip_gui_tests PRIVATE jlu_gui_widgets Qt6::Test)
endif()

# P5 第三批：两个独立 Qt 进程使用真实页面控件和 Socket，数据库各自隔离。
if(BUILD_TESTING)
    add_executable(p5_chat_gui_tests "${PROJECT_SOURCE_DIR}/tests/p5_chat_gui_tests.cpp")
    target_link_libraries(p5_chat_gui_tests PRIVATE jlu_gui_widgets Qt6::Test)
    if(Python3_Interpreter_FOUND)
        add_test(NAME p5_chat_gui COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/p5_chat_gui_acceptance.py"
            --exe "$<TARGET_FILE:p5_chat_gui_tests>"
            --output-dir "${CMAKE_BINARY_DIR}/p5_final/qq")
        set_tests_properties(p5_chat_gui PROPERTIES TIMEOUT 65
            ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms")
    endif()
endif()

# 第四批只补历史分页和异常重试的控件验收，双端收发继续复用 p5_chat_gui。
if(BUILD_TESTING)
    add_executable(p5_history_gui_tests "${PROJECT_SOURCE_DIR}/tests/p5_history_gui_tests.cpp")
    target_link_libraries(p5_history_gui_tests PRIVATE jlu_gui_widgets Qt6::Test)
    add_test(NAME p5_history_gui COMMAND p5_history_gui_tests -platform offscreen)
    set_tests_properties(p5_history_gui PROPERTIES TIMEOUT 40
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms")
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    foreach(service IN ITEMS 1 2)
        add_test(NAME p5_chat_gui_service_${service} COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/p5_chat_gui_acceptance.py"
            --exe "$<TARGET_FILE:p5_chat_gui_tests>" --service ${service}
            --output-dir "${CMAKE_BINARY_DIR}/p5_final/service_${service}")
        set_tests_properties(p5_chat_gui_service_${service} PROPERTIES TIMEOUT 65
            ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms")
    endforeach()
endif()

if(BUILD_TESTING)
    add_executable(p5_lifecycle_gui_tests "${PROJECT_SOURCE_DIR}/tests/p5_lifecycle_gui_tests.cpp")
    target_link_libraries(p5_lifecycle_gui_tests PRIVATE jlu_gui_widgets Qt6::Test)
    add_test(NAME p5_lifecycle_gui COMMAND p5_lifecycle_gui_tests -platform offscreen)
    set_tests_properties(p5_lifecycle_gui PROPERTIES TIMEOUT 40
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms")
endif()

# 长消息折叠和好友资料入口的交互验收。
if(BUILD_TESTING)
    add_executable(p5_ui_refinement_tests "${PROJECT_SOURCE_DIR}/tests/p5_ui_refinement_tests.cpp")
    target_link_libraries(p5_ui_refinement_tests PRIVATE jlu_gui_widgets Qt6::Test)
    add_test(NAME p5_ui_refinement COMMAND p5_ui_refinement_tests -platform offscreen)
    set_tests_properties(p5_ui_refinement PROPERTIES TIMEOUT 35
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms")
endif()

if(BUILD_TESTING)
    add_executable(p5_connection_ui_tests "${PROJECT_SOURCE_DIR}/tests/p5_connection_ui_tests.cpp")
    target_link_libraries(p5_connection_ui_tests PRIVATE jlu_gui_widgets Qt6::Test)
    add_test(NAME p5_connection_ui COMMAND p5_connection_ui_tests -platform offscreen)
    set_tests_properties(p5_connection_ui PROPERTIES TIMEOUT 45
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms")
endif()

if(BUILD_TESTING)
    add_executable(p5_group_network_tests "${PROJECT_SOURCE_DIR}/tests/p5_group_network_tests.cpp")
    target_link_libraries(p5_group_network_tests PRIVATE jlu_gui_widgets Qt6::Test)
    add_test(NAME p5_group_network COMMAND p5_group_network_tests)
    set_tests_properties(p5_group_network PROPERTIES TIMEOUT 45)
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_executable(p5_group_gui_tests "${PROJECT_SOURCE_DIR}/tests/p5_group_gui_tests.cpp")
    target_link_libraries(p5_group_gui_tests PRIVATE jlu_gui_widgets Qt6::Test)
    foreach(service IN ITEMS 0 1 2)
        add_test(NAME p5_group_gui_${service} COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/p5_group_gui_acceptance.py" --exe "$<TARGET_FILE:p5_group_gui_tests>"
            --service ${service} --output-dir "${CMAKE_BINARY_DIR}/p5_group/service_${service}")
        set_tests_properties(p5_group_gui_${service} PROPERTIES TIMEOUT 70
            ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms")
    endforeach()
    add_test(NAME p5_group_gui_subgroup COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/p5_group_gui_acceptance.py" --exe "$<TARGET_FILE:p5_group_gui_tests>"
        --group 2001 --output-dir "${CMAKE_BINARY_DIR}/p5_group/subgroup")
    set_tests_properties(p5_group_gui_subgroup PROPERTIES TIMEOUT 70
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms")
endif()

if(BUILD_TESTING)
    add_executable(p5_group_state_tests "${PROJECT_SOURCE_DIR}/tests/p5_group_state_tests.cpp")
    target_link_libraries(p5_group_state_tests PRIVATE jlu_gui_widgets Qt6::Test)
    add_test(NAME p5_group_state COMMAND p5_group_state_tests -platform offscreen)
    set_tests_properties(p5_group_state PROPERTIES TIMEOUT 40
        ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${JLU_QT_BIN}/../plugins/platforms")
endif()
