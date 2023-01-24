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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QtDebug>
#include "LisaLexer.h"
#include "LisaParser.h"
using namespace Lisa;

static QStringList collectFiles( const QDir& dir )
{
    // stats: ("F", 57)("OBJ", 8)("TEXT", 109)("text", 9)("txt", 1092)
    // *.F -> font
    // *.OBJ -> object files; source available unless TK-ALERT, TK-NullChange, TK-WorkDir, libtk-passwd and IconEdit
    // *.TEXT, *.text -> looks like different kinds of sources covered in binary format
    // *.txt -> Lisa Pascal (614/1092) or assembler source files

    QStringList res;
    QStringList files = dir.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );

    foreach( const QString& f, files )
        res += collectFiles( QDir( dir.absoluteFilePath(f) ) );

    files = dir.entryList( QStringList() << "*.txt", QDir::Files, QDir::Name );
    foreach( const QString& f, files )
    {
        QFile in(dir.absoluteFilePath(f));
        if( !in.open(QIODevice::ReadOnly) )
        {
            qCritical() << "cannot open file for reading:" << f;
            continue;
        }
        Lexer lex;
        lex.setStream(&in);
        lex.setIgnoreComments(false);
        Token t = lex.nextToken();
        if( t.d_type == Tok_Comment || t.d_type == Tok_program || t.d_type == Tok_unit ||
                t.d_type == Tok_procedure || t.d_type == Tok_function )
            res.append( in.fileName() );
        // else qDebug() << "** Not Pascal: " << in.fileName();
    }
    return res;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if( a.arguments().size() <= 1 )
        return -1;

    QStringList files;

    int off = 0;
    QFileInfo info(a.arguments()[1]);
    if( info.isDir() )
    {
        files = collectFiles(a.arguments()[1]);
        off = a.arguments()[1].size();
    }else
        files << a.arguments()[1];

    int ok = 0;
    foreach( const QString& file, files )
    {
        QFile in(file);
        if( !in.open(QIODevice::ReadOnly) )
        {
            qCritical() << "cannot open file for reading:" << file;
            return -1;
        }
        qDebug() << "**** parsing" << file.mid(off);
        Lexer lex;
        lex.setStream(&in);
    #if 0
        lex.setIgnoreComments(false);
        Token t = lex.nextToken();
        while(t.isValid())
        {
            qDebug() << tokenTypeName(t.d_type) << t.d_lineNr << t.d_colNr << t.d_val;
            t = lex.nextToken();
        }
    #else
        Parser p(&lex);
        p.RunParser();
        if( !p.errors.isEmpty() )
        {
            foreach( const Parser::Error& e, p.errors )
                qCritical() << e.row << e.col << e.msg;
        }else
        {
            ok++;
            qDebug() << "ok";
        }
    }
#endif
    qDebug() << "#### finished with" << ok << "files ok of total" << files.size() << "files";
    return 0;
}
