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

#include "mcucomponentpin.h"


McuComponentPin::McuComponentPin( McuComponent *mcuComponent, QString id, QString type, QString label, int pos, int xpos, int ypos, int angle )
               : QObject( mcuComponent )
               , eSource( id.toStdString(), 0l )
{
    m_id    = id;
    m_type  = type;
    m_angle = angle;
    
    m_pinType = 0;
    
    m_mcuComponent = mcuComponent;
    
    m_attached = false;
    m_isInput  = true;
    m_openColl = false;

    Pin* pin = new Pin( angle, QPoint (xpos, ypos), mcuComponent->itemID()+"-"+id, pos, m_mcuComponent );
    pin->setLabelText( label );
    m_ePin[0] = pin;

    eSource::setImp( high_imp );
    eSource::setVoltHigh( 5 );
    eSource::setOut( false );
    
    type = type.toLower();
    if( type == "gnd" 
     || type == "vdd" 
     || type == "vcc" 
     || type == "unused" 
     || type == "nc" ) 
     pin->setUnused( true );
     
    resetState();
}

McuComponentPin::~McuComponentPin()
{
}

void McuComponentPin::terminate()
{
    m_attached = false;
}

void McuComponentPin::resetState()
{
    if( m_pinType == 1 )
    {
        eSource::setImp( high_imp );// All  IO Pins should be inputs at start-up
        eSource::setOut(false);
        eSource::stamp();
    }
}

void McuComponentPin::initialize()
{
    //if( m_pinType == 1 ) eSource::setImp( high_imp );// All  IO Pins should be inputs at start-up
    
    if( m_ePin[0]->isConnected() && m_attached )
        m_ePin[0]->getEnode()->addToChangedFast(this);
        
    //if( m_pinType == 21 ) BaseProcessor::self()->hardReset( true );
    eSource::initialize();
}

void McuComponentPin::resetOutput()
{
    eSource::setOut(false);
    eSource::stampOutput();
}

void McuComponentPin::move( int dx, int dy )
{
    pin()->moveBy( dx, dy );
}


#include "moc_mcucomponentpin.cpp"
