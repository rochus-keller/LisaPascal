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
#include "LisaPpLexer.h"
#include "LisaParser.h" 
#include "AsmPpLexer.h"
#include "AsmParser.h"
#include <QFile>
#include <QPixmap>
#include <QtDebug>
#include <QCoreApplication>
using namespace Lisa;

#define LISA_WITH_MISSING

class PascalModelVisitor
{
    CodeModel* d_mdl;
    UnitFile* d_cf;
    QHash<Declaration*,Declaration*> d_redirect;
    struct Deferred
    {
        Symbol* sym;
        Type::Ref pointer;
        Scope* scope;
        SynTree* typeIdent;
    };
    QList<Deferred> d_deferred;
    QList<Declaration*> d_forwards;

public:
    PascalModelVisitor(CodeModel* m):d_mdl(m) {}

    void visit( UnitFile* cf, SynTree* top )
    {      
        d_cf = cf;
        cf->d_globals = d_mdl->getGlobals();
        if( top->d_children.isEmpty() )
            return;
        switch(top->d_children.first()->d_tok.d_type)
        {
        case SynTree::R_program_:
            program(cf,top->d_children.first());
            break;
        case SynTree::R_regular_unit:
            regular_unit(cf,top->d_children.first());
            break;
        case SynTree::R_non_regular_unit:
            qWarning() << "SynTree::R_non_regular_unit should no longer happen since we have include";
            break;
        }
    }
private:
    void program( UnitFile* cf, SynTree* st )
    {
        Scope* s = new Scope();
        s->d_owner = cf;
        s->d_kind = Thing::Body;
        cf->d_impl = s;
        for( int i = 0; i < st->d_children.size(); i++ )
        {
            switch(st->d_children[i]->d_tok.d_type)
            {
            case SynTree::R_block:
                block(cf->d_impl,st->d_children[i]);
                break;
            case SynTree::R_statement_part:
                statement_part(cf->d_impl,st->d_children[i]);
                break;
            case SynTree::R_program_heading:
                module_heading(cf,st->d_children[i]);
                break;
            case SynTree::R_uses_clause:
                uses_clause(cf,st->d_children[i]);
                break;
            }
        }
    }
    void module_heading(UnitFile* cf, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == Tok_identifier)
                addDecl(cf->d_globals,s->d_tok,Thing::Module);
    }
    void uses_clause(UnitFile* cf, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_identifier_list2)
                identifier_list2(cf,s);
    }
    void identifier_list2(UnitFile* cf, SynTree* st)
    {
        int i = 0;
        QList<SynTree*> idents;
        while( i < st->d_children.size())
        {
            SynTree* id = st->d_children[i];
            if( id->d_tok.d_type == Tok_identifier )
                idents.push_back(id);
            else if( id->d_tok.d_type == Tok_Slash )
                idents.pop_back();
            i++;
        }
        foreach(SynTree* s, idents)
        {
            addSym(cf->d_globals,s->d_tok);
        }
    }

    void regular_unit( UnitFile* cf, SynTree* st )
    {
        Scope* interf = 0;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_unit_heading)
                module_heading(cf,s);
            if( s->d_tok.d_type == SynTree::R_interface_part)
                interf = interface_part(cf,s);
            if( s->d_tok.d_type == SynTree::R_implementation_part)
                implementation_part(cf,interf,s);
        }
    }
    Scope* interface_part( UnitFile* cf, SynTree* st )
    {
        Scope* newScope = new Scope();
        newScope->d_owner = cf;
        newScope->d_kind = Thing::Interface;
        cf->d_intf = newScope;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_uses_clause)
                uses_clause(cf,s);
            if( s->d_tok.d_type == SynTree::R_constant_declaration_part)
                constant_declaration_part(newScope,s);
            if( s->d_tok.d_type == SynTree::R_type_declaration_part)
                type_declaration_part(newScope,s);
            if( s->d_tok.d_type == SynTree::R_variable_declaration_part)
                variable_declaration_part(newScope,s);
            if( s->d_tok.d_type == SynTree::R_procedure_and_function_interface_part)
                procedure_and_function_interface_part(newScope,s);
        }
        return newScope;
    }
    void procedure_and_function_interface_part(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_procedure_heading)
                func_proc_heading(scope,s,Thing::Proc);
            if( s->d_tok.d_type == SynTree::R_function_heading)
                func_proc_heading(scope,s, Thing::Func);
        }
    }
    void implementation_part( UnitFile* cf, Scope* intf, SynTree* st )
    {
        Scope* newScope = new Scope();
        newScope->d_owner = cf;
        newScope->d_kind = Thing::Implementation;
        newScope->d_outer = intf;
        cf->d_impl = newScope;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_constant_declaration_part)
                constant_declaration_part(newScope,s);
            if( s->d_tok.d_type == SynTree::R_type_declaration_part)
                type_declaration_part(newScope,s);
            if( s->d_tok.d_type == SynTree::R_variable_declaration_part)
                variable_declaration_part(newScope,s);
            if( s->d_tok.d_type == SynTree::R_subroutine_part)
                subroutine_part(newScope,s);
        }
    }
    void subroutine_part(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_procedure_declaration)
                func_proc_declaration(scope,s, Thing::Proc);
            if( s->d_tok.d_type == SynTree::R_function_declaration)
                func_proc_declaration(scope,s, Thing::Func);
            if( s->d_tok.d_type == SynTree::R_method_block)
                method_block(scope,s);
        }
    }
    void method_block(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == Tok_identifier)
            {
                Declaration* cls = scope->findDecl(s->d_tok.d_id);
                if( cls->d_type && cls->d_type->d_kind == Type::Class )
                {
                    Declaration* mb = addDecl(scope,s->d_tok,Thing::MethBlock, cls);

                    mb->d_body = new Scope();
                    mb->d_body->d_kind = Thing::Members;
                    mb->d_body->d_outer = cls->d_type->d_members;
                    mb->d_body->d_altOuter = scope;

                    scope = mb->d_body;

                    Token tmp;
                    tmp.d_val = "SELF";
                    // intenionally no loc or path in tmp
                    tmp.d_id = Token::toId(tmp.d_val);
                    Declaration* self = addDecl(scope,tmp,Thing::Self);
                    self->d_type = cls->d_type;
                    if( cls->d_type->d_type )
                    {
                        tmp.d_val = "SUPERSELF";
                        tmp.d_id = Token::toId(tmp.d_val);
                        Declaration* self = addDecl(scope,tmp,Thing::Self);
                        self->d_type = cls->d_type;
                    }
                }
            }
            if( s->d_tok.d_type == SynTree::R_procedure_and_function_declaration_part)
                procedure_and_function_declaration_part(scope,s);
            if( s->d_tok.d_type == SynTree::R_statement_part)
                statement_part(scope,s);
        }
    }
    void block( Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_label_declaration_part)
                label_declaration_part(scope,s);
            if( s->d_tok.d_type == SynTree::R_constant_declaration_part)
                constant_declaration_part(scope,s);
            if( s->d_tok.d_type == SynTree::R_type_declaration_part)
                type_declaration_part(scope,s);
            if( s->d_tok.d_type == SynTree::R_variable_declaration_part)
                variable_declaration_part(scope,s);
            if( s->d_tok.d_type == SynTree::R_procedure_and_function_declaration_part)
                procedure_and_function_declaration_part(scope,s);
        }
    }
    void label_declaration_part( Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_label_)
                label_(scope,s);
    }
    void label_( Scope* scope, SynTree* st)
    {
        // NOP
    }
    void constant_declaration_part( Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_constant_declaration)
                constant_declaration(scope,s);
    }
    void constant_declaration( Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == Tok_identifier)
                addDecl(scope, s->d_tok, Thing::Const);
            if( s->d_tok.d_type == SynTree::R_expression)
                expression(scope,s);
        }
    }
    void type_declaration_part( Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_type_declaration)
                type_declaration(scope,s);
        for( int i = 0; i < d_deferred.size(); i++ )
        {
            const Deferred& def = d_deferred[i];
            if( def.sym == 0 )
                def.pointer->d_type = resolvedType(type_identifier(def.scope,def.typeIdent));
#ifdef LISA_WITH_MISSING
            else if( def.sym->d_decl == 0 && !def.typeIdent->d_children.isEmpty())
            {
                Token t = def.typeIdent->d_children.first()->d_tok;
                Declaration* d = scope->findDecl(t.d_id);
                if( d && d_cf->d_kind == Thing::Unit )
                {
                    QHash<Declaration*,Declaration*>::const_iterator intf = d_redirect.find(d);
                    if( intf != d_redirect.end() )
                        d = intf.value(); // attach all refs to the interface declaration (otherwise they are not visible)
                }
                def.sym->d_decl = d;
                def.pointer->d_type = resolvedType(def.sym);
                if( d )
                    d->d_refs[t.d_sourcePath].append(def.sym);
            }
#endif
        }
        d_deferred.clear();
    }
    Declaration* findInForwards(const Token&t)
    {
        Declaration* res = 0;
        foreach( Declaration* d, d_forwards )
        {
            if( d->d_id == t.d_id )
            {
                res = d;
                break;
            }
        }
        return res;
    }
    Declaration* findInMembers(Scope* scope, const Token&t)
    {
        Declaration* res = 0;
        // if this is in a class declaration search there
        foreach( Declaration* d, scope->d_order )
        {
            if( d->d_id == t.d_id )
            {
                res = d;
                break;
            }
        }
        return res;
    }
    Declaration* addDecl(Scope* scope, const Token& t, int type, Declaration* cls = 0 )
    {
        Declaration* d = new Declaration();
        d->d_kind = type;
        d->d_name = t.d_val;
        d->d_id = t.d_id;
        d->d_loc.d_pos = t.toLoc();
        d->d_loc.d_filePath = t.d_sourcePath;
        d->d_owner = scope;
        scope->d_order.append(d);

        const bool isFuncProc = type == Thing::Proc || type == Thing::Func;

        if( isFuncProc && d_cf->d_intf && scope == d_cf->d_intf )
            d_forwards.append(d);

        // each decl is also a symbol

        Declaration* fwd = 0;

        Symbol* sy = new Symbol();
        sy->d_loc = t.toLoc();
        d_cf->d_syms[t.d_sourcePath].append(sy);
        d->d_me = sy;

        if( isFuncProc && scope == d_cf->d_impl && ( fwd = findInForwards(t) ) )
        {
            // add the present symbol which points to the interface twin
            sy->d_decl = fwd;
            fwd->d_refs[t.d_sourcePath].append(sy);

            d_redirect[d] = fwd;
            fwd->d_impl = d;
        }else if( isFuncProc && scope->d_kind == Thing::Members && ( fwd = findInMembers(scope->d_outer,t) ) )
        {
            sy->d_decl = fwd;
            fwd->d_refs[t.d_sourcePath].append(sy);
            fwd->d_impl = d;
        }else if( type == Thing::MethBlock && cls )
        {
            sy->d_decl = cls;
            cls->d_refs[t.d_sourcePath].append(sy);
            cls->d_impl = d;
        }else
        {
            sy->d_decl = d;
            d->d_refs[t.d_sourcePath].append(sy);
        }

        return d;
    }

    void type_declaration( Scope* scope, SynTree* st)
    {
        Declaration* d = 0;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == Tok_identifier)
                d = addDecl(scope, s->d_tok, Thing::TypeDecl);
            if( s->d_tok.d_type == SynTree::R_type_)
            {
                Type::Ref t = type_(scope,s);
                if( d )
                    d->d_type = t;
            }
        }
    }
    Type::Ref type_( Scope* scope, SynTree* st)
    {
        Type::Ref res;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_simple_type)
                res = simple_type(scope,s);
            if( s->d_tok.d_type == SynTree::R_string_type)
                string_type(scope,s);
            if( s->d_tok.d_type == SynTree::R_structured_type)
                res = structured_type(scope,s);
            if( s->d_tok.d_type == SynTree::R_pointer_type)
                res = pointer_type(scope,s);
        }
        return res;
    }
    Type::Ref resolvedType(Symbol* sym)
    {
        Type::Ref res;
        if( sym && sym->d_decl && sym->d_decl->d_kind == Thing::TypeDecl )
        {
            Declaration* d = static_cast<Declaration*>(sym->d_decl);
            res = d->d_type;
        }
        return res;
    }
    Type::Ref createAlias(Symbol* sym)
    {
        return resolvedType(sym);
    }

    Type::Ref simple_type(Scope* scope, SynTree* st)
    {
        Type::Ref res;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == Tok_identifier)
            {
                Symbol* sym = addSym(scope,s->d_tok);
                res = createAlias(sym);
            }
            if( s->d_tok.d_type == SynTree::R_subrange_type)
                subrange_type(scope,s);
            if( s->d_tok.d_type == SynTree::R_enumerated_type)
                enumerated_type(scope,s);
        }
        return res;
    }
    void subrange_type(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_constant)
                constant(scope,s);
    }
    void enumerated_type(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_identifier_list)
                identifier_list(scope,s, Thing::Const);
    }

    void string_type(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_size_attribute)
                size_attribute(scope,s);
    }
    void size_attribute(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == Tok_identifier)
                addSym(scope,s->d_tok);
    }
    Type::Ref structured_type(Scope* scope, SynTree* st)
    {
        Type::Ref res;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_array_type)
                res = array_type(scope,s);
            if( s->d_tok.d_type == SynTree::R_record_type)
                res = record_type(scope,s);
            if( s->d_tok.d_type == SynTree::R_set_type)
                set_type(scope,s);
            if( s->d_tok.d_type == SynTree::R_file_type)
                file_type(scope,s);
            if( s->d_tok.d_type == SynTree::R_class_type)
                res = class_type(scope,s);
        }
        return res;
    }
    Type::Ref array_type(Scope* scope, SynTree* st)
    {
        Type::Ref t;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_index_type && !s->d_children.isEmpty())
                ordinal_type(scope,s->d_children.first());
            if( s->d_tok.d_type == SynTree::R_type_)
                t = type_(scope,s);
        }
        if( t )
        {
            Type::Ref ptr( new Type() );
            ptr->d_kind = Type::Array;
            ptr->d_type = t;
            return ptr;
        }
        return Type::Ref();
    }
    void ordinal_type(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_simple_type)
                simple_type(scope,s);
    }
    Type::Ref record_type(Scope* scope, SynTree* st)
    {
        Type::Ref rec(new Type());
        rec->d_kind = Type::Record;
        rec->d_members = new Scope();
        rec->d_members->d_kind = Thing::Members;
        rec->d_members->d_outer = scope;
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_field_list)
                field_list(rec->d_members,s);
        return rec;
    }
    void field_list(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_fixed_part)
                fixed_part(scope,s);
            if( s->d_tok.d_type == SynTree::R_variant_part)
                variant_part(scope,s);
        }
    }
    void fixed_part(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_field_declaration)
                field_declaration(scope,s);
    }
    void field_declaration(Scope* scope, SynTree* st)
    {
        QList<Declaration*> fields;
        Type::Ref t;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_identifier_list)
                fields = identifier_list(scope,s,Thing::Field);
            if( s->d_tok.d_type == SynTree::R_type_)
                t = type_(scope,s);
        }
        if( t )
        {
            for( int i = 0; i < fields.size(); i++ )
                fields[i]->d_type = t;
        }
    }
    void variant_part(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_tag_field)
                type_identifier(scope,s); // misuse, just an ident in a box
            if( s->d_tok.d_type == SynTree::R_type_identifier)
                type_identifier(scope,s); // reference, no decl
            if( s->d_tok.d_type == SynTree::R_variant)
                variant(scope,s);
        }
    }
    void variant(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_case_label_list)
                case_label_list(scope,s);
            if( s->d_tok.d_type == SynTree::R_field_list)
                field_list(scope,s);
        }
    }
    void case_label_list(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_constant)
                constant(scope,s);
    }
    void set_type(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_ordinal_type)
                ordinal_type(scope,s);
    }
    void file_type(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_type_)
                type_(scope,s);
    }
    Type::Ref class_type(Scope* scope, SynTree* st)
    {
        Type::Ref rec(new Type());
        rec->d_kind = Type::Class;
        rec->d_members = new Scope();
        rec->d_members->d_kind = Thing::Members;
        rec->d_members->d_outer = scope;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_type_identifier)
            {
                Type::Ref super = resolvedType(type_identifier(scope,s));
                if( super && super->d_kind == Type::Class )
                {
                    rec->d_type = super;
                    rec->d_members->d_altOuter = scope;
                    rec->d_members->d_outer = super->d_members;
                }
            }
            if( s->d_tok.d_type == SynTree::R_field_list)
                field_list(rec->d_members,s);
            if( s->d_tok.d_type == SynTree::R_method_interface)
                method_interface(rec->d_members,s);
        }
        return rec;
    }
    void method_interface(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_procedure_heading)
                func_proc_heading(scope,s,Thing::Proc);
            if( s->d_tok.d_type == SynTree::R_function_heading)
                func_proc_heading(scope,s,Thing::Func);
            // the identifier is just a pseudo keyword
        }
    }
    void defer( Symbol* sym, Type* pointer, Scope* scope, SynTree* typeIdent )
    {
        // TODO: apparently in Clascal any type can be declared after its use; here we only support
        // the original Pascal scope rules
        Deferred def;
        def.sym = sym;
        def.pointer = pointer;
        def.scope = scope;
        def.typeIdent = typeIdent;
        d_deferred.append(def);
    }
    Type::Ref pointer_type(Scope* scope, SynTree* st)
    {
        Symbol* sym = 0;
        SynTree* id = 0;
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_type_identifier)
            {
                id = s;
                sym = type_identifier(scope,id);
            }
        Type::Ref t = resolvedType(sym);

        Type::Ref ptr(new Type());
        ptr->d_kind = Type::Pointer;
        ptr->d_type = t;

        if( t.data() == 0 || sym == 0 || sym->d_decl == 0 )
            defer( sym, ptr.data(), scope, id );
        return ptr;
    }
    void variable_declaration_part( Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_variable_declaration)
                variable_declaration(scope,s);
    }
    void variable_declaration(Scope* scope, SynTree* st)
    {
        QList<Declaration*> vars;
        Type::Ref t;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_identifier_list)
                vars = identifier_list(scope,s,Thing::Var);
            if( s->d_tok.d_type == SynTree::R_type_)
                t = type_(scope,s);
        }
        if( t )
        {
            for( int i = 0; i < vars.size(); i++ )
                vars[i]->d_type = t;
        }
    }
    QList<Declaration*> identifier_list(Scope* scope, SynTree* st, int type)
    {
        QList<Token> toks;
        QList<Declaration*> res;
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == Tok_identifier)
                toks.append(s->d_tok);
        foreach( const Token& t, toks )
            res << addDecl(scope, t, type);
        return res;
    }

    void procedure_and_function_declaration_part( Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_procedure_declaration)
                func_proc_declaration(scope,s, Thing::Proc);
            if( s->d_tok.d_type == SynTree::R_function_declaration)
                func_proc_declaration(scope,s, Thing::Func);
        }
    }
    void func_proc_declaration(Scope* scope, SynTree* st, int kind)
    {
        Declaration* d = 0;
        bool forward = false;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_procedure_heading ||
                    s->d_tok.d_type == SynTree::R_function_heading )
                d = func_proc_heading(scope,s,kind); // overwrite the scope for the body
            else if( s->d_tok.d_type == SynTree::R_body_)
                forward = body_( d ? d->d_body : scope,s);
        }
        if( forward && d )
            d_forwards.append(d);
    }
    Declaration* func_proc_heading(Scope* scope, SynTree* st, int type)
    {
        Token id = findIdent(st);
        Declaration* d = addDecl(scope,id, type);
        d->d_body = new Scope();
        d->d_body->d_kind = Thing::Body;
        d->d_body->d_owner = d;
        d->d_body->d_outer = scope;
        if( d->d_me && d->d_me->d_decl != d )
        {
            Q_ASSERT(d->d_me->d_decl->isDeclaration());
            Declaration* intf = static_cast<Declaration*>(d->d_me->d_decl);

            d->d_body->d_altOuter = intf->d_body;
            // TODO: the same must be implemented for FORWARD decls, the implementation of those also can have
            // the params left out, and both forward and implementation can be in the same Scope with the same name!
        }
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_formal_parameter_list)
                formal_parameter_list(d->d_body, s);
            if( s->d_tok.d_type == SynTree::R_result_type && !s->d_children.isEmpty())
                d->d_type = createAlias( type_identifier(d->d_body, s->d_children.first()) );
        }
        return d;
    }

    void formal_parameter_list(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_formal_parameter_section)
                formal_parameter_section(scope,s);
    }
    void formal_parameter_section(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_parameter_declaration)
                parameter_declaration(scope,s);
            if( s->d_tok.d_type == SynTree::R_procedure_heading)
                func_proc_heading(scope,s, Thing::Proc);
            if( s->d_tok.d_type == SynTree::R_function_heading)
                func_proc_heading(scope,s,Thing::Func);
        }
    }
    void parameter_declaration(Scope* scope, SynTree* st)
    {
        QList<Declaration*> vars;
        Symbol* id = 0;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_identifier_list)
                vars = identifier_list(scope,s,Thing::Param);
            if( s->d_tok.d_type == SynTree::R_type_identifier)
                id = type_identifier(scope,s);
        }
        if( id )
        {
            Type::Ref t  = createAlias(id); // t is potentially an alias
            for( int i = 0; i < vars.size(); i++ )
                vars[i]->d_type = t;
        }
    }
    Symbol* addSym(Scope* scope, const Token& t)
    {
        Declaration* d = scope->findDecl(t.d_id);
#ifndef LISA_WITH_MISSING
        if( d )
#else
        if( true )
#endif
        {
            if( d && d_cf->d_kind == Thing::Unit )
            {
                QHash<Declaration*,Declaration*>::const_iterator intf = d_redirect.find(d);
                if( intf != d_redirect.end() )
                {
                    d = intf.value(); // attach all refs to the interface declaration (otherwise they are not visible)
                }
            }
            Symbol* sy = new Symbol();
            sy->d_loc = t.toLoc();
            d_cf->d_syms[t.d_sourcePath].append(sy);
            sy->d_decl = d;
            if( d )
                d->d_refs[t.d_sourcePath].append(sy);
            return sy;
        }else
            return 0;
    }
    Symbol* type_identifier(Scope* scope, SynTree* st)
    {
        Symbol* res = 0;
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == Tok_identifier)
                res = addSym(scope,s->d_tok);
        return res;
    }
    Token findIdent(SynTree* st)
    {
        Token t;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == Tok_identifier)
                t = s->d_tok;
        }
        return t;
    }
    bool body_(Scope* scope, SynTree* st)
    {
        bool forward = false;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_block)
                block(scope,s);
            if( s->d_tok.d_type == SynTree::R_statement_part)
                statement_part(scope,s);
            if( s->d_tok.d_type == SynTree::R_constant)
                constant(scope,s);
            if( s->d_tok.d_type == Tok_forward )
                forward = true;
        }
        return forward;
    }
    void statement_part( Scope* scope, SynTree* st)
    {
        if( !st->d_children.isEmpty() && st->d_children.first()->d_tok.d_type == SynTree::R_compound_statement )
            compound_statement(scope, st->d_children.first());
    }
    void statement_sequence(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_statement)
                statement(scope,s);
    }
    void statement(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_simple_statement)
                simple_statement(scope,s);
            if( s->d_tok.d_type == SynTree::R_structured_statement)
                structured_statement(scope,s);
        }
    }
    void simple_statement(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            // goto statement is not interesting
            if( s->d_tok.d_type == SynTree::R_assigOrCall)
                assigOrCall(scope,s);
    }
    void structured_statement(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_compound_statement)
                compound_statement(scope,s);
            if( s->d_tok.d_type == SynTree::R_repetitive_statement)
                repetitive_statement(scope,s);
            if( s->d_tok.d_type == SynTree::R_conditional_statement)
                conditional_statement(scope,s);
            if( s->d_tok.d_type == SynTree::R_with_statement)
                with_statement(scope,s);
        }
    }
    void compound_statement(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_statement_sequence)
                statement_sequence(scope,s);
    }
    void repetitive_statement(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_while_statement)
                while_statement(scope,s);
            if( s->d_tok.d_type == SynTree::R_repeat_statement)
                repeat_statement(scope,s);
            if( s->d_tok.d_type == SynTree::R_for_statement)
                for_statement(scope,s);
        }
    }
    void while_statement(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_expression)
                expression(scope,s);
            if( s->d_tok.d_type == SynTree::R_statement)
                statement(scope,s);
        }
    }
    void repeat_statement(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_statement_sequence)
                statement_sequence(scope,s);
            if( s->d_tok.d_type == SynTree::R_expression)
                expression(scope,s);
        }
    }
    void for_statement(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_variable_identifier && !s->d_children.isEmpty())
                addSym(scope,s->d_children.first()->d_tok);
            if( s->d_tok.d_type == SynTree::R_initial_value && !s->d_children.isEmpty())
                expression(scope,s->d_children.first());
            if( s->d_tok.d_type == SynTree::R_final_value && !s->d_children.isEmpty())
                expression(scope,s->d_children.first());
            if( s->d_tok.d_type == SynTree::R_statement)
                statement(scope,s);
        }
    }

    void conditional_statement(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_if_statement)
                if_statement(scope,s);
            if( s->d_tok.d_type == SynTree::R_case_statement)
                case_statement(scope,s);
        }
    }
    void if_statement(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_statement)
                statement(scope,s);
            if( s->d_tok.d_type == SynTree::R_expression)
                expression(scope,s);
        }
    }
    void case_statement(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_case_limb)
                case_limb(scope,s);
            if( s->d_tok.d_type == SynTree::R_otherwise_clause)
                otherwise_clause(scope,s);
            if( s->d_tok.d_type == SynTree::R_expression)
                expression(scope,s);
        }
    }
    void case_limb(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_case_label_list)
                case_label_list(scope,s);
            if( s->d_tok.d_type == SynTree::R_statement)
                statement(scope,s);
        }
    }
    void otherwise_clause(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_statement)
                statement(scope,s);
    }
    void with_statement(Scope* scope, SynTree* st)
    {
        QList<Type*> types;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_variable_reference)
                types << variable_reference(scope,s);
            if( s->d_tok.d_type == SynTree::R_statement)
            {
                Scope tmp;
                tmp.d_outer = scope;
                for( int i = 0; i < types.size(); i++ )
                {
                    if( types[i] && types[i]->d_members )
                        tmp.d_order += types[i]->d_members->d_order; // borrow decls from records
                }
                statement(&tmp,s); // this needs a prepared scope
                tmp.d_order.clear(); // avoid that Scope deletes the temporarily borrowed decls
            }
        }
    }
    void assigOrCall(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_variable_reference)
                variable_reference(scope,s);
            if( s->d_tok.d_type == SynTree::R_expression)
                expression(scope,s);
        }
    }
    void constant(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == Tok_identifier)
                addSym(scope,s->d_tok);
            if( s->d_tok.d_type == SynTree::R_actual_parameter_list)
                actual_parameter_list(scope,s);
        }
    }
    Declaration* actual_parameter_list(Scope* scope, SynTree* st, Declaration* d = 0)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_actual_parameter)
                actual_parameter(scope,s);
        return 0;
    }
    void actual_parameter(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_expression)
                expression(scope,s);
    }
    void expression( Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_simple_expression)
                simple_expression(scope,s);
    }
    void simple_expression(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_term)
                term(scope,s);
    }
    void term(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_factor)
                factor(scope,s);
    }
    void factor(Scope* scope, SynTree* st)
    {
        Type* t = 0;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_variable_reference)
                variable_reference(scope,s);
            if( s->d_tok.d_type == SynTree::R_actual_parameter_list)
                actual_parameter_list(scope,s);
            if( s->d_tok.d_type == SynTree::R_set_literal)
                set_literal(scope,s);
            if( s->d_tok.d_type == SynTree::R_expression)
                expression(scope,s);
            if( s->d_tok.d_type == Tok_identifier)
            {
                Symbol* sym = addSym(scope,s->d_tok);
                if( sym && sym->d_decl && sym->d_decl->isDeclaration() )
                    t = static_cast<Declaration*>(sym->d_decl)->d_type.data();
            }
            if( s->d_tok.d_type == SynTree::R_factor)
                factor(scope,s);
            if( s->d_tok.d_type == SynTree::R_qualifier)
                t = qualifier(scope,s,t);
        }
    }
    Type* variable_reference(Scope* scope, SynTree* st)
    {
        Type* t = 0;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_variable_identifier && !s->d_children.isEmpty())
            {
                Symbol* sym = addSym(scope,s->d_children.first()->d_tok);
                if( sym && sym->d_decl && sym->d_decl->isDeclaration() )
                    t = static_cast<Declaration*>(sym->d_decl)->d_type.data();
                // if decl is a Func/Proc, t is the return type
            }
            if( s->d_tok.d_type == SynTree::R_qualifier)
                t = qualifier(scope,s, t);
            if( s->d_tok.d_type == SynTree::R_actual_parameter_list)
                actual_parameter_list(scope,s);
        }
        return t;
    }
    Type* dereferencer(Type* t)
    {
        if( t && t->d_kind == Type::Pointer )
            return t->d_type.data();
        else
            return 0;
    }
    void set_literal(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_member_group)
                member_group(scope,s);
    }
    void member_group(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_expression)
                expression(scope,s);
    }
    Type* qualifier(Scope* scope, SynTree* st, Type* t)
    {
        Type* res = 0;
        foreach( SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == SynTree::R_index)
            {
                index(scope,s);
                // if t is an array, the t->d_type is the element type
                if( t && t->d_kind == Type::Array )
                    res = t->d_type.data();
            }
            if( s->d_tok.d_type == SynTree::R_field_designator)
                res = field_designator(scope,s,t);
            if( s->d_tok.d_type == SynTree::R_dereferencer )
                res = dereferencer(t);
        }
        return res;
    }
    Type* field_designator(Scope* scope, SynTree* st, Type* t)
    {
        Type* res = 0;
        if( t && t->d_members )
        {
            foreach( SynTree* s, st->d_children )
                if( s->d_tok.d_type == SynTree::R_field_identifier && !s->d_children.isEmpty() )
                {
                    Symbol* sym = addSym(t->d_members,s->d_children.first()->d_tok);
                    if( sym && sym->d_decl && sym->d_decl->isDeclaration() )
                        res = static_cast<Declaration*>(sym->d_decl)->d_type.data();
                }
        }
        return res;
    }
    void index(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_expression_list)
                expression_list(scope,s);
    }
    void expression_list(Scope* scope, SynTree* st)
    {
        foreach( SynTree* s, st->d_children )
            if( s->d_tok.d_type == SynTree::R_expression)
                expression(scope,s);
    }

