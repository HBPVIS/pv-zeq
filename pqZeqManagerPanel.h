#ifndef _pqZeqManagerPanel_h
#define _pqZeqManagerPanel_h

#include <set>

#include "pqProxy.h"
#include "pqNamedObjectPanel.h"
#include "vtkZeqManager.h"

class vtkSMSourceProxy;
class pqView;

class pqZeqManagerPanel : public pqNamedObjectPanel
{
  Q_OBJECT

public:
  /// constructor
  pqZeqManagerPanel(pqProxy* proxy, QWidget* p);
 ~pqZeqManagerPanel();

  void LoadSettings();
  void SaveSettings();
  void AutoStart();

  bool ClientSideZeq();
  bool ClientSideZeqReady();

  void onHBPCamera( const zeq::Event& event );
  void onSelectedIds( const zeq::Event& event );
  void onSpike( const zeq::Event& event );

signals:
  void doUpdateGUIMessage(const QString &msg);
  void doUpdateRenderViews(vtkSMSourceProxy *proxy);

private slots:
  void onAccept();
  void onStart();
  void onNewNotificationSocket();
  void onNotified();
  void onUpdateGUIMessage(const QString &msg);
  void onUpdateRenderViews(vtkSMSourceProxy *proxy);

protected:

  void UpdateSelection(zeq::uint128_t Type, const void *buffer, size_t size);
  void UpdateCamera(zeq::uint128_t Type, const void *buffer, size_t size);
  void UpdateSpikes(zeq::uint128_t Type, const void *buffer, size_t size);
  //
  void GetViewsForPipeline(vtkSMSourceProxy *source, std::set<pqView*> &viewlist);
  void UpdateViews(vtkSMSourceProxy *proxy);

  class pqInternals;
  pqInternals* Internals;

protected slots:

};

#endif

