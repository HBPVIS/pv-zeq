#include "pqZeqManagerDockWindow.h"

// Qt includes
#include <QSet>
#include <QStyle>
#include <QStyleFactory>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>

// version includes
#include "vtkPVConfig.h"

// ParaView Server Manager includes
#include "vtkSMProxyManager.h"
#include "vtkSMProxy.h"

// ParaView includes
#include "pqApplicationCore.h"
#include "pqProxy.h"
#include "pqServer.h"
#include "pqServerManagerModelItem.h"
#include "pqServerManagerModel.h"
#include "pqActiveObjects.h"
#include "pqUndoStack.h"
//
#include "pqZeqManagerPanel.h"
//----------------------------------------------------------------------------
// This contains code copied from another paraview plugin and is therefore
// more complex than necessary - but will support future expansion.
//----------------------------------------------------------------------------
class pqZeqManagerDockWindow::pqUI : public QObject
{
public:
  pqUI(pqZeqManagerDockWindow* p) : QObject((QDockWidget*)p)
  {
    this->ZeqManagerPanel  = NULL;
    this->ZeqInitialized   = 0;
  }
  //
  ~pqUI() {
  }

  bool ZeqReady() {
    if (!this->ProxyReady()) return 0;
    //
    if (!this->ZeqInitialized) {
      this->ZeqProxy->UpdateVTKObjects();
      this->ZeqInitialized = 1;
    }
    return this->ZeqInitialized;
  }

  bool ProxyReady() {
    if (!this->ProxyCreated()) {
      this->CreateProxy();
      return this->ProxyCreated();
    }
    return true;
  }

  void CreateProxy() {
    vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
    this->ZeqProxy.TakeReference(pm->NewProxy("zeq_helpers", "ZeqManager"));
    if (this->ZeqProxy) {
      this->pqZeqProxy = new pqProxy("zeq_helpers", "ZeqManager", ZeqProxy, 0, 0);
      this->ZeqProxy->UpdatePropertyInformation();
    }
    else {
      vtkGenericWarningMacro(<<"Failed to create proxy, check plugin loaded on server");
    }
  }

  void DeleteProxy() {
    if (this->ProxyCreated()) {
      vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
      pm->UnRegisterProxy("zeq_helpers", "ZeqManager",this->ZeqProxy);
      this->ZeqProxy = NULL;
      delete this->ZeqManagerPanel;
      delete this->pqZeqProxy;
      this->ZeqManagerPanel = NULL;
      this->pqZeqProxy = NULL;
    }
  }

  void CreatePanel(QScrollArea *ScrollArea) {
    if (this->ZeqReady()) {
      this->ZeqManagerPanel = new pqZeqManagerPanel(this->pqZeqProxy,NULL);
      ScrollArea->setWidget(this->ZeqManagerPanel);
    }
  }

