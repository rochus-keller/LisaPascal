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

#include "LisaCodeNavigator.h"
#include "LisaHighlighter.h"
#include "LisaCodeModel.h"
#include <QApplication>
#include <QFileInfo>
#include <QtDebug>
#include <QDir>
#include <QVBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSettings>
#include <QDockWidget>
#include <QShortcut>
#include <QTreeView>
#include <QTreeWidget>
#include <QInputDialog>
#include <QFileDialog>
#include <QTimer>
#include <QElapsedTimer>
#include <QScrollBar>
using namespace Lisa;

Q_DECLARE_METATYPE(Symbol*)
Q_DECLARE_METATYPE(FilePos)

static CodeNavigator* s_this = 0;
static void report(QtMsgType type, const QString& message )
{
    if( s_this )
    {
        switch(type)
        {
        case QtDebugMsg:
            s_this->logMessage(QLatin1String("INF: ") + message);
            break;
        case QtWarningMsg:
            s_this->logMessage(QLatin1String("WRN: ") + message);
            break;
        case QtCriticalMsg:
        case QtFatalMsg:
            s_this->logMessage(QLatin1String("ERR: ") + message);
            break;
        }
    }
}

static QtMessageHandler s_oldHandler = 0;
void messageHander(QtMsgType type, const QMessageLogContext& ctx, const QString& message)
{
    if( s_oldHandler )
        s_oldHandler(type, ctx, message );
    report(type,message);
}

static QSet<QString> s_builtIns = QSet<QString>() << "ABS" << "ARCTAN" << "CHR" << "DISPOSE" << "EOF"
                                                          << "EOLN" << "EXP" << "GET" << "LN" << "NEW" << "ODD"
                                                          << "ORD" << "PACK" << "PAGE" << "PRED" << "PUT" << "READ"
                                                          << "READLN" << "RESET" << "REWRITE" << "ROUND" << "SIN"
                                                          << "SQR" << "SQRT" << "SUCC" << "TRUNC" << "UNPACK"
                                                          << "WRITE" << "WRITELN"
                                                          << "REAL" << "INTEGER" << "LONGINT" << "BOOLEAN"
                                                          << "STRING" << "EXIT" << "TRUE" << "FALSE" << "CHAR"
                                                          << "MARK" << "RELEASE" << "ORD4" << "POINTER"
                                                          << "PWROFTEN" << "LENGTH" << "POS" << "CONCAT"
                                                          << "COPY" << "DELETE" << "INSERT" << "MOVELEFT"
                                                          << "MOVERIGHT" << "SIZEOF" << "SCANEQ" << "SCANNE"
                                                          << "FILLCHAR" << "COS" << "HALT";
static QList<QByteArray> s_keywords = QList<QByteArray>() << "ABSTRACT" << "CLASSWIDE" << "OVERRIDE" << "DEFAULT";

class CodeNavigator::Viewer : public QPlainTextEdit
{
public:
    QString d_path;
    typedef QList<QTextEdit::ExtraSelection> ESL;
    ESL d_link, d_nonTerms;
    CodeNavigator* d_that;
    Symbol* d_goto;
    Highlighter* d_hl;
    QString d_find;

    Viewer(CodeNavigator* p):QPlainTextEdit(p),d_that(p),d_goto(0)
    {
        setReadOnly(true);
        setLineWrapMode( QPlainTextEdit::NoWrap );
        setTabStopWidth( 30 );
        setTabChangesFocus(true);
        setMouseTracking(true);
        d_hl = new Highlighter( document() );
        foreach( const QString& w, s_builtIns )
            d_hl->addBuiltIn(w.toUtf8());
        foreach( const QByteArray& w, s_keywords )
            d_hl->addKeyword(w);

        QFont monospace("Monospace",9);
        if( !monospace.exactMatch() )
        {
            monospace = QFont("DejaVu Sans Mono",9);
            monospace.setStyleName("Book");
        }
        setFont(monospace);
    }

    bool loadFile( const QString& path )
    {
        if( d_path == path )
            return true;
        d_path = path;

        QFile in(d_path);
        if( !in.open(QIODevice::ReadOnly) )
            return false;
        QByteArray buf = in.readAll();
        buf.chop(1);
        setPlainText( QString::fromLatin1(buf) );
#ifdef LISA_CHECK_COVERAGE
        testMarkAllNonTerm(); // TEST
#endif
        that()->syncModuleList();
        return true;
    }

