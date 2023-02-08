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
    d_stack.back().d_lex.setMacros(&d_macros);
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
        t = d_stack.back().d_lex.nextToken();
    }
    while( t.d_type == Tok_INCLUDE )
    {
        if( !handleInclude(t) )
        {
            Token err(Tok_Invalid,t.d_lineNr,t.d_colNr,d_err.toUtf8());
            err.d_sourcePath = t.d_sourcePath;
            return err;
        }
        t = d_stack.back().d_lex.nextToken();
    }
    return t;
}

bool PpLexer::handleInclude(Token t)
{
    const FileSystem::File* f = d_fs->findFile(t.d_sourcePath);
    Q_ASSERT( f );

    QStringList path;
    int line = t.d_lineNr, startCol = 0, endCol = 0;
    while( t.isValid() && t.d_type != Tok_eol )
    {
        t = d_stack.back().d_lex.nextToken();
        if( t.d_type == Tok_ident )
        {
            if( startCol == 0 )
                startCol = t.d_colNr;
            path.append( QString::fromUtf8(t.d_val).toLower() );
            endCol = t.d_val.size() + t.d_colNr;
        }
    }

    QString fileName = path.last();
    if( fileName.endsWith(".text") )
        fileName.chop(5);
    const FileSystem::File* found = 0;
    // TODO: find can still be improved
    if( path.size() == 1 )
    {
        found = d_fs->findFile(f->d_dir, QString(), fileName);
    }else if( path.size() == 2 )
        found = d_fs->findFile(f->d_dir, path[0], fileName);

    Include inc;
    inc.d_inc = found;
    inc.d_loc = RowCol(line,startCol);
    inc.d_sourcePath = t.d_sourcePath;
    inc.d_len = endCol - startCol;
    d_includes.append(inc);
    d_err.clear();
    if( found )
    {
        d_stack.push_back(Level());
        QFile* file = new QFile(found->d_realPath);
        if( !file->open(QIODevice::ReadOnly) )
        {
            delete file;
            d_stack.pop_back();
            d_err = QString("file '%1' cannot be opened").arg(path.join('/')).toUtf8();
        }else
        {
            d_stack.back().d_lex.setStream(file,found->d_realPath);
            d_stack.back().d_lex.setMacros(&d_macros);
        }
    }else
        d_err = QString("assembler include file '%1' not found").arg(path.join('/')).toUtf8();
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

