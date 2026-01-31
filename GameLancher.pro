QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Disable reorder warnings for cleaner build output
QMAKE_CXXFLAGS += -Wno-reorder

# Windows specific libraries for icon extraction
win32: LIBS += -lgdi32 -lshell32 -luser32

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    appinfo.cpp \
    appmanager.cpp \
    applauncher.cpp \
    iconextractor.cpp \
    addappdialog.cpp \
    categorymanager.cpp \
    appdiscovery.cpp \
    appdiscoverydialog.cpp \
    applistmodel.cpp \
    appicondelegate.cpp

HEADERS += \
    mainwindow.h \
    appinfo.h \
    appmanager.h \
    applauncher.h \
    iconextractor.h \
    addappdialog.h \
    categorymanager.h \
    appdiscovery.h \
    appdiscoverydialog.h \
    applistmodel.h \
    appicondelegate.h

FORMS += \
    mainwindow.ui \
    addappdialog.ui \
    appdiscoverydialog.ui

TRANSLATIONS += \
    GameLancher_ja_JP.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
