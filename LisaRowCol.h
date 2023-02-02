#ifndef LISAROWCOL
#define LISAROWCOL

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
#include <QPair>

namespace Lisa
{
struct RowCol
{
    enum { ROW_BIT_LEN = 19, COL_BIT_LEN = 32 - ROW_BIT_LEN };
    quint32 d_row : ROW_BIT_LEN;
    quint32 d_col : COL_BIT_LEN;
    RowCol(int row = 0, int col = 0):d_row(row),d_col(col){}
    quint32 packed() const { return ( d_row << COL_BIT_LEN ) | d_col; }
    bool operator==(const RowCol& rhs) const {
        return d_row == rhs.d_row && d_col == rhs.d_col;
    }
    bool operator<(const RowCol& rhs) const {
        return packed() < rhs.packed();
    }
};
typedef QPair<RowCol,RowCol> Range;
typedef QList<Range> Ranges;
struct FilePos
{
    RowCol d_pos;
    QString d_filePath;
    FilePos(RowCol pos = RowCol(), const QString& path = QString()):d_pos(pos),d_filePath(path){}
};
}

#endif // LISAROWCOL

