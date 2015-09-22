/*=========================================================================

  Project                 : pv-zeq
  Module                  : vtkZeqManager.h

  Authors:
     John Biddiscombe
     biddisco@cscs.ch

=========================================================================*/
// .NAME vtkZeqManager - Zeq Convenience Manager Class
// .SECTION Description

#ifndef __vtkZeqManager_h
#define __vtkZeqManager_h

#include "vtkObject.h"
#include "vtkSmartPointer.h" // for smartpointers

#include <lunchbox/servus.h>
#include <lunchbox/rng.h>
#include <zeq/zeq.h>
#include <zeq/hbp/hbp.h>
#include <boost/lexical_cast.hpp>

typedef boost::shared_ptr< zeq::Subscriber > SubscriberPtr;
typedef std::vector< SubscriberPtr > Subscribers;

class vtkMultiProcessController;
#include "vtkUnsignedIntArray.h"
//BTX
//ETX
#define VTK_ZEQ_MANAGER_DEFAULT_NOTIFICATION_PORT 11112
#define VTK_ZEQ_NOTIFICATION_CONNECTED 0
#define VTK_ZEQ_NOTIFICATION_EVENT     1

// So far this class is a placeholder and does not perform any function

class VTK_EXPORT vtkZeqManager : public vtkObject
{
public:

  //BTX
  struct vtkZeqManagerInternals;
  vtkZeqManagerInternals *ZeqManagerInternals;
  
  typedef std::function<void (const zeq::Event&)> zeq_callback;
  //ETX

  struct event_data {
    zeq::uint128_t Type;
    size_t         Size;
  };

  static vtkZeqManager *New();
  vtkTypeMacro(vtkZeqManager,vtkObject);

  void Start();

  // Description:
  // Set/Get the controller use in compositing (set to
  // the global controller by default)
  // If not using the default, this must be called before any
  // other methods.
  virtual void SetController(vtkMultiProcessController* controller);

  vtkGetObjectMacro(Controller, vtkMultiProcessController);
  void *NotificationThread();

  // Description:
  // Signal/Wait for the pipeline update to be finished (only valid when
  // a new notification has been received)
  virtual void SignalUpdated();
  virtual void WaitForUpdated();

  // Description:
  // Set the Xdmf description file.
  vtkGetStringMacro(HostsDescription);
  vtkSetStringMacro(HostsDescription);

  void SetSelectedGIDs(int maxvalues, unsigned int *values);

  vtkSetMacro(ClientSideMode,int);
  vtkGetMacro(ClientSideMode,int);
  vtkBooleanMacro(ClientSideMode,int);

  void SetSelectionCallback(zeq_callback cb);
  void SetCameraCallback(zeq_callback cb);

protected:
   vtkZeqManager();
  ~vtkZeqManager();

  void onHBPCamera( const zeq::Event& event );
  void onLookupTable1D( const zeq::Event& event );
  void onRequest( const zeq::Event& event );
  void onSelectedIds( const zeq::Event& event );

  int Create();
  void Discover();
  int CreateNotificationSocket();


  uint16_t          _port;
  std::string       _servicename;
  servus::Servus    _service;
  servus::Strings   _hosts;
  zeq::Subscriber  *_subscriber;

  //
  // Zeq manager variables
  //
  static vtkZeqManager *ZeqManagerSingleton;
  // The most recent selection list
  vtkUnsignedIntArray  *SelectedGIDs;


  // Description:
  // Wait for a notification - notifications are used to trigger user
  // defined tasks and are sent when the file has been unlocked
  virtual int  WaitForUnlock(const void *flag);

  //
  // Internal Variables
  //
  int            UpdatePiece;
  int            UpdateNumPieces;
  int            abort_poll;
  int            thread_done;
  int            ClientSideMode;

  zeq_callback SelectionCallback;
  zeq_callback CameraCallback;

  vtkMultiProcessController *Controller;

  char *HostsDescription;

private:
  vtkZeqManager(const vtkZeqManager&);  // Not implemented.
  void operator=(const vtkZeqManager&);  // Not implemented.
};

#endif
