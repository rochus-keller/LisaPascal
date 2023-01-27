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

#include "PpLexer.h"
#include <QFile>
#include <QtDebug>
using namespace Lisa;

PpLexer::PpLexer(FileSystem* fs):d_fs(fs)
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

    const FileSystem::File* f = d_fs->findFile(filePath);
    if( f == 0 )
        return false;
    d_stack.push_back(Lexer());
    d_stack.back().setIgnoreComments(false);
    QFile* file = new QFile(filePath);
    if( !file->open(QIODevice::ReadOnly) )
    {
        delete file;
        d_stack.pop_back();
        return false;
    }
    d_stack.back().setStream(file,filePath);

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
    Token t = d_stack.back().nextToken();
    while( t.d_type == Tok_Comment || t.d_type == Tok_Eof )
    {
        if( t.d_type == Tok_Eof )
        {
            d_files.removeAll(d_stack.back().getDevice());
            delete d_stack.back().getDevice();
            d_stack.pop_back();
            if( d_stack.isEmpty() )
                return Token(Tok_Eof);
        }
        if( t.d_type == Tok_Comment && ( t.d_val.startsWith("{$") || t.d_val.startsWith("(*$") ) )
        {
            const QByteArray tmp = t.d_val.toLower();
            if( tmp.startsWith("{$i ") || tmp.startsWith("(*$i ") )
            {
                QString path = QString::fromUtf8(
                        tmp[0] == '{' ? tmp.mid(4,tmp.size() - 5) : tmp.mid(5,tmp.size() - 7) ).trimmed();
                if( path.endsWith(".text") )
                    path.chop(5);
                const FileSystem::File* f = d_fs->findFile(t.d_sourcePath);
                Q_ASSERT( f );
                QStringList pathFile = path.split('/');
                const FileSystem::File* found = 0;
                if( pathFile.size() == 1 )
                    found = d_fs->findFile(f->d_dir, QString(), pathFile[1]);
                else if( pathFile.size() == 2 )
                    found = d_fs->findFile(f->d_dir, pathFile[0], pathFile[1]);
                if( found == 0 )
                {
                    Token tt(Tok_Invalid,t.d_lineNr,t.d_colNr,
                          QString("file '%1' not found").arg(t.d_val.constData()).toUtf8());
                    tt.d_sourcePath = t.d_sourcePath;
                    return tt;
                }else
                {
                    d_stack.push_back(Lexer());
                    d_stack.back().setIgnoreComments(false);
                    QFile* file = new QFile(found->d_realPath);
                    if( !file->open(QIODevice::ReadOnly) )
                    {
                        delete file;
                        d_stack.pop_back();
                        Token tt(Tok_Invalid,t.d_lineNr,t.d_colNr,
                              QString("file '%1' cannot be opened").arg(t.d_val.constData()).toUtf8());
                        tt.d_sourcePath = found->d_realPath;
                        return tt;
                    }
                    qDebug() << "including:" << path; // TEST
                    d_stack.back().setStream(file,found->d_realPath);

                }
            }
        }
        t = d_stack.back().nextToken(); // TODO handle other directives
    }
    return t;
}

