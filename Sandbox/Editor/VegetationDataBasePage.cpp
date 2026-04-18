////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   vegetationpanel.cpp
//  Version:     v1.00
//  Created:     31/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "VegetationDataBasePage.h"
#include "VegetationTool.h"
#include "Heightmap.h"
#include "VegetationMap.h"
#include "StringDlg.h"
#include "Viewport.h"
#include "CryEditDoc.h"
#include "SurfaceType.h"
#include "Material\MaterialManager.h"
#include "IViewPane.h"

#define IDC_TASKPANEL 1

#define IDC_SPLIT_WINDOW 3
#define IDC_REPORT_CONTROL AFX_IDW_PANE_FIRST
#define IDC_PROPERTYPANEL AFX_IDW_PANE_FIRST+1

#define ID_PANEL_VEG_DISABLE_PREVIEW 200

#define COLUMN_VISIBLE   0
#define COLUMN_OBJECT    1
#define COLUMN_CATEGORY  2
#define COLUMN_COUNT     3
#define COLUMN_TEXSIZE   4
#define COLUMN_MATERIAL  5

#define COLUMN_MAIL_ICON	0
#define COLUMN_CHECK_ICON	2

#define VDB_COMMAND_HIDE	1
#define VDB_COMMAND_UNHIDE	2


//////////////////////////////////////////////////////////////////////////
class CVegetationDBPageClass : public TRefCountBase<IViewPaneClass>
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
	virtual REFGUID ClassID()
	{
		// {8903493F-8946-40ab-B7EE-EEF0C0D15E5A}
		static const GUID guid = 
		{ 0x8903493f, 0x8946, 0x40ab, { 0xb7, 0xee, 0xee, 0xf0, 0xc0, 0xd1, 0x5e, 0x5a } };
		return guid;
	}
	virtual const char* ClassName() { return "Vegetation Editor"; };
	virtual const char* Category() { return "DataBaseView"; };
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVegetationDataBasePage); };
	virtual const char* GetPaneTitle() { return _T("Vegetation"); };
	virtual EDockingDirection GetDockingDirection() { return DOCK_FLOAT; };
	virtual CRect GetPaneRect() { return CRect(200,200,600,500); };
	virtual bool SinglePane() { return true; };
	virtual bool WantIdleUpdate() { return true; };
};

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::RegisterViewClass()
{
	GetIEditor()->GetClassFactory()->RegisterClass( new CVegetationDBPageClass );
}

/////////////////////////////////////////////////////////////////////////////
// CVegetationDataBasePage dialog
//////////////////////////////////////////////////////////////////////////
CVegetationDataBasePage::CVegetationDataBasePage(CWnd* pParent /*=NULL*/)
	: CDataBaseDialogPage(CVegetationDataBasePage::IDD, pParent)
{
	m_bIgnoreSelChange = false;
	m_bActive = false;
	m_bSyncSelection = true;

	m_nCommand = 0;

	m_vegetationMap = GetIEditor()->GetHeightmap()->GetVegetationMap();

	Create( IDD,pParent );
}

//////////////////////////////////////////////////////////////////////////
CVegetationDataBasePage::~CVegetationDataBasePage()
{
	GetIEditor()->UnregisterNotifyListener( this );
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
		//DDX_Control(pDX, IDC_RADIUS, m_radius);
	//DDX_Control(pDX, IDC_PAINT, m_paintButton);	
	//DDX_Control(pDX, IDC_OBJECTS, m_objectsTree);
	//DDX_Control(pDX, IDC_PROPERTIES, m_propertyCtrl);
	//DDX_Control(pDX, IDC_INFO, m_info);
}


BEGIN_MESSAGE_MAP(CVegetationDataBasePage, CDataBaseDialogPage)
	//{{AFX_MSG_MAP(CVegetationDataBasePage)
	//}}AFX_MSG_MAP
	//ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_RADIUS, OnReleasedcaptureRadius)
	ON_CONTROL( WMU_FS_CHANGED,IDC_RADIUS,OnBrushRadiusChange )
	ON_BN_CLICKED(IDC_PAINT, OnPaint)
	ON_WM_SIZE()

	ON_COMMAND( ID_PANEL_VEG_ADD,OnAdd )
	ON_COMMAND( ID_PANEL_VEG_ADDCATEGORY,OnNewCategory )
	ON_COMMAND( ID_PANEL_VEG_CLONE,OnClone )
	ON_COMMAND( ID_PANEL_VEG_REPLACE,OnReplace )
	ON_COMMAND( ID_PANEL_VEG_REMOVE,OnRemove )

	ON_COMMAND( ID_PANEL_VEG_DISTRIBUTE,OnDistribute )
	ON_COMMAND( ID_PANEL_VEG_CLEAR,OnClear )
	ON_COMMAND( ID_PANEL_VEG_MERGE,OnBnClickedMerge )
	ON_COMMAND( ID_PANEL_VEG_CREATE_SEL,OnBnClickedCreateSel )
	ON_COMMAND( ID_PANEL_VEG_IMPORT,OnBnClickedImport )
	ON_COMMAND( ID_PANEL_VEG_EXPORT,OnBnClickedExport )
	ON_COMMAND( ID_PANEL_VEG_SCALE,OnBnClickedScale )

	ON_COMMAND( ID_PANEL_VEG_HIDE,OnHide )
	ON_COMMAND( ID_PANEL_VEG_UNHIDE,OnUnhide )
	ON_COMMAND( ID_PANEL_VEG_HIDEALL,OnHideAll )
	ON_COMMAND( ID_PANEL_VEG_UNHIDEALL,OnUnhideAll )

	ON_MESSAGE(XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify)
	ON_NOTIFY(NM_CLICK, IDC_REPORT_CONTROL, OnReportClick)
	ON_NOTIFY(NM_RCLICK, IDC_REPORT_CONTROL, OnReportRClick)
	ON_NOTIFY(XTP_NM_REPORT_SELCHANGED, IDC_REPORT_CONTROL, OnReportSelChange)
	ON_NOTIFY(XTP_NM_REPORT_HYPERLINK, IDC_REPORT_CONTROL, OnReportHyperlink)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CVegetationDataBasePage message handlers