protected:
};

class AsmModelVisitor
{
    CodeModel* d_mdl;
    AsmFile* d_cf;

public:
    AsmModelVisitor(CodeModel* m):d_mdl(m) {}

    void visit( AsmFile* cf, Asm::SynTree* top )
    {
        d_cf = cf;
        cf->d_impl = new Scope();
        cf->d_impl->d_kind = Thing::Body;
        cf->d_impl->d_owner = cf;

        foreach( Asm::SynTree* s, top->d_children )
            if( s->d_tok.d_type == Asm::SynTree::R_line )
                line(s);
    }
private:
    void line(Asm::SynTree* st )
    {
        foreach( Asm::SynTree* s, st->d_children )
            if( s->d_tok.d_type == Asm::SynTree::R_directive)
                directive(s);
    }
    void directive(Asm::SynTree* st)
    {
        int type = 0;
        foreach( Asm::SynTree* s, st->d_children )
        {
            if( s->d_tok.d_type == Asm::Tok_PROC)
                type = Thing::Proc;
            else if( s->d_tok.d_type == Asm::Tok_FUNC)
                type = Thing::Func;
            else if( s->d_tok.d_type == Asm::Tok_ident)
            {
                addDecl(s->d_tok, type );
            }else if( s->d_tok.d_type == Asm::SynTree::R_filename)
                include(s);
        }
        // TODO: .DEF apparently also declares proc names
    }
    void include(Asm::SynTree* st)
    {
        QStringList path;
        int endCol = 0;
        for( int i = 0; i < st->d_children.size(); i++ )
            if( st->d_children[i]->d_tok.d_type == Asm::Tok_ident )
            {
                path.append( QString::fromUtf8(st->d_children[i]->d_tok.d_val).toLower() );
                endCol = st->d_children[i]->d_tok.d_val.size() + st->d_children[i]->d_tok.d_colNr;
            }
        if( path.isEmpty() )
            return;
        QString fileName = path.last();
        if( fileName.endsWith(".text") )
            fileName.chop(5);
        const FileSystem::File* found = 0;
        // TODO: find can still be improved
        if( path.size() == 1 )
        {
            found = d_mdl->getFs()->findFile(d_cf->d_file->d_dir, QString(), fileName);
        }else if( path.size() == 2 )
            found = d_mdl->getFs()->findFile(d_cf->d_file->d_dir, path[0], fileName);
        else
            // never seen so far
            qWarning() << "asm include with more than two segments" << d_cf->d_file->getVirtualPath(false) << st->d_tok.d_lineNr;

        AsmInclude* include = new AsmInclude();
        d_cf->d_includes.append(include);
        include->d_file = found;
        include->d_len = endCol - st->d_tok.d_colNr;
        include->d_col = st->d_tok.d_colNr;
        include->d_row = st->d_tok.d_lineNr;
        include->d_unit = d_cf;
        if( found == 0 )
        {
            const QString line = QString("%1:%2:%3: cannot find assembler include file '%4'")
                    .arg( d_cf->d_file->getVirtualPath(false) ).arg(include->d_col)
                    .arg(include->d_row).arg(path.join('/'));
            qCritical() << line.toUtf8().constData();
        }
    }
    Declaration* addDecl(const Asm::Token& t, int type )
    {
        Declaration* d = new Declaration();
        d->d_kind = type;
        d->d_name = t.d_val;
        d->d_id = Token::toId(t.d_val);
        d->d_loc.d_pos = t.toLoc();
        d->d_loc.d_filePath = t.d_sourcePath;
        d->d_owner = d_cf->d_impl;
        d_cf->d_impl->d_order.append(d);

        Symbol* sy = new Symbol();
        sy->d_loc = t.toLoc();
        d_cf->d_syms[t.d_sourcePath].append(sy);
        d->d_me = sy;
        sy->d_decl = d;
        d->d_refs[t.d_sourcePath].append(sy);

        return d;
    }
};

