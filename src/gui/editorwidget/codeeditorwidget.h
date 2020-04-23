/***************************************************************************
 *   Copyright (C) 2012 by santiago González                               *
 *   santigoro@gmail.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.  *
 *                                                                         *
 ***************************************************************************/

#ifndef CODEEDITORWIDGET_H
#define CODEEDITORWIDGET_H

#include <QWidget>

#include "codeeditor.h"
#include "ramtable.h"
#include "outpaneltext.h"

class MainWindow;

class CodeEditorWidget : public QWidget
{
    Q_OBJECT

    public:
        CodeEditorWidget( QWidget *parent );
        ~CodeEditorWidget();

        CodeEditor   *m_codeEditor;

    public slots:
        void setVisible( bool visible );

    private:
        void createWidgets();

        QGridLayout *baseWidgetLayout;

        RamTable     *m_ramTable;
        OutPanelText *m_outPane;
        MainWindow   *m_mainWindow;
};

#endif // CODEEDITORWIDGET_H
