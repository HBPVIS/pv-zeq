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

//BTX
//ETX

// So far this class is a placeholder and does not perform any function

class VTK_EXPORT vtkZeqManager : public vtkObject
{
public:
  static vtkZeqManager *New();
  vtkTypeMacro(vtkZeqManager,vtkObject);

  void Refresh();

protected:
   vtkZeqManager();
  ~vtkZeqManager();

  void onHBPCamera( const zeq::Event& event );
  void onLookupTable1D( const zeq::Event& event );
  void onRequest( const zeq::Event& event );

  uint16_t          _port;
  std::string       _servicename;
  lunchbox::Servus  _service;
  lunchbox::Strings _hosts;
  zeq::Subscriber   _subscriber;

  static vtkZeqManager *ZeqManagerSingleton;
private:
  vtkZeqManager(const vtkZeqManager&);  // Not implemented.
  void operator=(const vtkZeqManager&);  // Not implemented.
};

#endif