    CodeNavigator* that() { return d_that; }

    void mouseMoveEvent(QMouseEvent* e)
    {
        QPlainTextEdit::mouseMoveEvent(e);
        if( QApplication::keyboardModifiers() == Qt::ControlModifier )
        {
            QTextCursor cur = cursorForPosition(e->pos());
            Symbol* id = that()->d_mdl->findSymbolBySourcePos(d_path,cur.blockNumber() + 1,
                                                                          cur.positionInBlock() + 1);
            const bool alreadyArrow = !d_link.isEmpty();
            d_link.clear();
            if( id && id->d_decl )
            {
                const int off = cur.positionInBlock() + 1 - id->d_loc.d_col;
                cur.setPosition(cur.position() - off);
                cur.setPosition( cur.position() + id->d_decl->getLen(), QTextCursor::KeepAnchor );

                QTextEdit::ExtraSelection sel;
                sel.cursor = cur;
                sel.format.setFontUnderline(true);
                d_link << sel;
                d_goto = id;
                if( !alreadyArrow )
                    QApplication::setOverrideCursor(Qt::ArrowCursor);
            }
            if( alreadyArrow && d_link.isEmpty() )
                QApplication::restoreOverrideCursor();
            updateExtraSelections();
        }else if( !d_link.isEmpty() )
        {
            QApplication::restoreOverrideCursor();
            d_link.clear();
            updateExtraSelections();
        }
    }

    void mousePressEvent(QMouseEvent* e)
    {
        QPlainTextEdit::mousePressEvent(e);
        QTextCursor cur = cursorForPosition(e->pos());
        d_that->pushLocation( CodeNavigator::Place(
                                  FilePos(RowCol(cur.blockNumber()+1, cur.positionInBlock()+1),d_path),
                                                    verticalScrollBar()->value() ) );
        if( !d_link.isEmpty() )
        {
            QApplication::restoreOverrideCursor();
            d_link.clear();
            Q_ASSERT( d_goto );
            setPosition( d_goto->d_decl->getLoc(), false, true );
        }else if( QApplication::keyboardModifiers() == Qt::ControlModifier )
        {
            Symbol* id = that()->d_mdl->findSymbolBySourcePos(
                        d_path,cur.blockNumber() + 1,cur.positionInBlock() + 1);
            if( id && id->d_decl )
            {
                setPosition( id->d_decl->getLoc(), false, true );
            }
        }else
            updateExtraSelections();
    }

    void updateExtraSelections()
    {
        ESL sum;

        QTextEdit::ExtraSelection line;
        line.format.setBackground(QColor(Qt::yellow).lighter(150));
        line.format.setProperty(QTextFormat::FullWidthSelection, true);
        line.cursor = textCursor();
        line.cursor.clearSelection();
        sum << line;

        sum << d_nonTerms;

        sum << d_link;

        setExtraSelections(sum);
    }

    void setPosition(const FilePos& pos, bool center, bool pushPosition )
    {
        const int line = pos.d_pos.d_row - 1;
        const int col = pos.d_pos.d_col - 1;
        loadFile( pos.d_filePath );
        // Qt-Koordinaten
        if( line >= 0 && line < document()->blockCount() )
        {
            QTextBlock block = document()->findBlockByNumber(line);
            QTextCursor cur = textCursor();
            cur.setPosition( block.position() + col );
            setTextCursor( cur );
            if( center )
                centerCursor();
            else
                ensureCursorVisible();
            updateExtraSelections();
            if( pushPosition )
                that()->pushLocation(Place(pos,verticalScrollBar()->value()));
        }
    }

    void setCursorPosition(int line, int col, bool center, int sel )
    {
        // Qt-Koordinaten
        if( line >= 0 && line < document()->blockCount() )
        {
            QTextBlock block = document()->findBlockByNumber(line);
            QTextCursor cur = textCursor();
            cur.setPosition( block.position() + col );
            if( sel > 0 )
                cur.setPosition( block.position() + col + sel, QTextCursor::KeepAnchor );
            setTextCursor( cur );
            if( center )
                centerCursor();
            else
                ensureCursorVisible();
            updateExtraSelections();
        }
    }