CodeModel::CodeModel(QObject *parent) : ItemModel(parent),d_sloc(0)
{
    d_fs = new FileSystem(this);
}

bool CodeModel::load(const QString& rootDir)
{
    beginResetModel();
    d_root = ModelItem();
    d_top.clear();
    d_globals.clear();
    d_map1.clear();
    d_map2.clear();
    d_sloc = 0;
    d_mutes.clear();
    d_fs->load(rootDir);
    QList<ModelItem*> fileSlots;
    fillFolders(&d_root,&d_fs->getRoot(), &d_top, fileSlots);
    foreach( ModelItem* s, fileSlots )
    {
        Q_ASSERT( s->d_thing );
        if( s->d_thing->d_kind != Thing::Assembler )
            continue;
        AsmFile* f = static_cast<AsmFile*>(s->d_thing);
        Q_ASSERT( f->d_file );
        parseAndResolve(f);
        for( int i = 0; i < f->d_includes.size(); i++ )
        {
            AsmInclude* inc = f->d_includes[i];
            if( inc->d_file != 0 )
                new ModelItem(s, f->d_includes[i]);
            Symbol* sym = new Symbol();
            sym->d_decl = f->d_includes[i];
            sym->d_loc = RowCol(inc->d_row,inc->d_col);
            f->d_syms[f->d_file->d_realPath].append(sym);
            if( inc->d_file )
                d_map2[inc->d_file->d_realPath] = inc;
        }
    }
    foreach( ModelItem* s, fileSlots )
    {
        Q_ASSERT( s->d_thing );
        if( s->d_thing->d_kind != Thing::Unit )
            continue;
        UnitFile* f = static_cast<UnitFile*>(s->d_thing);
        Q_ASSERT( f->d_file );
        parseAndResolve(f);
        for( int i = 0; i < f->d_includes.size(); i++ )
        {
            if( f->d_includes[i]->d_file != 0 )
                new ModelItem(s, f->d_includes[i]);
        }
    }
    endResetModel();
    return true;
}

