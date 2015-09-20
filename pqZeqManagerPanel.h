#ifndef _pqZeqManagerPanel_h
#define _pqZeqManagerPanel_h

#include "pqProxy.h"
#include "pqNamedObjectPanel.h"
#include "vtkZeqManager.h"

class pqZeqManagerPanel : public pqNamedObjectPanel
{
  Q_OBJECT

public:
  /// constructor
  pqZeqManagerPanel(pqProxy* proxy, QWidget* p);
 ~pqZeqManagerPanel();

  void LoadSettings();
  void SaveSettings();

private slots:
  void onAccept();
  void onStart();
  void onNewNotificationSocket();
  void onNotified();

protected:

  void UpdateSelection(const vtkZeqManager::event_data &event_data, char *data);

  class pqInternals;
  pqInternals* Internals;

protected slots:

};

#endif