    void markNonTermsFromCursor()
    {
        QTextCursor cur = textCursor();
        Symbol* id = that()->d_mdl->findSymbolBySourcePos(d_path,cur.blockNumber() + 1,cur.positionInBlock() + 1);
        if( id && id->d_decl && id->d_decl->isDeclaration() )
        {
            Declaration* d = static_cast<Declaration*>(id->d_decl);
            QList<Symbol*> syms = d->d_refs.value(d_path);
            markNonTerms(syms);
        }
    }

    void testMarkAllNonTerm()
    {
        // TEST
        d_nonTerms.clear();
        QTextCharFormat missing;
        missing.setBackground( QColor(Qt::red).lighter(170));
        QTextCharFormat underline;
        underline.setFontUnderline(true);
        underline.setUnderlineColor(Qt::magenta);
        underline.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        UnitFile* uf = that()->d_mdl->getUnitFile(d_path);
        if( uf == 0 )
            return;
        const UnitFile::SymList& syms = uf->d_syms.value(d_path);
        foreach( const Symbol* n, syms )
        {
#if 1
            if( n->d_decl != 0 )
                continue;
#endif
            RowCol loc = n->d_loc;
            QTextCursor c( document()->findBlockByNumber( loc.d_row - 1) );
            c.setPosition( c.position() + loc.d_col - 1 );
            if( n->d_decl )
                c.setPosition( c.position() + n->d_decl->getLen(), QTextCursor::KeepAnchor );
            else
            {
                c.movePosition(QTextCursor::EndOfWord,QTextCursor::KeepAnchor);
                if( s_builtIns.contains(c.selectedText().toUpper()) )
                    continue;
            }

            QTextEdit::ExtraSelection sel;
            sel.format = n->d_decl ? underline : missing;
            sel.cursor = c;

            d_nonTerms << sel;
        }
        updateExtraSelections();
    }

    void markNonTerms(const QList<Symbol*>& list)
    {
        d_nonTerms.clear();
        QTextCharFormat format;
        format.setBackground( QColor(247,245,243).darker(120) );
        foreach( const Symbol* sym, list )
        {
            if( sym->d_decl == 0 )
                continue;
            QTextCursor c( document()->findBlockByNumber( sym->d_loc.d_row - 1) );
            c.setPosition( c.position() + sym->d_loc.d_col - 1 );
            c.setPosition( c.position() + sym->d_decl->getLen(), QTextCursor::KeepAnchor );

            QTextEdit::ExtraSelection sel;
            sel.format = format;
            sel.cursor = c;

            d_nonTerms << sel;
        }
        updateExtraSelections();
    }

    void find( bool fromTop )
    {
        QTextCursor cur = textCursor();
        int line = cur.block().blockNumber();
        int col = cur.positionInBlock();

        if( fromTop )
        {
            line = 0;
            col = 0;
        }else
            col++;
        const int count = document()->blockCount();
        int pos = -1;
        const int start = qMax(line,0);
        bool turnedAround = false;
        for( int i = start; i < count; i++ )
        {
            pos = document()->findBlockByNumber(i).text().indexOf( d_find, col, Qt::CaseInsensitive );
            if( pos != -1 )
            {
                line = i;
                col = pos;
                break;
            }else if( i < count )
                col = 0;
            if( pos == -1 && start != 0 && !turnedAround && i == count - 1 )
            {
                turnedAround = true;
                i = -1;
            }
        }
        if( pos != -1 )
        {
            setCursorPosition( line, col, true, d_find.size() );
        }
    }
};