ItemModel::ItemModel(QObject* parent):QAbstractItemModel(parent)
{

}

const Thing* ItemModel::getThing(const QModelIndex& index) const
{
    if( !index.isValid() )
        return 0;
    ModelItem* s = static_cast<ModelItem*>( index.internalPointer() );
    Q_ASSERT( s != 0 );
    return s->d_thing;
}

QModelIndex ItemModel::findThing(const Thing* nt) const
{
    return findThing( &d_root, nt );
}

QVariant ItemModel::data(const QModelIndex& index, int role) const
{
    return QVariant();
}

Symbol*CodeModel::findSymbolBySourcePos(const QString& path, int line, int col) const
{
    UnitFile::SymList syms;

    UnitFile* uf = getUnitFile(path);
    if( uf == 0 )
    {
        AsmFile* af = getAsmFile(path);
        if( af == 0 )
            return 0;
        syms = af->d_syms.value(path);
    }else
        syms = uf->d_syms.value(path);


    // TODO maybe make that faster by ordering by rowcol and e.g. binary search
    foreach( Symbol* s, syms )
    {
        if( s->d_decl && s->d_loc.d_row == line &&
                s->d_loc.d_col <= col && col < s->d_loc.d_col + s->d_decl->getLen() )
            return s;
    }
    return 0;
}

