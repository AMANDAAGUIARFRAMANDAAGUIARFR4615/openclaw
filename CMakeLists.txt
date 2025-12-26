cmake_minimum_required(VERSION 3.16)
project(RemotePro LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent Multimedia MultimediaWidgets WebSockets)

qt_standard_project_setup()

include_directories(include)

if(APPLE)
    set(PLATFORM_DIR "macOS") 
else()
    set(PLATFORM_DIR "mingw64")
endif()

include_directories(${PLATFORM_DIR}/libimobiledevice/include)
link_directories(${PLATFORM_DIR}/libimobiledevice/lib)

include_directories(${PLATFORM_DIR}/libplist/include)
link_directories(${PLATFORM_DIR}/libplist/lib)

include_directories(${PLATFORM_DIR}/libusbmuxd/include)
link_directories(${PLATFORM_DIR}/libusbmuxd/lib)

include_directories(${PLATFORM_DIR}/libimobiledevice-glue/include)
link_directories(${PLATFORM_DIR}/libimobiledevice-glue/lib)

qt_add_resources(RESOURCE_SOURCES icons.qrc)

if(WIN32)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/logo.rc" "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/icons/logo.ico\"")
    list(APPEND RESOURCE_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/logo.rc")
endif()

add_executable(${PROJECT_NAME}
    main.cpp SafeObject.h
    global.h global.cpp AesCrypto.h KeyMapping.h VideoVisibilityManager.h
    EmojiIconProvider.h Tools.h SettingsViewer.h JailbreakAssistantDialog.h
    Logger.h LogWindow.h EventHub.h
    BitMaskEditorDialog.h LiveStreamDevice.h
    LoginWidget.h
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

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent Qt6::Multimedia Qt6::MultimediaWidgets Qt6::WebSockets ${THIRD_PARTY_LIBS})

add_definitions(-DPROJECT_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")

file(GLOB_RECURSE DEBUG_SOURCES
    "${CMAKE_SOURCE_DIR}/../qtmultimedia/*.cpp"
    "${CMAKE_SOURCE_DIR}/../qtmultimedia/*.h"
)

add_custom_target(DebugSources SOURCES ${DEBUG_SOURCES})
