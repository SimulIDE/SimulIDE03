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

#include "circuit.h"
#include "itemlibrary.h"
#include "mainwindow.h"
#include "circuitwidget.h"
#include "propertieswidget.h"
#include "subpackage.h"
#include "connectorline.h"
#include "simuapi_apppath.h"
#include "node.h"
#include "utils.h"

#include "switch.h" // Delete in later versions (0.3.10)

static const char* Circuit_properties[] = {
    QT_TRANSLATE_NOOP("App::Property","Speed"),
    QT_TRANSLATE_NOOP("App::Property","ReactStep"),
    QT_TRANSLATE_NOOP("App::Property","NoLinStep"),
    QT_TRANSLATE_NOOP("App::Property","NoLinAcc"),
    QT_TRANSLATE_NOOP("App::Property","Draw Grid"),
    QT_TRANSLATE_NOOP("App::Property","Show ScrollBars")
};

Circuit* Circuit::m_pSelf = 0l;

Circuit::Circuit( qreal x, qreal y, qreal width, qreal height, QGraphicsView*  parent)
       : QGraphicsScene(x, y, width, height, parent)
{
    Q_UNUSED( Circuit_properties );
    
    setObjectName( "Circuit" );
    setParent( parent );
    m_graphicView = parent;
    m_scenerect.setRect( x, y, width, height );
    setSceneRect( QRectF(x, y, width, height) );

    m_pSelf = this;

    m_changed     = false;
    m_pasting     = false;
    m_deleting    = false;
    m_con_started = false;

    new_connector = 0l;
    m_seqNumber   = 0;
    
    m_hideGrid   = MainWindow::self()->settings()->value( "Circuit/hideGrid" ).toBool();
    m_showScroll = MainWindow::self()->settings()->value( "Circuit/showScroll" ).toBool();
    m_filePath = qApp->applicationDirPath()+"/new.simu";

    connect( &m_bckpTimer, SIGNAL(timeout() ), this, SLOT( saveChanges()) );
    //m_bckpTimer.start( m_autoBck*1000 );
}

Circuit::~Circuit()
{
    m_bckpTimer.stop();

    // Avoid PropertyEditor problem: comps not unregistered
    QPropertyEditorWidget::self()->removeObject( this );

    foreach( Component* comp, m_compList )
    {
        QPropertyEditorWidget::self()->removeObject( comp );
    }

    // Clear Undo/Redo stacks
    foreach( QDomDocument* doc, m_redoStack ) delete doc;
    foreach( QDomDocument* doc, m_undoStack ) delete doc;
    m_undoStack.clear();
    m_redoStack.clear();

    if( !m_backupPath.isEmpty() )
    {
        QFile::remove( m_backupPath ); // Remove backup file
    }
}

QList<Component*>* Circuit::compList() { return &m_compList; }
QList<Component*>* Circuit::conList()  { return &m_conList; }

int Circuit::noLinAcc()
{
    return Simulator::self()->noLinAcc();
}

void Circuit::setNoLinAcc( int ac )
{
    Simulator::self()->setNoLinAcc( ac );
}

int Circuit::reactStep()
{
    return Simulator::self()->reaClock();
}

void Circuit::setReactStep( int steps )
{
    Simulator::self()->setReaClock( steps );
}

int Circuit::noLinStep()
{
    return Simulator::self()->noLinClock();
}

void Circuit::setNoLinStep( int steps )
{
    Simulator::self()->setNoLinClock( steps );
}

int Circuit::circSpeed()
{
    return Simulator::self()->simuRate();
}
void Circuit::setCircSpeed( int rate )
{
    Simulator::self()->simuRateChanged( rate );
}

void Circuit::removeItems()                     // Remove Selected items
{
    if( m_con_started ) return;
    
    bool pauseSim = Simulator::self()->isRunning();
    if( pauseSim ) Simulator::self()->pauseSim();

    saveState();

    foreach( Component* comp, m_compList )
    {
        bool isNode = comp->objectName().contains( "Node" ); // Don't remove Graphical Nodes
        if( comp->isSelected() && !isNode )  removeComp( comp );
    }
    
    QList<QGraphicsItem*> itemlist = selectedItems();
    while( !itemlist.isEmpty() )
    {
        QList<Connector*> connectors;
        
        foreach( QGraphicsItem* item, itemlist )
        {
            ConnectorLine* line =  qgraphicsitem_cast<ConnectorLine* >( item );
            if( line->objectName() == "" ) 
            {
                Connector* con = line->connector();
                if( !connectors.contains( con ) ) connectors.append( con );
            }
        }
        foreach( Connector* con, connectors ) con->remove();
        itemlist = selectedItems();
    }
    if( pauseSim ) Simulator::self()->runContinuous();
}

void Circuit::removeComp( Component* comp )
{
    m_compRemoved = false;
    comp->remove();
    if( !m_compRemoved ) return;
    
    QPropertyEditorWidget::self()->removeObject( comp );
    compList()->removeOne( comp );
    if( items().contains( comp ) ) removeItem( comp );
    //comp->deleteLater();
    delete comp;
}

void Circuit::compRemoved( bool removed ) // Arduino doesn't like to be removed while circuit is running
{
    m_compRemoved = removed;
}

void Circuit::remove() // Remove everything
{
    if( m_con_started ) return;
    
    //qDebug() << m_compList.size();
    m_deleting = true;

    foreach( Component* comp, m_compList )
    {
        //qDebug() << "Circuit::remove" << comp->itemID();
        
        bool isNumber = false;               // Don't remove internal items
        
        comp->objectName().split("-").last().toInt( &isNumber ); // TODO: Find a proper way !!!!!!!!!!!
        
        bool isNode = comp->objectName().contains( "Node" );// Don't remove Graphical Nodes

        if( isNumber && !isNode )  removeComp( comp );
    }
    m_deleting = false;
}

