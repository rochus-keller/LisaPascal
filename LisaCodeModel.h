#ifndef LISACODEMODEL_H
#define LISACODEMODEL_H

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

#include <QAbstractItemModel>
#include <QHash>
#include <LisaFileSystem.h>
#include "LisaRowCol.h"

namespace Lisa
{
class Scope;
class UnitFile;
class Symbol;

class Thing
{
public:
    enum Type { Undefined,
       /* Declaration: */ Const, Type, Var, Func, Proc, Param, Label, Field, Module,
       /* Scope: */ Interface, Implementation, Body,
       /* UnitFile: */ Unit,
       /* IncludeFile: */ Include,
       /* CodeFolder: */ Folder
    };
    quint8 d_type;
    bool d_external; // Fund & Proc decls

    virtual FilePos getLoc() const { return FilePos(); }
    virtual quint16 getLen() const { return 0; }
    virtual QString getName() const;
    virtual const FileSystem::File* getFile() const { return 0; }
    bool isDeclaration() const { return d_type >= Const && d_type <= Module; }
    const char* typeName() const;
    Thing():d_type(Undefined),d_external(false){}
    virtual ~Thing();
};

class Declaration : public Thing
{
public:
    Scope* d_body; // owns
    QByteArray d_name;
    const char* d_id; // same as in Token

    FilePos d_loc; // place in unit or any include file where the decl is actually located
    Scope* d_owner;

    typedef QList<Symbol*> SymList;
    typedef QHash<QString,SymList> Refs;
    Refs d_refs; // file path -> Symbols in it
    Symbol* d_me; // this is the symbol by which the decl itself is represented in the file

    FilePos getLoc() const { return d_loc; }
    quint16 getLen() const { return d_name.size(); }
    QString getName() const;

    UnitFile* getUnitFile() const; // only for ownership, not for actual file position
    Declaration():d_body(0),d_owner(0),d_me(0),d_id(0){}
    ~Declaration();
};

class Scope : public Thing
{
public:
    QList<Declaration*> d_order; // owns
    Thing* d_owner; // either declaration or unit file
    Scope* d_outer;
    Scope* d_altOuter; // to access params defined in interface declaration of func/proc

    UnitFile* getUnitFile() const;
    Declaration* findDecl(const char* id, bool withImports = true) const;
    void clear();
    Scope():d_owner(0),d_outer(0),d_altOuter(0){}
    ~Scope();
};

class Symbol
{
public:
    Thing* d_decl;
    RowCol d_loc; // the position of the symbol in the file (declaration has other position, but same length)
    Symbol():d_decl(0){}
};

class IncludeFile;
class UnitFile;

class CodeFile : public Thing
{
public:
    const FileSystem::File* d_file;

    UnitFile* toUnit();
    IncludeFile* toInclude();

    const FileSystem::File* getFile() const { return d_file; }
    CodeFile():d_file(0) {}
    ~CodeFile();
};

class IncludeFile : public CodeFile
{
public:
    UnitFile* d_unit;
    quint16 d_len; // just to make the symbol of the include directive happy

    FilePos getLoc() const; // the landing place when we jump to this file
    quint16 getLen() const { return d_len; }
    QString getName() const;
    IncludeFile():d_unit(0),d_len(0){ d_type = Include; }
};

class UnitFile : public CodeFile
{
public:
    Scope* d_intf; // owns, 0 for Program
    Scope* d_impl; // owns
    Scope* d_globals;
    QList<UnitFile*> d_import;
    typedef QList<Symbol*> SymList;
    QHash<QString,SymList> d_syms; // owns, all things we can click on in a code file ordered by row/col
    QList<IncludeFile*> d_includes; // owns

    QString getName() const;
    QByteArrayList findUses() const;
    UnitFile():d_intf(0),d_impl(0),d_globals(0) { d_type = Unit; }
    ~UnitFile();
};

class CodeFolder : public Thing
{
public:
    FileSystem::Dir* d_dir;
    QList<CodeFolder*> d_subs; // owns
    QList<UnitFile*> d_files; // owns

    QString getName() const;
    void clear();
    CodeFolder():d_dir(0){ d_type = Folder; }
    ~CodeFolder() { clear(); }
};

class CodeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit CodeModel(QObject *parent = 0);

    bool load( const QString& rootDir );
    const Thing* getThing(const QModelIndex& index) const;
    QModelIndex findThing(const Thing* nt) const;
    Symbol* findSymbolBySourcePos(const QString& path, int line, int col) const;
    FileSystem* getFs() const { return d_fs; }
    quint32 getSloc() const { return d_sloc; }
    CodeFile* getCodeFile(const QString& path) const;
    UnitFile* getUnitFile(const QString& path) const;
    Scope* getGlobals() { return &d_globals; }

    // overrides
    int columnCount ( const QModelIndex & parent = QModelIndex() ) const { return 1; }
    QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
    QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
    QModelIndex parent ( const QModelIndex & index ) const;
    int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
    Qt::ItemFlags flags ( const QModelIndex & index ) const;

protected:
    void parseAndResolve(UnitFile*);

private:
    struct Slot
    {
        Thing* d_thing;
        QList<Slot*> d_children;
        Slot* d_parent;
        Slot(Slot* p = 0, Thing* t = 0):d_parent(p),d_thing(t){ if( p ) p->d_children.append(this); }
        ~Slot() { foreach( Slot* s, d_children ) delete s; }
    };
    static bool lessThan( const Slot* lhs, const Slot* rhs);
    void fillFolders(Slot* root, const FileSystem::Dir* super, CodeFolder* top, QList<Slot*>& fileSlots);
    QModelIndex findThing(const Slot* slot,const Thing* nt) const;
    Slot d_root;
    FileSystem* d_fs;
    CodeFolder d_top;
    Scope d_globals;
    QHash<const FileSystem::File*,UnitFile*> d_map1;
    QHash<QString,CodeFile*> d_map2; // real path -> file
    quint32 d_sloc; // number of lines of code without empty or comment lines
};
}

#endif // LISACODEMODEL_H