void CVegetationDataBasePage::OnBrushRadiusChange() 
{
	int radius = m_radius.GetValue();

	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationTool)))
	{
		((CVegetationTool*)pTool)->SetBrushRadius(radius);
	}

	AfxGetMainWnd()->SetFocus();
}

void CVegetationDataBasePage::OnPaint() 
{
	bool paint = m_paintButton.GetCheck()==0;

	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationTool)))
	{
		((CVegetationTool*)pTool)->SetMode( paint );
	}

	if (m_paintButton.GetCheck() == 0)
		m_paintButton.SetCheck(1);
	else
		m_paintButton.SetCheck(0);
	AfxGetMainWnd()->SetFocus();
}

//////////////////////////////////////////////////////////////////////////
BOOL CVegetationDataBasePage::OnInitDialog() 
{
	__super::OnInitDialog();

	CRect rc;
	GetClientRect( rc );

	m_splitWnd.CreateStatic( this,1,2,WS_CHILD|WS_VISIBLE,IDC_SPLIT_WINDOW );

	//////////////////////////////////////////////////////////////////////////
	// Properties.
	//////////////////////////////////////////////////////////////////////////
	m_propertyCtrl.Create( WS_VISIBLE|WS_CHILD|WS_BORDER,rc,this,IDC_PROPERTYPANEL );

	CRect rcprev(0,0,120,120);
	m_previewCtrl.Create( this,rcprev,WS_VISIBLE|WS_CHILD|WS_BORDER );
	m_previewCtrl.EnableWindow(TRUE);

	//////////////////////////////////////////////////////////////////////////
	// Task panel.
	//////////////////////////////////////////////////////////////////////////
	m_wndTaskPanel.Create( WS_CHILD|WS_BORDER|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,rc,this,IDC_TASKPANEL );

	CMFCUtils::LoadTrueColorImageList( m_taskImageList,IDR_PANEL_VEGETATION,14,RGB(192,192,192) );

	//m_taskImageList.Create( IDR_PANEL_VEGETATION, 13, 1, RGB (255,255,255));
	m_wndTaskPanel.SetImageList( &m_taskImageList );
	m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
	m_wndTaskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);
	m_wndTaskPanel.SetSelectItemOnFocus(TRUE);
	m_wndTaskPanel.AllowDrag(TRUE);

	m_wndTaskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(1,3,1,3);
	m_wndTaskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(4,4,4,4);
	m_wndTaskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(1,1,1,1);
	m_wndTaskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcControlMargins.SetRect(2,0,2,0);
	m_wndTaskPanel.GetPaintManager()->m_nGroupSpacing = 3;

	// Add default tasks.
	CXTPTaskPanelGroupItem *pItem =  NULL;
	CXTPTaskPanelGroup *pGroup = NULL;
	{
		pGroup = m_wndTaskPanel.AddGroup(1);
		pGroup->SetCaption( "Groups" );

		pItem =  pGroup->AddLinkItem(ID_PANEL_VEG_ADD,0); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Add" ); pItem->SetTooltip( _T("Add new vegetation group(s)") );
		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_CLONE,1); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Duplicate" ); pItem->SetTooltip( _T("Duplicate selected vegetation group(s)") );
		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_REPLACE,2); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Replace" ); pItem->SetTooltip( _T("Replace cgf file of the selected vegetation group(s)") );
		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_REMOVE,4); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Remove" ); pItem->SetTooltip( _T("Remove selected vegetation group(s)") );

		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_HIDE,10); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Hide" ); pItem->SetTooltip( _T("Hide selected vegetation group(s)") );
		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_UNHIDE,11); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Unhide" ); pItem->SetTooltip( _T("Unhide selected vegetation group(s)") );
		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_HIDEALL,12); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Hide All" ); pItem->SetTooltip( _T("Hide all vegetation Groups") );
		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_UNHIDEALL,13); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Unhide All" ); pItem->SetTooltip( _T("Unhide all vegetation groups)") );
	}
	{
		pGroup = m_wndTaskPanel.AddGroup(2);
		pGroup->SetCaption( "Instances" );

		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_DISTRIBUTE,7); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Distribute" ); pItem->SetTooltip( _T("Distribute instances of the selected groups on map") );
		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_CLEAR,8); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Clear" ); pItem->SetTooltip( _T("Clear instances of the selected groups from map") );
		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_SCALE,9); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Scale" ); pItem->SetTooltip( _T("Scale instances of the selected groups") );
	}
	
	{
		pGroup = m_wndTaskPanel.AddGroup(3);
		pGroup->SetCaption( "Manage" );

		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_ADDCATEGORY,3); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Change Category" ); pItem->SetTooltip( _T("Change the category of the selected vegetation group(s)") );

		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_CREATE_SEL,15); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Instances to Category" ); pItem->SetTooltip( _T("Move selected instances to category") );

		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_MERGE,14); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Merge" ); pItem->SetTooltip( _T("Merge vegetation instances") );
		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_IMPORT,6); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Import" ); pItem->SetTooltip( _T("Import vegetation groups and instances") );
		pItem = pGroup->AddLinkItem(ID_PANEL_VEG_EXPORT,5); pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption( "Export" ); pItem->SetTooltip( _T("Export selected vegetation groups and their instances") );
	}

	{
		pGroup = m_wndTaskPanel.AddGroup(4);
		pGroup->SetCaption( "Info" );

		pItem = pGroup->AddLinkItem(IDC_INFO); pItem->SetType(xtpTaskItemTypeText);
		pItem->SetCaption( "Info on vegetation" );
	}
	
	{
		pGroup = m_wndTaskPanel.AddGroup(5);
		pGroup->SetCaption( "Preview" );

		pItem = pGroup->AddLinkItem(IDC_PREVIEW); pItem->SetType(xtpTaskItemTypeControl);
		pItem->SetControlHandle( m_previewCtrl.GetSafeHwnd() );
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Report control.
	//////////////////////////////////////////////////////////////////////////
	VERIFY( m_wndReport.Create(WS_CHILD|WS_TABSTOP|WS_VISIBLE|WM_VSCROLL, CRect(0, 0, 0, 0), this, IDC_REPORT_CONTROL) );

	//m_imageList.Create(IDB_ERROR_REPORT, 16, 1, RGB (255, 255, 255));
	//CMFCUtils::LoadTrueColorImageList( m_imageList,IDB_ERROR_REPORT,16,RGB(255,255,255) );
	CMFCUtils::LoadTrueColorImageList( m_reportImageList,IDB_LAYER_BUTTONS,16,RGB(255,0,255) );

	m_wndReport.SetImageList( &m_reportImageList );

	CXTPReportColumn *pCategoryCol;
	//  Add sample columns
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_VISIBLE, _T(""), 24, FALSE,1 ));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_OBJECT, _T("Object"), 200, TRUE));
	pCategoryCol = m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_CATEGORY, _T("Category"), 30, TRUE,XTP_REPORT_NOICON,TRUE,FALSE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_COUNT, _T("Count"), 40, TRUE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_TEXSIZE, _T("TexMem Usage"), 40, TRUE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_MATERIAL, _T("Material"), 100, TRUE));

	m_wndReport.GetColumns()->GetGroupsOrder()->Add( pCategoryCol );
	m_wndReport.ShowGroupBy(TRUE);
	m_wndReport.SetMultipleSelection(TRUE);
	m_wndReport.SkipGroupsFocus(TRUE);
	//////////////////////////////////////////////////////////////////////////

	m_splitWnd.SetPane( 0,0,&m_wndReport,CSize(2*rc.Width()/3,100) );
	m_splitWnd.SetPane( 0,1,&m_propertyCtrl,CSize(1*rc.Width()/3,100) );

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	CRect rc;
	GetClientRect(rc);

	CRect taskRc(rc);
	CRect splitRc(rc);
	CRect previewRc(rc);
	CRect infoRc(rc);

	taskRc.DeflateRect(1,1,1,1);
	taskRc.right = taskRc.left + 160;
	//taskRc.bottom = rc.bottom - 180;

	infoRc = taskRc;
	infoRc.top = taskRc.bottom;
	infoRc.bottom = infoRc.top + 40;

	previewRc = taskRc;
	previewRc.top = infoRc.bottom;
	previewRc.bottom = rc.bottom;

	splitRc.left = taskRc.right + 0;

	if (m_wndTaskPanel.m_hWnd)
		m_wndTaskPanel.MoveWindow( taskRc,TRUE );
	if (m_splitWnd.m_hWnd)
		m_splitWnd.MoveWindow( splitRc,TRUE );
	//if (m_wndReport.m_hWnd)
		//m_wndReport.MoveWindow( rc,TRUE );
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::SetActive( bool bActive )
{
	if (bActive != m_bActive)
	{
		if (bActive)
			GetIEditor()->RegisterNotifyListener( this );
		else
			GetIEditor()->UnregisterNotifyListener( this );
	}
	m_vegetationMap = GetIEditor()->GetHeightmap()->GetVegetationMap();
	m_bActive = bActive;
	if (m_bActive)
		ReloadObjects();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::Update()
{
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnNewCategory()
{
	Selection objects;
	GetSelectedObjects( objects );
	if (!objects.empty())
	{
		CStringDlg dlg( "Change Selected Group(s) Category" );
		dlg.SetString( objects[0]->GetCategory() );
		if (dlg.DoModal() == IDOK)
		{
			for (int i = 0; i < objects.size(); i++)
			{
				objects[i]->SetCategory( dlg.GetString() );
			}
			ReloadObjects();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnDistribute()
{
	if(IDYES==MessageBox("Are you sure you want to Distribute?", "Vegetation Distribute", MB_YESNO | MB_ICONQUESTION))
	{
		CUndo undo( "Vegetation Distribute" );
		BeginWaitCursor();

		CEditTool *pTool = GetIEditor()->GetEditTool();
		if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationTool)))
		{
			((CVegetationTool*)pTool)->Distribute();
		}
		else
		{
			CVegetationTool *pTool = new CVegetationTool;
			pTool->Distribute();
			pTool->Release();
		}
		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnDistributeMask()
{
	CUndo undo( "Vegetation Distribute Mask" );
	CString file;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_TEXTURE,file ))
	{
		BeginWaitCursor();

		CEditTool *pTool = GetIEditor()->GetEditTool();
		if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationTool)))
		{
			((CVegetationTool*)pTool)->DistributeMask( file );
		}

		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnClear()
{
	if(IDYES==MessageBox("Are you sure you want to Clear?", "Vegetation Clear", MB_YESNO | MB_ICONQUESTION))
	{
		CUndo undo( "Vegetation Clear" );
		BeginWaitCursor();
		
		CEditTool *pTool = GetIEditor()->GetEditTool();
		if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationTool)))
		{
			((CVegetationTool*)pTool)->Clear();
		}
		else
		{
			CVegetationTool *pTool = new CVegetationTool;
			pTool->Clear();
			pTool->Release();
		}

		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnClearMask()
{
	CUndo undo( "Vegetation Clear Mask" );
	CString file;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_TEXTURE,file ))
	{
		BeginWaitCursor();

		CEditTool *pTool = GetIEditor()->GetEditTool();
		if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationTool)))
		{
			((CVegetationTool*)pTool)->ClearMask( file );
		}

		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnBnClickedScale()
{
	CUndo undo( "Vegetation Scale" );

	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationTool)))
	{
		((CVegetationTool*)pTool)->ScaleObjects();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnBnClickedImport()
{
	CUndo undo( "Vegetation Import" );
	CString file;
	if (CFileUtil::SelectFile( "Vegetation Objects(*.veg)|*.veg|All Files|*.*||",GetIEditor()->GetLevelFolder(),file ))
	{
		XmlParser parser;
		XmlNodeRef root = parser.parse(file);
		if (!root)
			return;

		m_propertyCtrl.DeleteAllItems();

		CWaitCursor wait;
		CUndo undo( "Import Vegetation" );
    for (int i = 0; i < root->getChildCount(); i++)
		{
			m_vegetationMap->ImportObject(root->getChild(i));
		}
		ReloadObjects();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnBnClickedMerge()
{
	if(IDYES==MessageBox("Are you sure you want to Merge?", "Vegetation Merge", MB_YESNO | MB_ICONQUESTION))
	{
		CWaitCursor wait;
		CUndo undo( "Vegetation Merge" );
		Selection objects;
		std::vector<CVegetationObject*> delObjects;
		GetSelectedObjects( objects );
		for (int i = 0; i < objects.size(); i++)
			for (int j = i+1; j < objects.size(); j++)
				//if(!strcmp(objects[i]->GetFileName(), objects[j]->GetFileName()) )
				{
					m_vegetationMap->MergeObjects( objects[i], objects[j]);
					delObjects.push_back(objects[j]);
				}
		for (int k = 0; k < delObjects.size(); k++)
			m_vegetationMap->RemoveObject(delObjects[k]);
		ReloadObjects();
		GetIEditor()->Notify( eNotify_OnVegetationPanelUpdate );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnBnClickedCreateSel()
{
	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationTool)) && ((CVegetationTool*)pTool)->GetCountSelectedInstances())
	{
		CStringDlg dlg( "Enter Category name" );
		if (dlg.DoModal() == IDOK)
		{
			((CVegetationTool*)pTool)->MoveSelectedInstancesToCategory(dlg.GetString());
			ReloadObjects();
			GetIEditor()->Notify( eNotify_OnVegetationPanelUpdate );
		}
		AfxGetMainWnd()->SetFocus();
	}
	else
		AfxMessageBox( "Select Instances first" );
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnBnClickedExport()
{
	CUndo undo( "Vegetation Export" );
	Selection objects;
	GetSelectedObjects( objects );
	if (objects.empty())
	{
		AfxMessageBox( "Select Objects For Export" );
		return;
	}

	CString fileName;
	if (CFileUtil::SelectSaveFile( "Vegetation Objects (*.veg)|*.veg||","veg",GetIEditor()->GetLevelFolder(),fileName ))
	{
		CWaitCursor wait;
		XmlNodeRef root = CreateXmlNode( "Vegetation" );
		for (int i = 0; i < objects.size(); i++)
		{
			XmlNodeRef node = root->newChild( "VegetationObject" );
			m_vegetationMap->ExportObject( objects[i],node );
		}
		SaveXmlNode( root, fileName );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnHide()
{
	CWaitCursor wait;
	Selection objects;
	GetSelectedObjects( objects );
	for (int i = 0; i < objects.size(); i++)
	{
		m_vegetationMap->HideObject( objects[i],true );
	}
	ReloadObjects();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnUnhide()
{
	CWaitCursor wait;
	Selection objects;
	GetSelectedObjects( objects );
	for (int i = 0; i < objects.size(); i++)
	{
		m_vegetationMap->HideObject( objects[i],false );
	}
	ReloadObjects();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnHideAll()
{
	CWaitCursor wait;
	m_vegetationMap->HideAllObjects(true);
	ReloadObjects();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnUnhideAll()
{
	CWaitCursor wait;
	m_vegetationMap->HideAllObjects(false);
	ReloadObjects();
}

//////////////////////////////////////////////////////////////////////////
class CVegObjRecord : public CXTPReportRecord  
{
	DECLARE_DYNAMIC(CVegObjRecord)
public:
	GUID m_objectGUID;
	CXTPReportRecordItem *m_pVisibleIconItem;

	CVegObjRecord( CVegetationObject *pObject )
	{
		m_objectGUID = pObject->GetGUID();

		CString mtlName;
		CString texSize;
		if (pObject->GetObject())
		{
			IMaterial *pMtl = pObject->GetObject()->GetMaterial();
			if (pMtl)
				mtlName = Path::GetFileName( pMtl->GetName() );

			int nTexSize = pObject->GetTextureMemUsage();
			texSize.Format( "%d K",nTexSize/1024 );
		}
		CString count;
		count.Format( "%d",pObject->GetNumInstances() );
		
		m_pVisibleIconItem = AddItem(new CXTPReportRecordItem());
		AddItem(new CXTPReportRecordItemText(pObject->GetFileName()));
		AddItem(new CXTPReportRecordItemText(pObject->GetCategory()));
		AddItem(new CXTPReportRecordItemText(count));
		AddItem(new CXTPReportRecordItemText(texSize));
		AddItem(new CXTPReportRecordItemText(mtlName))->AddHyperlink(new CXTPReportHyperlink(0,mtlName.GetLength()));

		int nIcon = 0;
		if (pObject->IsHidden())
		{
			nIcon = -1;
		}

		m_pVisibleIconItem->SetIconIndex(nIcon);
		m_pVisibleIconItem->SetGroupPriority(nIcon);
		m_pVisibleIconItem->SetSortPriority(nIcon);
	}

	virtual void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
	{
	}
};
IMPLEMENT_DYNAMIC(CVegObjRecord,CXTPReportRecord);

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::ReloadObjects()
{
	m_vegetationMap = GetIEditor()->GetHeightmap()->GetVegetationMap();
	m_wndReport.SetRedraw(FALSE);
	m_wndReport.GetRecords()->RemoveAll();

	if (!m_bActive)
	{
		m_wndReport.SetRedraw(TRUE);
		return;
	}

	/*
	// Clear all selections.
	for (int i = 0; i < m_vegetationMap->GetObjectCount(); i++)
	{
		m_vegetationMap->GetObject(i)->SetSelected(false);
	}
	// Clear items within category.
	for (cit = m_categoryMap.begin(); cit != m_categoryMap.end(); ++cit)
	{
		HTREEITEM hRoot = m_objectsTree.InsertItem( cit->first,0,1 );
		m_objectsTree.SetItemData( hRoot,(DWORD_PTR)0 );
		m_objectsTree.SetItemState(hRoot, TVIS_BOLD, TVIS_BOLD);
		cit->second = hRoot;
	}

	for (int i = 0; i < m_vegetationMap->GetObjectCount(); i++)
	{
		CVegetationObject* object = m_vegetationMap->GetObject(i);
		AddObjectToTree( object,false );
	}

	// Set initial category.
	if (m_category.IsEmpty() && !m_categoryMap.empty())
	{
		m_category = m_categoryMap.begin()->first;
	}
	m_objectsTree.Invalidate();
	*/


	// sort vegetation instances
	std::vector<CVegetationObject*> vegObjs;
	vegObjs.reserve(m_vegetationMap->GetObjectCount());

	for (int i = 0; i < m_vegetationMap->GetObjectCount(); i++)
	{
		CVegetationObject* pObject = m_vegetationMap->GetObject(i);

		bool bAdded = false;

		const char * chr = pObject->GetFileName();
		const char * chr1 = strrchr(chr, '/');
		const char * chr2 = strrchr(chr, '\\');
		if(chr2 && chr2>chr1)
			chr1 = chr2;

		if(chr1 && *(chr1+1))
		{
			chr1++;
			for (std::vector<CVegetationObject*>::iterator itID = vegObjs.begin(); itID != vegObjs.end(); ++itID)
			{
				const char * chl = (*itID)->GetFileName();
				const char * chl1 = strrchr(chl, '/');
				const char * chl2 = strrchr(chl, '\\');
				if(chl2 && chl2>chl1)
					chl1 = chl2;
				if(chl1 && *(chl1+1))
				{
					chl1++;
					if(strcmp(chr1, chl1)<0)
					{
						vegObjs.insert(itID, pObject);
						bAdded = true;
						break;
					}
				}
			}
		}
	
		if(!bAdded)
		{
			vegObjs.push_back(pObject);
			continue;
		}
	}



	for (int i = 0; i < vegObjs.size(); i++)
	{
		CVegetationObject* pObject = m_vegetationMap->GetObject(i);
		m_wndReport.AddRecord( new CVegObjRecord(vegObjs[i]) );
	}

	m_wndReport.SetRedraw(TRUE);
	m_wndReport.Populate();

	SyncSelection();

	// Give focus back to main view.
	AfxGetMainWnd()->SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::SyncSelection()
{
	if (m_wndReport.m_hWnd)
	{
		//////////////////////////////////////////////////////////////////////////
		// Select rows.
		m_wndReport.GetSelectedRows()->Clear();
		for (int i = 0; i < m_wndReport.GetRows()->GetCount(); i++)
		{
			CVegObjRecord* pRec = DYNAMIC_DOWNCAST(CVegObjRecord, m_wndReport.GetRows()->GetAt(i)->GetRecord());
			if (!pRec)
				continue;
			CVegetationObject *pObj = FindObject( pRec->m_objectGUID );
			if (pObj && pObj->IsSelected())
			{
				m_wndReport.GetSelectedRows()->Add( m_wndReport.GetRows()->GetAt(i) );
			}
		}
		//////////////////////////////////////////////////////////////////////////
		OnSelectionChange();
	}
}

/*
void CVegetationDataBasePage::AddObjectToTree( CVegetationObject *object,bool bExpandCategory )
{
	CString str;
	char filename[_MAX_PATH];

	std::map<CString,HTREEITEM>::iterator cit;
	CString category = object->GetCategory();

	HTREEITEM hRoot = TVI_ROOT;
	cit = m_categoryMap.find(category);
	if (cit != m_categoryMap.end())
	{
		hRoot = cit->second;
	}
	if (hRoot == TVI_ROOT)
	{
		hRoot = m_objectsTree.InsertItem( category,0,1 );
		m_objectsTree.SetItemData( hRoot,(DWORD_PTR)0 );
		m_objectsTree.SetItemState(hRoot, TVIS_BOLD, TVIS_BOLD);
		m_categoryMap[category] = hRoot;
	}

	// Add new category item.
	_splitpath( object->GetFileName(),0,0,filename,0 );
	str.Format( "%s (%d)",filename,object->GetNumInstances() );
	HTREEITEM hItem = m_objectsTree.InsertItem( str,2,3,hRoot );
	m_objectsTree.SetItemData( hItem,(DWORD_PTR)object );
	m_objectsTree.SetCheck( hItem,!object->IsHidden() );

	if (hRoot != TVI_ROOT)
	{
		if (bExpandCategory)
			m_objectsTree.Expand(hRoot,TVE_EXPAND);

		// Determine check state of category.
		std::vector<CVegetationObject*> objects;
		GetObjectsInCategory( category,objects );
		bool anyChecked = false;
		for (int i = 0; i < objects.size(); i++)
		{
			if (!objects[i]->IsHidden())
				anyChecked = true;
		}
		m_objectsTree.SetCheck( hRoot,anyChecked );
	}
}
*/


//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::SelectObject( CVegetationObject *object )
{
	m_wndReport.GetSelectedRows()->Clear();
	// find row.
	for (int i = 0; i < m_wndReport.GetRows()->GetCount(); i++)
	{
		CVegObjRecord* pRec = DYNAMIC_DOWNCAST(CVegObjRecord, m_wndReport.GetRows()->GetAt(i)->GetRecord());
		if (!pRec)
			continue;
		CVegetationObject *pObj = FindObject( pRec->m_objectGUID );
		if (pObj)
		{
			m_wndReport.GetSelectedRows()->Add( m_wndReport.GetRows()->GetAt(i) );
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::UpdateInfo()
{
	int nSelInstances = 0;
	Selection objects;
	GetSelectedObjects(objects);
	for (int i = 0; i < objects.size(); i++)
	{
		nSelInstances += objects[i]->GetNumInstances();
	}
	CString str;
	int nSelObjTexUsage = m_vegetationMap->GetTexureMemoryUsage(true);
	int nAllObjTexUsage = m_vegetationMap->GetTexureMemoryUsage(false);
	int numObjects = m_vegetationMap->GetObjectCount();
	int nAllInstances = m_vegetationMap->GetNumInstances();
	int nMemUsedBySprites = m_vegetationMap->GetSpritesMemoryUsage(true);
	int nMemUsedBySpritesAll = m_vegetationMap->GetSpritesMemoryUsage(false);
	int numSel = objects.size();

	str.Format( "Groups: %d / %d\r\nInstance Count:\r\n   %d / %d\r\nTexture Usage:\r\n   %d K / %d K\r\nSprite Memory Usage:\r\n   %d K / %d K",
		numSel,numObjects,
		nSelInstances,nAllInstances,
		nSelObjTexUsage/1024,nAllObjTexUsage/1024,
		nMemUsedBySprites/1024,nMemUsedBySpritesAll/1024 
		);

	// Update info in task panel.
	m_wndTaskPanel.FindGroup(4)->FindItem(IDC_INFO)->SetCaption( str );
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnAdd()
{
	////////////////////////////////////////////////////////////////////////
	// Add another static object to the list
	////////////////////////////////////////////////////////////////////////
	std::vector<CString> files;
	if (!CFileUtil::SelectMultipleFiles( EFILE_TYPE_GEOMETRY,files ))
		return;

	CString category;
	if (m_vegetationMap->GetObjectCount() > 0)
		category = m_vegetationMap->GetObject(0)->GetCategory();
	
	Selection objects;
	GetSelectedObjects( objects );
	if (!objects.empty())
	{
		category = objects[0]->GetCategory();
	}

	CWaitCursor wait;

	CUndo undo( "Add VegObject(s)" );
	ClearSelection();
	for (int i = 0; i < files.size(); i++)
	{
		// Create a new static object settings class
		CVegetationObject *obj = m_vegetationMap->CreateObject();
		if (!obj)
			continue;
		obj->SetFileName( files[i] );
		if (!category.IsEmpty())
			obj->SetCategory( category );
		obj->SetSelected(true);
	}
	ReloadObjects();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnClone()
{
	Selection objects;
	GetSelectedObjects( objects );

	CUndo undo( "Clone VegObject" );
	ClearSelection();
	for (int i = 0; i < objects.size(); i++)
	{
		CVegetationObject *object = objects[i];
		CVegetationObject *newObject = m_vegetationMap->CreateObject( object );
		newObject->SetSelected(true);
	}
	ReloadObjects();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnReplace()
{
	Selection objects;
	GetSelectedObjects( objects );
	if (objects.size() != 1)
		return;

	CVegetationObject *object = objects[0];
	if (!object)
		return;

	CString relFile = object->GetFileName();
	if (!CFileUtil::SelectSingleFile( EFILE_TYPE_GEOMETRY,relFile ))
		return;

	CWaitCursor wait;
	CUndo undo( "Replace VegObject" );
	object->SetFileName( relFile );
	m_vegetationMap->RepositionObject( object );

	ReloadObjects();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnRemove()
{
	Selection objects;
	GetSelectedObjects( objects );

	if (objects.empty())
		return;

	// validate
	if (AfxMessageBox( _T("Delete Vegetation Group(s)?"), MB_YESNO ) != IDYES)
	{
		return;
	}

	// Unselect all instances.
	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationTool)))
	{
		((CVegetationTool*)pTool)->ClearThingSelection();
	}

	CWaitCursor wait;
	CUndo undo( "Remove VegObject(s)" );

	m_bIgnoreSelChange = true;
	for (int i = 0; i < objects.size(); i++)
	{
		m_vegetationMap->RemoveObject( objects[i] );
	}
	m_bIgnoreSelChange = false;
	ReloadObjects();

	GetIEditor()->Notify( eNotify_OnVegetationPanelUpdate );

	GetIEditor()->UpdateViews( eUpdateStatObj );
}

/*
//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnTvnBeginlabeleditObjects(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
	if (pTVDispInfo->item.lParam || pTVDispInfo->item.pszText == 0)
	{
		// Not Category.
		// Cancel editing.
		*pResult = TRUE;
		return;
	}
	m_category = pTVDispInfo->item.pszText;
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnTvnEndlabeleditObjects(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
	if (pTVDispInfo->item.lParam == 0 && pTVDispInfo->item.pszText != 0)
	{
		if (m_categoryMap.find(pTVDispInfo->item.pszText) != m_categoryMap.end())
		{
			// Cancel.
			*pResult = 0;
			return;
		}

		// Accept category name change.
		Selection objects;
		GetObjectsInCategory( m_category,objects );
		for (int i= 0; i < objects.size(); i++)
		{
			objects[i]->SetCategory( pTVDispInfo->item.pszText );
		}
		// Replace item in m_category map.
		m_categoryMap[pTVDispInfo->item.pszText] = m_categoryMap[m_category];
		m_categoryMap.erase( m_category );

		*pResult = TRUE; // Accept change.
		return;
	}
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnTvnHideObjects(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	// Check if clicked on state image so we may be changed Checked state.
	*pResult = 0;

	HTREEITEM hItem = pNMTreeView->itemNew.hItem;

	CWaitCursor wait;

	CVegetationObject *object = (CVegetationObject*)pNMTreeView->itemNew.lParam;
	if (object)
	{
		bool bHidden = m_objectsTree.GetCheck(hItem) != TRUE;
		// Object.
		m_vegetationMap->HideObject( object,bHidden );
	}
	else
	{
		bool bHidden = m_objectsTree.GetCheck(hItem) != TRUE;
		// Category.
		CString category = m_objectsTree.GetItemText(hItem);
		std::vector<CVegetationObject*> objects;
		GetObjectsInCategory( category,objects );
		for (int i = 0; i < objects.size(); i++)
		{
			m_vegetationMap->HideObject( objects[i],bHidden );
		}
		// for all childs of this item set same check.
		hItem = m_objectsTree.GetNextItem(hItem,TVGN_CHILD);
		while (hItem)
		{
			m_objectsTree.SetCheck( hItem,!bHidden );
			hItem = m_objectsTree.GetNextSiblingItem(hItem);
		}
	}
}
*/

void CVegetationDataBasePage::GetSelectedObjects( std::vector<CVegetationObject*> &objects )
{
	objects.clear();
	for (int i = 0; i < m_vegetationMap->GetObjectCount(); i++)
	{
		CVegetationObject *obj = m_vegetationMap->GetObject(i);
		if (obj->IsSelected())
		{
			objects.push_back(obj);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnSelectionChange()
{
	Selection objects;
	GetSelectedObjects( objects );

	// Delete all var block.
	m_varBlock = 0;
	m_propertyCtrl.DeleteAllItems();

	if (objects.empty())
	{
		if (m_previewCtrl.IsWindowEnabled() && m_previewCtrl.IsWindowVisible())
			m_previewCtrl.LoadFile( "" );
		m_propertyCtrl.EnableWindow(FALSE);
		return;
	}
	else
	{
		m_propertyCtrl.EnableWindow(TRUE);
	}

	/*
	if (objects.size() == 1)
	{
		CVegetationObject *object = objects[0];
		if (m_previewCtrl)
		m_previewCtrl.LoadFile( object->GetFileName() );
		
		m_propertyCtrl.DeleteAllItems();
		m_propertyCtrl.AddVarBlock( object->GetVarBlock() );
		m_propertyCtrl.ExpandAll();

		m_propertyCtrl.SetDisplayOnlyModified(false);
	}
	else
	*/
	{
		m_varBlock = objects[0]->GetVarBlock()->Clone(true);
		for (int i = 0; i < objects.size(); i++)
		{
			m_varBlock->Wire( objects[i]->GetVarBlock() );
		}

		// Add Surface types to varblock.
		if (objects.size() == 1)
			AddLayerVars(m_varBlock,objects[0]);
		else
			AddLayerVars(m_varBlock);

		// Add variable blocks of all objects.
		m_propertyCtrl.AddVarBlock( m_varBlock,"Group Properties" );
		m_propertyCtrl.ExpandAll();

		if (objects.size() > 1)
		{
			m_propertyCtrl.SetDisplayOnlyModified(true);
		}
		else
		{
			m_propertyCtrl.SetDisplayOnlyModified(false);
		}
	}
	if (objects.size() == 1)
	{
		CVegetationObject *object = objects[0];
		if (m_previewCtrl.IsWindowEnabled() && m_previewCtrl.IsWindowVisible())
			m_previewCtrl.LoadFile( object->GetFileName() );
	}
	else
	{
		if (m_previewCtrl.IsWindowEnabled() && m_previewCtrl.IsWindowVisible())
		{
			m_previewCtrl.LoadFile( "" );
		}
	}
	UpdateInfo();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::AddLayerVars( CVarBlock *pVarBlock,CVegetationObject *pObject )
{
	IVariable *pTable = new CVariableArray;
	pTable->SetName( "Use On Terrain Layers" );
	pVarBlock->AddVariable(pTable);
	int num = GetIEditor()->GetDocument()->GetSurfaceTypeCount();

	std::vector<CString> layers;
	if (pObject)
		pObject->GetTerrainLayers(layers);

	for (int i = 0; i < num; i++)
	{
		CSurfaceType *pType = GetIEditor()->GetDocument()->GetSurfaceTypePtr(i);

		if (!pType)
			continue;

		CVariable<bool> *pBoolVar = new CVariable<bool>;
		pBoolVar->SetName( pType->GetName() );
		pBoolVar->AddOnSetCallback( functor(*this,&CVegetationDataBasePage::OnLayerVarChange) );
		if (stl::find(layers,pType->GetName()))
			pBoolVar->Set( (bool)true );
		else
			pBoolVar->Set( (bool)false );

		pTable->AddChildVar( pBoolVar );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnLayerVarChange( IVariable *pVar )
{
	if (!pVar)
		return;
	CString layerName = pVar->GetName();
	bool bValue;
	
	std::vector<CVegetationObject*> objects;
	GetSelectedObjects( objects );

	pVar->Get(bValue);

	if (bValue)
	{
		for (int i = 0; i < objects.size(); i++)
		{
			CVegetationObject *pObject = objects[i];
			std::vector<CString> layers;
			pObject->GetTerrainLayers( layers );
			stl::push_back_unique(layers,layerName);
			pObject->SetTerrainLayers( layers );
		}
	}
	else
	{
		for (int i = 0; i < objects.size(); i++)
		{
			CVegetationObject *pObject = objects[i];
			std::vector<CString> layers;
			pObject->GetTerrainLayers( layers );
			stl::find_and_erase(layers,layerName);
			pObject->SetTerrainLayers( layers );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::GotoObjectMaterial()
{
	std::vector<CVegetationObject*> objects;
	GetSelectedObjects( objects );
	if (!objects.empty())
	{
		CVegetationObject *pVegObj = objects[0];
		IStatObj *pStatObj = pVegObj->GetObject();
		if (pStatObj)
		{
			GetIEditor()->GetMaterialManager()->GotoMaterial( pStatObj->GetMaterial() );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CVegetationObject* CVegetationDataBasePage::FindObject( REFGUID guid )
{
	CVegetationObject* pObject = 0;
	for (int i = 0; i < m_vegetationMap->GetObjectCount(); i++)
	{
		CVegetationObject *obj = m_vegetationMap->GetObject(i);
		if (obj->GetGUID() == guid)
			return obj;
	}
	return pObject;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::ClearSelection()
{
	for (int i = 0; i < m_vegetationMap->GetObjectCount(); i++)
	{
		CVegetationObject *obj = m_vegetationMap->GetObject(i);
		if (obj->IsSelected())
		{
			obj->SetSelected(false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnReportSelChange( NMHDR * pNotifyStruct, LRESULT *result )
{
	m_bSyncSelection = false;
	ClearSelection();

	// Get selected items.
	POSITION pos = m_wndReport.GetSelectedRows()->GetFirstSelectedRowPosition();
	while (pos)
	{
		CVegObjRecord *pRec = DYNAMIC_DOWNCAST(CVegObjRecord,m_wndReport.GetSelectedRows()->GetNextSelectedRow( pos )->GetRecord());
		if (!pRec)
			continue;
		CVegetationObject *pObj = FindObject( pRec->m_objectGUID );
		if (pObj)
		{
			pObj->SetSelected(true);
		}
	}
	OnSelectionChange();
	*result = 0;
	m_bSyncSelection = true;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnReportKeyDown(NMHDR *pNMHDR, LRESULT *pResult)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNMHDR;

	/*
	if (pItemNotify->->wVKey == VK_DELETE)
	{
		// Delete current item.
		OnRemove();

		*pResult = TRUE;
		return;
	}
	*/
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnReportClick(NMHDR * pNotifyStruct, LRESULT *result )
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if (pItemNotify->pColumn && pItemNotify->pColumn->GetIndex() == 0)
	{
		// Clicked on visibility index
		CVegObjRecord* pRecord = DYNAMIC_DOWNCAST(CVegObjRecord, pItemNotify->pRow->GetRecord());
		if (pRecord)
		{
			CVegetationObject *pObject = FindObject(pRecord->m_objectGUID);
			if (pObject)
			{
				if (!pObject->IsHidden())
					m_nCommand = VDB_COMMAND_HIDE;
					//OnHide();
				else
					m_nCommand = VDB_COMMAND_UNHIDE;
					//OnUnhide();
			}
		}
	}
}

#define MENU_GOTO_MATERIAL 3
#define MENU_COLLAPSEALLGROUPS 4
#define MENU_EXPANDALLGROUPS 5

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnReportRClick(NMHDR * pNotifyStruct, LRESULT *result )
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

	CMenu menu;
	menu.CreatePopupMenu();

	/*
	if (pItemNotify->pRow && pItemNotify->pRow->IsGroupRow())
	{
		// track menu
		int nMenuResult = CXTPCommandBars::TrackPopupMenu( &menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN |TPM_RIGHTBUTTON, pItemNotify->pt.x, pItemNotify->pt.y, this, NULL);

		// general items processing
		switch (nMenuResult)
		{
		case ID_POPUP_COLLAPSEALLGROUPS:
			pItemNotify->pRow->GetControl()->CollapseAll();
			break;
		case ID_POPUP_EXPANDALLGROUPS:
			pItemNotify->pRow->GetControl()->ExpandAll();
			break;
		}
	} else if (pItemNotify->pItem)
	{

		// track menu
		int nMenuResult = CXTPCommandBars::TrackPopupMenu( &menu, TPM_NONOTIFY|TPM_RETURNCMD|TPM_LEFTALIGN|TPM_RIGHTBUTTON, pItemNotify->pt.x, pItemNotify->pt.y, this, NULL);
	}
	*/

	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_ADD,"Add Group(s)" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_ADDCATEGORY,"Change Category" );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_CLONE,"Clone" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_REPLACE,"Replace Geometry" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_REMOVE,"Delete" );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_HIDEALL,"Hide All" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_UNHIDEALL,"Unhide All" );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu(MF_STRING, MENU_COLLAPSEALLGROUPS,_T("Collapse &All Groups") );
	menu.AppendMenu(MF_STRING, MENU_EXPANDALLGROUPS,_T("E&xpand All Groups") );

	CPoint pos;
	GetCursorPos(&pos);
	int cmd = CXTPCommandBars::TrackPopupMenu( &menu, TPM_NONOTIFY|TPM_RETURNCMD|TPM_LEFTALIGN|TPM_RIGHTBUTTON, pItemNotify->pt.x, pItemNotify->pt.y, this, NULL);
	if (cmd > 0)
	{
		switch (cmd) {
		case MENU_GOTO_MATERIAL:
			GotoObjectMaterial();
			break;
		case MENU_COLLAPSEALLGROUPS:
			pItemNotify->pRow->GetControl()->CollapseAll();
			break;
		case MENU_EXPANDALLGROUPS:
			pItemNotify->pRow->GetControl()->ExpandAll();
			break;
		default:
			SendMessage( WM_COMMAND,MAKEWPARAM(cmd,0),0 );
			break;
		}
	}

	*result = 0;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnReportHyperlink(NMHDR * pNotifyStruct, LRESULT * result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if (pItemNotify->nHyperlink >= 0)
	{
		CVegObjRecord* pRecord = DYNAMIC_DOWNCAST(CVegObjRecord, pItemNotify->pRow->GetRecord());
		if (pRecord)
		{
			//if (pItemNotify->pColumn->GetIndex() == COLUMN_MATERIAL)
			{
				CVegetationObject *pObject = FindObject(pRecord->m_objectGUID);
				if (pObject)
				{
					if (pObject->GetObject())
					{
						GetIEditor()->GetMaterialManager()->GotoMaterial( pObject->GetObject()->GetMaterial() );
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
LRESULT CVegetationDataBasePage::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
{
	switch(wParam) {
	case XTP_TPN_CLICK:
		{
			CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
			UINT nCmdID = pItem->GetID();
			SendMessage( WM_COMMAND,MAKEWPARAM(nCmdID,0),0 );
		}
		break;

	case XTP_TPN_RCLICK:
		break;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::DoIdleUpdate()
{
	m_vegetationMap = GetIEditor()->GetVegetationMap();
	
	int nRecords = m_wndReport.GetRecords()->GetCount();
	if (nRecords != m_vegetationMap->GetObjectCount())
	{
		ReloadObjects();
		return;
	}

	bool bReloadObjects = false;
	for (int i = 0; i < m_wndReport.GetRows()->GetCount(); i++)
	{
		CXTPReportRow *pRow = m_wndReport.GetRows()->GetAt(i);
		CVegObjRecord* pRec = DYNAMIC_DOWNCAST(CVegObjRecord, pRow->GetRecord());
		if (!pRec)
			continue;
		CVegetationObject *pObj = FindObject( pRec->m_objectGUID );
		if (!pObj)
		{
			bReloadObjects = true;
			break;
		}
		// Select unselect rows.
		if (pObj->IsSelected() != (pRow->IsSelected()==TRUE))
		{
			if (pObj->IsSelected())
				m_wndReport.GetSelectedRows()->Add( pRow );
			else
				m_wndReport.GetSelectedRows()->Remove( pRow );
		}
	}

	if(m_nCommand == VDB_COMMAND_HIDE)
		OnHide();
	if(m_nCommand == VDB_COMMAND_UNHIDE)
		OnUnhide();
	m_nCommand = 0;

	//////////////////////////////////////////////////////////////////////////
}


//////////////////////////////////////////////////////////////////////////
void CVegetationDataBasePage::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
	case eNotify_OnEndSceneOpen:
		ReloadObjects();
		break;
	case eNotify_OnIdleUpdate:
		{
			// If we are active.
			DoIdleUpdate();
		}
		break;

	case eNotify_OnVegetationObjectSelection:
		if(m_bSyncSelection)
			SyncSelection();
		break;
	}
}