bool Circuit::deleting()
{
    return m_deleting;
}

void Circuit::saveState()
{
    if( m_con_started ) return;
    
    //qDebug() << "saving state";
    foreach( QDomDocument* doc, m_redoStack ) delete doc;

    m_redoStack.clear();

    circuitToDom();
    m_undoStack.append( new QDomDocument() );
    m_undoStack.last()->setContent( m_domDoc.toString() );

    m_changed = true;

    QString title = MainWindow::self()->windowTitle();
    if( !title.endsWith('*') ) MainWindow::self()->setWindowTitle(title+'*');
}

void Circuit::saveChanges()
{
    //qDebug() << "Circuit::saveChanges";
    if( !m_changed ) return;
    if( m_con_started ) return;
    m_changed = false;

    circuitToDom();

    m_backupPath = m_filePath;
    
    QFileInfo bckDir( m_backupPath );
    
    if( !bckDir.isWritable() )
        m_backupPath = SIMUAPI_AppPath::self()->RWDataFolder().absolutePath()+"/_backup.simu";

    if( !m_backupPath.endsWith( "_backup.simu" ))
        m_backupPath.replace( ".simu", "_backup.simu" );

    //qDebug() << "saving Backup"<<m_backupPath;

    if( saveDom( m_backupPath, &m_domDoc ) )
        MainWindow::self()->settings()->setValue( "backupPath", m_backupPath );
}

void Circuit::setChanged()
{
    m_changed = true;
}

bool Circuit::drawGrid()
{
    return !m_hideGrid;
}
void Circuit::setDrawGrid( bool draw )
{
    m_hideGrid = !draw;
    if( m_hideGrid ) MainWindow::self()->settings()->setValue( "Circuit/hideGrid", "true" );
    else             MainWindow::self()->settings()->setValue( "Circuit/hideGrid", "false" );
    update();
}

bool Circuit::showScroll()
{
    return m_showScroll;
}

void Circuit::setShowScroll( bool show )
{
    m_showScroll = show;
    if( show )
    {
        m_graphicView->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
        m_graphicView->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
        MainWindow::self()->settings()->setValue( "Circuit/showScroll", "true" );
    }
    else
    {
        m_graphicView->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        m_graphicView->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        MainWindow::self()->settings()->setValue( "Circuit/showScroll", "false" );
    }
}

bool Circuit::animate()
{
    return m_animate;
}

void Circuit::setAnimate( bool an )
{
    m_animate = an;
    update();
}

double Circuit::fontScale() 
{ 
    return MainWindow::self()->fontScale(); 
}

void Circuit::setFontScale( double scale )
{ 
    MainWindow::self()->setFontScale( scale ); 
}

int Circuit::autoBck()
{
    return MainWindow::self()->autoBck();
}

void Circuit::setAutoBck( int secs )
{
    //qDebug() << "Circuit::setAutoBck"<< secs;
    m_bckpTimer.stop();
    if( secs < 1 ) secs = 0;
    else           m_bckpTimer.start( secs*1000 );

    MainWindow::self()->setAutoBck( secs );
}

void Circuit::drawBackground ( QPainter*  painter, const QRectF & rect )
{
    Q_UNUSED( rect );
    /*painter->setBrush(QColor( 255, 255, 255 ) );
    painter->drawRect( m_scenerect );*/

    painter->setBrush( QColor( 240, 240, 210 ) );
    painter->drawRect( m_scenerect );
    painter->setPen( QColor( 210, 210, 210 ) );

    if( m_hideGrid ) return;
    
    int startx = int(m_scenerect.x());///2;
    int endx   = int(m_scenerect.width())/2;
    int starty = int(m_scenerect.y());///2;
    int endy   = int(m_scenerect.height())/2;

    for( int i = 4; i<endx; i+=8 )
    {
        painter->drawLine( i, starty, i, endy );
        painter->drawLine(-i, starty,-i, endy );
    }
    for( int i = 4; i<endy; i+=8 )
    {
        painter->drawLine( startx, i, endx, i);
        painter->drawLine( startx,-i, endx,-i);
    }
}

QString Circuit::getCompId( QString name )
{
    QStringList nameSplit = name.split("-");
    if( nameSplit.isEmpty() ) return "";

    QString compId  = nameSplit.takeFirst();
    if( nameSplit.isEmpty() ) return "";

    QString compNum = nameSplit.takeFirst();

    return compId+"-"+compNum;
}

Pin* Circuit::findPin( int x, int y, QString id )
{
    QRectF itemRect = QRectF ( x-4, y-4, 8, 8 );

    QList<QGraphicsItem*> list = items( itemRect ); // List of items in (x, y)
    foreach( QGraphicsItem* it, list )
    {
        Pin* pin =  qgraphicsitem_cast<Pin*>( it );

        if( pin && (pin->pinId().left(1) == id.left(1)) ) // Test if names start by same letter
        {
            return pin;
        }
    }
    foreach( QGraphicsItem* it, list ) // Not found by first letter, take first Pin
    {
        Pin* pin =  qgraphicsitem_cast<Pin*>( it );
        if( pin && !pin->isConnected() ) return pin;
    }
    return 0l;
}

void Circuit::importCirc(  QPointF eventpoint  )
{
    if( m_con_started ) return;
    
    m_pasting = true;

    m_deltaMove = QPointF( 160, 160 );//togrid(eventpoint);

    const QString dir = m_filePath;
    QString fileName = QFileDialog::getOpenFileName( 0l, tr("Load Circuit"), dir,
                                          tr("Circuits (*.simu);;All files (*.*)"));

    if( !fileName.isEmpty() && fileName.endsWith(".simu") )
        loadCircuit( fileName );
        
    m_pasting = false;
}