CodeNavigator::CodeNavigator(QWidget *parent) : QMainWindow(parent),d_pushBackLock(false)
{
    QWidget* pane = new QWidget(this);
    QVBoxLayout* vbox = new QVBoxLayout(pane);
    vbox->setMargin(0);
    vbox->setSpacing(0);

    d_pathTitle = new QLabel(this);
    d_pathTitle->setMargin(2);
    d_pathTitle->setWordWrap(true);
    d_pathTitle->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vbox->addWidget(d_pathTitle);

    d_view = new Viewer(this);
    vbox->addWidget(d_view);

    setCentralWidget(pane);

    setDockNestingEnabled(true);
    setCorner( Qt::BottomRightCorner, Qt::RightDockWidgetArea );
    setCorner( Qt::BottomLeftCorner, Qt::LeftDockWidgetArea );
    setCorner( Qt::TopRightCorner, Qt::RightDockWidgetArea );
    setCorner( Qt::TopLeftCorner, Qt::LeftDockWidgetArea );

    createModuleList();
    createUsedBy();
    createLog();

    connect( d_view, SIGNAL( cursorPositionChanged() ), this, SLOT(  onCursorPositionChanged() ) );

    QSettings s;
    const QVariant state = s.value( "DockState" );
    if( !state.isNull() )
        restoreState( state.toByteArray() );

    new QShortcut(tr("ALT+LEFT"),this,SLOT(onGoBack()) );
    new QShortcut(tr("ALT+RIGHT"),this,SLOT(onGoForward()) );
    new QShortcut(tr("CTRL+Q"),this,SLOT(close()) );
    new QShortcut(tr("CTRL+L"),this,SLOT(onGotoLine()) );
    new QShortcut(tr("CTRL+F"),this,SLOT(onFindInFile()) );
    new QShortcut(tr("CTRL+G"),this,SLOT(onFindAgain()) );
    new QShortcut(tr("F3"),this,SLOT(onFindAgain()) );
    new QShortcut(tr("F2"),this,SLOT(onGotoDefinition()) );
    new QShortcut(tr("CTRL+O"),this,SLOT(onOpen()) );

    s_this = this;
    s_oldHandler = qInstallMessageHandler(messageHander);

    setWindowTitle( tr("%1 v%2").arg( qApp->applicationName() ).arg( qApp->applicationVersion() ) );

    logMessage(tr("Welcome to %1 %2\nAuthor: %3\nSite: %4\nLicense: GPL\n").arg( qApp->applicationName() )
               .arg( qApp->applicationVersion() ).arg( qApp->organizationName() ).arg( qApp->organizationDomain() ));
    logMessage(tr("Shortcuts:"));
    logMessage(tr("CTRL+O to open the directory containing the Lisa Pascal files") );
    logMessage(tr("Double-click on the elements in the Modules or Uses lists to show in source code") );
    logMessage(tr("CTRL-click or F2 on the idents in the source to navigate to declarations") );
    logMessage(tr("CTRL+L to go to a specific line in the source code file") );
    logMessage(tr("CTRL+F to find a string in the current file") );
    logMessage(tr("CTRL+G or F3 to find another match in the current file") );
    logMessage(tr("ALT+LEFT to move backwards in the navigation history") );
    logMessage(tr("ALT+RIGHT to move forward in the navigation history") );
    logMessage(tr("ESC to close Message Log") );
}

CodeNavigator::~CodeNavigator()
{
    s_this = 0;
}

void CodeNavigator::open(const QString& sourceTreePath)
{
    d_msgLog->clear();
    d_usedBy->clear();
    d_view->d_path.clear();
    d_view->clear();
    d_pathTitle->clear();
    d_usedByTitle->clear();
    d_backHisto.clear();
    d_forwardHisto.clear();
    d_dir = sourceTreePath;
    QDir::setCurrent(sourceTreePath);
   setWindowTitle( tr("%3 - %1 v%2").arg( qApp->applicationName() ).arg( qApp->applicationVersion() )
                    .arg( QDir(sourceTreePath).dirName() ));
    QTimer::singleShot(500,this,SLOT(onRunReload()));
}

void CodeNavigator::logMessage(const QString& str)
{
    d_msgLog->parentWidget()->show();
    d_msgLog->appendPlainText(str);
}

