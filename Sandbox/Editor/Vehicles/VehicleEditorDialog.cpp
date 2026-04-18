////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   VehicleEditorDialog.h
//  Version:     v1.00
//  Created:     02-06-2005 by MichaelR.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Resource.h"

#include "VehicleEditorDialog.h"
#include "IViewPane.h"
#include "Objects\Entity.h"
#include "Controls\PropertyCtrl.h"
#include "Controls\PropertyItem.h"
#include "Util\PakFile.h"
#include "Objects\SelectionGroup.h"

#include "VehiclePrototype.h"
#include "VehicleData.h"
#include "VehicleXMLSaver.h"
#include "VehicleMovementPanel.h"
#include "VehiclePartsPanel.h"
#include "VehicleFXPanel.h"
#include "VehicleHelperObject.h"
#include "VehiclePart.h"
#include "VehicleComp.h"
#include "VehicleModificationDialog.h"

#define VEED_FILE_FILTER "Vehicle XML Files (*.xml)|*.xml"
#define VEED_DIALOGFRAME_CLASSNAME "VehicleEditorDialog"

#define IDW_VEED_TASK_PANE      AFX_IDW_CONTROLBAR_FIRST+13
#define IDW_VEED_MOVEMENT_PANE  AFX_IDW_CONTROLBAR_FIRST+14
#define IDW_VEED_PARTS_PANE     AFX_IDW_CONTROLBAR_FIRST+15
#define IDW_VEED_SEATS_PANE     AFX_IDW_CONTROLBAR_FIRST+16


//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVehicleEditorDialog,CXTPFrameWnd)

BEGIN_MESSAGE_MAP(CVehicleEditorDialog, CXTPFrameWnd)
  ON_WM_SIZE()
  ON_WM_SETFOCUS()
  ON_WM_DESTROY()
  ON_WM_CLOSE()
  //ON_WM_ERASEBKGND()    
  ON_COMMAND( ID_VEHICLE_NEW, OnFileNew )
  ON_COMMAND( ID_VEHICLE_OPENSELECTED, OnFileOpen )
  ON_COMMAND( ID_VEHICLE_SAVE, OnFileSave )
  ON_COMMAND( ID_VEHICLE_SAVEAS, OnFileSaveAs )  
    
  // XT Commands.
  ON_MESSAGE( XTPWM_DOCKINGPANE_NOTIFY, OnDockingPaneNotify )
  ON_MESSAGE( XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify )
 
  // Taskpanel items 
  ON_COMMAND( ID_VEED_MOVEMENT_EDIT, OnMovementEdit )
  ON_COMMAND( ID_VEED_MOVEMENT_APPLY, OnApplyChanges )
  ON_COMMAND( ID_VEED_PARTS_EDIT, OnPartsEdit )
  ON_COMMAND( ID_VEED_PARTS_APPLY, OnApplyChanges )
  ON_COMMAND( ID_VEED_SEATS_EDIT, OnFXEdit )
  ON_COMMAND( ID_VEED_SEATS_APPLY, OnApplyChanges  )  
  ON_COMMAND( ID_VEED_MODS_EDIT, OnModsEdit  )  
  //ON_NOTIFY(NM_CLICK, ID_MOVEMENT_CAR, OnNotifyMovement )
  //ON_BN_CLICKED( ID_MOVEMENT_CAR, OnEditMovement )

END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
class CVehicleEditorViewClass : public TRefCountBase<IViewPaneClass>
{
  //////////////////////////////////////////////////////////////////////////
  // IClassDesc
  //////////////////////////////////////////////////////////////////////////
  virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
  virtual REFGUID ClassID()
  {
    // {8780CFDE-5415-4ac8-AFA3-2B95922A0705}
    static const GUID guid = { 0x8780cfde, 0x5415, 0x4ac8, { 0xaf, 0xa3, 0x2b, 0x95, 0x92, 0x2a, 0x7, 0x5 } };
    return guid;
  }
  virtual const char* ClassName() { return "Vehicle Editor"; };
  virtual const char* Category() { return "Vehicle Editor"; };
  //////////////////////////////////////////////////////////////////////////
  virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVehicleEditorDialog); };
  virtual const char* GetPaneTitle() { return _T("Vehicle Editor"); };
  virtual EDockingDirection GetDockingDirection() { return DOCK_FLOAT; };
  virtual CRect GetPaneRect() { return CRect(200,200,600,500); };
  virtual bool SinglePane() { return false; };
  virtual bool WantIdleUpdate() { return true; };
};

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::RegisterViewClass()
{
	// Disabling obsolete vehicle editor
	// GetIEditor()->GetClassFactory()->RegisterClass( new CVehicleEditorViewClass );
}

