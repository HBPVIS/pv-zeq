#ifndef _pqZeqManagerPanel_h
#define _pqZeqManagerPanel_h

#include <set>

#include "pqProxy.h"
#include "pqNamedObjectPanel.h"
#include <QTimer>
#include <QMutex>
#include "vtkZeqManager.h"
#include <monsteer/streaming/vocabulary.h>

class vtkSMSourceProxy;
class pqView;

class pqZeqManagerPanel : public pqNamedObjectPanel
{
  Q_OBJECT
  //
  typedef struct zeq_event {
    void             *buffer;
    size_t            size;
    servus::uint128_t Type;
    ~zeq_event() {
      delete []buffer;
    }
  } zeq_event;
  //
  typedef QSharedPointer<zeq_event> event_signal;

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
  void doUpdateGUIMessage(const QString &);
  void doUpdateRenderViews(vtkSMSourceProxy *);
  void doInvokeStream(event_signal e, const QString &, const QString &, const QString &);

private slots:
  void onAccept();
  void onStart();
  void onPause();
  //
  void onNewNotificationSocket();
  void onNotified();
  void onUpdateGUIMessage(const QString &msg);
  void onUpdateRenderViews(vtkSMSourceProxy *proxy);
  void onInvokeStream(event_signal e, const QString &, const QString &, const QString &);
  void onTimerUpdate();

protected:

  void UpdateSelection(zeq::uint128_t Type, const void *buffer, size_t size);
  void UpdateCamera(zeq::uint128_t Type, const void *buffer, size_t size);
  void UpdateSpikes(zeq::uint128_t Type, const void *buffer, size_t size);
  //
  void GetViewsForPipeline(vtkSMSourceProxy *source, std::set<pqView*> &viewlist);
  void UpdateViews(vtkSMSourceProxy *proxy);

  class pqInternals;
  pqInternals* Internals;
  QTimer zeq_timer;
  QMutex zeq_mutex;

  monsteer::streaming::SpikeMap Spikes;

protected slots:

};

#endif