void CodeNavigator::createModuleList()
{
    QDockWidget* dock = new QDockWidget( tr("Modules"), this );
    dock->setObjectName("Modules");
    dock->setAllowedAreas( Qt::AllDockWidgetAreas );
    dock->setFeatures( QDockWidget::DockWidgetMovable );
    d_things = new QTreeView(dock);
    d_things->setAlternatingRowColors(true);
    d_things->setHeaderHidden(true);
    d_things->setSortingEnabled(false);
    d_things->setAllColumnsShowFocus(true);
    d_things->setRootIsDecorated(true);
    d_things->setExpandsOnDoubleClick(false);
    d_mdl = new CodeModel(this);
    d_things->setModel(d_mdl);
    dock->setWidget(d_things);
    addDockWidget( Qt::LeftDockWidgetArea, dock );
    connect( d_things,SIGNAL(doubleClicked(QModelIndex)), this, SLOT(onModuleDblClick(QModelIndex)) );
}

void CodeNavigator::createUsedBy()
{
    QDockWidget* dock = new QDockWidget( tr("Uses"), this );
    dock->setObjectName("UsedBy");
    dock->setAllowedAreas( Qt::AllDockWidgetAreas );
    dock->setFeatures( QDockWidget::DockWidgetMovable );
    QWidget* pane = new QWidget(dock);
    QVBoxLayout* vbox = new QVBoxLayout(pane);
    vbox->setMargin(0);
    vbox->setSpacing(0);
    d_usedByTitle = new QLabel(pane);
    d_usedByTitle->setWordWrap(true);
    d_usedByTitle->setMargin(2);
    vbox->addWidget(d_usedByTitle);
    d_usedBy = new QTreeWidget(pane);
    d_usedBy->setAlternatingRowColors(true);
    d_usedBy->setHeaderHidden(true);
    d_usedBy->setSortingEnabled(false);
    d_usedBy->setAllColumnsShowFocus(true);
    d_usedBy->setRootIsDecorated(false);
    vbox->addWidget(d_usedBy);
    dock->setWidget(pane);
    addDockWidget( Qt::RightDockWidgetArea, dock );
    connect(d_usedBy, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(onUsedByDblClicked()) );
}

void CodeNavigator::createLog()
{
    QDockWidget* dock = new QDockWidget( tr("Message Log"), this );
    dock->setObjectName("Log");
    dock->setAllowedAreas( Qt::AllDockWidgetAreas );
    dock->setFeatures( QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable );
    d_msgLog = new QPlainTextEdit(dock);
    d_msgLog->setReadOnly(true);
    d_msgLog->setLineWrapMode( QPlainTextEdit::NoWrap );
    new LogPainter(d_msgLog->document());
    dock->setWidget(d_msgLog);
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    new QShortcut(tr("ESC"), dock, SLOT(close()) );
}

void CodeNavigator::pushLocation(const Place& loc)
{
    if( d_pushBackLock )
        return;
    if( !d_backHisto.isEmpty() && d_backHisto.last() == loc )
        return; // o ist bereits oberstes Element auf dem Stack.
    d_backHisto.removeAll( loc );
    d_backHisto.push_back( loc );
}

void CodeNavigator::showViewer(const CodeNavigator::Place& p)
{
    d_view->setPosition( p.d_loc, false, false );
    d_view->verticalScrollBar()->setValue(p.d_yoff);
}

static bool SymsLessThan( const Symbol* lhs, const Symbol* rhs )
{

    return lhs->d_loc.packed() < rhs->d_loc.packed();
}

