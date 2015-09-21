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

private slots:
  void onAccept();
  void onStart();
  void onNewNotificationSocket();
  void onNotified();

protected:

  void UpdateSelection(const vtkZeqManager::event_data &event_data, char *data);

  void GetViewsForPipeline(vtkSMSourceProxy *source, std::set<pqView*> &viewlist);
  void UpdateViews(vtkSMSourceProxy *proxy);

  class pqInternals;
  pqInternals* Internals;

protected slots:

};

#endif

