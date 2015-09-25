#include "pqZeqManagerPanel.h"
//
#include <boost/bind.hpp>
#include <boost/core/null_deleter.hpp>

// Qt includes
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QLabel>
#include <QComboBox>
#include <QTableWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTimer>
#include <QInputDialog>
#include <QFileDialog>
#include <QUrl>
#include <QDesktopServices>
#include <QThread>

// VTK includes

// ParaView Server Manager includes
#include "vtkSMInputProperty.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMArraySelectionDomain.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMViewProxy.h"
#include "vtkSMRepresentationProxy.h"
#include "vtkSMPropertyHelper.h"
#include "vtkProcessModule.h"
#include "vtkClientServerStream.h"
#include "vtkSMSession.h"

// ParaView includes
#include "pqActiveObjects.h"
#include "pqApplicationCore.h"
#include "pqAutoGeneratedObjectPanel.h"
#include "pqSettings.h"
#include "pqOutputPort.h"
#include "pqPipelineSource.h"
#include "pqPropertyLinks.h"
#include "pqProxy.h"
#include "pqServer.h"
#include "pqServerManagerModelItem.h"
#include "pqServerManagerModel.h"
#include "pqSMAdaptor.h"
#include "pqTreeWidgetCheckHelper.h"
#include "pqTreeWidgetItemObject.h"
#include "pqTreeWidget.h"
#include "pqTreeWidgetItem.h"
#include "pqView.h"
#include "pqRenderView.h"
#include "pqDataRepresentation.h"
#include "pqDisplayPolicy.h"
#include "pqAnimationScene.h"
#include "pqPropertyManager.h"
#include "pqNamedWidgets.h"
//
#include <QTcpServer>
#include <QTcpSocket>
#include <QStringListModel>
//
#include "ui_pqZeqManagerPanel.h"
//
#include "vtkZeqManager.h"
#include <zeq/vocabulary.h>
#include <monsteer/streaming/vocabulary.h>
//
#include <vector>
#include <regex>
//
//----------------------------------------------------------------------------
class StringList : public QStringListModel
{
public:
  void append (const QString& string){
    insertRows(rowCount(), 1);
    setData(index(rowCount()-1), string);
  }
  StringList& operator<<(const QString& string){
    append(string);
    return *this;
  }
};

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
class pqZeqManagerPanel::pqInternals : public QObject, public Ui::ZeqManagerPanel
{
public:
  pqInternals(pqZeqManagerPanel* p) : QObject(p)
  {
    this->Links = new pqPropertyLinks;
    this->clientOnlyZeqManager = NULL;
    event_num = 0;
  }
  //
  ~pqInternals() {
    delete this->Links;
    if (this->clientOnlyZeqManager) {
      this->clientOnlyZeqManager->Delete();
    }
  }
  //
  pqPropertyLinks         *Links;
  QTcpServer*              TcpNotificationServer;
  QTcpSocket*              TcpNotificationSocket;
  StringList               listModel;
  static int               event_num;
  vtkZeqManager           *clientOnlyZeqManager;
};
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
int pqZeqManagerPanel::pqInternals::event_num = 0;
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
pqZeqManagerPanel::pqZeqManagerPanel(pqProxy* proxy, QWidget* p) :
  pqNamedObjectPanel(proxy, p) 
{
  this->Internals = new pqInternals(this);
  this->Internals->setupUi(this);

  // inherited from pqNamedObjectPanel
  this->linkServerManagerProperties();
  
  this->connect(this->Internals->start,
    SIGNAL(clicked()), this, SLOT(onStart()));

  //
  this->connect(this, SIGNAL(onaccept()), this, SLOT(onAccept()));

  // Create a new notification socket to send events from server to client
  this->Internals->TcpNotificationServer = new QTcpServer(this);
  this->connect(this->Internals->TcpNotificationServer,
    SIGNAL(newConnection()), SLOT(onNewNotificationSocket()));
  this->Internals->TcpNotificationServer->listen(QHostAddress::Any,
    VTK_ZEQ_MANAGER_DEFAULT_NOTIFICATION_PORT);

  QObject::connect(this, SIGNAL(doUpdateGUIMessage(const QString&)),
    this, SLOT(UpdateGUIMessage(const QString&)));
}
//----------------------------------------------------------------------------
pqZeqManagerPanel::~pqZeqManagerPanel()
{
}
//-----------------------------------------------------------------------------
void pqZeqManagerPanel::LoadSettings()
{
  pqSettings *settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("ZeqManager");

  // Autostart
  this->Internals->autostart->setChecked(settings->value("autostart", true).toBool());
  // mode
  this->Internals->zeq_both->setChecked(settings->value("zeq_both", true).toBool());
  this->Internals->zeq_gui->setChecked(settings->value("zeq_gui", true).toBool());
  this->Internals->zeq_server->setChecked(settings->value("zeq_server", true).toBool());
  settings->endGroup();
}
//----------------------------------------------------------------------------
void pqZeqManagerPanel::SaveSettings()
{
  pqSettings *settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("ZeqManager");

  // Autostart
  settings->setValue("autostart", this->Internals->autostart->isChecked());
  // mode
  settings->setValue("zeq_both", this->Internals->zeq_both->isChecked());
  settings->setValue("zeq_gui", this->Internals->zeq_gui->isChecked());
  settings->setValue("zeq_server", this->Internals->zeq_server->isChecked());
  settings->endGroup();
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::AutoStart()
{
  if (this->Internals->autostart->isChecked()) {
    this->onStart();
  }
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::onNewNotificationSocket()
{
  this->Internals->TcpNotificationSocket =
  this->Internals->TcpNotificationServer->nextPendingConnection();

  if (this->Internals->TcpNotificationSocket) {
    this->connect(this->Internals->TcpNotificationSocket,
                  SIGNAL(readyRead()), SLOT(onNotified()), Qt::QueuedConnection);
    this->Internals->TcpNotificationServer->close();
  }
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::onAccept()
{
}
//-----------------------------------------------------------------------------
void pqZeqManagerPanel::onStart()
{
  // normal client server operation uses a proxy
  if (!this->ClientSideZeq()) {
    this->referenceProxy()->getProxy()->InvokeCommand("Start");
  }
  // special gui mode create a local vtkZeqManager
  else {
    this->ClientSideZeqReady();
  }
}

//-----------------------------------------------------------------------------
//
// @TODO, clean this up so that the message from zeq is forwarded directly
// to the GUI and then deserialized once in the relevant function
//
void pqZeqManagerPanel::onNotified()
{
  int error = 0;
  qint64 bytes_read = 0;
  //
  const zeq::uint128_t new_connection = zeq::make_uint128("zeq::hbp::NewConnection");
  //
  vtkZeqManager::event_data event_data;
  //
  while (this->Internals->TcpNotificationSocket->size() > 0) {

    //
    std::stringstream temp;
    temp << this->Internals->event_num++ << " : ";

    // read event block
    bytes_read = this->Internals->TcpNotificationSocket->read(
      reinterpret_cast<char*>(&event_data), sizeof(vtkZeqManager::event_data));
    if (bytes_read != sizeof(vtkZeqManager::event_data)) {
      error = 1;
      temp << "Error in data header size";
      break;
    }

    // read data block
    std::vector<char> buffer;
    //
    buffer.resize(event_data.Size);
    bytes_read = this->Internals->TcpNotificationSocket->read(
                   reinterpret_cast<char*>(&buffer[0]), event_data.Size);
    if (bytes_read!=event_data.Size) {
      temp << "Error in data block size";
      continue;
    }

    // process the event
    if (event_data.Type == new_connection) {
      this->Internals->eventview->setModel(&this->Internals->listModel);
      emit doUpdateGUIMessage(QString(temp.str().c_str()) + "New Zeq connection");
    }
    // if (this->Internals->DsmProxyCreated() && this->Internals->DsmInitialized) {
    else if (event_data.Type == zeq::hbp::EVENT_CAMERA) {
      emit doUpdateGUIMessage(QString(temp.str().c_str()) + "CameraEvent");
      this->UpdateCamera(event_data.Type, buffer.data(), buffer.size());
    }
    else if (event_data.Type == zeq::hbp::EVENT_SELECTEDIDS) {
      this->UpdateSelection(event_data.Type, buffer.data(), buffer.size());
    }
    else if (event_data.Type == monsteer::streaming::EVENT_SPIKES) {
      this->UpdateSpikes(event_data.Type, buffer.data(), buffer.size());
    }

    // signal back to the server that we have done with this event and it can proceed with the next
    this->referenceProxy()->getProxy()->InvokeCommand("SignalUpdated");
  }
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::UpdateSelection(zeq::uint128_t Type, const void *buffer, size_t size)
{
  zeq::Event event(Type);
  event.setData(zeq::ConstByteArray((uint8_t*)(buffer), boost::null_deleter()), size);
  //std::cout << "Got a Selected Ids event " << event.getType() << std::endl;
  std::vector<unsigned int> Ids = zeq::hbp::deserializeSelectedIDs( event );
  //
  
  //
  pqServerManagerModel *sm = pqApplicationCore::instance()->getServerManagerModel();
  QList<pqPipelineSource*> pqsources = sm->findItems<pqPipelineSource*>(NULL);
  vtkSMSourceProxy *proxy = NULL;
  if (pqsources.size()>0) {
    std::regex bbp("BlueConfig.*");
    for (QList<pqPipelineSource*>::iterator it = pqsources.begin(); it != pqsources.end(); ++it) {
      proxy = (*it)->getSourceProxy();
      if (std::regex_match((*it)->getSMName().toLatin1().data(), bbp)) {
        //
        int numValues = Ids.size();
        emit doUpdateGUIMessage(QString("Setting ") + QString::number(numValues) + QString(" Ids on ") + (*it)->getSMName());
        //
        vtkClientServerStream stream;
        if (numValues>0) {
          vtkSMProperty *GIDs = proxy->GetProperty("SelectedGIds");
          vtkClientServerStream::Array array =
          {
            vtkClientServerStream::int32_array,
            static_cast<vtkTypeUInt32>(numValues),
            static_cast<vtkTypeUInt32>(sizeof(vtkClientServerStream::int32_value)*numValues),
            (int*)(&Ids[0])
          };

          stream << vtkClientServerStream::Invoke
            << VTKOBJECT(proxy)
            << "SetSelectedGIds"
            << static_cast<int>(numValues)
            << stream.InsertArray((unsigned int*)(&Ids[0]), static_cast<int>(numValues));
          stream << vtkClientServerStream::End;
        }
        else {
          stream << vtkClientServerStream::Invoke
            << VTKOBJECT(proxy)
            << "ClearSelectedGIds"
            << vtkClientServerStream::End;
        }
        proxy->GetSession()->ExecuteStream(proxy->GetLocation(), stream);
        (*it)->setModifiedState(pqProxy::ModifiedState::MODIFIED);
        proxy->Modified();
        proxy->MarkDirty(NULL);
        this->UpdateViews(proxy);
      }
    }
  }
  else {
    emit doUpdateGUIMessage("No BBP source proxy to set Ids on");
  }
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::UpdateCamera(zeq::uint128_t Type, const void *buffer, size_t size)
{
  emit doUpdateGUIMessage("Setting camera");
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::UpdateSpikes(zeq::uint128_t Type, const void *buffer, size_t size)
{
  emit doUpdateGUIMessage("Setting spikes");
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::GetViewsForPipeline(vtkSMSourceProxy *source, std::set<pqView*> &viewlist)
{
  // find the pipeline associated with this source
  pqPipelineSource* pqsource = pqApplicationCore::instance()->
    getServerManagerModel()->findItem<pqPipelineSource*>(source);
  // and find all views it is present in
  if (pqsource) {
    foreach (pqView *view, pqsource->getViews()) {
      pqDataRepresentation *repr = pqsource->getRepresentation(0, view);
      if (repr && repr->isVisible()) {
        // add them to the list
        viewlist.insert(view);
      }
    }
  }
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::UpdateViews(vtkSMSourceProxy *proxy)
{
  std::set<pqView*> viewlist;
  this->GetViewsForPipeline(proxy, viewlist);
  //
  // Update all views which are associated with out pipelines
  //
  for (std::set<pqView*>::iterator it=viewlist.begin(); it!=viewlist.end(); ++it) {
    (*it)->render();
  }
}

//-----------------------------------------------------------------------------
bool pqZeqManagerPanel::ClientSideZeq()
{
  if (this->Internals->zeq_gui->isChecked()) return true;
  return false;
}

//-----------------------------------------------------------------------------
bool pqZeqManagerPanel::ClientSideZeqReady()
{
    if (this->Internals->clientOnlyZeqManager==NULL) {
      this->Internals->clientOnlyZeqManager = vtkZeqManager::New();
      this->Internals->clientOnlyZeqManager->SetClientSideMode(1);
      this->Internals->clientOnlyZeqManager->SetSelectionCallback(
        boost::bind( &pqZeqManagerPanel::onSelectedIds, this, _1 ));
      this->Internals->clientOnlyZeqManager->SetCameraCallback(
        boost::bind( &pqZeqManagerPanel::onHBPCamera, this, _1 ));
      this->Internals->clientOnlyZeqManager->SetSpikeCallback(
        boost::bind( &pqZeqManagerPanel::onSpike, this, _1 ));
      //
      this->Internals->clientOnlyZeqManager->Start();
      //
      this->Internals->eventview->setModel(&this->Internals->listModel);
      this->Internals->listModel << "Client only zeq connection";
      this->Internals->eventview->scrollToBottom();
    }
    return true;
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::onSpike( const zeq::Event& event )
{
  const monsteer::streaming::SpikeMap& spikes = monsteer::streaming::deserializeSpikes( event );

  monsteer::streaming::SpikeMap _incoming;
  _incoming.insert( spikes.begin(), spikes.end( ));

  float _lastTimeStamp;

  if( !_incoming.empty() ) {
      _lastTimeStamp = _incoming.rbegin()->first;
  }

  emit doUpdateGUIMessage("Received Spike data ");
  //
  this->Internals->clientOnlyZeqManager->SignalUpdated();
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::onHBPCamera( const zeq::Event& event )
{
  emit doUpdateGUIMessage("Received camera data ");
  //
  this->Internals->clientOnlyZeqManager->SignalUpdated();
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::onSelectedIds( const zeq::Event& event )
{
  // forward the event directly to the client
  vtkZeqManager::event_data data = {event.getType(), event.getSize() };
  this->UpdateSelection(event.getType(), event.getData(), event.getSize());
  //
  this->Internals->clientOnlyZeqManager->SignalUpdated();
}

//-----------------------------------------------------------------------------
void pqZeqManagerPanel::UpdateGUIMessage(const QString &msg)
{
  this->Internals->listModel << msg;
  this->Internals->eventview->scrollToBottom();
}
