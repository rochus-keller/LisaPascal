QT       += core
QT       -= gui

TARGET = LisaPascal
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

INCLUDEPATH += ..
DEFINES += _DEBUG

SOURCES += main.cpp \
    LisaLexer.cpp \
    LisaParser.cpp \
    LisaSynTree.cpp \
    LisaTokenType.cpp

HEADERS += \
    LisaLexer.h \
    LisaParser.h \
    LisaSynTree.h \
    LisaToken.h \
    LisaTokenType.h
