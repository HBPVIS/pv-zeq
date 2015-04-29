#ifndef _pqZeqManagerPanel_h
#define _pqZeqManagerPanel_h

#include "pqProxy.h"
#include "pqNamedObjectPanel.h"

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
  void onRefresh();

protected:

  class pqUI;
  pqUI* UI;

protected slots:

};

#endif

