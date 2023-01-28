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
#include "Converter.h"
#include "LisaLexer.h"
#include <QFile>
#include <QtDebug>
using namespace Lisa;

FileSystem::FileSystem(QObject *parent) : QObject(parent)
{

}

static QString oneUp(const QString& path)
{
    QStringList segs = path.split('/');
    if( segs.size() > 2 )
    {
        segs.pop_back();
        return segs.join('/');
    }else
        return path;
}

static QString replaceLast(const QString& path, const QString& newLast )
{
    QStringList segs = path.split('/');
    segs.pop_back();
    segs.push_back(newLast);
    return segs.join('/');
}

static QStringList collectFiles( const QDir& dir, const QStringList& suffix )
{
    QStringList res;
    QStringList files = dir.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );

    foreach( const QString& f, files )
        res += collectFiles( QDir( dir.absoluteFilePath(f) ), suffix );

    files = dir.entryList( suffix, QDir::Files, QDir::Name );
    foreach( const QString& f, files )
    {
        res.append(dir.absoluteFilePath(f));
    }
    return res;
}

bool FileSystem::load(const QString& rootDir)
{
    if( !QFileInfo(rootDir).isDir() )
        return error("not a directory");

    d_root.clear();
    d_map.clear();

    const QStringList files = collectFiles(rootDir,QStringList() << "*.txt");
    const int off = rootDir.size();

    foreach( const QString& f, files )
    {
        QFile in(f);
        if( !in.open(QIODevice::ReadOnly) )
            return error(tr("cannot open file for reading: %1").arg(f));
        const FileType fileType = detectType(&in);
        if( fileType == UnknownFile )
            continue;
        in.close();

        QFileInfo info(f);

        const QString relDirPath = info.absolutePath().mid(off+1);
        const QString fileName = info.fileName().toLower();

        QString name;
        if( fileName.endsWith("text.unix.txt") )
        {
            name = fileName;
            name.chop(14); // including dot
        }else
            name = info.baseName();

        QStringList parts = name.contains('-') ? name.split('-') : name.split('.');

        if( parts.size() == 1 )
        {
            // file with no dir
            Dir* dir = getDir(relDirPath);
            File* file = new File();
            file->d_type = fileType;
            file->d_name = name;
            file->d_realPath = f;
            file->d_dir = dir;
            dir->d_files.append(file);
            d_map[f] = file;
        }else if( parts.size() >= 2 )
        {
            Dir* dir = getDir(replaceLast(relDirPath,parts.front()));
            parts.pop_front();
            const QString newName = parts.join('_');
            File* file = new File();
            file->d_type = fileType;
            file->d_name = newName;
            file->d_realPath = f;
            file->d_dir = dir;
            dir->d_files.append(file);
            d_map[f] = file;
        }
    }
}

static void walkForPas(const FileSystem::Dir* d, QList<const FileSystem::File*>& res )
{
    for( int i = 0; i < d->d_subdirs.size(); i++ )
        walkForPas( d->d_subdirs[i], res );
    for( int i = 0; i < d->d_files.size(); i++ )
        if( d->d_files[i]->d_type == FileSystem::PascalProgram || d->d_files[i]->d_type == FileSystem::PascalUnit )
            res.append(d->d_files[i]);
}

QList<const FileSystem::File*> FileSystem::getAllPas() const
{
    QList<const FileSystem::File*> res;
    walkForPas(&d_root,res);
    return res;
}

const FileSystem::File*FileSystem::findFile(const QString& realPath) const
{
    return d_map.value(realPath);
}

const FileSystem::File*FileSystem::findFile(const Dir* startFrom, const QString& dir, const QString& name) const
{
    Q_ASSERT(startFrom);
    const File* res = 0;
    const Dir* d = startFrom;
    if( res == 0 && d )
    {
        if( d->d_name == dir )
            res = d->file(name);
        d = d->d_dir;
    }
    return res;
}

FileSystem::FileType FileSystem::detectType(QIODevice* in)
{
    Q_ASSERT(in);
    in->reset();
    Lexer lex;
    lex.setStream(in);
    lex.setIgnoreComments(false);
    Token t = lex.nextToken();
    FileType res = UnknownFile;
    while( t.isValid() )
    {
        switch(t.d_type)
        {
        case Tok_Comment:
            if( t.d_val.startsWith("(*") || t.d_val.startsWith("{$") )
                res = PascalFragment;
            break;
        case Tok_program:
            return PascalProgram;
        case Tok_unit:
            return PascalUnit;
        case Tok_function:
        case Tok_procedure:
        case Tok_const:
        case Tok_methods:
            // exceptions: aplw-compflags
            return PascalFragment;
        default:
            return res;
        }

        t = lex.nextToken();
    }
    return res;
}

bool FileSystem::error(const QString& msg)
{
    d_error = msg;
    return false;
}

FileSystem::Dir*FileSystem::getDir(const QString& relPath)
{
    const QStringList segs = relPath.split('/');
    Dir* cur = &d_root;
    Dir* res = 0;
    for( int i = 0; i < segs.size(); i++ )
    {
        res = cur->subdir(segs[i]);
        if( res == 0 )
        {
            res = new Dir();
            res->d_name = segs[i];
            cur->d_subdirs.append(res);
            res->d_dir = cur;
        }
        cur = res;
    }
    return res;
}

void FileSystem::Dir::clear()
{
    for( int i = 0; i < d_subdirs.size(); i++ )
        d_subdirs[i]->clear();
    d_subdirs.clear();
    for( int i = 0; i < d_files.size(); i++ )
        delete d_files[i];
    d_files.clear();
}

static const char* typeName(int t)
{
    switch( t )
    {
    case FileSystem::PascalProgram:
    case FileSystem::PascalUnit:
        return ".pas";
    case FileSystem::PascalFragment:
        return ".inc";
    default:
        return "";
    }
}

void FileSystem::Dir::dump(int level) const
{
    QTextStream out(stdout);
    QByteArray indent;
    for( int i = 0; i < level; i++ )
        indent += "|  ";
    out << indent.constData() << ( d_name.isEmpty() ? "<root>" : d_name ) << endl;
    for( int i = 0; i < d_files.size(); i++ )
        out << indent.constData() << "|  " << d_files[i]->d_name << typeName(d_files[i]->d_type) << endl;
    for( int i = 0; i < d_subdirs.size(); i++ )
        d_subdirs[i]->dump(level+1);
}

FileSystem::Dir*FileSystem::Dir::subdir(const QString& name) const
{
    for(int i = 0; i < d_subdirs.size(); i++ )
        if( d_subdirs[i]->d_name == name )
            return d_subdirs[i];
    return 0;
}

const FileSystem::File*FileSystem::Dir::file(const QString& name) const
{
    for(int i = 0; i < d_files.size(); i++ )
        if( d_files[i]->d_name == name )
            return d_files[i];
    return 0;
}

QString FileSystem::File::getVirtualPath() const
{
    const Dir* d = d_dir;
    QString res;
    while( d && !d->d_name.isEmpty() )
    {
        res = d->d_name + "/" + res;
        d = d->d_dir;
    }
    res += d_name;
    res += typeName(d_type);
    return res;
}
