TARGET = sync-ui

CONFIG += sailfishapp

QT += dbus qml

SOURCES += \
    sync-ui.cpp

HEADERS += \
    sync-ui.h

LIBS += -L../src/dbus/qt/.libs -lsyncevolution-qt-dbus
INCLUDEPATH += ../src/dbus/qt
