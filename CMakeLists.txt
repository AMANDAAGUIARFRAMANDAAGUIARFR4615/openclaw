cmake_minimum_required(VERSION 3.16)
project(RemotePro LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent Multimedia MultimediaWidgets)

qt_standard_project_setup()

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

add_executable(${PROJECT_NAME}
    main.cpp Tools.h
    Logger.h LogWindow.h
    EventHub.h EventHub.cpp
    LiveStreamDevice.h
    MainWindow.h MainWindow.cpp
    VideoFrameWidget.h
    DeviceInfo.h DeviceInfo.cpp
    UsbDeviceManager.h UsbDeviceManager.cpp
    DeviceConnection.h DeviceConnection.cpp
    DeviceView.h DeviceView.cpp
    DeviceWidget.h DeviceWidget.cpp
    DeviceWindow.h DeviceWindow.cpp
    TcpServer.h TcpServer.cpp
    UdpTransport.h UdpTransport.cpp
    ToastWidget.h
    NetworkUtils.h
    RemoteFileExplorer.h RemoteFileExplorer.cpp FileTransfer.h
    AppListWidget.h AppListWidget.cpp
    Recorder.h
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

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent Qt6::Multimedia Qt6::MultimediaWidgets ${THIRD_PARTY_LIBS})

add_definitions(-DPROJECT_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")
