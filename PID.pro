# Compatibility entry point for Qt Creator users. CMake is the primary build.
QT += core gui widgets
TARGET = PIDStudio
TEMPLATE = app
CONFIG += c++17
INCLUDEPATH += src
SOURCES += src/main.cpp src/mainwindow.cpp src/trendplot.cpp src/controlengine.cpp src/csv.cpp \
    src/experimentplan.cpp src/csvmonitor.cpp src/sessionlogger.cpp
HEADERS += src/mainwindow.h src/trendplot.h src/controlengine.h src/csv.h src/experimentplan.h \
    src/csvmonitor.h src/sessionlogger.h
RESOURCES += resources.qrc
msvc: QMAKE_CXXFLAGS += /utf-8
