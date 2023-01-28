#ifndef LISACODENAVIGATOR_H
#define LISACODENAVIGATOR_H

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

#include <QMainWindow>

class QLabel;
class QPlainTextEdit;
class QTreeView;
class QTreeWidget;
class QModelIndex;

namespace Lisa
{
class CodeModel;
class Symbol;

class CodeNavigator : public QMainWindow
{
    Q_OBJECT
public:
    explicit CodeNavigator(QWidget *parent = 0);
    void open( const QString& sourceTreePath);
    void logMessage(const QString&);

protected:
    void createModuleList();
    void createUsedBy();
    void createLog();
    void pushLocation( Symbol*, const QString& path );

    // overrides
    void closeEvent(QCloseEvent* event);

protected slots:
    void onCursorPositionChanged();
    void onModuleDblClick(const QModelIndex&);
    void onUsedByDblClicked();
    void onGoBack();
    void onGoForward();
    void onGotoLine();
    void onFindInFile();
    void onFindAgain();
    void onGotoDefinition();
    void onOpen();

private:
    class Viewer;
    Viewer* d_view;
    QLabel* d_loc;
    QPlainTextEdit* d_msgLog;
    QTreeView* d_things;
    QLabel* d_usedByTitle;
    QTreeWidget* d_usedBy;
    CodeModel* d_mdl;
    QString d_dir;

    struct Place
    {
        Symbol* d_sym;
        QString d_path;
        Place(Symbol* s = 0, const QString& p = QString() ):d_sym(s),d_path(p){}
    };
    Place d_cur;
    QList<Place> d_backHisto;
    QList<Place> d_forwardHisto;
    bool d_pushBackLock;
};
}

#endif // LISACODENAVIGATOR_H
