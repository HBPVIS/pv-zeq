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
#include <lunchbox/uri.h>
#include <boost/bind.hpp>

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
  , _subscriber( lunchbox::URI( "hbp://" ))
{
  if (vtkZeqManager::ZeqManagerSingleton==NULL) {
    vtkZeqManager::ZeqManagerSingleton = this;
  }

  lunchbox::RNG rng;
  _port = (rng.get< uint16_t >() % 60000) + 1024;

  const lunchbox::Servus::Result& result = _service.announce( _port,
          boost::lexical_cast< std::string >( _port ));

  if ( _service.getName() != _servicename)
  {
      vtkErrorMacro("Something is wrong after service announce");
  }
  if( !lunchbox::Servus::isAvailable( ))
  {
      std::cout << "result == lunchbox::Servus::Result::NOT_SUPPORTED" << result << std::endl;
      return;
  }

  _subscriber.registerHandler( zeq::hbp::EVENT_CAMERA,
                               boost::bind( &vtkZeqManager::onHBPCamera,
                                            this, _1 ));
  _subscriber.registerHandler( zeq::hbp::EVENT_LOOKUPTABLE1D,
                               boost::bind( &vtkZeqManager::onLookupTable1D,
                                            this, _1 ));

  _subscriber.registerHandler( zeq::vocabulary::EVENT_REQUEST,
                               boost::bind( &vtkZeqManager::onRequest,
                                            this, _1 ));

//  this->Refresh();
}
//----------------------------------------------------------------------------
vtkZeqManager::~vtkZeqManager()
{
  vtkZeqManager::ZeqManagerSingleton=NULL;
}
//---------------------------------------------------------------------------
void vtkZeqManager::Refresh()
{
    std::cout << "Received a refresh command " << std::endl;
    _hosts = _service.discover( lunchbox::Servus::IF_ALL, 2000 );
    if( _hosts.empty() ) {
        vtkErrorMacro("No hosts found");
    }

    for (auto &name : _hosts) {
      std::cout << name.c_str() << std::endl;
    }

    _subscriber.receive();
}
//---------------------------------------------------------------------------
void vtkZeqManager::onHBPCamera( const zeq::Event& event )
{
    std::cout << "Got a onHBPCamera event" << std::endl;
}
//---------------------------------------------------------------------------
void vtkZeqManager::onLookupTable1D( const zeq::Event& event )
{
    std::cout << "Got a onLookupTable1D event" << std::endl;
}
//---------------------------------------------------------------------------
void vtkZeqManager::onRequest( const zeq::Event& event )
{
    std::cout << "Got a request (vocabulary) event" << std::endl;
}
//---------------------------------------------------------------------------
