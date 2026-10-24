#-------------------------------------------------
#
# Project created by QtCreator 2017-09-16T14:17:53
#
#-------------------------------------------------

QT += core gui serialport
CONFIG += c++17

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = tinyserial
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += main.cpp\
    aboutdialog.cpp \
    displaybuffer.cpp \
    mainwindow.cpp \
    serialcodec.cpp \
    serialdisplayformatter.cpp \
    serialportselection.cpp \
    terminalhighlighter.cpp \
    textcodec.cpp \
    xmodemtransfer.cpp \
    timeline.cpp \
    modemlines.cpp \
    uilanguage.cpp

HEADERS  += \
    global.h \
    aboutdialog.h \
    displaybuffer.h \
    mainwindow.h \
    serialcodec.h \
    serialdisplayformatter.h \
    serialportselection.h \
    terminalhighlighter.h \
    textcodec.h \
    xmodemtransfer.h \
    timeline.h \
    modemlines.h \
    uilanguage.h

FORMS    += serialport.ui \
    aboutdialog.ui

RESOURCES += \
    resource/resource.qrc

unix:!macx {
    isEmpty(PREFIX): PREFIX = /usr/local
    target.path = $$PREFIX/bin
    desktop.files = $$PWD/tinyserial-deb-prj/usr/share/applications/tinyserial.desktop
    desktop.path = $$PREFIX/share/applications
    icon.files = $$PWD/tinyserial-deb-prj/usr/share/pixmaps/tinyserial.png
    icon.path = $$PREFIX/share/pixmaps
    hicolor.files = $$PWD/tinyserial-deb-prj/usr/share/pixmaps/tinyserial.png
    hicolor.path = $$PREFIX/share/icons/hicolor/72x72/apps
    INSTALLS += target desktop icon hicolor
}
