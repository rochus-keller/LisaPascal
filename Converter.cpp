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

#include "Converter.h"
#include "LisaLexer.h"
#include <QtDebug>
using namespace Lisa;

// stats: ("F", 57)("OBJ", 8)("TEXT", 109)("text", 9)("txt", 1092)
// *.F -> font
// *.OBJ -> object files; source available unless TK-ALERT, TK-NullChange, TK-WorkDir, libtk-passwd and IconEdit
// *.TEXT, *.text -> looks like different kinds of sources covered in binary format
// *.txt -> Lisa Pascal (614/1092) or assembler source files

// NOTES:
// both (*$ and {$ are used for directives
// there are no $DECL in files categorized as .inc;
//   but there are a lot of $IFC in .inc files (even $I in two cases);
//   we can assume that the decls of the including file also apply to included files
// each pascal file has the suffix "text.unix.txt" with no exception

QStringList Converter::collectFiles( const QDir& dir, const QStringList& suffix )
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

int Converter::detectPascal( QIODevice* in )
{
    Q_ASSERT(in->reset());
    Lexer lex;
    lex.setStream(in);
    lex.setIgnoreComments(false);
    Token t = lex.nextToken();
    int res = Unknown;
    while( t.isValid() )
    {
        switch(t.d_type)
        {
        case Tok_Comment:
            if( t.d_val.startsWith("(*") || t.d_val.startsWith("{$") )
                res = AnyPascal;
            break;
        case Tok_program:
        case Tok_unit:
            return FullUnit;
        case Tok_function:
        case Tok_procedure:
        case Tok_const:
#ifdef LISA_CLASCAL
        case Tok_methods:
#endif
            // exceptions: aplw-compflags
            return PartialUnit;
        default:
            return res;
        }

        t = lex.nextToken();
    }
    return res;
}

bool Converter::detectAsm( QIODevice* in )
{
    Q_ASSERT(in->reset());
    int lastSemi = -1;
    int count = 0;
    bool semiStretch = false;
    int mnemonic = 0;
    for( int i = 0; i < 50; i++ )
    {
        const QByteArray line = in->readLine().trimmed().toUpper();
        if( line.startsWith(';') )
        {
            if( lastSemi == -1 || lastSemi == (i - 1) )
            {
                count++;
                lastSemi = i;
                if( count >= 15 )
                    semiStretch = true;
            }else
            {
                count = 0;
                lastSemi = -1;
            }
        }else
        {
            count = 0;
            lastSemi = -1;
            if( line.startsWith(".SEG") || line.startsWith(".PROC") || line.startsWith(".FUNC")
                 || line.startsWith(".REF") || line.startsWith(".SEG")
                 || line.startsWith(".WORD") || line.startsWith(".BYTE")
                 || line.startsWith("BGE.S") || line.startsWith("BNE.S")
                    || line.startsWith("MOVE.W") || line.startsWith("TST.W")
                    || line.startsWith("MOVE.L")
                    || line.contains(".EQU ")
                 )
                mnemonic++;
        }
    }
    return semiStretch || mnemonic;
}

bool Converter::detectScript(QIODevice* in )
{
    Q_ASSERT(in->reset());
    const QByteArray line = in->readLine().trimmed().toUpper();
    const bool found = line.startsWith("$EXEC") || line.startsWith("EXEC(");
    return found;
}

// assembler files have asm, 68k in the filename
//   exceptions: apll-qicode apll-stgcomp apll-xxfer aplt-convert aplw-fastgen aplw-sp-util appw-xfer
//   libhw-cursor libhw-drivers libhw-hwiequ libhw-hwintl libhw-keybd libhw-legends libhw-machine libhw-mouse
//   libhw-sprkeybd libhw-timers libam-alertprocs libdb-qsort libfp-bindec1/2/3
//   actually all assembler files start with a few lines where each starts with a ';'
// command files start with $EXEC, there are many of these
// menu files start with 1
// clascal files: libtk-uabc4, libtk-uabc3, and seven more in lisa_toolkit/tk3/4/5

bool Converter::convert(const QDir& fromDir, const QDir& toDir)
{
    const int off = fromDir.path().size();
    const QStringList files = collectFiles(fromDir, QStringList() << "*.txt");
    foreach( const QString& f, files )
    {
        QString newFilePath = toDir.absoluteFilePath(f.mid(off+1).toLower());
        if( newFilePath.endsWith("text.unix.txt") )
            newFilePath.chop(13);
        else if(newFilePath.endsWith("unix.txt") )
            newFilePath.chop(8);
        else if(newFilePath.endsWith("txt") )
            newFilePath.chop(3);
        const QString name = QFileInfo(f).baseName().toLower();
        const QString newDirPath = QFileInfo(f).dir().path().mid(off+1).toLower();
        if( !newDirPath.isEmpty() )
            toDir.mkpath( newDirPath );
        QFile in(f);
        in.open(QIODevice::ReadOnly);

        const int kind = detectPascal(&in);
        if( kind == FullUnit )
            newFilePath += "pas";
        else if( kind == PartialUnit || kind == AnyPascal )
        {
            newFilePath += "inc";
            //qDebug() << QFileInfo(newFilePath).baseName();
        }else if( detectScript(&in) )
            newFilePath += "sh";
        else if( detectAsm(&in) || name.contains("asm") || name.contains("68k") )
            newFilePath += "asm";
        else
            newFilePath += "txt";

        Q_ASSERT(in.reset());
        QFile out(newFilePath);
        if( !out.open(QIODevice::WriteOnly) )
        {
            qCritical() << "### cannot open for writing:" << newFilePath;
            continue;
        }
        QByteArray text = in.readAll();
        if( text.endsWith(0xff) )
            text.chop(1);
        if( text.size() != out.write(text) )
        {
            qCritical() << "### could not write everything to:" << newFilePath;
            continue;
        }
    }
    return true;
}
