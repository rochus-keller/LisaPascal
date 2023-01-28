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

#include "LisaCodeModel.h"
#include "PpLexer.h"
#include "LisaParser.h"
#include <QPixmap>
#include <QtDebug>
using namespace Lisa;

CodeModel::CodeModel(QObject *parent) : QAbstractItemModel(parent),d_sloc(0)
{
    d_fs = new FileSystem(this);
}

bool CodeModel::load(const QString& rootDir)
{
    beginResetModel();
    d_root = Slot();
    d_top.clear();
    d_map.clear();
    d_sloc = 0;
    d_fs->load(rootDir);
    QList<Slot*> fileSlots;
    fillFolders(&d_root,&d_fs->getRoot(), &d_top, fileSlots);
    QList<const FileSystem::File*> files = d_fs->getAllPas();
    fillTop();
    foreach( const FileSystem::File* f, files )
        parseAndResolve(f);
    endResetModel();
    return true;
}

const Thing* CodeModel::getThing(const QModelIndex& index) const
{
    if( !index.isValid() )
        return 0;
    Slot* s = static_cast<Slot*>( index.internalPointer() );
    Q_ASSERT( s != 0 );
    return s->d_thing;
}

Symbol*CodeModel::findSymbolBySourcePos(const QString& path, int line, int col) const
{
    return 0; // TODO
}

QVariant CodeModel::data(const QModelIndex& index, int role) const
{
    Slot* s = static_cast<Slot*>( index.internalPointer() );
    Q_ASSERT( s != 0 );
    switch( role )
    {
    case Qt::DisplayRole:
        switch( s->d_thing->d_type )
        {
        case Thing::File:
            return static_cast<CodeFile*>(s->d_thing)->d_file->d_name;
        case Thing::Folder:
            return static_cast<CodeFolder*>(s->d_thing)->d_dir->d_name;
        }
        break;
    case Qt::DecorationRole:
        switch( s->d_thing->d_type )
        {
        case Thing::File:
            return QPixmap(":/images/unit.png");
        case Thing::Folder:
            return QPixmap(":/images/folder.png");
        }
        break;
    case Qt::FontRole:
        break;
    case Qt::ForegroundRole:
        break;
    }
    return QVariant();
}

QModelIndex CodeModel::index(int row, int column, const QModelIndex& parent) const
{
    const Slot* s = &d_root;
    if( parent.isValid() )
    {
        s = static_cast<Slot*>( parent.internalPointer() );
        Q_ASSERT( s != 0 );
    }
    if( row < s->d_children.size() && column < columnCount( parent ) )
        return createIndex( row, column, s->d_children[row] );
    else
        return QModelIndex();
}

QModelIndex CodeModel::parent(const QModelIndex& index) const
{
    if( index.isValid() )
    {
        Slot* s = static_cast<Slot*>( index.internalPointer() );
        Q_ASSERT( s != 0 );
        if( s->d_parent == &d_root )
            return QModelIndex();
        // else
        Q_ASSERT( s->d_parent != 0 );
        Q_ASSERT( s->d_parent->d_parent != 0 );
        return createIndex( s->d_parent->d_parent->d_children.indexOf( s->d_parent ), 0, s->d_parent );
    }else
        return QModelIndex();
}

int CodeModel::rowCount(const QModelIndex& parent) const
{
    if( parent.isValid() )
    {
        Slot* s = static_cast<Slot*>( parent.internalPointer() );
        Q_ASSERT( s != 0 );
        return s->d_children.size();
    }else
        return d_root.d_children.size();
}

Qt::ItemFlags CodeModel::flags(const QModelIndex& index) const
{
    Q_UNUSED(index)
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable; //  | Qt::ItemIsDragEnabled;
}

void CodeModel::parseAndResolve(const FileSystem::File* file)
{
    const QString path = file->getVirtualPath();
    if( d_map.contains(path) )
        return;

    PpLexer lex(d_fs);
    lex.reset(file->d_realPath);
    Parser p(&lex);
    p.RunParser();
    const int off = d_fs->getRootPath().size();
    if( !p.errors.isEmpty() )
    {
        foreach( const Parser::Error& e, p.errors )
        {
            const FileSystem::File* f = d_fs->findFile(e.path);
            const QString line = tr("%1:%2:%3: %4").arg( f ? f->getVirtualPath() : e.path.mid(off) ).arg(e.row)
                    .arg(e.col).arg(e.msg);
            qCritical() << line.toUtf8().constData();
        }

    }
    d_sloc += lex.getSloc();

    // TODO resolve syntree and import uses
}

void CodeModel::fillTop()
{

}

void CodeModel::fillFolders(CodeModel::Slot* root, const FileSystem::Dir* super, CodeFolder* top, QList<CodeModel::Slot*>& fileSlots)
{
    for( int i = 0; i < super->d_subdirs.size(); i++ )
    {
        CodeFolder* f = new CodeFolder();
        f->d_dir = super->d_subdirs[i];
        top->d_subs.append(f);
        Slot* s = new Slot(root,f);
        fillFolders(s,super->d_subdirs[i],f,fileSlots);
    }
    for( int i = 0; i < super->d_files.size(); i++ )
    {
        if( super->d_files[i]->d_type == FileSystem::PascalProgram ||
                super->d_files[i]->d_type == FileSystem::PascalUnit )
        {
            CodeFile* f = new CodeFile();
            f->d_file = super->d_files[i];
            top->d_files.append(f);
            Slot* s = new Slot(root,f);
            fileSlots.append(s);
        }
    }
}

CodeFile::~CodeFile()
{
    if( d_impl )
        delete d_impl;
    if( d_intf )
        delete d_intf;
    for( int i = 0; i < d_syms.size(); i++ )
        delete d_syms[i];
}

CodeFile*Scope::getCodeFile() const
{
    Q_ASSERT( d_owner != 0 );
    if( d_owner->d_type == Thing::File )
        return static_cast<CodeFile*>(d_owner);
    else if( d_owner->isDeclaration() )
    {
        Declaration* d = static_cast<Declaration*>(d_owner);
        Q_ASSERT( d->d_owner != 0 );
        return d->d_owner->getCodeFile();
    }else
        Q_ASSERT(false);
}

Scope::~Scope()
{
    for( int i = 0; i < d_order.size(); i++ )
        delete d_order[i];
}

CodeFile*Declaration::getCodeFile() const
{
    Q_ASSERT( d_owner != 0 );
    return d_owner->getCodeFile();
}

Declaration::~Declaration()
{
    if( d_body )
        delete d_body;
}

void CodeFolder::clear()
{
    for( int i = 0; i < d_subs.size(); i++ )
        d_subs[i]->clear();
    d_subs.clear();
    for( int i = 0; i < d_files.size(); i++ )
        delete d_files[i];
    d_files.clear();
}
