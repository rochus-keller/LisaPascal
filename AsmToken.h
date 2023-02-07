#ifndef LISAASMTOKEN
#define LISAASMTOKEN

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

#include "AsmTokenType.h"
#include "LisaRowCol.h"

namespace Asm
{
using namespace Lisa;

struct Token
{
#ifdef _DEBUG
        union
        {
            int d_type; // TokenType
            TokenType d_tokenType;
        };
#else
        quint8 d_type; // TokenType
#endif
    quint32 d_lineNr : RowCol::ROW_BIT_LEN;
    quint32 d_colNr : RowCol::COL_BIT_LEN -1;
    quint32 d_dotPrefix : 1;
    QString d_sourcePath;

    QByteArray d_val;
    Token(quint16 t = 0, quint32 line = 0, quint16 col = 0, const QByteArray& val = QByteArray()):
        d_type(t), d_lineNr(line),d_colNr(col),d_val(val),d_dotPrefix(0){}
    bool isValid() const { return d_type != Tok_Eof && d_type != Tok_Invalid; }
    RowCol toLoc() const { return RowCol(d_lineNr,d_colNr); }
    static bool isDirective(int t) {
        switch( t )
        {
        case Tok_PROC: case Tok_FUNC: case Tok_DEF: case Tok_REF: case Tok_SEG: case Tok_ASCII:
        case Tok_TITLE: case Tok_END: case Tok_ENDM: case Tok_ELSE: case Tok_ENDC: case Tok_LIST:
        case Tok_NOLIST: case Tok_MACROLIST: case Tok_NOMACROLIST: case Tok_PATCHLIST: case Tok_NOPATCHLIST:
        case Tok_BYTE: case Tok_WORD: case Tok_ORG: case Tok_RORG: case Tok_IF: case Tok_EQU:
        case Tok_INCLUDE: case Tok_MACRO:
            return true;
        default:
            return false;
        }
    }
    bool isDirective() const { return isDirective(d_type); }
};
}

#endif // LISAASMTOKEN

