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

#include "LisaLexer.h"
#include <QBuffer>
#include <QtDebug>
using namespace Lisa;

Lexer::Lexer():
    d_lastToken(Tok_Invalid),d_lineNr(0),d_colNr(0),d_in(0),
    d_ignoreComments(true), d_packComments(true),d_sloc(0),d_lineCounted(false)
{

}

Lexer::~Lexer()
{
}

void Lexer::setStream(QIODevice* in, const QString& filePath)
{
    d_in = in;
    d_lineNr = 0;
    d_colNr = 0;
    d_lastToken = Tok_Invalid;
    d_filePath = filePath;
    d_sloc = 0;
    d_lineCounted = false;
}

Token Lexer::nextToken()
{
    Token t;
    if( !d_buffer.isEmpty() )
    {
        t = d_buffer.first();
        d_buffer.pop_front();
    }else
        t = nextTokenImp();
    while( t.d_type == Tok_Comment && d_ignoreComments )
        t = nextToken();
    return t;
}

Token Lexer::peekToken(quint8 lookAhead)
{
    Q_ASSERT( lookAhead > 0 );
    while( d_buffer.size() < lookAhead )
    {
        Token t = nextTokenImp();
        while( t.d_type == Tok_Comment && d_ignoreComments )
            t = nextTokenImp();
        d_buffer.push_back( t );
    }
    return d_buffer[ lookAhead - 1 ];
}

QList<Token> Lexer::tokens(const QString& code)
{
    QBuffer in;
    in.setData( code.toLatin1() );
    in.open(QIODevice::ReadOnly);
    setStream( &in );

    QList<Token> res;
    Token t = nextToken();
    while( t.isValid() )
    {
        res << t;
        t = nextToken();
    }
    return res;
}

Token Lexer::nextTokenImp()
{
    if( d_in == 0 )
        return token(Tok_Eof);
    skipWhiteSpace();

    while( d_colNr >= d_line.size() )
    {
        if( d_in->atEnd() )
        {
            Token t = token( Tok_Eof, 0 );
            return t;
        }
        nextLine();
        skipWhiteSpace();
    }
    Q_ASSERT( d_colNr < d_line.size() );
    while( d_colNr < d_line.size() )
    {
        const char ch = quint8(d_line[d_colNr]);

        if( ch == '\'' )
            return string();
        else if( ch == '$')
            return hexnumber();
        else if( ::isalpha(ch) ||
                 ch == '%' || ch == '_' )
                    // leading % apparently supported, e.g. in libpl-BLOCKIO2.TEXT.unix.txt
                    // leading _ apparently supported, e.g. in libdb-HEAP.TEXT.unix.txt
            return ident();
        else if( ::isdigit(ch) )
            return number();
        // else
        int pos = d_colNr;
        TokenType tt = tokenTypeFromString(d_line,&pos);

        if( tt == Tok_Latt )
            return comment();
        else if( tt == Tok_Lbrace )
            return comment(true);
        else if( tt == Tok_Invalid || pos == d_colNr )
            return token( Tok_Invalid, 1, QString("unexpected character '%1' %2").arg(char(ch)).arg(int(ch)).toUtf8() );
        else {
            const int len = pos - d_colNr;
            return token( tt, len, d_line.mid(d_colNr,len) );
        }
    }
    Q_ASSERT(false);
    return token(Tok_Invalid);
}

int Lexer::skipWhiteSpace()
{
    const int colNr = d_colNr;
    while( d_colNr < d_line.size() && ( ::isspace( d_line[d_colNr] ) || d_line[d_colNr] == 0xff ) )
        d_colNr++;
    return d_colNr - colNr;
}

void Lexer::nextLine()
{
    d_colNr = 0;
    d_lineNr++;
    d_line = d_in->readLine();
    d_lineCounted = false;

    if( d_line.endsWith("\r\n") )
        d_line.chop(2);
    else if( d_line.endsWith('\n') || d_line.endsWith('\r') || d_line.endsWith('\025') )
        d_line.chop(1);
}

int Lexer::lookAhead(int off) const
{
    if( int( d_colNr + off ) < d_line.size() )
    {
        return d_line[ d_colNr + off ];
    }else
        return 0;
}

Token Lexer::token(TokenType tt, int len, const QByteArray& val)
{
    if( tt != Tok_Invalid && tt != Tok_Comment && tt != Tok_Eof )
        countLine();
    Token t( tt, d_lineNr, d_colNr + 1, val );
    d_lastToken = t;
    d_colNr += len;
    t.d_len = len;
    t.d_sourcePath = d_filePath;
    return t;
}

