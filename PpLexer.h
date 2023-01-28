#ifndef PPLEXER_H
#define PPLEXER_H

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

#include "FileSystem.h"
#include "LisaLexer.h"

class QIODevice;

namespace Lisa
{
class FileSystem;

class PpLexer
{
public:
    enum PpSym { PpNone, PpIncl, PpSetc, PpIfc, PpElsec, PpEndc };
    typedef QHash<QByteArray,int> PpVars;


    PpLexer(FileSystem*);
    ~PpLexer();

    bool reset(const QString& filePath);

    Token nextToken();
    Token peekToken(quint8 lookAhead = 1);
    quint32 getSloc() const { return d_sloc; }
protected:
    Token nextTokenImp();
    static PpSym checkPp(QByteArray&);
    bool handleInclude(const QByteArray& data, const QString& sourcePath);
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
    FileSystem* d_fs;
    QList<Lexer> d_stack;
    QList<QIODevice*> d_files;
    QList<Token> d_buffer;
    QString d_err;
    quint32 d_sloc; // number of lines of code without empty or comment lines
    PpVars d_ppVars;
    QList<ppstatus> d_conditionStack;

};
}

#endif // PPLEXER_H