CodeFile*CodeModel::getCodeFile(const QString& path) const
{
    return d_map2.value(path);
}

UnitFile*CodeModel::getUnitFile(const QString& path) const
{
    CodeFile* cf = d_map2.value(path);;
    if( cf )
    {
        UnitFile* uf = cf->toUnit();
        if( uf == 0 )
        {
            IncludeFile* inc = cf->toInclude();
            if( inc )
                uf = inc->d_unit;
        }
        return uf;
    }
    return 0;
}

AsmFile*CodeModel::getAsmFile(const QString& path) const
{
    CodeFile* cf = d_map2.value(path);;
    if( cf )
    {
        AsmFile* uf = cf->toAsmFile();
        if( uf == 0 )
        {
            AsmInclude* inc = cf->toAsmInclude();
            if( inc )
                uf = inc->d_unit;
        }
        return uf;
    }
    return 0;
}

Ranges CodeModel::getMutes(const QString& path)
{
    return d_mutes.value(path);
}

QVariant CodeModel::data(const QModelIndex& index, int role) const
{
    ModelItem* s = static_cast<ModelItem*>( index.internalPointer() );
    Q_ASSERT( s != 0 );
    switch( role )
    {
    case Qt::DisplayRole:
        switch( s->d_thing->d_kind )
        {
        case Thing::Unit:
            return static_cast<UnitFile*>(s->d_thing)->d_file->d_name;
        case Thing::Assembler:
            return static_cast<AsmFile*>(s->d_thing)->d_file->d_name;
        case Thing::AsmIncl:
            return static_cast<AsmInclude*>(s->d_thing)->d_file->d_name;
        case Thing::Include:
            return static_cast<IncludeFile*>(s->d_thing)->d_file->d_name;
        case Thing::Folder:
            return static_cast<CodeFolder*>(s->d_thing)->d_dir->d_name;
        }
        break;
    case Qt::DecorationRole:
        switch( s->d_thing->d_kind )
        {
        case Thing::Unit:
            return QPixmap(":/images/source.png");
        case Thing::Include:
            return QPixmap(":/images/include.png");
        case Thing::Assembler:
        case Thing::AsmIncl:
            return QPixmap(":/images/assembler.png");
        case Thing::Folder:
            return QPixmap(":/images/folder.png");
        }
        break;
    case Qt::ToolTipRole:
        switch( s->d_thing->d_kind )
        {
        case Thing::Unit:
            {
                UnitFile* cf = static_cast<UnitFile*>(s->d_thing);
                return QString("<html><b>%1 %2</b><br>"
                               "<p>Logical path: %3</p>"
                               "<p>Real path: %4</p></html>")
                        .arg(cf->d_file->d_type == FileSystem::PascalUnit ? "Unit" : "Program")
                        .arg(cf->d_file->d_moduleName)
                        .arg(cf->d_file->getVirtualPath())
                        .arg(cf->d_file->d_realPath);
            }
        case Thing::Assembler:
            return static_cast<AsmFile*>(s->d_thing)->d_file->d_realPath;
        case Thing::AsmIncl:
            return static_cast<AsmInclude*>(s->d_thing)->d_file->d_realPath;
        case Thing::Include:
            return static_cast<IncludeFile*>(s->d_thing)->d_file->d_realPath;
        }
        break;
    case Qt::FontRole:
        break;
    case Qt::ForegroundRole:
        break;
    }
    return QVariant();
}

