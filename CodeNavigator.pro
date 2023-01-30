QT       += core gui widgets

TARGET = LisaCodeNavigator
TEMPLATE = app

INCLUDEPATH +=  ..


CONFIG(debug, debug|release) {
        DEFINES += _DEBUG
}

!win32{
QMAKE_CXXFLAGS += -Wno-reorder -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable
}

HEADERS += \
    LisaLexer.h \
    LisaSynTree.h \
    LisaToken.h \
    LisaTokenType.h \
    LisaHighlighter.h \
    LisaCodeNavigator.h \
    LisaCodeModel.h \
    FileSystem.h \
    PpLexer.h \
    LisaParser.h \
    LisaRowCol.h

SOURCES += \
    LisaLexer.cpp \
    LisaSynTree.cpp \
    LisaTokenType.cpp \
    LisaHighlighter.cpp \
    LisaCodeNavigator.cpp \
    LisaCodeModel.cpp \
    FileSystem.cpp \
    PpLexer.cpp \
    LisaParser.cpp \
    LisaToken.cpp

RESOURCES += \
    CodeNavigator.qrc