void Circuit::loadCircuit( QString &fileName )
{
    if( m_con_started ) return;
    
    m_filePath = fileName;
    QFile file( fileName );

    if( !file.open(QFile::ReadOnly | QFile::Text) )
    {
        QMessageBox::warning(0l, "Circuit::loadCircuit",
        tr("Cannot read file %1:\n%2.").arg(fileName).arg(file.errorString()));
        return;
    }
    //qDebug() << "Circuit::loadCircuit"<<m_filePath;

    if( !m_domDoc.setContent(&file) )
    {
        QMessageBox::warning( 0l, "Circuit::loadCircuit",
        tr("Cannot set file %1\nto DomDocument").arg(fileName));
        file.close();
        return;
    }
    file.close();

    m_error = 0;
    loadDomDoc( &m_domDoc );
    m_domDoc.clear();
    
    if( m_error != 0 ) 
    {
        remove();
        foreach( Component* comp, m_compList ) removeComp( comp ); // Clean Nodes
    }
    else m_graphicView->centerOn( QPointF( 1200+itemsBoundingRect().center().x(), 950+itemsBoundingRect().center().y() ) );
    
    foreach( Component* comp, *(conList()) )
    {
        Connector* con = static_cast<Connector*>( comp );
        con->startPin()->isMoved();
        con->endPin()->isMoved();
    }
    if( MainWindow::self()->autoBck() )
    {
        saveState();
        saveChanges();
    }
}

void Circuit::loadDomDoc( QDomDocument* doc )
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    //int firstSeqNumber = m_seqNumber+1;
    QList<Component*> compList;   // Component List
    QList<Component*> conList;    // Connector List
    QList<Node*>      jointList;  // Joint List
    QHash<QString, QString> idMap;
    QHash<QString, eNode*> nodMap;
    m_animate = false;

    QDomElement circuit = doc->documentElement();
    //QString docType = circuit.attribute("type");
    
    if( circuit.hasAttribute( "speed" ))     setCircSpeed( circuit.attribute("speed").toInt() );
    if( circuit.hasAttribute( "reactStep" )) setReactStep( circuit.attribute("reactStep").toInt() );
    if( circuit.hasAttribute( "noLinStep" )) setNoLinStep( circuit.attribute("noLinStep").toInt() );
    if( circuit.hasAttribute( "noLinAcc" ))  setNoLinAcc( circuit.attribute("noLinAcc").toInt() );
    if( circuit.hasAttribute( "animate" ))   setAnimate( circuit.attribute("animate").toInt() );

    QDomNode    node = circuit.firstChild();

    while( !node.isNull() )
    {
        QDomElement   element = node.toElement();
        const QString tagName = element.tagName();

        if( tagName == "item" )
        {
            QString objNam = element.attribute( "objectName"  ); // Data in simu file
            QString type   = element.attribute( "itemtype"  );
            QString id     = objNam.split("-").first()+"-"+newSceneId(); // Create new id

            element.setAttribute( "objectName", id  );

            if( type == "Connector" )
            {
                Pin* startpin  = 0l;
                Pin* endpin    = 0l;
                QString startpinid    = element.attribute( "startpinid" );
                QString endpinid      = element.attribute( "endpinid" );
                QString startCompName = getCompId( startpinid );
                QString endCompName   = getCompId( endpinid );

                startpinid.replace( startCompName, idMap[startCompName] );
                endpinid.replace( endCompName, idMap[endCompName] );

                startpin = m_pinMap[startpinid];
                endpin   = m_pinMap[endpinid];
                
                if( !startpin ) // Pin not found by name... find it by pos
                {
                    QStringList pointList   = element.attribute( "pointList" ).split(",");
                    int itemX = pointList.first().toInt();
                    int itemY = pointList.at(1).toInt();

                    startpin = findPin( itemX, itemY, startpinid );
                }
                if( !endpin ) // Pin not found by name... find it by pos
                {
                    QStringList pointList   = element.attribute( "pointList" ).split(",");
                    int itemX = pointList.at(pointList.size()-2).toInt();
                    int itemY = pointList.last().toInt();

                    endpin = findPin( itemX, itemY, endpinid );
                }

                if( m_pasting )
                {
                    if( startpin && !startpin->component()->isSelected() ) startpin = 0l;
                    if( endpin   && !endpin->component()->isSelected() )   endpin = 0l;
                }
                if( startpin && startpin->isConnected() ) startpin = 0l;
                if( endpin   && endpin->isConnected() )   endpin   = 0l;

                if( startpin && endpin )    // Create Connector
                {
                    Connector* con  = new Connector( this, type, id, startpin, endpin );

                    element.setAttribute( "startpinid", startpin->pinId() );
                    element.setAttribute(   "endpinid", endpin->pinId() );

                    loadProperties( element, con );

                    QString enodeId = element.attribute( "enodeid" );
                    eNode*  enode   = nodMap[enodeId];
                    if( !enode )                    // Create eNode and add to enodList
                    {
                        enode = new eNode( "Circ_eNode-"+newSceneId() );
                        nodMap[enodeId] = enode;
                    }
                    con->setEnode( enode );

                    QStringList plist = con->pointList();   // add lines to connector
                    int p1x = snapToGrid( plist.first().toInt() );
                    int p1y = snapToGrid( plist.at(1).toInt() );
                    int p2x = snapToGrid( plist.at(plist.size()-2).toInt() );
                    int p2y = snapToGrid( plist.last().toInt() );

                    con->addConLine( con->x(),con->y(), p1x, p1y, 0 );

                    int count = plist.size();
                    for (int i=2; i<count; i+=2)
                    {
                        p2x = snapToGrid( plist.at(i).toInt() );
                        p2y = snapToGrid( plist.at(i+1).toInt() );
                        con->addConLine( p1x, p1y, p2x, p2y, i/2 );
                        p1x = p2x;
                        p1y = p2y;
                    }
                    con->updateConRoute( startpin, startpin->scenePos() );
                    con->updateConRoute( endpin, endpin->scenePos() );
                    con->remNullLines();
                    conList.append( con );
                }
                else // Start or End pin not found
                {
                    if( !startpin ) qDebug() << "\n   ERROR!!  Circuit::loadDomDoc:  null startpin in " << objNam << startpinid;
                    if( !endpin )   qDebug() << "\n   ERROR!!  Circuit::loadDomDoc:  null endpin in "   << objNam << endpinid;
                }
            }
            else if( type == "Node")
            {
                idMap[objNam] = id;                              // Map simu id to new id
                
                Node* joint = new Node( this, type, id );
                loadProperties( element, joint );
                joint->moveTo( togrid( joint->pos() ) );
                compList.append( joint );
                jointList.append( joint );

                if( m_pasting ) joint->setSelected( true );
            }
            else if( type == "LEDSMD" ); // TODO: this type shouldnt be saved to circuit
                                         // bcos is created inside another component, for example boards
            else if( type == "Plotter")
            {
                loadObjectProperties( element, PlotterWidget::self() );
            }
            else if( type == "SerialPort")
            {
                loadObjectProperties( element, SerialPortWidget::self() );
            }
            else
            {
                idMap[objNam] = id;                              // Map simu id to new id
                
                Component* item = 0l;
                
                if( (type == "InBus")||( type == "OutBus") ) type = "Bus";
                else if( type == "Ram8bit" ) type = "Memory";
                
                if( type == "ToggleSwitch" ) item = createItem( "Switch", id );
                else                         item = createItem( type, id );
                
                if( item )
                {
                    loadProperties( element, item );
                    item->moveTo( togrid( item->pos() ) );
                    compList.append( item );
                    if( m_pasting ) item->setSelected( true );
                }
                else 
                {
                    qDebug() << " ERROR Creating Component: "<< type << id;
                    QApplication::restoreOverrideCursor();
                    m_error = 1;
                    return;
                }
                if( type == "ToggleSwitch" )
                {
                    Switch* sw = static_cast<Switch*>( item );
                    sw->setDt( true );
                }
            }
        }
        node = node.nextSibling();
    }
    if( m_pasting )
    {
        foreach( Component *item, compList )
        {
            item->move( m_deltaMove );
        }
        foreach( Component* item, conList )
        {
            Connector* con = static_cast<Connector*>( item );
            con->setSelected( true );
            con->move( m_deltaMove );
        }
    }
    // Take care about unconnected Joints
    foreach( Node* joint, jointList ) joint->remove(); // Only removed if some missing connector
    
    QApplication::restoreOverrideCursor();
}

