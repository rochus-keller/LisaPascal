#ifndef LISAHIGHLIGHTER_H
#define LISAHIGHLIGHTER_H

/*
* Copyright 2023 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the Lisa Pascal Navigator application.
*
* The following is the license that applies to this copy of the
* application. For a license to use the library under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include <QSyntaxHighlighter>
#include <QSet>

namespace Lisa
{
    class PascalPainter : public QSyntaxHighlighter
    {
    public:
        enum { TokenProp = QTextFormat::UserProperty };
        explicit PascalPainter(QObject *parent = 0);
        void addBuiltIn(const QByteArray& bi);
        void addKeyword(const QByteArray& kw);

    protected:
        QTextCharFormat formatForCategory(int) const;

        // overrides
        void highlightBlock(const QString &text);

    private:
        enum Category { C_Num, C_Str, C_Kw, C_Type, C_Ident, C_Op, C_Pp, C_Cmt, C_Label, C_Max };
        QTextCharFormat d_format[C_Max];
        QSet<QByteArray> d_builtins, d_keywords;
    };

    class AsmPainter : public QSyntaxHighlighter
    {
    public:
        AsmPainter(QObject* parent);

        // overrides
        void highlightBlock(const QString &text);
    private:
        enum Category { C_Num, C_Str, C_Kw, C_Ident, C_Op, C_Pp, C_Cmt, C_Label, C_Max };
        QTextCharFormat d_format[C_Max];
    };

    class LogPainter : public QSyntaxHighlighter
    {
    public:
        explicit LogPainter(QTextDocument *parent = 0);
    protected:
        // overrides
        void highlightBlock(const QString &text);
    };
}

#endif // LISAHIGHLIGHTER_H
