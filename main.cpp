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
#include <QCryptographicHash>
#include "LisaLexer.h"
#include "LisaParser.h"
#include "Converter.h"
using namespace Lisa;



static void compareFiles( const QStringList& files, int off )
{
    foreach( const QString& f, files )
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        QFile in(f);
        in.open(QIODevice::ReadOnly);
        hash.addData(in.readAll());
        QFileInfo info(f);
        qDebug() << f.mid(off) << "\t" << info.fileName().toLower() << "\t" << hash.result().toHex();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if( a.arguments().size() <= 1 )
        return -1;
#if 0
    QFileInfo info(a.arguments()[1]);
    if( info.isDir() )
    {
        QDir from = info.dir();
        QDir to( from.path() + "/converted" );
        Converter::convert(from,to);
    }
#else
    QStringList files;

    int off = 0;
    QFileInfo info(a.arguments()[1]);
    if( info.isDir() )
    {
        files = Converter::collectFiles(info.dir(),"*.pas");
        off = a.arguments()[1].size();
    }else
        files << a.arguments()[1];

#if 0
    compareFiles(files,off);
#else
    int ok = 0;
    QMap<QByteArray,int> count;
    foreach( const QString& file, files )
    {
        QFile in(file);
        if( !in.open(QIODevice::ReadOnly) )
        {
            qCritical() << "cannot open file for reading:" << file;
            return -1;
        }
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
#if 1
        qDebug() << "**** parsing" << file.mid(off);
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
#else
        lex.setStream(&in);
        lex.setIgnoreComments(false);
        Token t = lex.nextToken();
        while(t.isValid())
        {
            if( t.d_type == Tok_Comment && t.d_val.startsWith("{$") )
            {
                const int pos = t.d_val.indexOf(' ');
                if( pos >= 0 )
                {
                    const QByteArray d = t.d_val.left(pos).mid(2).toUpper();
                    count[d]++;
                    if( d == "I" )
                        qDebug() << "***" << in.fileName().mid(off) << "includes" <<
                                    t.d_val.left(t.d_val.size()-1).mid(pos).trimmed();
                }
            }
            t = lex.nextToken();
        }

#endif
    }
#endif
    qDebug() << count;
    qDebug() << "#### finished with" << ok << "files ok of total" << files.size() << "files";
#endif
#endif
    return 0;
}
