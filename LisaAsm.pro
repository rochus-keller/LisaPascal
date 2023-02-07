QT       += core
QT       -= gui

TARGET = LisaAsm
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

INCLUDEPATH += ..
DEFINES += _DEBUG

SOURCES += \
    LisaAsm.cpp \
    AsmTokenType.cpp \
    AsmLexer.cpp \
    AsmPpLexer.cpp \
    AsmSynTree.cpp \
    AsmParser.cpp \
    LisaFileSystem.cpp \
    LisaLexer.cpp \
    LisaTokenType.cpp \
    LisaToken.cpp

HEADERS += \
    AsmToken.h \
    AsmTokenType.h \
    AsmLexer.h \
    AsmPpLexer.h \
    AsmSynTree.h \
    AsmParser.h \
    LisaFileSystem.h \
    LisaLexer.h \
    LisaTokenType.h \
    LisaToken.h