QModelIndex ItemModel::index(int row, int column, const QModelIndex& parent) const
{
    const ModelItem* s = &d_root;
    if( parent.isValid() )
    {
        s = static_cast<ModelItem*>( parent.internalPointer() );
        Q_ASSERT( s != 0 );
    }
    if( row < s->d_children.size() && column < columnCount( parent ) )
        return createIndex( row, column, s->d_children[row] );
    else
        return QModelIndex();
}

QModelIndex ItemModel::parent(const QModelIndex& index) const
{
    if( index.isValid() )
    {
        ModelItem* s = static_cast<ModelItem*>( index.internalPointer() );
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

int ItemModel::rowCount(const QModelIndex& parent) const
{
    if( parent.isValid() )
    {
        ModelItem* s = static_cast<ModelItem*>( parent.internalPointer() );
        Q_ASSERT( s != 0 );
        return s->d_children.size();
    }else
        return d_root.d_children.size();
}

void CodeModel::parseAndResolve(UnitFile* unit)
{
    if( unit->d_file->d_parsed )
        return; // already done

    QByteArrayList usedNames = unit->findUses();
    for( int i = 0; i < usedNames.size(); i++ )
    {
        const FileSystem::File* u = d_fs->findModule(unit->d_file->d_dir,usedNames[i].toLower());
        if( u == 0 )
        {
            const QString line = tr("%1: cannot resolve referenced unit '%2'")
                    .arg( unit->d_file->getVirtualPath(false) ).arg(usedNames[i].constData());
            qCritical() << line.toUtf8().constData();
        }else
        {
            UnitFile* uf = d_map1.value(u);
            Q_ASSERT( uf );
            unit->d_import.append( uf );
            parseAndResolve(uf);
        }
    }

    const_cast<FileSystem::File*>(unit->d_file)->d_parsed = true;
    PpLexer lex(d_fs);
    lex.reset(unit->d_file->d_realPath);
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
    foreach( const PpLexer::Include& f, lex.getIncludes() )
    {
        IncludeFile* inc = new IncludeFile();
        inc->d_file = f.d_inc;
        inc->d_len = f.d_len;
        inc->d_unit = unit;
        Symbol* sym = new Symbol();
        sym->d_decl = inc;
        sym->d_loc = f.d_loc;
        unit->d_syms[f.d_sourcePath].append(sym);
        if( f.d_inc )
            d_map2[f.d_inc->d_realPath] = inc;
        unit->d_includes.append(inc);
    }
    d_sloc += lex.getSloc();
    for( QHash<QString,Ranges>::const_iterator i = lex.getMutes().begin(); i != lex.getMutes().end(); ++i )
        d_mutes.insert(i.key(),i.value());

    PascalModelVisitor v(this);
    v.visit(unit,&p.d_root); // visit takes ~8% more time than just parsing

    QCoreApplication::processEvents();
}

void CodeModel::parseAndResolve(AsmFile* unit)
{
    const_cast<FileSystem::File*>(unit->d_file)->d_parsed = true;
    Asm::PpLexer lex(d_fs);
    lex.reset(unit->d_file->d_realPath);
    Asm::Parser p(&lex);
    p.RunParser();
    const int off = d_fs->getRootPath().size();
    if( !p.errors.isEmpty() )
    {
        foreach( const Asm::Parser::Error& e, p.errors )
        {
            const FileSystem::File* f = d_fs->findFile(e.path);
            const QString line = tr("%1:%2:%3: %4").arg( f ? f->getVirtualPath() : e.path.mid(off) ).arg(e.row)
                    .arg(e.col).arg(e.msg);
            qCritical() << line.toUtf8().constData();
        }

    }
    d_sloc += lex.getSloc();

    AsmModelVisitor v(this);
    v.visit(unit,&p.d_root);


    QCoreApplication::processEvents();
}

QModelIndex ItemModel::findThing(const ModelItem* slot, const Thing* nt) const
{
    for( int i = 0; i < slot->d_children.size(); i++ )
    {
        ModelItem* s = slot->d_children[i];
        if( s->d_thing == nt )
            return createIndex( i, 0, s );
        QModelIndex index = findThing( s, nt );
        if( index.isValid() )
            return index;
    }
    return QModelIndex();
}

bool ModelItem::lessThan(const ModelItem* lhs, const ModelItem* rhs)
{
    if( lhs->d_thing == 0 || rhs->d_thing == 0 )
        return false;
    return lhs->d_thing->getName().compare(rhs->d_thing->getName(),Qt::CaseInsensitive) < 0;
}

void CodeModel::fillFolders(ModelItem* root, const FileSystem::Dir* super, CodeFolder* top, QList<ModelItem*>& fileSlots)
{
    for( int i = 0; i < super->d_subdirs.size(); i++ )
    {
        CodeFolder* f = new CodeFolder();
        f->d_dir = super->d_subdirs[i];
        top->d_subs.append(f);
        ModelItem* s = new ModelItem(root,f);
        fillFolders(s,super->d_subdirs[i],f,fileSlots);
    }
    for( int i = 0; i < super->d_files.size(); i++ )
    {
        if( super->d_files[i]->d_type == FileSystem::PascalProgram ||
                super->d_files[i]->d_type == FileSystem::PascalUnit )
        {
            UnitFile* f = new UnitFile();
            f->d_file = super->d_files[i];
            d_map1[f->d_file] = f;
            d_map2[f->d_file->d_realPath] = f;
            top->d_files.append(f);
            ModelItem* s = new ModelItem(root,f);
            fileSlots.append(s);
        }else if( super->d_files[i]->d_type == FileSystem::AsmUnit )
        {
            AsmFile* f = new AsmFile();
            f->d_file = super->d_files[i];
            //d_map1[f->d_file] = f;
            d_map2[f->d_file->d_realPath] = f;
            top->d_files.append(f);
            ModelItem* s = new ModelItem(root,f);
            fileSlots.append(s);
        }
    }
    std::sort( root->d_children.begin(), root->d_children.end(), ModelItem::lessThan );
}

QString CodeFile::getName() const
{
    return d_file->d_name;
}

QByteArrayList UnitFile::findUses() const
{
    QByteArrayList res;
    if( d_file == 0 || !(d_file->d_type == FileSystem::PascalProgram ||
                         d_file->d_type == FileSystem::PascalUnit) )
        return res;
    QFile f(d_file->d_realPath);
    f.open(QIODevice::ReadOnly);
    Lexer lex;
    lex.setStream(&f);
    Token t = lex.nextToken();
    while( t.isValid() )
    {
        switch( t.d_type )
        {
        case Tok_uses:
            t = lex.nextToken();
            while( t.isValid() && t.d_type != Tok_Semi )
            {
                if( t.d_type == Tok_Comma )
                {
                    t = lex.nextToken();
                    continue;
                }
                if( t.d_type == Tok_identifier )
                {
                    const QByteArray id = t.d_val;
                    t = lex.nextToken();
                    if( t.d_type == Tok_Slash )
                    {
                        t = lex.nextToken();
                        if( t.d_type == Tok_identifier ) // just to make sure
                        {
                            res.append( t.d_val );
                            t = lex.nextToken();
                        }
                    }else
                        res.append(id);
                }else
                    t = lex.nextToken();
            }
            return res;
        case Tok_label:
        case Tok_var:
        case Tok_const:
        case Tok_type:
        case Tok_procedure:
        case Tok_function:
        case Tok_implementation:
            return res;
        }
        t = lex.nextToken();
    }
    return res;
}

UnitFile::~UnitFile()
{
    if( d_impl )
        delete d_impl;
    if( d_intf )
        delete d_intf;
    QHash<QString,SymList>::const_iterator j;
    for( j = d_syms.begin(); j != d_syms.end(); ++j )
        for( int i = 0; i < j.value().size(); i++ )
            delete j.value()[i];
    for( int i = 0; i < d_includes.size(); i++ )
        delete d_includes[i];
}

UnitFile*Scope::getUnitFile() const
{
    if( d_owner == 0 )
        return 0; // happens in global scope
    if( d_owner->d_kind == Thing::Unit )
        return static_cast<UnitFile*>(d_owner);
    else if( d_owner->isDeclaration() )
    {
        Declaration* d = static_cast<Declaration*>(d_owner);
        Q_ASSERT( d->d_owner != 0 );
        return d->d_owner->getUnitFile();
    }else
        Q_ASSERT(false);
}

Declaration*Scope::findDecl(const char* id, bool withImports) const
{
    foreach( Declaration* d, d_order )
    {
        if( d->d_id == id )
            return d;
    }
    if( d_altOuter )
    {
        foreach( Declaration* d, d_altOuter->d_order )
        {
            if( d->d_id == id )
                return d;
        }
    }
    if( d_outer )
        return d_outer->findDecl(id, withImports);

    UnitFile* cf = getUnitFile();
    if( cf == 0 )
        return 0; // TODO: this happens, check

    if( withImports )
    {
        // searching through imports takes ~12% more time
        foreach( UnitFile* imp, cf->d_import )
        {
            if( imp->d_intf )
            {
                Declaration* d = imp->d_intf->findDecl(id,false); // don't follow imports of imports
                if( d )
                    return d;
            }
        }
    }
    if( false ) // cf->d_globals )
        // not actually needed since uses resolution goes directly to globals
        return cf->d_globals->findDecl(id, false);
    return 0;
}

void Scope::clear()
{
    for( int i = 0; i < d_order.size(); i++ )
        delete d_order[i];
    d_order.clear();
}

Scope::~Scope()
{
    clear();
}

QString Declaration::getName() const
{
    return d_name;
}

UnitFile*Declaration::getUnitFile() const
{
    Q_ASSERT( d_owner != 0 );
    return d_owner->getUnitFile();
}

Declaration::~Declaration()
{
    if( d_body )
        delete d_body;
}

QString CodeFolder::getName() const
{
    return d_dir->d_name;
}

void CodeFolder::clear()
{
    for( int i = 0; i < d_subs.size(); i++ )
    {
        d_subs[i]->clear();
        delete d_subs[i];
    }
    d_subs.clear();
    for( int i = 0; i < d_files.size(); i++ )
        delete d_files[i];
    d_files.clear();
}

FilePos IncludeFile::getLoc() const
{
    if( d_file == 0 )
        return FilePos();
    return FilePos( RowCol(1,1), d_file->d_realPath );
}

QString Thing::getName() const
{
    return QString();
}

const char*Thing::typeName() const
{
    switch( d_kind )
    {
    case Const:
        return "Const";
    case TypeDecl:
        return "Type";
    case Var:
        return "Var";
    case Func:
        return "Function";
    case Proc:
        return "Procedure";
    case Module:
        return "Module";
    case Param:
        return "Parameter";
    case Field:
        return "Field";
    default:
        return "";
    }
}

Thing::~Thing()
{
}

UnitFile*CodeFile::toUnit()
{
    if( d_kind == Unit )
        return static_cast<UnitFile*>(this);
    else
        return 0;
}

IncludeFile*CodeFile::toInclude()
{
    if( d_kind == Include )
        return static_cast<IncludeFile*>(this);
    else
        return 0;
}

AsmFile*CodeFile::toAsmFile()
{
    if( d_kind == Assembler )
        return static_cast<AsmFile*>(this);
    else
        return 0;
}

AsmInclude*CodeFile::toAsmInclude()
{
    if( d_kind == AsmIncl )
        return static_cast<AsmInclude*>(this);
    else
        return 0;
}

CodeFile::~CodeFile()
{
}

Type::~Type()
{
    if( d_members )
        delete d_members;
}


ModuleDetailMdl::ModuleDetailMdl(QObject* parent)
{

}

void ModuleDetailMdl::load(Scope* intf, Scope* impl)
{
    if( d_intf == intf && d_impl == impl )
        return;

    beginResetModel();
    d_root = ModelItem();
    d_intf = intf;
    d_impl = impl;

    if( intf && impl )
    {
        ModelItem* title = new ModelItem( &d_root, intf );
        fillItems(title, intf);

        title = new ModelItem( &d_root, impl );
        fillItems(title, impl);

    }else if( impl )
        fillItems(&d_root, impl);

    endResetModel();
}

QVariant ModuleDetailMdl::data(const QModelIndex& index, int role) const
{
    ModelItem* s = static_cast<ModelItem*>( index.internalPointer() );
    Q_ASSERT( s != 0 );
    switch( role )
    {
    case Qt::DisplayRole:
        switch( s->d_thing->d_kind )
        {
        case Thing::Interface:
            return "interface";
        case Thing::Implementation:
            return "implementation";
        default:
            return s->d_thing->getName();
        }
        break;
    case Qt::DecorationRole:
        switch( s->d_thing->d_kind )
        {
        case Thing::Interface:
            return QPixmap(":/images/interface.png");
        case Thing::Implementation:
            return QPixmap(":/images/implementation.png");
        case Thing::Const:
            return QPixmap(":/images/constant.png");
        case Thing::TypeDecl:
            return QPixmap(":/images/type.png");
        case Thing::Var:
            return QPixmap(":/images/variable.png");
        case Thing::Func:
            return QPixmap(":/images/function.png");
        case Thing::Proc:
            return QPixmap(":/images/procedure.png");
        case Thing::Label:
            return QPixmap(":/images/label.png");
        case Thing::MethBlock:
            return QPixmap(":/images/category.png");
        }
        break;
    }
    return QVariant();
}

static bool DeclLessThan(Declaration* lhs, Declaration* rhs)
{
    return lhs->getName() < rhs->getName();
}

void ModuleDetailMdl::fillItems(ModelItem* parentItem, Scope* scope)
{
    QVector<QList<Declaration*> > elems(Thing::Label);
    foreach( Declaration* d, scope->d_order )
    {
        if( d->d_kind > Thing::Const && d->d_kind < Thing::Label )
            elems[ d->d_kind >= Thing::Proc ? d->d_kind-1 : d->d_kind ].append(d);
    }
    for( int i = 1; i < elems.size()-1; i++ ) // leave out consts and labels
    {
        QList<Declaration*>& d = elems[i];
        std::sort( d.begin(), d.end(), DeclLessThan );
        for( int j = 0; j < d.size(); j++ )
        {
            Declaration* dd = d[j];
            ModelItem* item = new ModelItem(parentItem,dd);
            if( dd->d_kind == Thing::TypeDecl && dd->d_type && dd->d_type->d_kind == Type::Class )
                fillSubs(item,dd->d_type->d_members);
            else if( dd->d_kind == Thing::MethBlock )
                fillSubs(item,dd->d_body);
        }
    }
}

void ModuleDetailMdl::fillSubs(ModelItem* parentItem, Scope* scope)
{
    QList<Declaration*> funcs;
    foreach( Declaration* d, scope->d_order )
    {
        if( ( d->d_kind == Thing::Func || d->d_kind == Thing::Proc ) )
            funcs.append(d);
    }
    std::sort( funcs.begin(), funcs.end(), DeclLessThan );
    for( int j = 0; j < funcs.size(); j++ )
        new ModelItem(parentItem,funcs[j]);
}

AsmFile::~AsmFile()
{
    if( d_impl )
        delete d_impl;
    QHash<QString,SymList>::const_iterator j;
    for( j = d_syms.begin(); j != d_syms.end(); ++j )
        for( int i = 0; i < j.value().size(); i++ )
            delete j.value()[i];
    for( int i = 0; i < d_includes.size(); i++ )
        delete d_includes[i];
}

FilePos AsmInclude::getLoc() const
{
    if( d_file )
        return FilePos( RowCol(d_row,d_col), d_file->d_realPath );
    else
        return FilePos();
}
