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
using namespace Lisa;

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

static QList<QByteArray> s_builtIns = QList<QByteArray>() << "ABS" << "ARCTAN" << "CHR" << "DISPOSE" << "EOF"
                                                          << "EOLN" << "EXP" << "GET" << "LN" << "NEW" << "ODD"
                                                          << "ORD" << "PACK" << "PAGE" << "PRED" << "PUT" << "READ"
                                                          << "READLN" << "RESET" << "REWRITE" << "ROUND" << "SIN"
                                                          << "SQR" << "SQRT" << "SUCC" << "TRUNC" << "UNPACK"
                                                          << "WRITE" << "WRITELN"
                                                          << "REAL" << "INTEGER" << "LONGINT" << "BOOLEAN"
                                                          << "STRING" << "EXIT" << "TRUE" << "FALSE"
                                                          << "MARK" << "RELEASE" << "ORD4" << "POINTER"
                                                          << "PWROFTEN" << "LENGTH" << "POS" << "CONCAT"
                                                          << "COPY" << "DELETE" << "INSERT" << "MOVELEFT"
                                                          << "MOVERIGHT" << "SIZEOF" << "SCANEQ" << "SCANNE"
                                                          << "FILLCHAR";

class CodeNavigator::Viewer : public QPlainTextEdit
{
public:
    QString d_path;
    typedef QList<QTextEdit::ExtraSelection> ESL;
    ESL d_link, d_nonTerms;
    CodeNavigator* d_that;
    Declaration* d_goto;
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
        foreach( const QByteArray& w, s_builtIns )
            d_hl->addBuiltIn(w);
        QFont f; // TODO
        f.setStyleHint( QFont::TypeWriter );
        f.setFamily("Mono");
        f.setPointSize(9);
        setFont(f);
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
                cur.setPosition( cur.position() + id->d_decl->d_name.size(), QTextCursor::KeepAnchor );

                QTextEdit::ExtraSelection sel;
                sel.cursor = cur;
                sel.format.setFontUnderline(true);
                d_link << sel;
                d_goto = id->d_decl;
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
        if( !d_link.isEmpty() )
        {
            QApplication::restoreOverrideCursor();
            d_link.clear();
            Q_ASSERT( d_goto );
            setCursorPosition( d_goto->d_loc, d_goto->d_owner->getCodeFile()->d_file->d_realPath, true );
        }else if( QApplication::keyboardModifiers() == Qt::ControlModifier )
        {
            QTextCursor cur = cursorForPosition(e->pos());
            Symbol* id = that()->d_mdl->findSymbolBySourcePos(
                        d_path,cur.blockNumber() + 1,cur.positionInBlock() + 1);
            if( id && id->d_decl )
            {
                //pushLocation( Location( cur.blockNumber(), cur.positionInBlock() ) );
                setCursorPosition( id->d_loc, id->d_decl->d_owner->getCodeFile()->d_file->d_realPath, true );
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

    void setCursorPosition(RowCol loc, const QString& path, bool center )
    {
        const int line = loc.d_row - 1;
        const int col = loc.d_col - 1;
        loadFile( path );
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
        }
    }

    void setCursorPosition(int line, int col, bool center, int sel = -1 )
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

    d_loc = new QLabel(this);
    d_loc->setMargin(2);
    vbox->addWidget(d_loc);

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

void CodeNavigator::open(const QString& sourceTreePath)
{
    d_msgLog->clear();
    d_usedBy->clear();
    d_view->d_path.clear();
    d_view->clear();
    d_loc->clear();
    d_usedByTitle->clear();
    d_cur = Place();
    d_backHisto.clear();
    d_forwardHisto.clear();
    d_mdl->load(sourceTreePath);
    d_dir = sourceTreePath;
    setWindowTitle( tr("%3 - %1 v%2").arg( qApp->applicationName() ).arg( qApp->applicationVersion() )
                    .arg( QDir(sourceTreePath).dirName() ));

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

void CodeNavigator::pushLocation(Symbol* s, const QString& path)
{
    if( d_pushBackLock || s == 0 )
        return;
    if( d_cur.d_sym == s && d_cur.d_path == path )
        return;
    if( d_cur.d_sym )
        d_backHisto.push_back( d_cur );
    d_cur = Place(s,path);
}

void CodeNavigator::closeEvent(QCloseEvent* event)
{
    QSettings s;
    s.setValue( "DockState", saveState() );
    event->setAccepted(true);
}

void CodeNavigator::onCursorPositionChanged()
{
    // TODO
}

void CodeNavigator::onModuleDblClick(const QModelIndex& i)
{
    const Thing* nt = d_mdl->getThing(i);

    if( nt == 0 )
        return;

    if( nt->d_type == Thing::File )
    {
        const CodeFile* f = static_cast<const CodeFile*>(nt);
        d_loc->setText(f->d_file->getVirtualPath());
        d_loc->setToolTip(f->d_file->d_realPath);
        d_view->loadFile(f->d_file->d_realPath);
    }

    if( nt->isDeclaration() )
    {
        const Declaration* d = static_cast<const Declaration*>(nt);
        FileSystem::File* f = d->d_owner->getCodeFile()->d_file;
        d_loc->setText(f->getVirtualPath());
        d_view->setCursorPosition( d->d_loc, f->d_realPath, true );
    }
#if 0
    // TODO
    else
        fillUsedBy( 0, nt );
#endif
}

void CodeNavigator::onUsedByDblClicked()
{
    // TODO
}

void CodeNavigator::onGoBack()
{
    if( d_backHisto.isEmpty() )
        return;
    d_pushBackLock = true;
    Place last = d_backHisto.last();
    d_backHisto.pop_back();
    d_forwardHisto.push_back( d_cur );
    d_cur = last;
    d_view->setCursorPosition( last.d_sym->d_loc, last.d_path, true );
    d_pushBackLock = false;
}

void CodeNavigator::onGoForward()
{
    if( d_forwardHisto.isEmpty() )
        return;
    Place last = d_forwardHisto.last();
    d_forwardHisto.pop_back();
    d_backHisto.push_back( d_cur );
    d_cur = last;
    d_view->setCursorPosition( d_cur.d_sym->d_loc, d_cur.d_path, true );
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
        d_view->setCursorPosition( id->d_decl->d_loc,
                                   id->d_decl->d_owner->getCodeFile()->d_file->d_realPath, true );
#if 0
    // TODO
    if( id.second )
    {
        QModelIndex i = d_ntm->findSymbol( id.second );
        if( i.isValid() )
        {
            d_things->setCurrentIndex(i);
            d_things->scrollTo( i ,QAbstractItemView::PositionAtCenter );
        }
    }
#endif
}

void CodeNavigator::onOpen()
{
    QString path = QFileDialog::getExistingDirectory(this,tr("Open Project Directory"),QDir::currentPath() );
    if( path.isEmpty() )
        return;
    open(path);
}


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("me@rochus-keller.ch");
    a.setOrganizationDomain("github.com/rochus-keller/LisaPascal");
    a.setApplicationName("LisaCodeNavigator");
    a.setApplicationVersion("0.1.0");
    a.setStyle("Fusion");

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

    CodeNavigator w;
    if( !dirPath.isEmpty() )
        w.open(dirPath);
    w.showMaximized();

    return a.exec();
}