void CodeNavigator::fillUsedBy(Symbol* id, Declaration* nt)
{
    d_usedBy->clear();
    if( !nt->d_name.isEmpty() )
        d_usedByTitle->setText(QString("%1 '%2'").arg(nt->typeName()).arg(nt->d_name.data()) );
    else
        d_usedByTitle->setText(QString("%1").arg(nt->typeName()) );

    QList< QPair<QString,Declaration::SymList> > all;
    Declaration::Refs::const_iterator dri;
    for( dri = nt->d_refs.begin(); dri != nt->d_refs.end(); ++dri )
    {
        Declaration::SymList list = dri.value();
        std::sort(list.begin(),list.end(), SymsLessThan);
        all << qMakePair(dri.key(),list);
    }


    QTreeWidgetItem* curItem = 0;
    for( int i = 0; i < all.size(); i++ )
    {
        const QString path = all[i].first;
        const FileSystem::File* file = d_mdl->getFs()->findFile(path);
        const QString fileName = file ? file->getVirtualPath(false) : QFileInfo(path).fileName();
        const Declaration::SymList& list = all[i].second;
        for( int j = 0; j < list.size(); j++ )
        {
            Symbol* sym = list[j];
            QTreeWidgetItem* item = new QTreeWidgetItem(d_usedBy);
            item->setText( 0, QString("%1 %2:%3%4").arg(fileName)
                        .arg(sym->d_loc.d_row).arg( sym->d_loc.d_col)
                           .arg( sym == nt->d_me ? " decl" : "" ) );
            if( id && sym->d_loc == id->d_loc && path == d_view->d_path )
            {
                QFont f = item->font(0);
                f.setBold(true);
                item->setFont(0,f);
                curItem = item;
            }
            item->setToolTip( 0, item->text(0) );
            item->setData( 0, Qt::UserRole, QVariant::fromValue(FilePos(sym->d_loc,path)) );
            item->setData( 0, Qt::UserRole+1, QVariant::fromValue(sym) );
            if( path != d_view->d_path )
                item->setForeground( 0, Qt::gray );
            else if( curItem == 0 )
                curItem = item;
        }
    }
    if( curItem )
        d_usedBy->scrollToItem( curItem );
}

void CodeNavigator::setPathTitle(const FileSystem::File* f, int row, int col)
{
    if( f == 0 )
        d_pathTitle->setText("<no file>");
    else
        d_pathTitle->setText(QString("%1 - %2 - %3:%4").
                         arg(f->getVirtualPath(true)).
                         arg(f->d_realPath).
                         arg(row).
                             arg(col));
}

void CodeNavigator::syncModuleList(Declaration* d)
{
    // sync the module list with what is pointed on
    QModelIndex i = d_mdl->findThing( d->getUnitFile() );
    if( i.isValid() )
    {
        d_things->setCurrentIndex(i);
        d_things->scrollTo( i ,QAbstractItemView::PositionAtCenter );
    }
}

void CodeNavigator::syncModuleList()
{
    QModelIndex i = d_mdl->findThing( d_mdl->getCodeFile(d_view->d_path) );
    if( i.isValid() )
    {
        d_things->setCurrentIndex(i);
        d_things->scrollTo( i ,QAbstractItemView::PositionAtCenter );
    }
}

void CodeNavigator::closeEvent(QCloseEvent* event)
{
    QSettings s;
    s.setValue( "DockState", saveState() );
    event->setAccepted(true);
}

void CodeNavigator::onCursorPositionChanged()
{
    QTextCursor cur = d_view->textCursor();
    const int line = cur.blockNumber() + 1;
    const int col = cur.positionInBlock() + 1;

    setPathTitle(d_mdl->getFs()->findFile(d_view->d_path), line, col);

    Symbol* id = d_mdl->findSymbolBySourcePos(d_view->d_path,line,col);
    if( id && id->d_decl && id->d_decl->isDeclaration() )
    {
        fillUsedBy( id, static_cast<Declaration*>(id->d_decl) );

        // mark all symbols in file which have the same declaration
        Declaration* d = static_cast<Declaration*>(id->d_decl);
        QList<Symbol*> syms = d->d_refs.value(d_view->d_path);
        d_view->markNonTerms(syms);

        // syncModuleList(d);
    }
}

void CodeNavigator::onModuleDblClick(const QModelIndex& i)
{
    const Thing* nt = d_mdl->getThing(i);

    if( nt == 0 )
        return;

    if( nt->d_kind == Thing::Unit )
    {
        const UnitFile* f = static_cast<const UnitFile*>(nt);
        d_view->loadFile(f->d_file->d_realPath);
    }else if( nt->d_kind == Thing::Include )
    {
        const IncludeFile* f = static_cast<const IncludeFile*>(nt);
        d_view->loadFile(f->d_file->d_realPath);
    }


    if( nt->isDeclaration() )
    {
        const Declaration* d = static_cast<const Declaration*>(nt);
        d_view->setPosition( d->getLoc(), true, false );
    }
}