//////////////////////////////////////////////////////////////////////////
CVehicleEditorDialog::CVehicleEditorDialog()
: m_pVehicle( 0 )
, m_movementPanel( 0 )
, m_partsPanel( 0 )
, m_FXPanel( 0 )
{
  WNDCLASS wndcls;
  HINSTANCE hInst = AfxGetInstanceHandle();
  if (!(::GetClassInfo(hInst, VEED_DIALOGFRAME_CLASSNAME, &wndcls)))
  {
    // otherwise we need to register a new class
    //wndcls.style            = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wndcls.style            = CS_DBLCLKS;
    wndcls.lpfnWndProc      = ::DefWindowProc;
    wndcls.cbClsExtra       = wndcls.cbWndExtra = 0;
    wndcls.hInstance        = hInst;
    wndcls.hIcon            = NULL;
    wndcls.hCursor          = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
    wndcls.hbrBackground    = (HBRUSH) (COLOR_3DFACE + 1);
    //wndcls.hbrBackground    = 0;
    wndcls.lpszMenuName     = NULL;
    wndcls.lpszClassName    = VEED_DIALOGFRAME_CLASSNAME;
    if (!AfxRegisterClass(&wndcls))
    {
      AfxThrowResourceException();
    }
  }
  CRect rc(0,0,0,0);
  BOOL bRes = Create( WS_CHILD|WS_VISIBLE,rc,AfxGetMainWnd() );
  if (!bRes)
    return;
  
  OnInitDialog();
}

//////////////////////////////////////////////////////////////////////////
CVehicleEditorDialog::~CVehicleEditorDialog()
{ 
}

//////////////////////////////////////////////////////////////////////////
BOOL CVehicleEditorDialog::Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd )
{
  return __super::Create( VEED_DIALOGFRAME_CLASSNAME,"",dwStyle,rect,pParentWnd );
}


//////////////////////////////////////////////////////////////////////////
BOOL CVehicleEditorDialog::OnInitDialog()
{
  ModifyStyle(0, WS_CLIPCHILDREN );

  CRect rc;
  GetClientRect( &rc );
  //rc = CRect(0,0,600,600);

  try
  {    
    // Initialize the command bars
    if (!InitCommandBars())      
      return -1;
  }	catch (CResourceException *e){
    e->Delete();
    return -1;
  }

  // Get a pointer to the command bars object.
  CXTPCommandBars* pCommandBars = GetCommandBars();
  if(pCommandBars == NULL)
  {
    TRACE0("Failed to create command bars object.\n");
    return -1; 
  }

  // Add the menu bar
  CXTPCommandBar* pMenuBar = pCommandBars->SetMenu( _T("Menu Bar"),IDR_VEHICLE_EDITOR );	
  pMenuBar->SetFlags(xtpFlagStretched);
  pMenuBar->EnableCustomization(FALSE);
  
  // Create standard toolbar.
  //CXTPToolBar *pStdToolBar = pCommandBars->Add( _T("VehicleEditor ToolBar"),xtpBarTop );
  //VERIFY(pStdToolBar->LoadToolBar(IDR_AVI_RECORDER_BAR));
    
  GetDockingPaneManager()->InstallDockingPanes(this);
  GetDockingPaneManager()->SetTheme(xtpPaneThemeOffice2003);
  GetDockingPaneManager()->SetThemedFloatingFrames(TRUE);
  
  // init TaskPanel
  CreateTaskPanel();

  RecalcLayout();
     
  return TRUE;
}


