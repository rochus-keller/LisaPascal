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

#include "LisaToken.h"
#include <QHash>
#include <QtDebug>

static QHash<QByteArray,QByteArray> d_symbols;


const char* Lisa::Token::toId(const QByteArray& ident)
{
    if( ident.isEmpty() )
        return "";
    const QByteArray lc = ident.toLower();
    QByteArray& sym = d_symbols[lc];
    if( sym.isEmpty() )
        sym = lc;
    return sym.constData();
}
