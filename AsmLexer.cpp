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

#include "AsmLexer.h"
#include <QtDebug>
#include <QBuffer>
using namespace Asm;

Lexer::Lexer():
    d_lastToken(Tok_Invalid),d_lineNr(0),d_colNr(0),d_in(0),
    d_ignoreComments(true), d_packComments(true),d_sloc(0),d_lineCounted(false)
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
            if( d_lineCounted )
            {
                d_lineCounted = false;
                Token t(Tok_eol, d_lineNr, d_colNr );
                t.d_sourcePath = d_filePath;
                return t;
            }
            return token( Tok_Eof, 0 );
        }
        const bool sendEol = d_lineCounted;
        const int line = d_lineNr;
        const int col = d_colNr;
        nextLine();
        if( sendEol )
        {
            Token t(Tok_eol, line, col );
            t.d_sourcePath = d_filePath;
            d_lastToken = t;
            return t;
        }
        skipWhiteSpace();
    }
    Q_ASSERT( d_colNr < d_line.size() );
    while( d_colNr < d_line.size() )
    {
        const char ch = quint8(d_line[d_colNr]);

        if( ch == '`' )
        {
            // ignore stray '`' e.g. in libfp-saneasm
            d_colNr++;
            continue;
        }
        if( ch == '\'' )
            return string1();
        else if( ch == '"' )
            return string2();
        else if( ch == '$')
            return hexnumber();
        else if( ch == '.' )
            return ident(true);
        else if( ch == '%' )
        {
            const char ch2 = lookAhead();
            if( ::isdigit(ch2) )
                // NOTE: this is only seen in the highlighter; the parser only sees the instantiated macro;
                // e.g. in libfp-x80arith.asm we see constructs like "#$%1", or in
                // libfp-real.asm "add" is used as a macro argument.
                // the original assembler apparently substitutes on character level, not token level.
                return token( Tok_substitute, 2, d_line.mid(d_colNr,2) );
            else
                return ident(); // apparently idents can start with % too
        }
        else if( ch == '@' )
            return label();
        else if( ::isalpha(ch) || ch == '_' ) // idents can apparently start with underscore too
            return ident();
        else if( ::isdigit(ch) )
            return number();
        // else
        int pos = d_colNr;
        TokenType tt = tokenTypeFromString(d_line,&pos);

        if( tt == Tok_Semi )
        {
            const int len = d_line.size() - d_colNr;
            return token( Tok_Comment, len, d_line.mid(d_colNr,len) );
        }else if( tt == Tok_Invalid || pos == d_colNr )
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
    while( d_colNr < d_line.size() && ( ::isspace( d_line[d_colNr] ) || d_line[d_colNr] == char(0xff) ) )
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

Token Lexer::token(TokenType tt, int len, const QByteArray& val, bool dotPrefix)
{
    if( tt != Tok_Invalid && tt != Tok_Comment && tt != Tok_Eof )
        countLine();
    Token t( tt, d_lineNr, d_colNr + 1, val );
    d_lastToken = t;
    d_colNr += len;
    t.d_sourcePath = d_filePath;
    t.d_dotPrefix = dotPrefix;
    return t;
}

Token Lexer::ident(bool dotPrefix)
{
    int off = 1;
    while( true )
    {
        const char c = lookAhead(off);
        if( dotPrefix && !::isalnum(c) )
            break;
        else if( !::isalnum(c) && c != '_' && c != '%' && c != '.' )
            break;
        else
            off++;
    }
    QByteArray str = d_line.mid(d_colNr, off );
    if( dotPrefix )
    {
        str = str.mid(1);
        if( str.size() == 1 )
        {
            const char ch = str[0];
            if( ch == 'W' || ch == 'w' )
                return token( Tok_dotW, 2, d_line.mid(d_colNr,2), true );
            else if( ch == 'B' || ch == 'b' )
                return token( Tok_dotB, 2, d_line.mid(d_colNr,2), true );
            else if( ch == 'L' || ch == 'l' )
                return token( Tok_dotL, 2, d_line.mid(d_colNr,2), true );
            else if( ch == 'S' || ch == 's' )
                return token( Tok_dotS, 2, d_line.mid(d_colNr,2), true );
        }
    }else if( str.size() >= 2 && str[str.size()-2] == '.' )
    {
        const char suffix = str[str.size()-1];
        if( suffix == 'W' || suffix == 'w' || suffix == 'L' || suffix == 'l'
                || suffix == 'B' || suffix == 'b' || suffix == 'S' || suffix == 's' )
        {
            off -= 2;
            str.chop(2);
        }
    }
    if( dotPrefix && str.isEmpty() )
        return token( Tok_Invalid, 1, "unexpected character '.'" );

    Q_ASSERT( !str.isEmpty() );
    int pos = 0;
    const QByteArray keyword = str.toUpper();
    TokenType t = tokenTypeFromString( keyword, &pos );
    if( t != Tok_Invalid && pos != str.size() )
        t = Tok_Invalid;
    if( t != Tok_Invalid )
    {
        const bool isDirective = Token::isDirective(t);
        if( !isDirective || ( dotPrefix && isDirective ) || ( !dotPrefix && t == Tok_EQU ) )
        {
            Token res = token( t, off, str, dotPrefix );
            if( isDirective && t == Tok_MACRO )
                readMacro();
            return res;
        }
    }
    // else
    if( !findMacro( str ).isEmpty() )
        return token( Tok_Comment, d_line.size() - d_colNr, str ); // just eat it
    else
        return token( Tok_ident, off, str, dotPrefix );
}

static inline bool isHexDigit( char c )
{
    return ::isdigit(c) || c == 'A' || c == 'B' || c == 'C' || c == 'D' || c == 'E' || c == 'F'
            || c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' || c == 'f';
}

static inline bool checkOctalNumber( const QByteArray& str )
{
    for( int i = 0; i < str.size() - 1; i++ )
        if( !( str[i] >= '0' && str[i] <= '7' ) )
            return false;
    return true;
}

static inline bool checkBinaryNumber( const QByteArray& str )
{
    for( int i = 0; i < str.size() - 1; i++ )
        if( !( str[i] >= '0' && str[i] <= '1' ) )
            return false;
    return true;
}

static inline bool checkDecimalNumber( const QByteArray& str )
{
    for( int i = 0; i < str.size() - 1; i++ )
        if( !( str[i] >= '0' && str[i] <= '9' ) )
            return false;
    return true;
}

Token Lexer::number()
{
    int off = 1;
    while( true )
    {
        const char c = lookAhead(off);
        if( !isHexDigit(c) )
            break;
        else
            off++;
    }
    bool isHex = false;
    bool isOctal = false;
    bool isBinary = false;
    const char o1 = lookAhead(off);
    if( o1 == 'O' || o1 == 'o' )
    {
        isOctal = true;
        off++;
    }else if( o1 == 'B' || o1 == 'b' )
    {
        isBinary = true;
        off++;
    }else if( o1 == 'H' || o1 == 'h' )
    {
        isHex = true;
        off++;
    }
    const QByteArray str = d_line.mid(d_colNr, off );
    Q_ASSERT( !str.isEmpty() );
    if( isOctal && !checkOctalNumber(str) )
        return token( Tok_Invalid, off, "invalid octal integer" );
    else if( isBinary && !checkBinaryNumber(str) )
        return token( Tok_Invalid, off, "invalid binary integer" );
    else if( !isHex && !checkDecimalNumber(str) )
        return token( Tok_Invalid, off, "invalid decimal integer" );
    return token( Tok_number, off, str );
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
    return token( Tok_number, off, str );
}

Token Lexer::string1()
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
    return token( Tok_string, off, str );
}

