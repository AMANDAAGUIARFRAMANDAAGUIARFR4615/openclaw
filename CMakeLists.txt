cmake_minimum_required(VERSION 3.16)
project(RemotePro LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent Multimedia)

qt_standard_project_setup()

set(LIBIMOBILEDEVICE_DIR "mingw64/libimobiledevice")
include_directories(${LIBIMOBILEDEVICE_DIR}/include)
link_directories(${LIBIMOBILEDEVICE_DIR}/lib)

set(LIBPLIST_DIR "mingw64/libplist")
include_directories(${LIBPLIST_DIR}/include)
link_directories(${LIBPLIST_DIR}/lib)

set(LIBIMOBILEDEVICE_GLUE_DIR "mingw64/libimobiledevice-glue")
# include_directories(${LIBIMOBILEDEVICE_GLUE_DIR}/include)
link_directories(${LIBIMOBILEDEVICE_GLUE_DIR}/lib)

set(LIBUSBMUXD_DIR "mingw64/libusbmuxd")
# include_directories(${LIBUSBMUXD_DIR}/include)
link_directories(${LIBUSBMUXD_DIR}/lib)

qt_add_resources(RESOURCE_SOURCES icons.qrc)

add_executable(${PROJECT_NAME}
    main.cpp Tools.h
    Logger.h LogWindow.h
    EventHub.h EventHub.cpp
    MainWindow.h MainWindow.cpp
    VideoFrameWidget.h
    DeviceInfo.h
    DeviceConnection.h DeviceConnection.cpp
    DeviceView.h DeviceView.cpp
    DeviceWidget.h DeviceWidget.cpp
    DeviceWindow.h DeviceWindow.cpp
    TcpServer.h TcpServer.cpp
    # TcpClient.h TcpClient.cpp
    UdpTransport.h UdpTransport.cpp
    ToastWidget.h
    NetworkUtils.h
    RemoteFileExplorer.h RemoteFileExplorer.cpp FileTransfer.h
    ${RESOURCE_SOURCES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE

    # MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.qt6.remotepro"
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent Qt6::Multimedia libimobiledevice-1.0 libplist-2.0)

add_definitions(-DPROJECT_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")
