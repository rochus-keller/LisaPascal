#ifndef FILESYSTEM_H
#define FILESYSTEM_H

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

#include <QHash>
#include <QObject>

class QIODevice;

namespace Lisa
{
class FileSystem : public QObject
{
public:
    struct File;

    struct Dir
    {
        QList<Dir*> d_subdirs;
        QList<File*> d_files;
        QString d_name;
        Dir* d_dir;

        void clear();
        void dump(int level = 0) const;
        Dir* subdir(const QString& name) const;
        const File* file(const QString& name) const;
        const File* module(const QByteArray& nameLc) const;
        Dir() {}
        ~Dir() { clear(); }
    };

    enum FileType { UnknownFile, PascalProgram, PascalUnit, PascalFragment };
    struct File
    {
        quint8 d_type;
        bool d_doublette;
        bool d_forceParse;
        bool d_parsed;
        QString d_realPath;
        QString d_name; // fileName
        QString d_moduleName;
        QByteArray d_moduleLc; // lower-case version
        Dir* d_dir;
        QString getVirtualPath(bool suffix = true) const;
        int level() const;

        File():d_doublette(false),d_type(UnknownFile),d_dir(0),d_forceParse(false),d_parsed(false){}
    };

    explicit FileSystem(QObject *parent = 0);
    bool load( const QString& rootDir );
    const QString& getError() const { return d_error; }
    const Dir& getRoot() const { return d_root; }
    const QString& getRootPath() const { return d_rootDir; }
    QList<const File*> getAllPas() const;
    const File* findFile(const QString& realPath) const;
    const File* findFile(const Dir* startFrom, const QString& dir, const QString& name) const;
    const File* findModule(const Dir* startFrom, const QByteArray& nameLc) const;

    static FileType detectType(QIODevice* in, QByteArray* = 0);
protected:
    bool error( const QString& );
    Dir* getDir( const QString& relPath );

private:
    QString d_rootDir;
    QString d_error;
    Dir d_root;
    QHash<QString,File*> d_fileMap;
    QHash<QByteArray,File*> d_moduleMap; // module to File* is ambig, but besides "prmgr" (nearly) identical
};
}

#endif // FILESYSTEM_H