  //
  bool ProxyCreated() { return this->ZeqProxy!=NULL; }
  pqZeqManagerPanel          *ZeqManagerPanel;
  int                         ZeqInitialized;
  vtkSmartPointer<vtkSMProxy> ZeqProxy;
  pqProxy                    *pqZeqProxy;
};
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
pqZeqManagerDockWindow::pqZeqManagerDockWindow(QWidget* p) : QDockWidget("Zeq Manager", p)
{
  this->UI = new pqZeqManagerDockWindow::pqUI(this);

  QWidget *dockWidgetContents = new QWidget();
  this->PanelLayout = new QVBoxLayout(dockWidgetContents);
  this->PanelLayout->setMargin(0);

  //
  // Most of the following code is taken from pqObjectInspector
  //

  this->ScrollArea = new QScrollArea();
  this->ScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  this->ScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  this->ScrollArea->setWidgetResizable(true);
  this->ScrollArea->setFrameShape(QFrame::NoFrame);

  QBoxLayout* buttonlayout = new QHBoxLayout();
  this->AcceptButton = new QPushButton(this);
  this->AcceptButton->setObjectName("Accept");
  this->AcceptButton->setText(tr("&Apply"));
  this->AcceptButton->setIcon(QIcon(QPixmap(":/pqWidgets/Icons/pqUpdate16.png")));
#ifdef Q_WS_MAC
  this->AcceptButton->setShortcut(QKeySequence(Qt::AltModifier + Qt::Key_A));
#endif
  this->ResetButton = new QPushButton(this);
  this->ResetButton->setObjectName("Reset");
  this->ResetButton->setText(tr("&Reset"));
  this->ResetButton->setIcon(QIcon(QPixmap(":/pqWidgets/Icons/pqCancel16.png")));
#ifdef Q_WS_MAC
  this->ResetButton->setShortcut(QKeySequence(Qt::AltModifier + Qt::Key_R));
#endif

  this->InitButton = new QPushButton(this);
  this->InitButton->setObjectName("Init");
  this->InitButton->setText(tr("&Init"));

  buttonlayout->addStretch();
  buttonlayout->addWidget(InitButton);
  buttonlayout->addWidget(this->AcceptButton);
  buttonlayout->addWidget(this->ResetButton);
  buttonlayout->addStretch();

  this->PanelLayout->addLayout(buttonlayout);
  this->PanelLayout->addWidget(this->ScrollArea);

  this->connect(this->AcceptButton, SIGNAL(clicked()), SLOT(accept()));
  this->connect(this->ResetButton, SIGNAL(clicked()), SLOT(reset()));
  this->connect(this->InitButton, SIGNAL(clicked()), SLOT(init()));
  
  this->InitButton->setEnabled(true);
  this->AcceptButton->setEnabled(false);
  this->ResetButton->setEnabled(false);

  // if XP Style is being used
  // swap it out for cleanlooks which looks almost the same
  // so we can have a green accept button
  // make all the buttons the same
  QString styleName = this->AcceptButton->style()->metaObject()->className();
  if(styleName == "QWindowsXPStyle")
     {
     QStyle* st = QStyleFactory::create("cleanlooks");
     st->setParent(this);
     this->AcceptButton->setStyle(st);
     this->ResetButton->setStyle(st);
     QPalette buttonPalette = this->AcceptButton->palette();
     buttonPalette.setColor(QPalette::Button, QColor(244,246,244));
     this->AcceptButton->setPalette(buttonPalette);
     this->ResetButton->setPalette(buttonPalette);
     }

  // Change the accept button palette so it is green when it is active.
  QPalette acceptPalette = this->AcceptButton->palette();
  acceptPalette.setColor(QPalette::Active, QPalette::Button, QColor(161, 213, 135));
  this->AcceptButton->setPalette(acceptPalette);
  this->AcceptButton->setDefault(true);

  //
  this->setWidget(dockWidgetContents);

  //
  // Link paraview server events to callbacks
  //
  pqServerManagerModel* smModel =
    pqApplicationCore::instance()->getServerManagerModel();

  this->connect(smModel, SIGNAL(aboutToRemoveServer(pqServer *)),
    this, SLOT(StartRemovingServer(pqServer *)));

  this->connect(smModel, SIGNAL(serverAdded(pqServer *)),
    this, SLOT(serverAdded(pqServer *)));

  this->serverAdded(pqActiveObjects::instance().activeServer());
}
//----------------------------------------------------------------------------
pqZeqManagerDockWindow::~pqZeqManagerDockWindow()
{
  if (this->UI->ZeqManagerPanel) {
    this->UI->ZeqManagerPanel->SaveSettings();
  }
  this->UI->DeleteProxy();
}
//----------------------------------------------------------------------------
void pqZeqManagerDockWindow::init()
{

  if (this->UI->ZeqReady()) {
    this->InitButton->setEnabled(false);

    this->UI->CreatePanel(this->ScrollArea);
    //
    // Link gui/proxy events to callbacks
    //
    QObject::connect(this->UI->ZeqManagerPanel, SIGNAL(modified()),
      this, SLOT(updateAcceptState()));

    QObject::connect(this->UI->pqZeqProxy,
      SIGNAL(modifiedStateChanged(pqServerManagerModelItem*)),
      this, SLOT(updateAcceptState()));
    //
    this->UI->ZeqManagerPanel->LoadSettings();
    //
    this->updateAcceptState();
  }
}
//----------------------------------------------------------------------------
void pqZeqManagerDockWindow::serverAdded(pqServer *server)
{
  this->init();
  this->accept();
  this->UI->ZeqManagerPanel->AutoStart();
}
//----------------------------------------------------------------------------
void pqZeqManagerDockWindow::StartRemovingServer(pqServer *server)
{
  this->UI->DeleteProxy();
  this->InitButton->setEnabled(true);
}
//-----------------------------------------------------------------------------
void pqZeqManagerDockWindow::setModified()
{
  emit this->modified();
}
//----------------------------------------------------------------------------
void pqZeqManagerDockWindow::showEvent( QShowEvent * event )
{
  QDockWidget::showEvent( event );
}
//-----------------------------------------------------------------------------
void pqZeqManagerDockWindow::accept()
{
  BEGIN_UNDO_SET("Apply");
  emit this->preaccept();

  QSet<pqProxy*> proxies_to_show;

  if (this->UI->ZeqManagerPanel)
    {
    int modified_state = this->UI->pqZeqProxy->modifiedState();
    if (modified_state == pqProxy::UNINITIALIZED)
      {
//      proxies_to_show.insert(refProxy);
      }
    this->UI->ZeqManagerPanel->accept();
    }

  emit this->accepted();
  emit this->postaccept();

  END_UNDO_SET();

  // Essential to render all views.
//  pqApplicationCore::instance()->render();
}

//-----------------------------------------------------------------------------
void pqZeqManagerDockWindow::reset()
{
  emit this->prereject();

  if(this->UI->ZeqManagerPanel)
    {
    this->UI->ZeqManagerPanel->reset();
    }

  emit this->postreject();
}
//-----------------------------------------------------------------------------
void pqZeqManagerDockWindow::canAccept(bool status)
{
  this->AcceptButton->setEnabled(status);

  bool resetStatus = status;
  if(resetStatus && this->UI->ZeqManagerPanel &&
     this->UI->pqZeqProxy->modifiedState() ==
     pqProxy::UNINITIALIZED)
    {
    resetStatus = false;
    }
  this->ResetButton->setEnabled(resetStatus);
}
//-----------------------------------------------------------------------------
void pqZeqManagerDockWindow::updateAcceptState()
{
  // watch for modified state changes
  bool acceptable = false;
  if(this->UI->pqZeqProxy->modifiedState() != pqProxy::UNMODIFIED)
    {
    acceptable = true;
    }
  this->canAccept(acceptable);
  if (acceptable)
    {
    emit this->canAccept();
    }
}
//-----------------------------------------------------------------------------
