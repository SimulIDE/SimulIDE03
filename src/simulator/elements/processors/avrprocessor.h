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

#ifndef AVRPROCESSOR_H
#define AVRPROCESSOR_H

#include <QtGui>
#include <QHash>

#include "baseprocessor.h"

// simavr includes
#include "sim_avr.h"
struct avr_t;

class AvrProcessor : public BaseProcessor
{
    Q_OBJECT
    public:
        AvrProcessor( QObject* parent=0 );
        ~AvrProcessor();

        bool loadFirmware( QString file );
        void terminate();

        void reset();
        void step();
        void stepOne();
        void stepCpu();
        int pc();

        int getRamValue( int address );

        QVector<int> eeprom();
        
        avr_t* getCpu() { return m_avrProcessor; }
        void setCpu( avr_t* avrProc ) { m_avrProcessor = avrProc; }

        void uartIn( uint32_t value );
        
        static void uart_pty_out_hook( struct avr_irq_t* irq, uint32_t value, void* param )
        {
            Q_UNUSED(irq);
            // get the pointer out of param and asign it to AvrProcessor*
            AvrProcessor* ptrAvrProcessor = reinterpret_cast<AvrProcessor*> (param);
            
            ptrAvrProcessor->uartOut( value );
        }

    private:
        virtual int  validate( int address );

        //From simavr
        avr_t*     m_avrProcessor;
        avr_irq_t* m_uartInIrq;
};


#endif
