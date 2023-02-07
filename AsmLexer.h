#ifndef LISAASMLEXER_H
#define LISAASMLEXER_H

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

#include "AsmToken.h"

#include <QList>

class QIODevice;

namespace Asm
{
class Lexer
{
public:
    Lexer();
    void setStream(QIODevice*, const QString& filePath = QString());
    QIODevice* getDevice() const { return d_in; }
    void setIgnoreComments( bool b ) { d_ignoreComments = b; }
    void setPackComments( bool b ) { d_packComments = b; }

    Token nextToken();
    Token peekToken(quint8 lookAhead = 1);
    QList<Token> tokens( const QString& code );
    quint32 getSloc() const { return d_sloc; }
protected:
    Token nextTokenImp();
    int skipWhiteSpace();
    void nextLine();
    int lookAhead(int off = 1) const;
    Token token(TokenType tt, int len = 1, const QByteArray &val = QByteArray(), bool = false);
    Token ident(bool dotPrefix = false);
    Token number();
    Token hexnumber();
    Token string1();
    Token string2();
    Token label();
    void countLine();
private:
    QIODevice* d_in;
    QString d_filePath;
    quint32 d_lineNr;
    quint16 d_colNr;
    QByteArray d_line;
    QList<Token> d_buffer;
    Token d_lastToken;
    quint32 d_sloc;
    bool d_ignoreComments;
    bool d_packComments;
    bool d_lineCounted;
};
}

#endif // LISAASMLEXER_H
