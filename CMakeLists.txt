cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/appversion.h" TINYSERIAL_VERSION_LINE
     REGEX "^#define APP_VERSION \"[0-9]+\\.[0-9]+\\.[0-9]+\"$")
string(REGEX REPLACE "^#define APP_VERSION \"([^\"]+)\"$" "\\1"
       TINYSERIAL_VERSION "${TINYSERIAL_VERSION_LINE}")
if(NOT TINYSERIAL_VERSION)
    message(FATAL_ERROR "Could not read APP_VERSION from appversion.h")
endif()

project(TinySerial VERSION ${TINYSERIAL_VERSION} LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets SerialPort)

set(TINYSERIAL_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    aboutdialog.cpp
    aboutdialog.h
    displaybuffer.cpp
    displaybuffer.h
    serialcodec.cpp
    serialcodec.h
    serialdisplayformatter.cpp
    serialdisplayformatter.h
    serialportselection.cpp
    serialportselection.h
    terminalhighlighter.cpp
    terminalhighlighter.h
    textcodec.cpp
    textcodec.h
    xmodemtransfer.cpp
    xmodemtransfer.h
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

include(GNUInstallDirs)
install(TARGETS tinyserial RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES tinyserial-deb-prj/usr/share/applications/tinyserial.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES tinyserial-deb-prj/usr/share/pixmaps/tinyserial.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(tst_serialcodec tests/tst_serialcodec.cpp serialcodec.cpp serialcodec.h)
    target_include_directories(tst_serialcodec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_serialcodec PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME serialcodec COMMAND tst_serialcodec)

    add_executable(tst_serialdisplayformatter
        tests/tst_serialdisplayformatter.cpp
        serialdisplayformatter.cpp serialdisplayformatter.h
        serialcodec.cpp serialcodec.h
    )
    target_include_directories(tst_serialdisplayformatter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_serialdisplayformatter PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME serialdisplayformatter COMMAND tst_serialdisplayformatter)

    add_executable(tst_serialportselection
        tests/tst_serialportselection.cpp
        serialportselection.cpp serialportselection.h
    )
    target_include_directories(tst_serialportselection PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_serialportselection PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME serialportselection COMMAND tst_serialportselection)

    add_executable(tst_textcodec tests/tst_textcodec.cpp textcodec.cpp textcodec.h)
    target_include_directories(tst_textcodec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_textcodec PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME textcodec COMMAND tst_textcodec)

    add_executable(tst_xmodemtransfer tests/tst_xmodemtransfer.cpp xmodemtransfer.cpp xmodemtransfer.h)
    target_include_directories(tst_xmodemtransfer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_xmodemtransfer PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME xmodemtransfer COMMAND tst_xmodemtransfer)

    add_executable(tst_uilanguage tests/tst_uilanguage.cpp uilanguage.cpp uilanguage.h)
    target_include_directories(tst_uilanguage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_uilanguage PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME uilanguage COMMAND tst_uilanguage)

    add_executable(tst_timeline tests/tst_timeline.cpp timeline.cpp timeline.h)
    target_include_directories(tst_timeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_timeline PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME timeline COMMAND tst_timeline)
    # The model uses QFont, so the test needs a GUI platform; CI has no display.
    set_tests_properties(timeline PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(tst_displaybuffer tests/tst_displaybuffer.cpp displaybuffer.cpp displaybuffer.h)
    target_include_directories(tst_displaybuffer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_displaybuffer PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME displaybuffer COMMAND tst_displaybuffer)

    add_executable(tst_mainwindow_layout
        tests/tst_mainwindow_layout.cpp
        mainwindow.cpp mainwindow.h
        aboutdialog.cpp aboutdialog.h
        displaybuffer.cpp displaybuffer.h
        serialcodec.cpp serialcodec.h
        serialdisplayformatter.cpp serialdisplayformatter.h
        serialportselection.cpp serialportselection.h
        terminalhighlighter.cpp terminalhighlighter.h
        textcodec.cpp textcodec.h
        xmodemtransfer.cpp xmodemtransfer.h
        timeline.cpp timeline.h
        uilanguage.cpp uilanguage.h
        serialport.ui aboutdialog.ui resource/resource.qrc
    )
    target_include_directories(tst_mainwindow_layout PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_mainwindow_layout PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::SerialPort
        Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME mainwindow_layout COMMAND tst_mainwindow_layout)
    set_tests_properties(mainwindow_layout PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.5;LANG=zh_CN.UTF-8"
    )
endif()
