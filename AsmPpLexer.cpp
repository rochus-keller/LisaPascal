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

#include "AsmPpLexer.h"
#include <QBuffer>
#include <QFile>
#include <QtDebug>
using namespace Asm;
using namespace Lisa;

PpLexer::PpLexer(FileSystem* fs):d_fs(fs),d_sloc(0)
{
    Q_ASSERT(fs);
}

PpLexer::~PpLexer()
{
    for( int i = 0; i < d_files.size(); i++ )
        delete d_files[i];
}

bool PpLexer::reset(const QString& filePath)
{
    d_stack.clear();
    d_buffer.clear();
    for( int i = 0; i < d_files.size(); i++ )
        delete d_files[i];
    d_files.clear();
    d_sloc = 0;
    d_includes.clear();

    const FileSystem::File* f = d_fs->findFile(filePath);
    if( f == 0 )
        return false;
    d_stack.push_back(Level());
    QFile* file = new QFile(filePath);
    if( !file->open(QIODevice::ReadOnly) )
    {
        delete file;
        d_stack.pop_back();
        return false;
    }
    d_stack.back().d_lex.setStream(file,filePath);
    return true;
}

Token PpLexer::nextToken()
{
    Token t;
    if( !d_buffer.isEmpty() )
    {
        t = d_buffer.first();
        d_buffer.pop_front();
    }else
        t = nextTokenImp();
    Q_ASSERT( t.d_type != Tok_Comment );
    return t;
}

Token PpLexer::peekToken(quint8 lookAhead)
{
    Q_ASSERT( lookAhead > 0 );
    while( d_buffer.size() < lookAhead )
    {
        Token t = nextTokenImp();
        Q_ASSERT( t.d_type != Tok_Comment );
        d_buffer.push_back( t );
    }
    return d_buffer[ lookAhead - 1 ];
}

Token PpLexer::nextTokenImp()
{
    if( d_stack.isEmpty() )
        return Token(Tok_Eof);
    Token t = d_stack.back().d_lex.nextToken();
    while( t.d_type == Tok_Comment || t.d_type == Tok_Eof )
    {
        const bool statusBefore = ppthis().open;
        if( t.d_type == Tok_Eof )
        {
            d_files.removeAll(d_stack.back().d_lex.getDevice());
            delete d_stack.back().d_lex.getDevice();
            d_sloc += d_stack.back().d_lex.getSloc();
            d_mutes.insert(t.d_sourcePath, d_stack.back().d_mutes);
            d_stack.pop_back();
            if( d_stack.isEmpty() )
                return Token(Tok_Eof);
        }
        if( t.d_type == Tok_Comment && ( t.d_val.startsWith("{$") || t.d_val.startsWith("(*$") ) )
        {
            QByteArray data = t.d_val;
            const PpSym sym = checkPp(data);
            bool ok = true;
            if( sym == PpIncl )
                ok = handleInclude(data,t);
            else if( sym == PpSetc )
                ok = handleSetc(data);
            else if( sym == PpIfc )
                ok = handleIfc(data);
            else if( sym == PpElsec )
                ok = handleElsec();
            else if( sym == PpEndc )
                ok = handleEndc();
            if( !ok )
            {
                Token err(Tok_Invalid,t.d_lineNr,t.d_colNr,d_err.toUtf8());
                err.d_sourcePath = t.d_sourcePath;
                return err;
            }
        }
        const bool statusAfter = ppthis().open;
        if( statusBefore != statusAfter )
        {
            if( !ppthis().open )
            {
                d_startMute = t.toLoc();
                d_startMute.d_col += t.d_val.size();
            }else
                d_stack.back().d_mutes.append(qMakePair(d_startMute,t.toLoc()));
        }
        if( !ppthis().open )
        {
            t = d_stack.back().d_lex.peekToken();
            while( t.d_type != Tok_Comment && t.d_type != Tok_Eof )
            {
                t = d_stack.back().d_lex.nextToken();
                t = d_stack.back().d_lex.peekToken();
            }
        }
        t = d_stack.back().d_lex.nextToken();
    }
    return t;
}

class PpMiniLex
{
public:
    PpMiniLex(const QByteArray& str):d_pos(0),d_str(str){}
    char next()
    {
        if( d_pos < d_str.size() )
            return ::tolower(d_str[d_pos++]);
        else
            return 0;
    }
    int getPos() const { return d_pos; }
private:
    QByteArray d_str;
    int d_pos;
};

