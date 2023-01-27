#ifndef PPLEXER_H
#define PPLEXER_H

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

#include "FileSystem.h"
#include "LisaLexer.h"

class QIODevice;

namespace Lisa
{
class FileSystem;

class PpLexer
{
public:
    PpLexer(FileSystem*);
    ~PpLexer();

    bool reset(const QString& filePath);

    Token nextToken();
    Token peekToken(quint8 lookAhead = 1);
protected:
    Token nextTokenImp();

private:
    FileSystem* d_fs;
    QList<Lexer> d_stack;
    QList<QIODevice*> d_files;
    QList<Token> d_buffer;
};
}

#endif // PPLEXER_H