void CodeNavigator::onUsedByDblClicked()
{
    if( d_usedBy->currentItem() == 0 )
        return;

    FilePos pos = d_usedBy->currentItem()->data(0,Qt::UserRole).value<FilePos>();
    if( pos.d_filePath.isEmpty() )
        return;
#if 0
    Symbol* sym = d_usedBy->currentItem()->data(0,Qt::UserRole+1).value<Symbol*>();
    if( sym && sym->d_decl && sym->d_decl->isDeclaration() )
        syncModuleList(static_cast<Declaration*>(sym->d_decl));
#endif
    d_view->setPosition( pos, true, true );
}

void CodeNavigator::onGoBack()
{
    if( d_backHisto.size() <= 1 )
        return;

    d_pushBackLock = true;
    d_forwardHisto.push_back( d_backHisto.last() );
    d_backHisto.pop_back();
    showViewer(d_backHisto.last());
    d_pushBackLock = false;

}

void CodeNavigator::onGoForward()
{
    if( d_forwardHisto.isEmpty() )
        return;
    Place cur = d_forwardHisto.last();
    d_forwardHisto.pop_back();
    showViewer(cur);
    d_backHisto.push_back(cur);
}

void CodeNavigator::onGotoLine()
{
    QTextCursor cur = d_view->textCursor();
    int line = cur.blockNumber();
    bool ok	= false;
    line = QInputDialog::getInt(
                this, tr("Goto Line"),
        tr("Enter a valid line number:"),
        line + 1, 1, 999999, 1,	&ok );
    if( !ok )
        return;
    QTextBlock block = d_view->document()->findBlockByNumber(line-1);
    cur.setPosition( block.position() );
    d_view->setTextCursor( cur );
    d_view->centerCursor();
    d_view->updateExtraSelections();
}

void CodeNavigator::onFindInFile()
{
    bool ok	= false;
    const QString sel = d_view->textCursor().selectedText();
    QString res = QInputDialog::getText( this, tr("Find in File"),
        tr("Enter search string:"), QLineEdit::Normal, sel, &ok );
    if( !ok )
        return;
    d_view->d_find = res;
    d_view->find( sel.isEmpty() );
}

void CodeNavigator::onFindAgain()
{
    if( !d_view->d_find.isEmpty() )
        d_view->find( false );
}

void CodeNavigator::onGotoDefinition()
{
    QTextCursor cur = d_view->textCursor();
    Symbol* id = d_mdl->findSymbolBySourcePos(
                d_view->d_path,cur.blockNumber() + 1,cur.positionInBlock() + 1);
    if( id && id->d_decl )
    {
        FilePos pos = id->d_decl->getLoc();
        d_view->setPosition( pos, true, true );
    }
}

void CodeNavigator::onOpen()
{
    QString path = QFileDialog::getExistingDirectory(this,tr("Open Project Directory"),QDir::currentPath() );
    if( path.isEmpty() )
        return;
    open(path);
}

void CodeNavigator::onRunReload()
{
    QElapsedTimer t;
    t.start();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    d_mdl->load(d_dir);
    QApplication::restoreOverrideCursor();
    qDebug() << "parsed" << d_mdl->getSloc() << "SLOC in" << t.elapsed() << "[ms]";
}


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("me@rochus-keller.ch");
    a.setOrganizationDomain("github.com/rochus-keller/LisaPascal");
    a.setApplicationName("LisaCodeNavigator");
    a.setApplicationVersion("0.7.0");
    a.setStyle("Fusion");
    QFontDatabase::addApplicationFont(":/fonts/DejaVuSansMono.ttf"); 
#ifdef Q_OS_LINUX
    QFontDatabase::addApplicationFont(":/fonts/NotoSans.ttf"); 
    QFont af("Noto Sans",9);
    a.setFont(af);
#endif


    QString dirPath;
    const QStringList args = QCoreApplication::arguments();
    for( int i = 1; i < args.size(); i++ )
    {
        if( !args[ i ].startsWith( '-' ) )
        {
            if( !dirPath.isEmpty() )
            {
                qCritical() << "error: only one argument (path to source tree) supported";
                return -1;
            }
            dirPath = args[ i ];
        }else
        {
            qCritical() << "error: invalid command line option " << args[i] << endl;
            return -1;
        }
    }

    CodeNavigator* w = new CodeNavigator();
    w->showMaximized();
    if( !dirPath.isEmpty() )
        w->open(dirPath);

    const int res = a.exec();
    delete w;

    return res;
}
