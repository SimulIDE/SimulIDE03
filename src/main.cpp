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

#include <QApplication>
#include <QTranslator>

#include "mainwindow.h"

int main(int argc, char *argv[])
{

#ifdef _WIN32
    QStringList paths = QCoreApplication::libraryPaths();
    paths.append("plugins");
    paths.append("plugins/platforms");
    paths.append("plugins/imageformats");
    paths.append("plugins/sqldrivers");
    paths.append("plugins/bearer");
    paths.append("plugins/generic");
    paths.append("plugins/iconengines");
    paths.append("plugins/qmltooling");
    paths.append("plugins/printsupport");
    QCoreApplication::setLibraryPaths(paths);
    
    if (AttachConsole(ATTACH_PARENT_PROCESS)) 
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif

    //QApplication::setGraphicsSystem( "raster" );//native, raster, opengl
    QApplication app( argc, argv );

    QString locale   = QLocale::system().name().split("_").first();
    QString langFile = "../share/simulide/translations/simulide_"+locale+".qm";
    
    QFile file( langFile );
    if( !file.exists() ) langFile = "../share/simulide/translations/simulide_en.qm";
    
    QTranslator translator;
    translator.load( langFile );
    app.installTranslator( &translator );

    MainWindow window;
    
    /*QRect screenGeometry = QApplication::desktop()->screenGeometry();
    int x = ( screenGeometry.width()-window.width() ) / 2;
    int y = ( screenGeometry.height()-window.height() ) / 2;
    window.move( x, y );*/
    window.scroll( 0, 50 );

    window.show();
    app.setApplicationVersion( APP_VERSION );
    return app.exec();
}

