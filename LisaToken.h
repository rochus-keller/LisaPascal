#ifndef LISATOKEN_H
#define LISATOKEN_H

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

#include <QString>
#include <LisaTokenType.h>

namespace Lisa
{
    struct Token
    {
#ifdef _DEBUG
        union
        {
		    int d_type; // TokenType
            TokenType d_tokenType;
        };
#else
        quint16 d_type; // TokenType
#endif
        enum { ROW_BIT_LEN = 19, COL_BIT_LEN = 32 - ROW_BIT_LEN };
        quint32 d_lineNr : ROW_BIT_LEN;
        quint32 d_colNr : COL_BIT_LEN;
        QString d_sourcePath;

        QByteArray d_val; // using raw utf8 values pointing to buffered file content
        Token(quint16 t = Tok_Invalid, quint32 line = 0, quint16 col = 0, const QByteArray& val = QByteArray()):
            d_type(t), d_lineNr(line),d_colNr(col),d_val(val){}
        bool isValid() const { return d_type != Tok_Eof && d_type != Tok_Invalid; }
    };
}

#endif // LISATOKEN_H
