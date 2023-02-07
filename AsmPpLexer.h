#ifndef ASMPPLEXER_H
#define ASMPPLEXER_H

/*
** Copyright (C) 2023 Rochus Keller (me@rochus-keller.ch)
**
** This file is part of the LisaPascal project.
**
** $QT_BEGIN_LICENSE:LGPL21$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
*/

#include "LisaFileSystem.h"
#include "AsmLexer.h"
#include "LisaRowCol.h"

class QIODevice;

namespace Lisa
{
class FileSystem;

}
namespace Asm
{
class PpLexer
{
public:
    enum PpSym { PpNone, PpIncl, PpSetc, PpIfc, PpElsec, PpEndc };
    typedef QHash<QByteArray,int> PpVars;
    struct Include
    {
        const FileSystem::File* d_inc; // the file to include
        QString d_sourcePath; // the file where the include lives
        RowCol d_loc; // the pos of the include directive in sourcePath
        quint16 d_len; // the len of the include directive
    };

    PpLexer(FileSystem*);
    ~PpLexer();

    bool reset(const QString& filePath);

    Token nextToken();
    Token peekToken(quint8 lookAhead = 1);
    quint32 getSloc() const { return d_sloc; }
    const QList<Include>& getIncludes() const { return d_includes; }
    const QHash<QString,Ranges>& getMutes() const { return d_mutes; }
protected:
    Token nextTokenImp();

    // TODO: implement preprocessor compatible with Assembler syntax
    static PpSym checkPp(QByteArray&);
    bool handleInclude(const QByteArray& data, const Token& t);
    bool handleSetc(const QByteArray& data);
    bool handleIfc(const QByteArray& data);
    bool handleElsec();
    bool handleEndc();
    bool error( const QString& msg);

    struct ppstatus
    {
        bool open; // this is the open condition which renders tokens
        bool openSeen; // at least one true condition seen
        bool elseSeen; // there was already an else part
        ppstatus(bool o = true):open(o),openSeen(false),elseSeen(false){}
    };

    ppstatus ppouter()
    {
        ppstatus res;
        if( d_conditionStack.size() >= 2 )
            res = d_conditionStack[d_conditionStack.size()-2];
        return res;
    }
    ppstatus ppthis()
    {
        ppstatus res;
        if( !d_conditionStack.isEmpty() )
            res = d_conditionStack.back();
        return res;
    }
    void ppsetthis(bool open, bool thisIsElse = false)
    {
        if( !d_conditionStack.isEmpty() )
        {
            ppstatus& stat = d_conditionStack.back();
            stat.open = open;
            if( thisIsElse )
                stat.elseSeen = true;
            if( open )
                stat.openSeen = true;
        }
    }
private:
    struct Level
    {
        Lexer d_lex;
        Ranges d_mutes;
    };

    FileSystem* d_fs;
    QList<Level> d_stack;
    QList<QIODevice*> d_files;
    QList<Token> d_buffer;
    QString d_err;
    quint32 d_sloc; // number of lines of code without empty or comment lines
    PpVars d_ppVars;
    QList<ppstatus> d_conditionStack;
    QList<Include> d_includes;
    QHash<QString,Ranges> d_mutes;
    RowCol d_startMute;
};
}

#endif // ASMPPLEXER_H
