cmake_minimum_required(VERSION 3.16)

project(TinySerial VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets SerialPort Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets SerialPort Test)

set(TINYSERIAL_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    aboutdialog.cpp
    aboutdialog.h
    serialcodec.cpp
    serialcodec.h
    timeline.cpp
    timeline.h
    uilanguage.cpp
    uilanguage.h
    serialport.ui
    aboutdialog.ui
    resource/resource.qrc
)

add_executable(tinyserial ${TINYSERIAL_SOURCES})
target_link_libraries(tinyserial PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
)
target_compile_definitions(tinyserial PRIVATE QT_DEPRECATED_WARNINGS)
target_compile_options(tinyserial PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
)

include(CTest)
if(BUILD_TESTING)
    add_executable(tst_serialcodec tests/tst_serialcodec.cpp serialcodec.cpp serialcodec.h)
    target_include_directories(tst_serialcodec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_serialcodec PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME serialcodec COMMAND tst_serialcodec)

    add_executable(tst_uilanguage tests/tst_uilanguage.cpp uilanguage.cpp uilanguage.h)
    target_include_directories(tst_uilanguage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_uilanguage PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME uilanguage COMMAND tst_uilanguage)

    add_executable(tst_timeline tests/tst_timeline.cpp timeline.cpp timeline.h)
    target_include_directories(tst_timeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_timeline PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME timeline COMMAND tst_timeline)
endif()
