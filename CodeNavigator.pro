QT       += core gui widgets

TARGET = LisaCodeNavigator
TEMPLATE = app

INCLUDEPATH +=  ..

#DEFINES += LISA_CHECK_COVERAGE

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
    LisaParser.h \
    LisaRowCol.h \
    LisaFileSystem.h \
    LisaPpLexer.h

SOURCES += \
    LisaLexer.cpp \
    LisaSynTree.cpp \
    LisaTokenType.cpp \
    LisaHighlighter.cpp \
    LisaCodeNavigator.cpp \
    LisaCodeModel.cpp \
    LisaParser.cpp \
    LisaToken.cpp \
    LisaFileSystem.cpp \
    LisaPpLexer.cpp

RESOURCES += \
    CodeNavigator.qrc