Token Lexer::string2()
{
    int off = 1;
    while( true )
    {
        const char c = lookAhead(off);
        off++;
        if( c == '"')
        {
            if( lookAhead(off) == '"')
                off++;
            else
             break;
        }
        if( c == 0 )
            return token( Tok_Invalid, off, "non-terminated string" );
    }
    const QByteArray str = d_line.mid(d_colNr, off );
    return token( Tok_string, off, str );
}

Token Lexer::label()
{
    int off = 1;
    while( true )
    {
        const char c = lookAhead(off);
        if( !::isdigit(c) )
            break;
        else
            off++;
    }
    const QByteArray str = d_line.mid(d_colNr, off );
    return token( Tok_label, off, str );
}

void Lexer::countLine()
{
    if( !d_lineCounted )
        d_sloc++;
    d_lineCounted = true;
}

void Lexer::readMacro()
{
    skipWhiteSpace();
    Token name = ident(false);
    QByteArray body;
    while( !d_in->atEnd() )
    {
        const int pos = d_line.indexOf('.',d_colNr);
        if( pos == -1 )
        {
            body += d_line.mid(d_colNr).trimmed();
            nextLine();
            body += '\n';
        }else
        {
            const QByteArray keyword = d_line.mid(pos+1, 4).toUpper();
            if( keyword == "ENDM" )
            {
                body += d_line.mid(d_colNr, pos - d_colNr).trimmed();
                d_colNr = pos + 4;
                countLine();
                break;
            }else
            {
                body += d_line.mid(d_colNr, pos - d_colNr + 1).trimmed();
                d_colNr = pos + 1;
            }
        }
    }
    //qDebug() << "MACRO" << name.d_val << d_filePath << name.d_lineNr << body;
    d_macro.append( qMakePair(name.d_val.toLower(),body) );
}

QByteArray Lexer::findMacro(const QByteArray& name) const
{
    QByteArray lc = name.toLower();
    for(int i = 0; i < d_macro.size(); i++ ) // TODO more efficient
        if( d_macro[i].first == lc )
            return d_macro[i].second;
    return QByteArray();
}
