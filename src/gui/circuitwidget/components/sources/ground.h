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

#ifndef GROUND_H
#define GROUND_H

#include "e-source.h"
#include "component.h"
#include "pin.h"

class LibraryItem;

class MAINMODULE_EXPORT Ground : public Component
{
    Q_OBJECT
    public:
        QRectF boundingRect() const { return QRect( -10, -10, 20, 20 ); }

        Ground( QObject* parent, QString type, QString id );
        ~Ground();

        static Component* construct( QObject* parent, QString type, QString id );
        static LibraryItem *libraryItem();

        virtual void paint( QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget );

    public slots:
        virtual void remove();

    private:
        double m_Rth;

        ePin *groundpin;
        eSource *m_out;
};

#endif
