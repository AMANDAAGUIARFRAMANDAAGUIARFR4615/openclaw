cmake_minimum_required(VERSION 3.16)
project(RemotePro LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent Multimedia MultimediaWidgets WebSockets)

qt_standard_project_setup()

# =========================================================
# 1. 全局配置 (必须放在 add_executable 之前)
# =========================================================

# 符号隐藏：默认隐藏所有符号，极大增加逆向分析难度
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# LTO/IPO 优化：开启跨模块优化，打乱函数布局
include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT output)
if(result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(WARNING "IPO is not supported: ${output}")
endif()

# =========================================================
# 2. 头文件与库路径配置
# =========================================================

include_directories(include)

if(APPLE)
    set(PLATFORM_DIR "macOS") 
else()
    set(PLATFORM_DIR "mingw64")
endif()

# 包含路径
include_directories(${PLATFORM_DIR}/libimobiledevice/include)
include_directories(${PLATFORM_DIR}/libplist/include)
include_directories(${PLATFORM_DIR}/libusbmuxd/include)
include_directories(${PLATFORM_DIR}/libimobiledevice-glue/include)

# 库路径
link_directories(${PLATFORM_DIR}/libimobiledevice/lib)
link_directories(${PLATFORM_DIR}/libplist/lib)
link_directories(${PLATFORM_DIR}/libusbmuxd/lib)
link_directories(${PLATFORM_DIR}/libimobiledevice-glue/lib)

qt_add_resources(RESOURCE_SOURCES icons.qrc)

if(WIN32)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/logo.rc" "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/icons/logo.ico\"")
    list(APPEND RESOURCE_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/logo.rc")
endif()

# =========================================================
# 3. 定义可执行文件
# =========================================================
add_executable(${PROJECT_NAME}
    main.cpp SafeObject.h
    global.h global.cpp AesCrypto.h KeyMapping.h VideoVisibilityManager.h
    EmojiIconProvider.h Tools.h SettingsViewer.h JailbreakAssistantDialog.h
    Logger.h LogWindow.h EventHub.h
    BitMaskEditorDialog.h LiveStreamDevice.h
    LoginWidget.h Account.h
    MainWindow.h MainWindow.cpp
    VideoFrameWidget.h
    DeviceInfo.h RenewalDialog.h
    UsbDeviceManager.h UsbDeviceManager.cpp
    DeviceConnection.h
    DeviceView.h DeviceView.cpp
    DeviceWidget.h DeviceWidget.cpp
    DeviceWindow.h DeviceWindow.cpp
    TcpServer.h UdpTransport.h WebSocketClient.h
    ToastWidget.h
    NetworkUtils.h
    AppSettingsDialog.h
    RemoteFileExplorer.h RemoteFileExplorer.cpp FileTransfer.h
    AppListWidget.h FileViewer.h Recorder.h
    qrcodegen.hpp qrcodegen.cpp
    ${RESOURCE_SOURCES}
)

# =========================================================
# 4. 目标特定编译优化 (必须放在 add_executable 之后)
# =========================================================

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # --- MinGW / GCC / Clang ---
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE
            -O3                 # 最高级优化
            -g0                 # 关闭调试信息
            # -fno-rtti         # 禁用 RTTI (如果代码没有用 dynamic_cast 可开启)
            -fstack-protector-strong 
        )

        if(APPLE)
            target_link_options(${PROJECT_NAME} PRIVATE -Wl,-x -Wl,-S)
        else()
            target_link_options(${PROJECT_NAME} PRIVATE 
                -s              # 移除符号表
                -Wl,--strip-all 
            )
        endif()
    endif()

    # --- MSVC ---
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /O2 /Ob2 /Oi /Ot /GR- /Gy /GS)
        target_link_options(${PROJECT_NAME} PRIVATE /OPT:REF /OPT:ICF /LTCG)
        set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_PDB_OUTPUT_DIRECTORY "" PDB_OUTPUT_DIRECTORY "")
    endif()
endif()

# =========================================================
# 5. 属性与其他链接配置
# =========================================================

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE

    # MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.qt6.remotepro"
)

if(APPLE)
    set(THIRD_PARTY_LIBS
        libimobiledevice-1.0.dylib
        libplist-2.0.dylib
    )
else()
    set(THIRD_PARTY_LIBS
        libimobiledevice-1.0
        libplist-2.0
    )
endif()

# 链接 Qt 和第三方库
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network 
    Qt6::Concurrent Qt6::Multimedia Qt6::MultimediaWidgets Qt6::WebSockets 
    ${THIRD_PARTY_LIBS}
)

add_definitions(-DPROJECT_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")

# Debug 辅助目标
file(GLOB_RECURSE DEBUG_SOURCES
    "${CMAKE_SOURCE_DIR}/../qtmultimedia/*.cpp"
    "${CMAKE_SOURCE_DIR}/../qtmultimedia/*.h"
)
add_custom_target(DebugSources SOURCES ${DEBUG_SOURCES})