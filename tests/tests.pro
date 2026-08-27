QT += core gui qml quick quickcontrols2 quickdialogs2 svg dbus testlib

CONFIG += c++17 console testcase link_pkgconfig
PKGCONFIG += poppler-qt6

TARGET = tst_omasign
TEMPLATE = app

INCLUDEPATH += $$PWD/../src

HEADERS += \
    ../src/appearance.h \
    ../src/appearanceitem.h \
    ../src/backend.h \
    ../src/identity.h \
    ../src/ink.h \
    ../src/pageimageprovider.h \
    ../src/signaturepad.h \
    ../src/signer.h \
    ../src/systemtheme.h

SOURCES += \
    ../src/appearance.cpp \
    ../src/appearanceitem.cpp \
    ../src/backend.cpp \
    ../src/identity.cpp \
    ../src/ink.cpp \
    ../src/pageimageprovider.cpp \
    ../src/signaturepad.cpp \
    ../src/signer.cpp \
    ../src/systemtheme.cpp \
    tst_omasign.cpp

RESOURCES += ../src/resources.qrc