bool Circuit::saveCircuit( QString &fileName )
{
    if( m_con_started ) return false;
    
    QApplication::setOverrideCursor(Qt::WaitCursor);

    if( !fileName.endsWith(".simu") ) fileName.append(".simu");

    circuitToDom();

    bool saved = saveDom( fileName, &m_domDoc );

    if( saved && !m_backupPath.isEmpty() )
    {
        QFile::remove( m_backupPath ); // remove backup file
        m_backupPath = "";
        m_filePath = fileName;
    }
    QApplication::restoreOverrideCursor();
    return saved;
}

bool Circuit::saveDom( QString &fileName, QDomDocument* doc )
{
    QFile file( fileName );

    if( !file.open(QFile::WriteOnly | QFile::Text) )
    {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(0l, "Circuit::saveCircuit",
        tr("Cannot write file %1:\n%2.").arg(fileName).arg(file.errorString()));
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << doc->toString();
    file.close();

    return true;
}

void Circuit::bom()
{
    if( m_con_started ) return;
    
    QString fileName = m_filePath; 
    fileName.replace( fileName.lastIndexOf( ".simu" ), 5, "-bom.txt" );
    
    fileName = QFileDialog::getSaveFileName( MainWindow::self()
                            , tr( "Bill Of Materials" )
                            , fileName
                            , "(*.*)"  );

    if( fileName.isEmpty() ) return;
    
    QStringList bom;
    
    foreach( Component* comp, m_compList )
    {
        bool isNumber = false;
        comp->objectName().split("-").last().toInt( &isNumber );

        if( isNumber ) bom.append( comp->print() );
    }

    QFile file( fileName );

    if( !file.open(QFile::WriteOnly | QFile::Text) )
    {
          QMessageBox::warning(0l, "Circuit::bom",
          tr("Cannot write file %1:\n%2.").arg(fileName).arg(file.errorString()));
    }
    bom.sort();
    
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out <<  "\nCircuit: ";
    out <<  QFileInfo( m_filePath ).fileName();
    out <<  "\n\n";
    out <<  "Bill of Materials:\n\n";
    foreach( QString line, bom ) out << line;
    
    file.close();
}

void Circuit::circuitToDom()
{
    m_domDoc.clear();
    QDomElement circuit = m_domDoc.createElement("circuit");
    
    circuit.setAttribute( "type",      "simulide_0.1" );
    circuit.setAttribute( "speed",     QString::number( circSpeed() ) );
    circuit.setAttribute( "reactStep", QString::number( reactStep() ) );
    circuit.setAttribute( "noLinStep", QString::number( noLinStep() ) );
    circuit.setAttribute( "noLinAcc",  QString::number( noLinAcc() ) );
    circuit.setAttribute( "animate",  QString::number( animate() ) );
    //circuit.setAttribute( "drawGrid",    QString( drawGrid()?"true":"false"));
    //circuit.setAttribute( "showScroll",  QString( showScroll()?"true":"false"));
    
    m_domDoc.appendChild(circuit);

    listToDom( &m_domDoc, &m_compList );

    foreach( Component* comp, m_conList )
    {
        Connector* con = static_cast<Connector*>( comp );
        con->remNullLines();
    }
    listToDom( &m_domDoc, &m_conList );
    
    objectToDom( &m_domDoc, PlotterWidget::self() );
    objectToDom( &m_domDoc, SerialPortWidget::self() );

    circuit.appendChild( m_domDoc.createTextNode( "\n \n" ) );
}

void Circuit::listToDom( QDomDocument* doc, QList<Component*>* complist )
{
    int count = complist->count();
    for( int i=0; i<count; i++ )
    {
        Component* item = complist->at(i);

        // Don't save internal items
        bool isNumber = false;
        item->objectName().split("-").last().toInt( &isNumber );

        if( isNumber ) objectToDom( doc, item );
    }
}

void Circuit::objectToDom( QDomDocument* doc, QObject* object )
{
    QDomElement root = doc->firstChild().toElement();
    QDomElement elm = m_domDoc.createElement("item");
    const QMetaObject* metaobject = object->metaObject();

    int count = metaobject->propertyCount();
    for( int i=0; i<count; i++ )
    {
        QMetaProperty metaproperty = metaobject->property(i);
        const char* name = metaproperty.name();

        QVariant value = object->property( name );
        if( metaproperty.type() == QVariant::StringList )
        {
            QStringList list= value.toStringList();
            elm.setAttribute( name, list.join(",") );
        }
        else if( (QString(name)=="Mem") || (QString(name)=="eeprom") )
        {            
            QVector<int> vmem = value.value<QVector<int>>();
            
            QStringList list;
            foreach( int val, vmem ) list << QString::number( val );
                
            elm.setAttribute( name, list.join(",") );
            
            //qDebug() << "typename" << value.typeName();
            //qDebug() << "Value:\n" << vmem;
            //qDebug() << "Data:\n" << list;
            //qDebug() << "type" << value.type()<< "typename" << value.typeName()<< "name " << name
            //     << "   value " << value << "saved" << value.toString();
        }
        else 
        {
            elm.setAttribute( name, value.toString() );
            /*if( QString(name)=="Mem" )
            qDebug() << "type" << value.type()<< "typename" << value.typeName()<< "name " << name
                 << "   value " << value << "saved" << value.toString();*/
        }

    }
    QDomText blank = m_domDoc.createTextNode( "\n \n" );
    QDomText objNme = m_domDoc.createTextNode( object->objectName() );
    root.appendChild( blank );
    root.appendChild( objNme );
    blank = m_domDoc.createTextNode( ": \n" );
    root.appendChild( blank );
    root.appendChild( elm );
}

void Circuit::undo()
{
    if( m_con_started ) return;
    
    if( m_undoStack.isEmpty() ) return;

    bool pauseSim = Simulator::self()->isRunning();
    if( pauseSim ) Simulator::self()->stopSim();

    circuitToDom();
    m_redoStack.prepend( new QDomDocument() );
    m_redoStack.first()->setContent( m_domDoc.toString() );

    remove();
    QDomDocument* doc = m_undoStack.takeLast();
    m_domDoc.setContent( doc->toString());

    m_seqNumber = 0;
    loadDomDoc( &m_domDoc );

    if( pauseSim ) Simulator::self()->runContinuous();
}

void Circuit::redo()
{
    if( m_con_started ) return;
    
    if( m_redoStack.isEmpty() ) return;

    bool pauseSim = Simulator::self()->isRunning();
    if( pauseSim ) Simulator::self()->stopSim();

    circuitToDom();
    m_undoStack.append( new QDomDocument() );
    m_undoStack.last()->setContent( m_domDoc.toString() );

    remove();
    QDomDocument* doc = m_redoStack.takeFirst();
    m_domDoc.setContent( doc->toString());

    m_seqNumber = 0;
    loadDomDoc( &m_domDoc );

    if( pauseSim ) Simulator::self()->runContinuous();
}

void Circuit::updatePin(ePin* epin, std::string newId )
{
    QString pinId = QString::fromStdString( newId );
    Pin* pin = static_cast<Pin*>( epin );

    addPin( pin, pinId );
}

void Circuit::addPin( Pin* pin, QString pinId )
{
    m_pinMap[ pinId ] = pin;
}

void Circuit::removePin( QString pinId )
{
    m_pinMap.remove( pinId );
}

Component* Circuit::createItem( QString type, QString id )
{
    //qDebug() << "Circuit::createItem" << type << id;
    foreach( LibraryItem* libItem, ItemLibrary::self()->items() )
    {
        if( libItem->type()==type )
        {
            Component* comp = libItem->createItemFnPtr()( this, type, id );
            
            if( comp )
            {
                QString category = libItem->category();
                if( ( category != "Meters" )
                &&  ( category != "Sources" )
                &&  ( category != "Other" ) )
                    comp->setPrintable( true );
            }
            return comp;
        }
    }
    return 0l;
}

void Circuit::loadProperties( QDomElement element, Component* Item )
{
    loadObjectProperties( element, Item );
    
    Item->setLabelPos();
    Item->setValLabelPos();

    addItem(Item);

    int number = Item->objectName().split("-").last().toInt();

    if ( number > m_seqNumber ) m_seqNumber = number;               // Adjust item counter: m_seqNumber
}

void Circuit::loadObjectProperties( QDomElement element, QObject* Item )
{
    const QMetaObject* metaobject = Item->metaObject();
    int count = metaobject->propertyCount();
    
    for( int i=0; i<count; ++i )
    {
        QMetaProperty metaproperty = metaobject->property(i);
        const char* chName = metaproperty.name();
        QString n = chName;

        if( !element.hasAttribute( chName ) ) // Take care of new capitalization in some properties
        {
            n.replace(0, 1, n[0].toLower());
            if( !element.hasAttribute( n.toUtf8() ) ) continue;
        }
        QVariant value( element.attribute( n.toUtf8() ) );
        
        if     ( metaproperty.type() == QVariant::Int    ) Item->setProperty( chName, value.toInt() );
        else if( metaproperty.type() == QVariant::Double ) Item->setProperty( chName, value.toDouble() );
        else if( metaproperty.type() == QVariant::PointF ) Item->setProperty( chName, value.toPointF() );
        else if( metaproperty.type() == QVariant::Bool   ) Item->setProperty( chName, value.toBool() );
        else if( metaproperty.type() == QVariant::StringList )
        {
            QStringList list= value.toString().split(",");
            Item->setProperty( chName, list );
        }
        else if( (n=="Mem") || (n=="eeprom") )
        {
            QStringList list = value.toString().split(",");
            
            QVector<int> vmem;
            int lsize = list.size();
            vmem.resize( lsize );
            //qDebug() << "Circuit::loadObjectProperties eeprom size:" << lsize;
            
            for( int x=0; x<lsize; x++ )
            {
                vmem[x] = list.at(x).toInt();
                //qDebug() << x << vmem[x];
            }
            QVariant value = QVariant::fromValue( vmem );
            Item->setProperty( chName, value );
        }
        else Item->setProperty( chName, value );
        //else qDebug() << "    ERROR!!! Circuit::loadObjectProperties\n  unknown type:  "<<"name "<<name<<"   value "<<value ;
    }
}

void Circuit::copy( QPointF eventpoint )
{
    if( m_con_started ) return;
    
    m_eventpoint = togrid(eventpoint);

    QList<Component*> complist;

    QList<QGraphicsItem*> itemlist = selectedItems();

    foreach( QGraphicsItem* item , itemlist )
    {
        Component* comp =  qgraphicsitem_cast<Component*>( item );
        if( comp )
        {
            if( comp->itemType() == "Connector" )
            {
                Connector* con = static_cast<Connector*>( comp );
                con->remNullLines();

                complist.append( con );
            }
            else
            {
                complist.prepend( comp );
            }
        }
    }
    m_copyDoc.clear();
    QDomElement root = m_copyDoc.createElement("circuit");
    root.setAttribute( "type", "simulide_0.1" );
    m_copyDoc.appendChild(root);

    listToDom( &m_copyDoc, &complist );

    QString px = QString::number( m_eventpoint.x() );
    QString py = QString::number( m_eventpoint.y() );
    QString clipTextText = px+","+py+"eventpoint"+m_copyDoc.toString();
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText( clipTextText );
}

void Circuit::paste( QPointF eventpoint )
{
    if( m_con_started ) return;
    
    QClipboard *clipboard = QApplication::clipboard();
    QString clipText = clipboard->text();
    if( !clipText.contains( "eventpoint") ) return;

    bool pauseSim = Simulator::self()->isRunning();
    if( pauseSim ) Simulator::self()->stopSim();
    
    bool animate = m_animate;
    saveState();
    m_pasting = true;
    foreach( QGraphicsItem*item, selectedItems() ) item->setSelected( false );

    QStringList clipData = clipText.split( "eventpoint" );
    clipText = clipData.last();
    m_copyDoc.setContent( clipText );

    clipData = clipData.first().split(",");
    int px = clipData.first().toInt();
    int py = clipData.last().toInt();
    m_eventpoint = QPointF( px, py );

    m_deltaMove = togrid(eventpoint) - m_eventpoint;

    loadDomDoc( &m_copyDoc );

    m_pasting = false;
    setAnimate( animate );

    if( pauseSim ) Simulator::self()->runContinuous();
}

bool  Circuit::pasting() { return m_pasting; }
QPointF Circuit::deltaMove(){ return m_deltaMove; }

void Circuit::createSubcircuit()
{
    if( m_con_started ) return;
    
    QString fileName = m_filePath;
    fileName.replace( m_filePath.lastIndexOf( ".simu" ), 5, "" );
    fileName = QFileDialog::getSaveFileName( MainWindow::self()
                            , tr( "Create Subcircuit" )
                            , fileName
                            , "All files (*)"  );

    QFileInfo fi( fileName );
    QString ext = fi.suffix();
    QString filePath = fileName;
    if( !ext.isEmpty() ) filePath.remove( fileName.lastIndexOf( ext )-1, ext.size()+1 );
    //qDebug() <<"Circuit::createSubcircuit filePath"<< filePath;
    
    QHash<QString, QString> compList;        // Get Components properties

    //qDebug() << compIdTip<<"--------------------------";
    foreach( Component* component, m_compList )
    {
        if( component->itemType() == "Package" )
        {
            SubPackage* pkg = (static_cast<SubPackage*>(component));

            QString ext = ".package";
            if( pkg->logicSymbol() ) ext = "_LS.package";

            pkg->savePackage( filePath+ext );
        }
        
        QString compId     = component->objectName();
        QString propString = "";

        const QMetaObject* metaObject = component->metaObject();

        int count = metaObject->propertyCount();
        for( int i=0; i<count; ++i )
        {
            QMetaProperty property = metaObject->property(i);
            if( property.isUser() )
            {
                QString name = property.name();
                
                if( !name.contains( "Show" ) 
                 && !name.contains( "Unit" ) 
                 && !name.contains( "itemtype" ) )
                {
                    QString valString = "";
                    
                    if(( name == "Resistance" ) 
                     | ( name == "Capacitance" )
                     | ( name == "Inductance" )
                     | ( name == "Voltage" )
                     | ( name == "Current" ))
                    {
                        valString = QString::number( component->getmultValue() );
                    }
                    else
                    {
                        const char* charname = property.name();

                        QVariant value = component->property( charname );
                        valString = value.toString();
                    }
                    if( name == "Functions" ) valString = valString.replace("&", "&amp;");
                    if( name == "id") ;//compId = valString;
                    else
                    {
                        name[0] = name[0].toLower();
                        name = name.replace( "_", "" );
                        propString += "        "+name+" = \""+valString+"\"\n";
                    }
                }
            }
        }
        compList[compId] = propString;
    }
    QList<eNode*> eNodeList = simulator.geteNodes();
    QList<QStringList> connectionList;

    int nodes = 0;
    foreach( eNode* node,  eNodeList  ) // Get all the connections in each eNode
    {
        //qDebug() << "\nCircuit::createSubcircuit New Node ";
        if( ! node ) continue;
        
        QStringList pinConList;
        QList<ePin*> pinList =  node->getEpins();

        foreach( ePin* epin,  pinList  )
        {
            Pin* pin = (static_cast<Pin*>(epin));
            Component* component = pin->component();
            QString    compId    = component->itemID();
            QString    compType  = component->itemType();
            QString    pinId     = pin->pinId().split( "-" ).last().replace( " ", "" );
            
            if( compType == "Package" )
            {
                if( pin->inverted() && !pinId.startsWith( "!" ) ) pinId = "!"+pinId;
                
                pinConList.prepend( "Package_"+pinId );
            }
            else if( ( compType == "Probe" )
                   ||( compType == "Fixed Voltage" ) )
            {
                // Take care about "packagepin" bad spelling
                //compId = compId.replace( test.indexOf("packagepin"), 10, "packagePin");
                pinConList.prepend( "Package_"+component->idLabel().replace("-","") );
            }
            else if( compId.contains( "Node") ) ;
            else
            {
                pinConList.append( compId );
                pinConList.append( compType );
                pinConList.append( pinId );
            }
        }
        QString conType = "Node";
        if( pinConList.length() == 4 ) conType = "Connection";

        if( conType == "Connection" )               // PackagePin to pin
        {
            QString pin1   = pinConList.takeLast();     // Component pin
            QString pin2   = pinConList.takeFirst();      // Package Pin
            QString compty = pinConList.takeFirst();   // Component type
            QString compId = pinConList.takeFirst();     // Component Id

            pinConList << compty << compId << pin1+"-"+pin2;
            connectionList.append( pinConList );
            //qDebug() << "Circuit::createSubcircuit PackagePin to pin\n" << pinConList;
        }
        else                                      // Multiple connection
        {
            QString pin2   = "eNode"+QString::number(nodes);
            bool isNode = true;

            int packPins = 0;
            for( QString entry : pinConList )
            {
                if( entry.contains("Package") ) // No Node, connection to packagePin
                {
                    pin2 = entry;
                    pinConList.removeOne( entry );
                    isNode = false;
                    packPins++;
                    //break;
                }
            }
            if( packPins > 1 )                 // 2 Package Pins connected together
            {
                MessageBoxNB( "Circuit::createSubcircuit", "                               \nERROR:\n"+
                          tr( "2 Package Pins connected together" ) );
                return;
            }
            while( !pinConList.isEmpty() ) // Create connection entries
            {
                QStringList pinConList2;
                QString compId = pinConList.takeFirst();
                QString compty = pinConList.takeFirst();
                QString pin1   = pinConList.takeFirst();

                pinConList2 << compId << compty << pin1+"-"+pin2;
                connectionList.append( pinConList2 );
                //qDebug() << "Circuit::createSubcircuit Multiple connection\n" << pinConList2;
            }
            if( isNode ) nodes++;
        }
    }
    QString subcircuit = "<!DOCTYPE SimulIDE>\n";
    subcircuit += "<!-- This file was generated by SimulIDE -->\n\n";
    subcircuit += "<subcircuit enodes=\""+QString::number(nodes)+"\">\n\n";

    while( !connectionList.isEmpty() )
    {
        QStringList list = connectionList.takeFirst();
        if( list.isEmpty() ) continue;

        QString compId = list.takeFirst();
        QString compty = "e"+list.takeFirst().replace( " ", "");
        QString conect = list.takeFirst();

        subcircuit += "    <item itemtype=\""+compty+"\"\n";
        //subcircuit += "    <item ";
        subcircuit += compList[compId];                        // Properties
        subcircuit += "        connections=\"";
        subcircuit += "\n        "+conect;

        int counter = -1;
        foreach( QStringList list2, connectionList )
        {
            counter++;
            if( list2.isEmpty() ) continue;
            QString compId2 = list2.at(0);

            if( compId == compId2)
            {
                list2.removeFirst();
                list2.removeFirst();
                QString conect2 = list2.takeFirst();
                subcircuit += "\n        "+conect2;
                connectionList.replace( counter, list2 );
            }
        }
        subcircuit +="\" >\n    </item>\n\n";
    }
    subcircuit +="</subcircuit>";
        
    QFile file( filePath+".subcircuit" );

    if( !file.open(QFile::WriteOnly | QFile::Text) )
    {
          QMessageBox::warning(0l, "Circuit::createSubcircuit",
          tr("Cannot write file %1:\n%2.").arg(fileName).arg(file.errorString()));
    }
    QTextStream out( &file );
    out.setCodec("UTF-8");
    out << subcircuit;
    file.close();
    //qDebug() <<"Circuit::createSubcircuit\n" << subcircuit;
}

QString Circuit::newSceneId()
{
    return QString("%1").arg(++m_seqNumber) ;
}

void Circuit::newconnector( Pin*  startpin )
{
    saveState();

    //if ( m_subcirmode ) return;
    m_con_started = true;

    QString type = QString("Connector");
    QString id = type;
    id.append( "-" );
    id.append( newSceneId() );

    new_connector = new Connector( this, type, id, startpin );

    QPoint p1 = startpin->scenePos().toPoint();
    QPoint p2 = startpin->scenePos().toPoint();

    new_connector->addConLine( p1.x(), p1.y(), p2.x(), p2.y(), 0 );

    addItem(new_connector);
}

void Circuit::closeconnector( Pin* endpin )
{
    m_con_started = false;
    new_connector->closeCon( endpin, /*connect=*/true );
}

void Circuit::updateConnectors()
{
    foreach( Component* comp, m_conList )
    {
        Connector* con = static_cast<Connector*>( comp );
        con->updateLines();
    }
}

void Circuit::constarted( bool started ) { m_con_started = started; }
bool Circuit::is_constarted() { return m_con_started ; }

void Circuit::mousePressEvent( QGraphicsSceneMouseEvent* event )
{
    if( event->button() == Qt::LeftButton )
    {
        QPropertyEditorWidget::self()->setObject( this );
        PropertiesWidget::self()->setHelpText( MainWindow::self()->circHelp() );

        if( m_con_started )  event->accept();//new_connector->incActLine() ;
        QGraphicsScene::mousePressEvent( event );
    }
    else if( event->button() == Qt::RightButton )
    {
        if( m_con_started ) event->accept();
        else                QGraphicsScene::mousePressEvent( event );
    }
}

void Circuit::mouseReleaseEvent( QGraphicsSceneMouseEvent* event )
{
    if( event->button() == Qt::LeftButton )
    {
        if( m_con_started )  new_connector->incActLine() ;
        QGraphicsScene::mouseReleaseEvent( event );
    }
    else if( event->button() == Qt::RightButton )
    {
        if( m_con_started )
        {
            event->accept();
            new_connector->remove();
            m_con_started = false;
        }
        else QGraphicsScene::mouseReleaseEvent( event );
    }
}

void Circuit::mouseMoveEvent( QGraphicsSceneMouseEvent* event )
{
    if( m_con_started )
    {
        event->accept();
        
        if(event->modifiers() & Qt::ShiftModifier)
        {
            new_connector->m_freeLine = true;
        }
        new_connector->updateConRoute( 0l, event->scenePos() );
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void Circuit::keyPressEvent( QKeyEvent* event )
{
    if( m_con_started ) return;
    
    int key = event->key();

    if( event->modifiers() & Qt::ControlModifier )
    {
        if( key == Qt::Key_C )
        {
            QPoint p = CircuitWidget::self()->mapFromGlobal(QCursor::pos());
            copy( m_graphicView->mapToScene( p ) );
            clearSelection();
        }
        else if( key == Qt::Key_V )
        {
            QPoint p = CircuitWidget::self()->mapFromGlobal(QCursor::pos());
            paste( m_graphicView->mapToScene( p ) );
        }
        else if( key == Qt::Key_S )
        {
            if( event->modifiers() & Qt::ShiftModifier) CircuitWidget::self()->saveCircAs();
            else                                        CircuitWidget::self()->saveCirc();
        }
        else if( key == Qt::Key_Z ) { undo(); }
        else if( key == Qt::Key_Y ) { redo(); }
        else if( key == Qt::Key_N ) { CircuitWidget::self()->newCircuit(); }
        else if( key == Qt::Key_O ) { CircuitWidget::self()->openCirc(); }
        else if( key == Qt::Key_Plus ) { CircuitView::self()->zoom( 120 ); }
        else if( key == Qt::Key_Minus ) { CircuitView::self()->zoom( -120 ); }
        else QGraphicsScene::keyPressEvent( event );
    }
    else if( key == Qt::Key_Delete ) removeItems();
    else QGraphicsScene::keyPressEvent(event);
}

void Circuit::contextMenuEvent( QGraphicsSceneContextMenuEvent* event )
{
    QGraphicsScene::contextMenuEvent( event );

    /*if( !event->isAccepted() )
    {
        QMenu menu;

        QAction* openCircAct = menu.addAction(QIcon(":/opencirc.png"), tr("Open Circuit") );
        connect(openCircAct, SIGNAL(triggered()), MainWindow::self(), SLOT(openCirc()));

        QAction* newCircAct = menu.addAction( QIcon(":/newcirc.png"), tr("New Circuit") );
        connect( newCircAct, SIGNAL(triggered()), MainWindow::self(), SLOT(newCircuit()));

        QAction* saveCircAct = menu.addAction(QIcon(":/savecirc.png"), tr("Save Circuit") );
        connect(saveCircAct, SIGNAL(triggered()), MainWindow::self(), SLOT(saveCirc()));

        QAction* saveCircAsAct = menu.addAction(QIcon(":/savecircas.png"),tr("Save Circuit As...") );
        connect(saveCircAsAct, SIGNAL(triggered()), MainWindow::self(), SLOT(saveCircAs()));

        menu.exec( event->screenPos() );
    }*/
}

#include "moc_circuit.cpp"


