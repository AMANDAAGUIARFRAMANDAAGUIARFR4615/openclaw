cmake_minimum_required(VERSION 3.16)
project(RemotePro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS CorePrivate Widgets Network Concurrent Multimedia MultimediaWidgets WebSockets)

qt_standard_project_setup()

# =========================================================
#  将 __FILE__ 映射为相对路径
# =========================================================
if(MSVC)
    # Visual Studio 2019 16.10+ 支持
    add_compile_options(/d1trimfile:${CMAKE_SOURCE_DIR})
else()
    # GCC / Clang / MinGW 支持
    add_compile_options("-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=")
endif()

# =========================================================
# 1. 全局配置 (必须放在 add_executable 之前)
# =========================================================
if(CMAKE_BUILD_TYPE STREQUAL "Release")
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
endif()

add_subdirectory(nodeeditor)

# =========================================================
# 2. 头文件与库路径配置
# =========================================================

include_directories(include)
include_directories(visualscripting)

if(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(PLATFORM_DIR "macOS")
elseif(WIN32)
    set(PLATFORM_DIR "mingw64")
endif()

if(DEFINED PLATFORM_DIR)
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
endif()

qt_add_resources(RESOURCE_SOURCES icons.qrc)

if(WIN32)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/logo.rc" "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/icons/logo.ico\"")
    list(APPEND RESOURCE_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/logo.rc")
elseif(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # 告诉 CMake 把这个图标拷贝到 .app 包内的 Contents/Resources 目录下
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/icons/logo.icns" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    list(APPEND RESOURCE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/icons/logo.icns")
endif()

# =========================================================
# 3. 定义可执行文件
# =========================================================
qt_add_executable(${PROJECT_NAME}
    main.cpp Safe.h SafeObject.h SwitchButton.h ExplicitSelectionListWidget.h NaturalSortListWidgetItem.h
    global.h AesCrypto.h KeyMapping.h VideoVisibilityManager.h
    EmojiIconProvider.h Tools.h SettingsViewer.h JailbreakAssistantDialog.h AccountListDialog.h SwapExpirationDialog.h
    Logger.h LogTextBrowser.h EventHub.h
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
    ToastWidget.h ZipUtils.h
    NetworkUtils.h
    AppSettingsDialog.h
    RemoteFileExplorer.h RemoteFileExplorer.cpp FileTransfer.h
    AppListWidget.h FileViewer.h Recorder.h VersionManagerDialog.h
    qrcodegen.hpp qrcodegen.cpp

    visualscripting/Definitions.hpp
    visualscripting/PureDataModels.hpp
    visualscripting/FlowModels.hpp
    visualscripting/FlowEditorDialog.h

    ${RESOURCE_SOURCES}
)

# =========================================================
# 4. 目标特定编译优化 (必须放在 add_executable 之后)
# =========================================================

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG QT_NO_DEBUG_OUTPUT QT_NO_WARNING_OUTPUT)

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
                -Wl,--strip-all # 彻底移除
            )
        endif()
    endif()

    # --- MSVC ---
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE
            /O2 /Ob2            # 最大优化，激进内联
            /Oi /Ot             # 内部函数优化，代码速度优先
            /GR-                # 禁用 RTTI (对应 gcc 的 -fno-rtti)
            /Gy                 # 启用函数级链接 (允许链接器移除未使用的函数)
            /GS                 # 缓冲区安全检查 (增加逆向时的栈分析干扰)
        )
        target_link_options(${PROJECT_NAME} PRIVATE
            /OPT:REF            # 移除未引用的函数
            /OPT:ICF            # 相同的函数合并 (COMDAT Folding)，让逆向者困惑
            /LTCG               # 链接时代码生成 (配合 CMAKE_INTERPROCEDURAL_OPTIMIZATION)
        )
        # 禁用生成 PDB 文件 (这是调试符号文件)
        set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_PDB_OUTPUT_DIRECTORY "")
        set_target_properties(${PROJECT_NAME} PROPERTIES PDB_OUTPUT_DIRECTORY "")
    endif()
endif()

# =========================================================
# 5. 属性与其他链接配置
# =========================================================

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE

    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.qtproject.remotepro"

    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
)