//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::CreateTaskPanel()
{
  CRect rc;  
  GetClientRect( &rc );
  
  /////////////////////////////////////////////////////////////////////////
  // Docking Pane for TaskPanel
  m_pTaskDockPane = GetDockingPaneManager()->CreatePane( IDW_VEED_TASK_PANE, CRect(0,0,180,500), dockLeftOf );
  m_pTaskDockPane->SetOptions(xtpPaneNoCloseable|xtpPaneNoFloatable);
      
  /////////////////////////////////////////////////////////////////////////
  // Create empty Task Panel  
  m_taskPanel.Create( WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN, CRect(0,0,180,500), this, 0 );
  
  CMFCUtils::LoadTrueColorImageList( m_taskImageList, IDB_VEED_TREE, 16, RGB(255,0,255) );  
  m_taskPanel.SetImageList( &m_taskImageList );

  m_taskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);  
  m_taskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);  
  //m_taskPanel.SetSelectItemOnFocus(TRUE);
  m_taskPanel.AllowDrag(FALSE);    
  //m_taskPanel.SetAnimation( xtpTaskPanelAnimationNo );  
  m_taskPanel.SetHotTrackStyle(xtpTaskPanelHighlightItem);
  m_taskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(4,4,4,4);
  m_taskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(2,2,2,2);
  m_taskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(1,1,1,1);
  m_taskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(0,2,0,2);
  m_taskPanel.GetPaintManager()->m_rcControlMargins.SetRect(2,2,2,2);
  m_taskPanel.GetPaintManager()->m_nGroupSpacing = 0; 

  CXTPTaskPanelGroupItem *pItem =  NULL;
  CXTPTaskPanelGroup *pGroup = NULL;
  int groupId = 0;
  
  //////////////////////
  // Open/Save group
  //////////////////////
  pGroup = m_taskPanel.AddGroup(++groupId);
  pGroup->SetCaption( _T("Task") );

  // placeholder
  pItem = pGroup->AddTextItem(""); pItem->SetType(xtpTaskItemTypeText);

  pItem = pGroup->AddLinkItem(ID_VEHICLE_OPENSELECTED), pItem->SetType(xtpTaskItemTypeLink); 
  pItem->SetCaption( _T("Open selected") ); pItem->SetTooltip( _T("Open currently selected vehicle") );
  pItem->SetIconIndex(VEED_OPEN_ICON);

  // apply button
  pItem = pGroup->AddLinkItem(ID_VEED_PARTS_APPLY); pItem->SetType(xtpTaskItemTypeLink);
  pItem->SetCaption( _T("Save") ); pItem->SetTooltip( _T("Save Changes") );
  pItem->SetEnabled(false);        
  pItem->SetIconIndex(VEED_SAVE_ICON);

  pItem = pGroup->AddLinkItem(ID_VEHICLE_SAVEAS); pItem->SetType(xtpTaskItemTypeLink);
  pItem->SetCaption( _T("Save As..") ); pItem->SetTooltip( _T("Apply Changes") );
  pItem->SetEnabled(false);        
  //pItem->SetIconIndex(VEED_SAVE_ICON);

  //////////////////////
  // Edit group
  //////////////////////
  pGroup = m_taskPanel.AddGroup(++groupId);
  pGroup->SetCaption( _T("Edit") );  

  // placeholder
  pItem = pGroup->AddTextItem(""); pItem->SetType(xtpTaskItemTypeText);
  
  pItem = pGroup->AddLinkItem(ID_VEED_PARTS_EDIT); pItem->SetType(xtpTaskItemTypeLink);
  pItem->SetCaption( _T("Structure") ); pItem->SetTooltip( _T("Edit part structure") );
  pItem->SetEnabled(false);    
  pItem->SetIconIndex(VEED_PART_ICON);
  
  pItem = pGroup->AddLinkItem(ID_VEED_MOVEMENT_EDIT); pItem->SetType(xtpTaskItemTypeLink);
  pItem->SetCaption( _T("Movement") ); pItem->SetTooltip( _T("Edit Movement") );
  pItem->SetEnabled(false);    
  pItem->SetIconIndex(VEED_WHEEL_ICON);
      
  pItem = pGroup->AddLinkItem(ID_VEED_SEATS_EDIT); pItem->SetType(xtpTaskItemTypeLink);
  pItem->SetCaption( _T("Particles") ); pItem->SetTooltip( _T("Edit Particles") );    
  pItem->SetEnabled(false);
  pItem->SetIconIndex(VEED_PARTICLE_ICON);    

  pItem = pGroup->AddLinkItem(ID_VEED_MODS_EDIT); pItem->SetType(xtpTaskItemTypeLink);
  pItem->SetCaption( _T("Modifications") ); pItem->SetTooltip( _T("Edit Modifications") );    
  pItem->SetEnabled(false);
  pItem->SetIconIndex(VEED_MOD_ICON);    
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::SetVehiclePrototype( CVehiclePrototype* pProt )
{   
  // delete current objects, if present
  DeleteEditorObjects();

  // delete old prototype
  if (m_pVehicle)
  {
		m_pVehicle->RemoveEventListener( functor(*this,&CVehicleEditorDialog::OnPrototypeEvent) );    
    
    if (m_pVehicle->GetCEntity())
    {
      m_pVehicle->GetCEntity()->RemoveEventListener( functor(*this,&CVehicleEditorDialog::OnEntityEvent) );      
    }

    if (m_pVehicle != pProt)
      GetIEditor()->DeleteObject(m_pVehicle);

    m_pVehicle = 0;
  }  

  assert (pProt && pProt->GetVariable());
  m_pVehicle = pProt;

  m_pVehicle->AddEventListener( functor(*this,&CVehicleEditorDialog::OnPrototypeEvent) );
    
  if (m_pVehicle->GetCEntity())
  {
    m_pVehicle->GetCEntity()->AddEventListener( functor(*this,&CVehicleEditorDialog::OnEntityEvent) );    
  }

  for (TVeedComponent::iterator it = m_panels.begin(); it!=m_panels.end(); ++it)
  {
    (*it)->UpdateVehiclePrototype(m_pVehicle);
  }

  // if no panel open, 
  if (m_panels.size() == 0)  
    OnPartsEdit();
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnPrototypeEvent( CBaseObject *object, int event )
{
  // called upon prototype deletion
  if (event == CBaseObject::ON_DELETE)
  {    
    m_pVehicle = 0; 
    EnableEditingLinks(false);
  }
  else if (event == CBaseObject::ON_SELECT)
  {    
  }
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnEntityEvent( CBaseObject *object, int event )
{
  // called upon deletion of the entity the prototype points to
  if (event == CBaseObject::ON_DELETE)
  {    
    // delete prototype
    if (m_pVehicle)
      GetIEditor()->DeleteObject(m_pVehicle);
  }
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::EnableEditingLinks(bool enable)
{
  m_taskPanel.FindGroup(1)->FindItem(ID_VEED_PARTS_APPLY)->SetEnabled(enable);    
  m_taskPanel.FindGroup(1)->FindItem(ID_VEHICLE_SAVEAS)->SetEnabled(enable);    

  m_taskPanel.FindGroup(2)->FindItem(ID_VEED_PARTS_EDIT)->SetEnabled(enable);    
  m_taskPanel.FindGroup(2)->FindItem(ID_VEED_MOVEMENT_EDIT)->SetEnabled(enable);    
  m_taskPanel.FindGroup(2)->FindItem(ID_VEED_SEATS_EDIT)->SetEnabled(enable);   
  m_taskPanel.FindGroup(2)->FindItem(ID_VEED_MODS_EDIT)->SetEnabled(enable);   
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnMovementEdit()
{ 
  if (!m_pVehicle)
    return;

  if (!m_movementPanel)
  { 
    CXTPDockingPane* pTaskPane = GetDockingPaneManager()->FindPane(IDW_VEED_TASK_PANE);

    // open dock
    CRect rc(0,0,300,400);    
    GetClientRect( &rc );

    CXTPDockingPane* pDockPane = GetDockingPaneManager()->CreatePane( IDW_VEED_MOVEMENT_PANE, rc, dockRightOf, pTaskPane );
    //if (pTaskPane)
      //GetDockingPaneManager()->DockPane( pTaskPane, dockLeftOf, pDockPane );
    
    pDockPane->SetTitle( _T("Movement") );

    // create panel
    m_movementPanel = new CVehicleMovementPanel(this);  
    m_movementPanel->Create( NULL, NULL, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN, CRect(0,0,0,0), this, IDD_VEED_MOVEMENT_PANEL );  
    m_panels.push_back(m_movementPanel);

    m_movementPanel->UpdateVehiclePrototype(m_pVehicle);      
  } 
  else 
  {
    m_movementPanel->ShowWindow(SW_SHOW);
  }
}


//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnPartsEdit()
{ 
  if (!m_pVehicle)
    return;

  if (!m_partsPanel)
  {
    CXTPDockingPane* pTaskPane = GetDockingPaneManager()->FindPane(IDW_VEED_TASK_PANE);

    // open dock
    CRect rc(0,0,300,400);    
    GetClientRect( &rc );
    
    CXTPDockingPane* pDockPane = GetDockingPaneManager()->CreatePane( IDW_VEED_PARTS_PANE, rc, dockRightOf, pTaskPane );
    pDockPane->SetTitle( _T("Structure") );
    
    // create panel
    m_partsPanel = new CVehiclePartsPanel(this);  
    m_partsPanel->Create( NULL, NULL, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN, CRect(0,0,0,0), this, IDD_VEED_PARTS_PANEL );  
    m_panels.push_back(m_partsPanel);
        
    m_partsPanel->UpdateVehiclePrototype(m_pVehicle);  
  } 
  else 
  {
    m_partsPanel->ShowWindow(SW_SHOW);
  }
}


//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnApplyChanges()
{
  if (!m_pVehicle)
    return;

  if (m_partsPanel)
    m_partsPanel->UpdateVariables();

  ApplyToVehicle();
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnFXEdit()
{ 
  if (!m_pVehicle)
    return;

  if (!m_FXPanel)
  {
    CXTPDockingPane* pTaskPane = GetDockingPaneManager()->FindPane(IDW_VEED_TASK_PANE);

    // open dock
    CRect rc(0,0,300,400);    
    GetClientRect( &rc );

    CXTPDockingPane* pDockPane = GetDockingPaneManager()->CreatePane( IDW_VEED_SEATS_PANE, rc, dockRightOf, pTaskPane );
    pDockPane->SetTitle( _T("Particles") );
    
    // create panel
    m_FXPanel = new CVehicleFXPanel(this);  
    m_FXPanel->Create( NULL, NULL, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN, CRect(0,0,0,0), this, IDD_VEED_SEATS_PANEL );  
    m_panels.push_back(m_FXPanel);

    m_FXPanel->UpdateVehiclePrototype(m_pVehicle);  

    //m_taskPanel.FindGroup(1)->FindItem(ID_VEED_PARTS_APPLY)->SetEnabled(true);    
  } 
  else 
  {
    m_FXPanel->ShowWindow(SW_SHOW);
  }
}


//////////////////////////////////////////////////////////////////////////
bool CVehicleEditorDialog::ApplyToVehicle(CString filename)
{   
  if (!m_pVehicle)
    return false;

  // save xml 
  if (filename.GetLength() == 0)  
    filename = m_pVehicle->GetCEntity()->GetEntityClass() + CString(".xml");

  
  //update vehicle name corresponding to chosen filename
  if (IVariable* pName = GetChildVar(m_pVehicle->GetVariable(), "name"))
  {
    pName->Set(Path::GetFileName(filename));
  }  

  m_pVehicle->ApplyClonedData();

  XmlNodeRef node = VehicleDataSave( VEHICLE_XML_DEF, m_pVehicle->GetVehicleData() );
  
  
  /*CPakFile pakFile;
  if (pakFile.Open( VEHICLE_XML_PATH + filename )) // absolute path?
  {
    CString xmlString = node->getXML();
    pakFile.UpdateFile( VEHICLE_XML_PATH + filename, (void*)((const char*)xmlString), xmlString.GetLength() );
  }*/

  CString absDir = Path::GamePathToFullPath(VEHICLE_XML_PATH);  
  CFileUtil::CreateDirectory( absDir ); // make sure dir exist
  CString targetFile = absDir + filename;

  if (node != 0 && SaveXmlNode( node,targetFile ) )
  { 
    Log( "[CVehicleEditorDialog]: saved to %s.", targetFile );
  }
  else
  {
    Log( "[CVehicleEditorDialog]: not saved!", targetFile );
    return false;
  }

  // update the vehicle prototype
  bool res = m_pVehicle->ReloadEntity();
  
  return res;  
}




//////////////////////////////////////////////////////////////////////////
LRESULT CVehicleEditorDialog::OnDockingPaneNotify(WPARAM wParam, LPARAM lParam)
{
  if (wParam == XTP_DPN_SHOWWINDOW)
  {
    // get a pointer to the docking pane being shown.
    CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;    
    if (!pwndDockWindow->IsValid())
    {
      switch (pwndDockWindow->GetID())
      {
      case IDW_VEED_TASK_PANE:
        pwndDockWindow->Attach(&m_taskPanel);
        m_taskPanel.SetOwner(this);
        break;
      case IDW_VEED_MOVEMENT_PANE:
        pwndDockWindow->Attach(m_movementPanel);
        m_movementPanel->SetOwner(this);        
        break;      
      case IDW_VEED_PARTS_PANE:
        pwndDockWindow->Attach(m_partsPanel);
        m_partsPanel->SetOwner(this);        
        break;    
      case IDW_VEED_SEATS_PANE:
        pwndDockWindow->Attach(m_FXPanel);
        m_FXPanel->SetOwner(this);        
        break;      
      default:
        return FALSE;
      }
    }
    return TRUE;
  }
  else if (wParam == XTP_DPN_CLOSEPANE)
  {
    // get a pointer to the docking pane being closed.
    CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
    if (pwndDockWindow->IsValid())
    {
      switch (pwndDockWindow->GetID())
      {
      case IDW_VEED_TASK_PANE:      
        break;
      case IDW_VEED_MOVEMENT_PANE:
        // fixme: this can be called with pane already deleted
        if (m_movementPanel)
        {
          m_movementPanel->OnPaneClose();
          m_panels.remove((IVehicleDialogComponent*)m_movementPanel);
          m_movementPanel = 0;          
        }
        break;
      case IDW_VEED_PARTS_PANE:
        if (m_partsPanel)
        {
          m_partsPanel->OnPaneClose();
          m_panels.remove((IVehicleDialogComponent*)m_partsPanel);
          m_partsPanel = 0;          
        }        
        break;      
      case IDW_VEED_SEATS_PANE:
        if (m_FXPanel)
        {
          m_FXPanel->OnPaneClose();
          m_panels.remove((IVehicleDialogComponent*)m_FXPanel);
          m_FXPanel = 0;          
        }        
        break;
      }
    }
  }

  return FALSE;
}


//////////////////////////////////////////////////////////////////////////
BOOL CVehicleEditorDialog::PreTranslateMessage(MSG* pMsg)
{
  // allow tooltip messages to be filtered
  if (__super::PreTranslateMessage(pMsg))
    return TRUE;

  if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
  {
    // All keypresses are translated by this frame window

    ::TranslateMessage(pMsg);
    ::DispatchMessage(pMsg);

    return TRUE;
  }
  return FALSE;
}


//////////////////////////////////////////////////////////////////////////
LRESULT CVehicleEditorDialog::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
{
  switch(wParam) {
  case XTP_TPN_CLICK:
    {
      CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
      UINT nCmdID = pItem->GetID();
      SendMessage( WM_COMMAND, MAKEWPARAM(nCmdID, 0), 0 );
    }
    break;

  case XTP_TPN_RCLICK:
    break;
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnClose()
{
  DeleteEditorObjects();

  // hide when prototype still existent
  if (m_pVehicle)
  {
    ShowWindow(SW_HIDE);
  }
  else
  {
    __super::OnClose();
  }    
}


//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::PostNcDestroy()
{
  delete this;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::DoDataExchange(CDataExchange* pDX)
{
  __super::DoDataExchange(pDX);
}


//////////////////////////////////////////////////////////////////////////
//BOOL CVehicleEditorDialog::OnEraseBkgnd(CDC* pDC) 
//{
//  return TRUE;
//}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::RecalcLayout( BOOL bNotify )
{
  __super::RecalcLayout( bNotify );

  //if (m_pTaskDockPane->m_hWnd)
  //{
  //  CRect rc;
  //  GetClientRect(rc);
  //  rc.right = 250;    
  //  m_pTaskDockPane->MoveWindow(rc,FALSE);
  //}  
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnSize(UINT nType, int cx, int cy)
{
  __super::OnSize(nType, cx, cy);  

 /* if (m_taskPanel.m_hWnd)
  {
    CRect rc;
    GetClientRect(rc);
    CRect rc1(rc);
    rc1.right = 200;
    m_taskPanel.MoveWindow(rc1,FALSE);    
  }*/ 
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnSetFocus(CWnd* pOldWnd)
{
  __super::OnSetFocus(pOldWnd);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::Close()
{
  //DestroyWindow();
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnDestroy()
{
  DeleteEditorObjects();

  if (m_pVehicle)
  {
    m_pVehicle->RemoveEventListener( functor(*this,&CVehicleEditorDialog::OnPrototypeEvent) );    

    if (m_pVehicle->GetCEntity())
    {
      m_pVehicle->GetCEntity()->RemoveEventListener( functor(*this,&CVehicleEditorDialog::OnEntityEvent) );      
    }
    
    // delete prototype when dialog closed.
    GetIEditor()->DeleteObject(m_pVehicle);    
  }

  __super::OnDestroy();  
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnFileNew()
{
  AfxMessageBox(IDS_VEED_NEW, MB_OK|MB_ICONINFORMATION);  
}



//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnFileOpen()
{
  OpenVehicle();  
}


//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnFileSave()
{
  if (!m_pVehicle)
    Log("No active vehicle prototype");
  else
    ApplyToVehicle();
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnFileSaveAs()
{  
  if (!m_pVehicle)
  {
    Log("No active vehicle prototype");
    return;
  }
  
  // CString dir = Path::GetPath(filename);
  CString filename("MyVehicle.xml");
  CString path = Path::GamePathToFullPath(VEHICLE_XML_PATH);
  if (CFileUtil::SelectSaveFile( VEED_FILE_FILTER, "xml", path, filename ))
  {
    CWaitCursor wait;
    if (ApplyToVehicle(Path::GetFile(filename)))
    {
      Warning("The new vehicle entity will be registered at next Editor/Game launch.");
    }
  }  
}


//////////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::GetObjectsByClass(const CRuntimeClass* pClass, std::vector<CBaseObject*>& objects)
{ 
  objects.clear();

  for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
  {
    if ((*it).first->IsKindOf(pClass))
    {
      objects.push_back(it->first);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
inline bool SortObjectsByName( CBaseObject* pObject1, CBaseObject* pObject2 )
{
  return stricmp(pObject1->GetName(),pObject2->GetName()) < 0;
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnPropsSelChanged(CPropertyItem* pItem)
{
  // dynamically fills selectboxes

  if (!pItem)
    return;

  int varType = pItem->GetVariable()->GetDataType();    

  if (varType != IVariable::DT_VEEDHELPER && varType != IVariable::DT_VEEDPART 
    && varType != IVariable::DT_VEEDCOMP)
    return;

  std::vector<CBaseObject*> objects;

  switch (varType)
  {
  case IVariable::DT_VEEDHELPER:
    GetObjectsByClass(RUNTIME_CLASS(CVehicleHelper), objects);
    break;
  case IVariable::DT_VEEDPART:
    GetObjectsByClass(RUNTIME_CLASS(CVehiclePart), objects);    
    break;
  case IVariable::DT_VEEDCOMP:
    GetObjectsByClass(RUNTIME_CLASS(CVehicleComponent), objects);    
    break;
  }

  std::sort(objects.begin(), objects.end(), SortObjectsByName);

  // store current value
  CString val; 
  pItem->GetVariable()->Get(val);

  CVarEnumList<CString>* list = new CVarEnumList<CString>;
  list->AddItem("", "");

  for (std::vector<CBaseObject*>::iterator it = objects.begin(); it != objects.end(); ++it)
  {
    CString name = (*it)->GetName();
    list->AddItem(name, name);
  }

  CVariableEnum<CString>* pEnum = (CVariableEnum<CString>*)pItem->GetVariable();
  pEnum->SetEnumList(list);
  //pEnum->Set(val);
}


//////////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::DeleteEditorObjects()
{
  CUndoSuspend susp;
  IObjectManager* pObjMan = GetIEditor()->GetObjectManager();

  // notify panels of deletion
  for (TVeedComponent::iterator it = m_panels.begin(); it!=m_panels.end(); ++it)
  {
    (*it)->NotifyObjectsDeletion(m_pVehicle);
  }  

  // gather objects for deletion
  std::list<CBaseObject*> delObjs;
  
  for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
  {
    // this function is used for cleanup only and therefore must not delete variables
    if (IVeedObject* pVO = IVeedObject::GetVeedObject(it->first)) 
    {
      pVO->DeleteVar(false);    
    }

    // only mark top-level parts for deletion, they take care of children
    if ((*it).first->GetParent() == m_pVehicle)
    {
      delObjs.push_back(it->first);
    }
  }
  
  for (std::list<CBaseObject*>::iterator it = delObjs.begin(); it != delObjs.end(); ++it)
  {
    pObjMan->DeleteObject(*it);
  }
  
  m_partToTree.clear();
  GetIEditor()->SetModifiedFlag();
}


//////////////////////////////////////////////////////////////////////////
bool CVehicleEditorDialog::OpenVehicle(bool silent /*=false*/)
{
  // create prototype for currently selected vehicle
  CSelectionGroup* group = GetIEditor()->GetSelection();

  if (!group || group->IsEmpty())
  {
    if (!silent) 
      AfxMessageBox("No vehicle selected.");
    return false;
  }

  if (group->GetCount() > 1)
  {
    if (!silent) 
      AfxMessageBox("Error, multiple objects selected.");
    return false;
  }

  bool ok = false;
  CString sFile, sClass;

  CBaseObject* obj = group->GetObject(0);

  if (!obj) 
    return false;

  if (!obj->IsKindOf(RUNTIME_CLASS(CEntity)))
  {
    if (!silent) 
      Log("Selected object %s is no vehicle entity.", obj->GetName());
    return false;
  }

  CEntity* pEnt = (CEntity*)obj;
  CEntityScript* pScript = pEnt->GetScript();

  if (!pScript)
  {
    if (!silent) 
      Warning("%s: Entity script is NULL!");
    return false;
  }

  sFile = pScript->GetFile();
  if (-1 == sFile.MakeLower().Find("vehiclepool"))      
  {
    if (!silent) 
      Warning("%s doesn't seem to be a valid vehicle (got script: %s)", pEnt->GetName(), sFile);
    return false;
  }

  // create prototype and set entity on it
  CVehiclePrototype* pProt = (CVehiclePrototype*)GetIEditor()->GetObjectManager()->NewObject("VehiclePrototype", 0, pEnt->GetEntityClass());

  if (!pProt)
  {
    if (!silent) 
      Warning("Spawn of prototype failed, please inform Code!");
    return false;
  }

  pProt->SetVehicleEntity(pEnt);
  SetVehiclePrototype(pProt);
  EnableEditingLinks(true); 

  return true;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleEditorDialog::OnModsEdit()
{  
  CVehicleModificationDialog dlg;

  IVariable* pMods = GetOrCreateChildVar( m_pVehicle->GetVariable(), "Modifications" );  
  dlg.SetVariable( pMods );

  if ( dlg.DoModal() == IDOK )
  { 
    // store updated mods
    ReplaceChildVars( dlg.GetVariable(), pMods );    
  }
}

