/*=========================================================================

  Project                 : pv-zeq
  Module                  : vtkZeqManager.h

  Authors:
     John Biddiscombe
     biddisco@cscs.ch

=========================================================================*/
#include "vtkZeqManager.h"
//
#include "vtkObjectFactory.h"
#include "vtkSmartPointer.h"
//
// For PARAVIEW_USE_MPI
#include "vtkPVConfig.h"
#ifdef PARAVIEW_USE_MPI
#include "vtkMPI.h"
#include "vtkMPIController.h"
#include "vtkMPICommunicator.h"
#endif
// Otherwise
#include "vtkMultiProcessController.h"
//
#include "vtkProcessModule.h"
#include "vtkPVOptions.h"
#include "vtkClientSocket.h"
#include "vtkMultiThreader.h"
#include "vtkMutexLock.h"
#include "vtkConditionVariable.h"
//
#include <servus/uri.h>
#include <zeq/vocabulary.h>
#include <monsteer/streaming/vocabulary.h>
#include <boost/bind.hpp>
#include <random>

//----------------------------------------------------------------------------
vtkCxxSetObjectMacro(vtkZeqManager, Controller, vtkMultiProcessController);

//----------------------------------------------------------------------------
VTK_EXPORT VTK_THREAD_RETURN_TYPE vtkZeqManagerNotificationThread(void *arg)
{
  vtkZeqManager *zeqManager = static_cast<vtkZeqManager*>(
    static_cast<vtkMultiThreader::ThreadInfo*>(arg)->UserData);
  zeqManager->NotificationThread();
  return(VTK_THREAD_RETURN_VALUE);
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
struct vtkZeqManager::vtkZeqManagerInternals
{
  vtkZeqManagerInternals() {
    this->NotificationThread = vtkSmartPointer<vtkMultiThreader>::New();

    // Updated event
    this->IsUpdated = false;
    this->UpdatedCond = vtkSmartPointer<vtkConditionVariable>::New();
    this->UpdatedMutex = vtkSmartPointer<vtkMutexLock>::New();

    // NotifThreadCreated event
    this->IsNotifThreadCreated = false;
    this->NotifThreadCreatedCond = vtkSmartPointer<vtkConditionVariable>::New();
    this->NotifThreadCreatedMutex = vtkSmartPointer<vtkMutexLock>::New();
  }

  ~vtkZeqManagerInternals() {
    //    if (this->NotificationThread->IsThreadActive(this->NotificationThreadID)) {
    //      this->NotificationThread->TerminateThread(this->NotificationThreadID);
    //    }
  }

  void SignalNotifThreadCreated() {
    this->NotifThreadCreatedMutex->Lock();
    this->IsNotifThreadCreated = true;
    this->NotifThreadCreatedCond->Signal();
    this->NotifThreadCreatedMutex->Unlock();
  }

  void WaitForNotifThreadCreated() {
    this->NotifThreadCreatedMutex->Lock();
    while (!this->IsNotifThreadCreated) {
      this->NotifThreadCreatedCond->Wait(this->NotifThreadCreatedMutex);
    }
    this->NotifThreadCreatedMutex->Unlock();
  }

  vtkSmartPointer<vtkClientSocket>  NotificationSocket;
  vtkSmartPointer<vtkMultiThreader> NotificationThread;
  int NotificationThreadID;

  // Updated event
  bool                                  IsUpdated;
  vtkSmartPointer<vtkMutexLock>         UpdatedMutex;
  vtkSmartPointer<vtkConditionVariable> UpdatedCond;

  // NotifThreadCreated event
  bool                                  IsNotifThreadCreated;
  vtkSmartPointer<vtkMutexLock>         NotifThreadCreatedMutex;
  vtkSmartPointer<vtkConditionVariable> NotifThreadCreatedCond;
};

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
extern vtkObject* vtkInstantiatorvtkZeqManagerNew();
vtkZeqManager *vtkZeqManager::ZeqManagerSingleton = NULL;
//----------------------------------------------------------------------------
#undef ErrorMacro
#define ErrorMacro(x)                                           \
   {                                                            \
   if (vtkObject::GetGlobalWarningDisplay())                    \
     {                                                          \
     vtkOStreamWrapper::EndlType endl;                          \
     vtkOStreamWrapper::UseEndl(endl);                          \
     vtkOStrStreamWrapper vtkmsg;                               \
     vtkmsg << "ERROR: In " __FILE__ ", line " << __LINE__      \
            << "\n" x << "\n\n";                                \
     vtkOutputWindowDisplayErrorText(vtkmsg.str());             \
     vtkmsg.rdbuf()->freeze(0); vtkObject::BreakOnError();      \
     }                                                          \
   }
//----------------------------------------------------------------------------
vtkZeqManager *vtkZeqManager::New()
{
  if (vtkZeqManager::ZeqManagerSingleton) {
    // increase the reference count
    vtkZeqManager::ZeqManagerSingleton->Register(NULL);
    return vtkZeqManager::ZeqManagerSingleton;
  }
  //
  vtkZeqManager *temp = new vtkZeqManager();
  if (temp!=vtkZeqManager::ZeqManagerSingleton) {
    ErrorMacro(<<"A serious (probably thread related error) has occured in vtkZeqManager singleton creation")
  }
  return vtkZeqManager::ZeqManagerSingleton;
}
//----------------------------------------------------------------------------
vtkObject *vtkInstantiatorvtkZeqManagerNew()
{
  return vtkZeqManager::New();
}
//----------------------------------------------------------------------------
// Singleton creation, not really thread-safe, but unlikely to ever be tested
vtkZeqManager::vtkZeqManager() : _servicename("_hbp._tcp"), _service(_servicename)
{
  this->UpdatePiece             = 0;
  this->UpdateNumPieces         = 0;
  this->HostsDescription        = NULL;
  this->abort_poll              = 0;
  this->thread_done             = 1;
  this->_subscriber             = NULL;
  this->ClientSideMode          = 0;
  //
#ifdef VTK_USE_MPI
  this->Controller              = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
  this->UpdatePiece     = this->Controller->GetLocalProcessId();
  this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
#endif

  if (vtkZeqManager::ZeqManagerSingleton==NULL) {
    vtkZeqManager::ZeqManagerSingleton = this;
  }

  this->ZeqManagerInternals = new vtkZeqManagerInternals();

}
//----------------------------------------------------------------------------
vtkZeqManager::~vtkZeqManager()
{
  this->abort_poll = 1;
  while (!this->thread_done) {
    sleep(1);
  }
  //
  if (_subscriber) delete _subscriber;
  //
  if (this->ZeqManagerInternals) delete this->ZeqManagerInternals;
  this->ZeqManagerInternals = NULL;
  //
  this->SetHostsDescription(NULL);
  //
  vtkZeqManager::ZeqManagerSingleton=NULL;
}

//---------------------------------------------------------------------------
void vtkZeqManager::Discover()
{
    _hosts = _service.discover( servus::Servus::IF_ALL, 50 );
    if( _hosts.empty() ) {
        vtkErrorMacro("No hosts found");
    }

    for (auto &name : _hosts) {
      std::cout << name.c_str() << std::endl;
    }
}

//---------------------------------------------------------------------------
void vtkZeqManager::onHBPCamera( const zeq::Event& event )
{
  // forward the event directly to the client
  event_data data = {event.getType(), event.getSize() };
  this->ZeqManagerInternals->NotificationSocket->Send(&data, sizeof(event_data));
  this->ZeqManagerInternals->NotificationSocket->Send(event.getData(), event.getSize());
}

//---------------------------------------------------------------------------
void vtkZeqManager::onSelectedIds( const zeq::Event& event )
{
  // forward the event directly to the client
  event_data data = {event.getType(), event.getSize() };
  this->ZeqManagerInternals->NotificationSocket->Send(&data, sizeof(event_data));
  this->ZeqManagerInternals->NotificationSocket->Send(event.getData(), event.getSize());
}

//---------------------------------------------------------------------------
void vtkZeqManager::onSpike( const zeq::Event& event )
{
  // forward the event directly to the client
  event_data data = {event.getType(), event.getSize() };
  this->ZeqManagerInternals->NotificationSocket->Send(&data, sizeof(event_data));
  this->ZeqManagerInternals->NotificationSocket->Send(event.getData(), event.getSize());

/*
    const monsteer::streaming::SpikeMap& spikes = monsteer::streaming::deserializeSpikes( event );

    monsteer::streaming::SpikeMap _incoming;
    _incoming.insert( spikes.begin(), spikes.end( ));

    float _lastTimeStamp;

    if( !_incoming.empty() )
        _lastTimeStamp = _incoming.rbegin()->first;
*/

//
//  event_data data = {event.getType(), Ids.size() };
}

//---------------------------------------------------------------------------
void vtkZeqManager::Start()
{
  if (this->UpdatePiece == 0) {
    this->Create();
    this->Discover();
  }
}

//----------------------------------------------------------------------------
void* vtkZeqManager::NotificationThread()
{
  event_data data = {zeq::make_uint128("zeq::hbp::NewConnection"), 0 };

  this->ZeqManagerInternals->SignalNotifThreadCreated();
  //
  if (!this->ClientSideMode) {
    this->ZeqManagerInternals->NotificationSocket->Send(&data, sizeof(event_data));
  }
  //
  int FAIL_FAIL = 0;
  while (!this->abort_poll && this->WaitForUnlock(NULL) != FAIL_FAIL) {
    // poll for 100ms, if nothing try again.
    // we do it like this so that we can exit more cleanly than setting timeout to zero
    // and getting a segfault when we destruct
    int event = _subscriber->receive(100);
    if (event) {
      this->WaitForUpdated();
    }
  }
  this->thread_done = 1;
  return((void *)this);
}

//----------------------------------------------------------------------------
void vtkZeqManager::SignalUpdated()
{
  this->ZeqManagerInternals->UpdatedMutex->Lock();

  this->ZeqManagerInternals->IsUpdated = true;
  this->ZeqManagerInternals->UpdatedCond->Signal();
  vtkDebugMacro("Sent updated condition signal");

  this->ZeqManagerInternals->UpdatedMutex->Unlock();

}

//----------------------------------------------------------------------------
void vtkZeqManager::WaitForUpdated()
{
  this->ZeqManagerInternals->UpdatedMutex->Lock();

  if (!this->ZeqManagerInternals->IsUpdated) {
    vtkDebugMacro("Thread going into wait for pipeline updated...");
    this->ZeqManagerInternals->UpdatedCond->Wait(this->ZeqManagerInternals->UpdatedMutex);
    vtkDebugMacro("Thread received updated signal");
  }
  if (this->ZeqManagerInternals->IsUpdated) {
    this->ZeqManagerInternals->IsUpdated = false;
  }

  this->ZeqManagerInternals->UpdatedMutex->Unlock();
}

//----------------------------------------------------------------------------
int vtkZeqManager:: WaitForUnlock(const void *flag) { return 1; }

//----------------------------------------------------------------------------
int vtkZeqManager::CreateNotificationSocket()
{
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  vtkPVOptions *pvOptions = pm->GetOptions();
  const char *pvClientHostName = pvOptions->GetHostName();
  int notificationPort = VTK_ZEQ_MANAGER_DEFAULT_NOTIFICATION_PORT;
  if ((this->UpdatePiece == 0) && pvClientHostName && pvClientHostName[0]) {
    int r, tryConnect = 0;
    this->ZeqManagerInternals->NotificationSocket = vtkSmartPointer<vtkClientSocket>::New();
    do {
      std::cout << "Creating notification socket to "
      << pvClientHostName << " on port " << notificationPort << "...";
      r = this->ZeqManagerInternals->NotificationSocket->ConnectToServer(pvClientHostName,
                                                                         notificationPort);
      if (r == 0) {
        std::cout << "Connected pv-zeq server to client" << std::endl;
      } else {
        std::cout << "Failed to connect pv-zeq server to client" << std::endl;
        tryConnect++;
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
      }
    } while (r < 0 && tryConnect < 5);
    if (r < 0) {
      this->ZeqManagerInternals->NotificationSocket = NULL;
      return(-1);
    }
  }

  return 1;
}

//----------------------------------------------------------------------------
int vtkZeqManager::Create()
{
  this->UpdatePiece     = this->Controller->GetLocalProcessId();
  this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
  if (this->ZeqManagerInternals->NotificationSocket!=NULL) {
    // already created
    return 0;
  }

  if (!this->ClientSideMode) {
    this->CreateNotificationSocket();
  }

  _subscriber = new zeq::Subscriber( servus::URI( "hbp://" ));

  std::default_random_engine generator;
  std::uniform_int_distribution<int> distribution(0 + 1024,60000 + 1024);
  _port = distribution(generator);

  const servus::Servus::Result& result = _service.announce( _port,
                                                           boost::lexical_cast< std::string >( _port ));

  if ( _service.getName() != _servicename)
  {
    vtkErrorMacro("Something is wrong after service announce");
  }
  if( !servus::Servus::isAvailable( ))
  {
    std::cout << "result == servus::Servus::Result::NOT_SUPPORTED" << result << std::endl;
    return 0;
  }

  if (!this->ClientSideMode) {
    this->SelectionCallback = zeq_callback(boost::bind( &vtkZeqManager::onSelectedIds, this, _1 ));
    this->CameraCallback = zeq_callback(boost::bind( &vtkZeqManager::onHBPCamera, this, _1 ));
    this->SpikeCallback = zeq_callback(boost::bind( &vtkZeqManager::onSpike, this, _1 ));
  }
  _subscriber->registerHandler( zeq::hbp::EVENT_CAMERA,      this->CameraCallback);
  _subscriber->registerHandler( zeq::hbp::EVENT_SELECTEDIDS, this->SelectionCallback);
  _subscriber->registerHandler( monsteer::streaming::EVENT_SPIKES, this->SpikeCallback);

  //
  // start thread to listen on zeq events
  //
  this->thread_done = 0;
  this->ZeqManagerInternals->NotificationThreadID =
    this->ZeqManagerInternals->NotificationThread->SpawnThread(
      vtkZeqManagerNotificationThread, (void *) this);
  this->ZeqManagerInternals->WaitForNotifThreadCreated();

  return 0;
}

//----------------------------------------------------------------------------
void vtkZeqManager::SetSelectionCallback(zeq_callback cb)
{
  this->SelectionCallback = cb;
}

//----------------------------------------------------------------------------
void vtkZeqManager::SetCameraCallback(zeq_callback cb)
{
  this->CameraCallback = cb;
}

//----------------------------------------------------------------------------
void vtkZeqManager::SetSpikeCallback(zeq_callback cb)
{
  this->SpikeCallback = cb;
}