if(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(THIRD_PARTY_LIBS
        libimobiledevice-1.0.dylib
        libplist-2.0.dylib
    )
elseif(WIN32)
    set(THIRD_PARTY_LIBS
        libimobiledevice-1.0
        libplist-2.0
    )
endif()

# 链接 Qt 和第三方库
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt6::Core Qt6::CorePrivate Qt6::Gui Qt6::Widgets Qt6::Network 
    Qt6::Concurrent Qt6::Multimedia Qt6::MultimediaWidgets Qt6::WebSockets
    QtNodes
    ${THIRD_PARTY_LIBS}
)

if(IOS)
    # 自动配置 RPATH，这样无需在 Github Actions 中手动执行 install_name_tool
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "@executable_path/Frameworks"
        INSTALL_RPATH "@executable_path/Frameworks"
    )

    set(ALL_XCFRAMEWORKS "")

    get_filename_component(QT_LIB_DIR "${Qt6_DIR}/../.." ABSOLUTE)
    file(GLOB FFMPEG_XCFRAMEWORKS "${QT_LIB_DIR}/ffmpeg/*.xcframework")
    file(GLOB EXTERNAL_FRAMEWORKS "${CMAKE_CURRENT_SOURCE_DIR}/external/*.framework")
    list(APPEND ALL_XCFRAMEWORKS ${FFMPEG_XCFRAMEWORKS} ${EXTERNAL_FRAMEWORKS})

    # 针对本地 Xcode IDE (XCODE 生成器) 的配置
    # 必须加上 KNOWN_LOCATION 才能让 XCODE_EMBED_FRAMEWORKS 接受绝对路径
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_LINK_BUILD_PHASE_MODE "KNOWN_LOCATION"
        XCODE_EMBED_FRAMEWORKS "${ALL_XCFRAMEWORKS}"
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
        XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY ON
    )

    # 链接 XCFramework
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${ALL_XCFRAMEWORKS}
        bz2 z iconv
        "-framework VideoToolbox"
        "-framework AudioToolbox"
        "-framework CoreMedia"
        "-framework AVFoundation"
        "-framework CoreVideo"
        "-framework CoreAudio"
    )

    # 针对 GitHub Actions (Ninja 等非 Xcode 生成器) 的兜底自动化脚本
    # Ninja 不支持自动 Embed，所以在这里自动帮它把 framework 复制到 App 目录内
    if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Frameworks"
            COMMENT "Creating Frameworks directory for Ninja generator..."
        )
        
        # FRAMEWORK_PATH 可能是 xcframework 也可能是 framework
        foreach(FRAMEWORK_PATH IN LISTS ALL_XCFRAMEWORKS)
            get_filename_component(FRAMEWORK_NAME "${FRAMEWORK_PATH}" NAME)
            
            if(FRAMEWORK_PATH MATCHES "\\.xcframework$")
                # 使用正则安全地将结尾的 .xcframework 替换为 .framework
                string(REGEX REPLACE "\\.xcframework$" ".framework" TARGET_FRAMEWORK_NAME "${FRAMEWORK_NAME}")
                
                # 复制 iOS 真机版 (.framework) 到 app bundle 的 Frameworks 文件夹中
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${FRAMEWORK_PATH}/ios-arm64/${TARGET_FRAMEWORK_NAME}"
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Frameworks/${TARGET_FRAMEWORK_NAME}"
                    COMMENT "Copying ${TARGET_FRAMEWORK_NAME} into app bundle..."
                )
            else()
                # 直接复制普通的 .framework 到 app bundle 的 Frameworks 文件夹中
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${FRAMEWORK_PATH}"
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Frameworks/${FRAMEWORK_NAME}"
                    COMMENT "Copying ${FRAMEWORK_NAME} into app bundle..."
                )
            endif()
        endforeach()
    endif()
endif()

# Debug 辅助目标
file(GLOB_RECURSE DEBUG_SOURCES
    "${CMAKE_SOURCE_DIR}/../qtmultimedia/*.cpp"
    "${CMAKE_SOURCE_DIR}/../qtmultimedia/*.h"
)
add_custom_target(DebugSources SOURCES ${DEBUG_SOURCES})