static PpLexer::PpSym decodePp( PpMiniLex& lex )
{
    char ch;
    switch( lex.next() )
    {
    case 'i':
        ch = lex.next();
        if( ::isspace(ch) )
            return PpLexer::PpIncl;
        else if( ch == 'f' )
            if( lex.next() == 'c' )
                return PpLexer::PpIfc;
        break;
    case 'e':
        switch( lex.next() )
        {
        case 'l':
            if( lex.next() == 's' )
                if( lex.next() == 'e' )
                    if( lex.next() == 'c' )
                        return PpLexer::PpElsec;
            break;
        case 'n':
            if( lex.next() == 'd' )
                if( lex.next() == 'c' )
                    return PpLexer::PpEndc;
            break;
        }
        break;
    case 's':
        if( lex.next() == 'e' )
            if( lex.next() == 't' )
                if( lex.next() == 'c' )
                    return PpLexer::PpSetc;
        break;
    }
    return PpLexer::PpNone;
}

PpLexer::PpSym PpLexer::checkPp(QByteArray& str)
{
    PpSym res = PpNone;
    PpMiniLex lex(str);
    switch( lex.next() )
    {
    case '{':
        if( lex.next() == '$' )
        {
            res = decodePp(lex);
            str = str.mid(lex.getPos(), str.size() - lex.getPos() - 1);
        }
        break;
    case '(':
        if( lex.next() == '*' )
            if( lex.next() == '$' )
            {
                res = decodePp(lex);
                str = str.mid(lex.getPos(), str.size() - lex.getPos() - 2);
            }
        break;
    }
    return res;
}

bool PpLexer::handleInclude(const QByteArray& data, const Token& t)
{
    QString path = QString::fromUtf8(data).trimmed().toLower();
    if( path.endsWith(".text") )
        path.chop(5);
    const FileSystem::File* f = d_fs->findFile(t.d_sourcePath);
    Q_ASSERT( f );
    QStringList pathFile = path.split('/');
    const FileSystem::File* found = 0;
    if( pathFile.size() == 1 )
    {
        QString name = pathFile[0];
        const int colon = name.indexOf(':');
        if( colon != -1 )
            name = name.mid(colon+1);
        found = d_fs->findFile(f->d_dir, QString(), name);
    }else if( pathFile.size() == 2 )
        found = d_fs->findFile(f->d_dir, pathFile[0], pathFile[1]);

    Include inc;
    inc.d_inc = found;
    inc.d_loc = t.toLoc();
    inc.d_sourcePath = t.d_sourcePath;
    inc.d_len = t.d_val.size();
    d_includes.append(inc);
    if( found )
    {
        d_stack.push_back(Level());
        QFile* file = new QFile(found->d_realPath);
        if( !file->open(QIODevice::ReadOnly) )
        {
            delete file;
            d_stack.pop_back();
            d_err = QString("file '%1' cannot be opened").arg(data.constData()).toUtf8();
            return false;
        }
        d_stack.back().d_lex.setStream(file,found->d_realPath);
    }else
        d_err = QString("include file '%1' not found").arg(data.constData()).toUtf8();
    return found;
}

bool PpLexer::handleSetc(const QByteArray& data)
{
#if 0
    // TODO
    QByteArray statement = data;
    QBuffer buf(&statement);
    buf.open(QIODevice::ReadOnly);
    Lexer lex;
    lex.setStream(&buf);
    Token tt = lex.nextToken();
    if( tt.d_type != Tok_ident )
        return error("expecting identifier on left side of SETC assignment");
    const QByteArray var = tt.d_val.toLower();
    if( var == "true" || var == "false" )
        return error("cannot assign to true or false in SETC");
    tt = lex.nextToken();
    if( tt.d_type != Tok_ColonEq && tt.d_type != Tok_Eq )
        return error("expecting ':=' or '=' in SETC assignment");
    PpEval e(d_ppVars,lex);
    if( !e.eval() )
        return error(QString("%1 in SETC expression").arg(e.getErr().constData()));
    d_ppVars[var] = e.getRes();
#endif
    return true;
}

bool PpLexer::handleIfc(const QByteArray& data)
{
#if 0
    // TODO
    QByteArray statement = data;
    QBuffer buf(&statement);
    buf.open(QIODevice::ReadOnly);
    Lexer lex;
    lex.setStream(&buf);
    PpEval e(d_ppVars,lex);
    if( !e.eval() )
        return error(QString("%1 in SETC expression").arg(e.getErr().constData()));

    const bool cond = e.getRes();
    d_conditionStack.append( ppstatus(false) );
    ppsetthis( ppouter().open && cond );
#endif
    return true;
}

bool PpLexer::handleElsec()
{
    if( ppthis().elseSeen || d_conditionStack.isEmpty() )
        return error("ELSEC not expected here");
    else
        ppsetthis( ppouter().open && !ppthis().openSeen, true );
    return true;
}

bool PpLexer::handleEndc()
{
    if( d_conditionStack.isEmpty() )
        return error("spurious ENDC");
    else
        d_conditionStack.pop_back();
    return true;
}

bool PpLexer::error(const QString& msg)
{
    d_err = msg;
    return false;
}

