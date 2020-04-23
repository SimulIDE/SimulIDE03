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

 
#ifndef PICCOMPONENTPIN_H
#define PICCOMPONENTPIN_H

#include "mcucomponentpin.h"

class pic_processor;
class IOPIN;

class PICComponentPin : public McuComponentPin
{
    Q_OBJECT
        public:
        PICComponentPin( McuComponent *mcu, QString id, QString type, QString label, int pos, int xpos, int ypos, int angle );
        ~PICComponentPin();

        void attach( pic_processor *PicProcessor );
        
        void setVChanged();
        void resetOutput();
        
        // GpSim Interface
        void update_direction( bool out );
        void update_state( bool state );
        void update_pullup( bool pullup );

    protected:
        char m_port;
        int  m_pinN;
        int  m_pos;

        bool m_state;

        pic_processor *m_PicProcessor;
        IOPIN         *m_pIOPIN;
};

#endif
