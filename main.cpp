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
#include "LisaPpLexer.h"
#include "LisaParser.h"
#include "Converter.h"
#include "LisaFileSystem.h"
using namespace Lisa;

static void dump(QTextStream& out, const SynTree* node, int level)
{
    QByteArray str;
    if( node->d_tok.d_type == Tok_Invalid )
        level--;
    else if( node->d_tok.d_type < SynTree::R_First )
    {
        if( tokenTypeIsKeyword( node->d_tok.d_type ) )
            str = tokenTypeString(node->d_tok.d_type);
        else if( node->d_tok.d_type > TT_Specials )
            str = QByteArray("\"") + node->d_tok.d_val + QByteArray("\"");
        else
            str = QByteArray("\"") + tokenTypeString(node->d_tok.d_type) + QByteArray("\"");

    }else
        str = SynTree::rToStr( node->d_tok.d_type );
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
    foreach( SynTree* sub, node->d_children )
        dump( out, sub, level + 1 );
}

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

static void checkDir( const QStringList& files, int off)
{
    foreach( const QString& f, files )
    {
        QFile in(f);
        if( !in.open(QIODevice::ReadOnly) )
        {
            qCritical() << "cannot open file for reading:" << f;
            return;
        }
        if( Converter::detectPascal(&in) == Converter::Unknown )
            continue;
        QFileInfo info(f);
        QDir dir = info.absoluteDir();
        const QString fileName = info.fileName().toLower();
        QString name;
        if( fileName.endsWith("text.unix.txt") )
        {
            name = fileName;
            name.chop(13);
        }else
            name = info.baseName();
        const QStringList parts = name.contains('-') ? name.split('-') : name.split('.');
                // name.split(QRegExp("[\-,\.]"));
        if( parts.size() == 1 )
            qDebug() << "** file with no dir:" << f.mid(off);
        else if( parts.size() >= 2 )
        {
            const QString dirname = dir.dirName().toLower();
            if( parts.first() != dirname )
                qDebug() << "** actual and virtual dir differ:" << f.mid(off) << dirname << parts.first();
            else if( parts.size() > 2 )
                qDebug() << "** more than two parts:" << f.mid(off) << dirname << parts.first();
        }
    }
}

static void runParser(const QString& root)
{
    FileSystem fs;
    fs.load(root);
    // fs.getRoot().dump();

    QList<const FileSystem::File*> files = fs.getAllPas();
    int ok = 0;
    foreach( const FileSystem::File* file, files )
    {
        const QString path = file->getVirtualPath();
        PpLexer lex(&fs);
        lex.reset(file->d_realPath);
        Parser p(&lex);
        qDebug() << "**** parsing" << path;
        p.RunParser();
        if( !p.errors.isEmpty() )
        {
            foreach( const Parser::Error& e, p.errors )
                qCritical() << e.path.mid(root.size()) << e.row << e.col << e.msg;
                // qCritical() << fs.findFile(e.path)->getVirtualPath() << e.row << e.col << e.msg;

        }else
        {
            ok++;
            qDebug() << "ok";
        }
#if 0
        QFile out(file->d_realPath + ".st");
        out.open(QIODevice::WriteOnly);
        QTextStream s(&out);
        dump(s,&p.d_root,0);
#endif
    }
    qDebug() << "#### finished with" << ok << "files ok of total" << files.size() << "files";
}

static void runParser(const QString& root, const QString& path)
{
    FileSystem fs;
    fs.load(root);

    PpLexer lex(&fs);
    lex.reset(path);
    Parser p(&lex);
    qDebug() << "**** parsing" << path;
    p.RunParser();
    if( !p.errors.isEmpty() )
    {
        foreach( const Parser::Error& e, p.errors )
            qCritical() << e.path.mid(root.size()) << e.row << e.col << e.msg;
        // qCritical() << fs.findFile(e.path)->getVirtualPath() << e.row << e.col << e.msg;

    }else
    {
        qDebug() << "ok";
    }
#if 1
    QFile out(path + ".st");
    out.open(QIODevice::WriteOnly);
    QTextStream s(&out);
    dump(s,&p.d_root,0);
#endif
}

static void checkTokens(const QStringList& files)
{
    foreach( const QString& file, files )
    {
        QFile in(file);
        if( !in.open(QIODevice::ReadOnly) )
        {
            qCritical() << "cannot open file for reading:" << file;
            return;
        }
        Lexer lex;
        lex.setStream(&in);
        lex.setIgnoreComments(false);
        Token t = lex.nextToken();
        while(t.isValid())
        {
            // qDebug() << tokenTypeName(t.d_type) << t.d_lineNr << t.d_colNr << t.d_val;
            if( t.d_type == Tok_Comment && ( t.d_val.startsWith("{$") || t.d_val.startsWith("(*$") ) )
            {
                //const QByteArray tmp = t.d_val.toUpper();
                //if( tmp.startsWith("{$DECL") || tmp.startsWith("(*$DECL") )
                    qDebug() << "directive" << QFileInfo(file).fileName() << t.d_lineNr << t.d_colNr << t.d_val;
            }
            t = lex.nextToken();
        }
    }
}

#if 0
static void checkIncludes(const QStringList& files, int off)
{
    QMap<QByteArray,int> count;
    foreach( const QString& file, files )
    {
        QFile in(file);
        if( !in.open(QIODevice::ReadOnly) )
        {
            qCritical() << "cannot open file for reading:" << file;
            return;
        }
        Lexer lex;
        lex.setStream(&in);
        Parser p(&lex);
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

    }
    qDebug() << count;
}
#endif

static void convert(const QString& root)
{
    QFileInfo info(root);
    if( info.isDir() )
    {
        QDir from = info.dir();
        QDir to( from.path() + "/converted" );
        Converter::convert(from,to);
    }
}

static void checkFileNames(const QStringList& files)
{
    foreach( const QString& file, files )
    {
        QFile in(file);
        if( !in.open(QIODevice::ReadOnly) )
        {
            qCritical() << "cannot open file for reading:" << file;
            return;
        }
        if( Converter::detectPascal(&in) != Converter::Unknown )
        {
            const QString name = QFileInfo(file).fileName().toLower();
            if( !name.endsWith("text.unix.txt") )
                qDebug() << "** Pascal file with unexpected suffix:" << files;
        }
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if( a.arguments().size() <= 1 )
        return -1;

#if 0
    QStringList files;

    int off = 0;
    QFileInfo info(a.arguments()[1]);
    if( info.isDir() )
    {
        files = Converter::collectFiles(info.dir(),QStringList() << "*.txt");
        off = a.arguments()[1].size();
    }else
        files << a.arguments()[1];

    //convert(a.arguments()[1]);
    //compareFiles(files,off);
    //checkIncludes(files,off);
    //checkDir(files,off);
    //checkFileNames(files);
    //checkTokens(files);
#else
    QFileInfo info(a.arguments()[1]);
    if( info.isDir() )
        runParser(a.arguments()[1]);
    else
        runParser(info.absolutePath(),info.absoluteFilePath());
#endif

    return 0;
}
