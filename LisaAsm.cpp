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
#include "AsmParser.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QtDebug>

static void dump(QTextStream& out, const Asm::SynTree* node, int level)
{
    QByteArray str;
    if( node->d_tok.d_type == Asm::Tok_Invalid )
        level--;
    else if( node->d_tok.d_type < Asm::SynTree::R_First )
    {
        if( Asm::tokenTypeIsKeyword( node->d_tok.d_type ) )
            str = Asm::tokenTypeString(node->d_tok.d_type);
        else if( node->d_tok.d_type > Asm::TT_Specials )
            str = QByteArray("\"") + node->d_tok.d_val + QByteArray("\"");
        else
            str = QByteArray("\"") + Asm::tokenTypeString(node->d_tok.d_type) + QByteArray("\"");

    }else
        str = Asm::SynTree::rToStr( node->d_tok.d_type );
    if( !str.isEmpty() )
    {
        str += QByteArray("\t") + QFileInfo(node->d_tok.d_sourcePath).baseName().toUtf8() +
                ":" + QByteArray::number(node->d_tok.d_lineNr) +
                ":" + QByteArray::number(node->d_tok.d_colNr);
        QByteArray ws;
        for( int i = 0; i < level; i++ )
            ws += "|  ";
        str = ws + str;
        out << str.data() << endl;
    }
    foreach( Asm::SynTree* sub, node->d_children )
        dump( out, sub, level + 1 );
}

QStringList collectFiles( const QDir& dir, const QStringList& suffix )
{
    QStringList res;
    QStringList files = dir.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );

    foreach( const QString& f, files )
        res += collectFiles( QDir( dir.absoluteFilePath(f) ), suffix );

    files = dir.entryList( suffix, QDir::Files, QDir::Name );
    foreach( const QString& f, files )
    {
        res.append(dir.absoluteFilePath(f));
    }
    return res;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if( a.arguments().size() <= 1 )
        return -1;

#if 0
    QStringList files;

    if( QFileInfo(a.arguments()[1]).isDir() )
        files = collectFiles(a.arguments()[1],QStringList() << "*.asm");
    else
        files << a.arguments()[1];

    int ok = 0;
    foreach( const QString& file, files )
    {
        qDebug() << "***** lexing" << file;
        Asm::Lexer lex;
        //lex.setIgnoreComments(false);
        QFile in(file);
        if( !in.open(QIODevice::ReadOnly) )
            return -1;
        lex.setStream(&in);
        Asm::Token t = lex.nextToken();
        int n = 0, count = 0;
        while( t.isValid() )
        {
#if 0
            count++;
            if( t.isDirective() && t.d_dotPrefix )
                n++;
            if( count == 10 && n == 0 )
                qDebug() << "file has no .DIRECTIVE in the first ten tokens" << QFileInfo(file).baseName();
                // there are 30 files to which this applies, e.g. libhw-cursor, machine, mouse, or libfp-elems68k2
#endif
            //if( t.d_type == Asm::Tok_5c )
            qDebug() << t.d_lineNr << t.d_colNr << Asm::tokenTypeName(t.d_type) << t.d_val;
            t = lex.nextToken();
        }
        if( t.d_type == Asm::Tok_Invalid )
        {
            qCritical() << t.d_sourcePath << t.d_lineNr << t.d_colNr << t.d_val;
            return -1;
        }
    }
#else
    Lisa::FileSystem fs;
    fs.load(a.arguments()[1]);
    QList<const Lisa::FileSystem::File*> files = fs.getAllAsm();
    int ok = 0;
    foreach( const Lisa::FileSystem::File* f, files )
    {
        Asm::PpLexer lex(&fs);
        lex.reset(f->d_realPath);
        Asm::Parser p(&lex);
        //qDebug() << "**** parsing" << file;
        p.RunParser();
        if( !p.errors.isEmpty() )
        {
            foreach( const Asm::Parser::Error& e, p.errors )
                qCritical() << f->getVirtualPath() << e.row << e.col << e.msg;

        }else
        {
            ok++;
            //qDebug() << "ok";
        }
#if 0
        QFile out(f->d_realPath + ".st");
        out.open(QIODevice::WriteOnly);
        QTextStream s(&out);
        dump(s,&p.d_root,0);
#endif
    }
#endif
    qDebug() << "#### finished with" << ok << "files ok of total" << files.size() << "files";

    return 0;
}