Token Lexer::ident()
{
    int off = 1;
    while( true )
    {
        const char c = lookAhead(off);
        if( !::isalnum(c) && c != '_' && c != '%' )
            // % in ident apparently supported, as seen in libfp-FPMODES.TEXT.unix.txt
            break;
        else
            off++;
    }
    const QByteArray str = d_line.mid(d_colNr, off );
    Q_ASSERT( !str.isEmpty() );
    int pos = 0;
    const QByteArray keyword = str.toLower();
    TokenType t = tokenTypeFromString( keyword, &pos );
    if( t != Tok_Invalid && pos != str.size() )
        t = Tok_Invalid;
    if( t != Tok_Invalid )
        return token( t, off );
    else
        return token( Tok_identifier, off, str );
}

static inline bool isHexDigit( char c )
{
    return ::isdigit(c) || c == 'A' || c == 'B' || c == 'C' || c == 'D' || c == 'E' || c == 'F'
            || c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' || c == 'f';
}

Token Lexer::number()
{
    // unsigned_real ::= // digit_sequence [ '.' digit_sequence ] [ scale_factor ]
    // scale_factor ::= ('E' | 'e') [sign] digit_sequence
    // digit_sequence ::= // digit { digit }


    int off = 1;
    while( true )
    {
        const char c = lookAhead(off);
        if( !::isdigit(c) )
            break;
        else
            off++;
    }
    bool isReal = false;
    if( lookAhead(off) == '.' && lookAhead(off+1) != '.' )
    {
        isReal = true;
        off++;
        if( !::isdigit(lookAhead(off)))
            return token( Tok_Invalid, off, "invalid real, digit expected after dot" );
        while( true )
        {
            const char c = lookAhead(off);
            if( !::isdigit(c) )
                break;
            else
                off++;
        }
    }
    if( lookAhead(off) == 'E' || lookAhead(off) == 'e' )
    {
        isReal = true;
        off++;
        char o = lookAhead(off);
        if( o == '+' || o == '-' )
        {
            off++;
            o = lookAhead(off);
        }
        if( !::isdigit(o) )
            return token( Tok_Invalid, off, "invalid real, digit expected after exponent" );
        while( true )
        {
            const char c = lookAhead(off);
            if( !::isdigit(c) )
                break;
            else
                off++;
        }
    }
    const QByteArray str = d_line.mid(d_colNr, off );
    Q_ASSERT( !str.isEmpty() );
    if( isReal)
        return token( Tok_unsigned_real, off, str );
    else
        return token( Tok_digit_sequence, off, str );
}

Token Lexer::hexnumber()
{
    // hex_digit_sequence ::= // '$' hex_digit { hex_digit }
    // hex_digit ::= digit | 'A'..'F'

    int off = 1;
    while( true )
    {
        const char c = lookAhead(off);
        if( !isHexDigit(c) )
            break;
        else
            off++;
    }
    const QByteArray str = d_line.mid(d_colNr, off );
    Q_ASSERT( !str.isEmpty() );
    return token( Tok_digit_sequence, off, str );
}

Token Lexer::comment(bool brace)
{
    const int startLine = d_lineNr;
    const int startCol = d_colNr;
    // startLine and startCol point to first char of (* or {

    const QByteArray tag = brace ? "}" : "*)";
    int pos = d_line.indexOf(tag,d_colNr);

    QByteArray str;
    bool terminated = false;
    if( pos < 0 )
        str = d_line.mid(d_colNr);
    else
    {
        terminated = true;
        pos += tag.size();
        str = d_line.mid(d_colNr,pos-d_colNr);
    }
    while( !terminated && !d_in->atEnd() )
    {
        nextLine();
        pos = d_line.indexOf(tag,d_colNr);
        if( !str.isEmpty() )
            str += '\n';
        if( pos < 0 )
            str += d_line.mid(d_colNr);
        else
        {
            terminated = true;
            pos += tag.size();
            str += d_line.mid(d_colNr,pos-d_colNr);
        }
    }
    if( d_packComments && !terminated && d_in->atEnd() )
    {
        d_colNr = d_line.size();
        Token t( Tok_Invalid, startLine, startCol + 1, "non-terminated comment" );
        t.d_sourcePath = d_filePath;
        return t;
    }
    // Col + 1 weil wir immer bei Spalte 1 beginnen, nicht bei Spalte 0
    Token t( ( d_packComments ? Tok_Comment : ( brace ? Tok_Lbrace : Tok_Latt ) ), startLine, startCol + 1, str );
    d_lastToken = t;
    d_colNr = pos;
    t.d_sourcePath = d_filePath;
    return t;
}

Token Lexer::string()
{
    int off = 1;
    while( true )
    {
        const char c = lookAhead(off);
        off++;
        if( c == '\'')
        {
            if( lookAhead(off) == '\'')
                off++;
            else
             break;
        }
        if( c == 0 )
            return token( Tok_Invalid, off, "non-terminated string" );
    }
    const QByteArray str = d_line.mid(d_colNr, off );
    return token( Tok_string_literal, off, str );
}

void Lexer::countLine()
{
    if( !d_lineCounted )
        d_sloc++;
    d_lineCounted = true;
}
