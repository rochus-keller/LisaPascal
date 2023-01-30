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

static quint32 s_maxId = 0;
static QHash<QByteArray,quint16> s_dir;


quint16 Lisa::Token::toId(const QByteArray& ident)
{
    quint16& id = s_dir[ ident.toLower() ];
    if( id == 0 )
    {
        if( ++s_maxId > 0xffff )
            qWarning() << "Lisa::Token::toId: run out of IDs, required" << s_maxId;
        else
            id = s_maxId;
    }
    return id;
}
