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
#include "VegetationPanel.h"
#include "VegetationTool.h"
#include "Heightmap.h"
#include "VegetationMap.h"
#include "StringDlg.h"
#include "Viewport.h"
#include "PanelPreview.h"
#include "CryEditDoc.h"
#include "SurfaceType.h"
#include "Material\MaterialManager.h"
#include "GameEngine.h"

#define ID_PANEL_VEG_RANDOM_ROTATE 20000
#define ID_PANEL_VEG_CLEAR_ROTATE 20001

/////////////////////////////////////////////////////////////////////////////
// CVegetationPanel dialog
//////////////////////////////////////////////////////////////////////////
CVegetationPanel::CVegetationPanel(CVegetationTool *tool,CWnd* pParent /*=NULL*/)
	: CToolbarDialog(CVegetationPanel::IDD, pParent)
{
	//{{AFX_DATA_INIT(CVegetationPanel)
	//}}AFX_DATA_INIT
	//xtAfxData.bControlBarMenus = TRUE; // Turned off in constructor of CToolbarDialog.

	assert( tool );
	m_tool = tool;
	m_category = "";
	m_previewPanel = NULL;
	m_vegetationMap = GetIEditor()->GetHeightmap()->GetVegetationMap();
	m_bIgnoreSelChange = false;

	Create( IDD,pParent );

	GetIEditor()->RegisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
CVegetationPanel::~CVegetationPanel()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

void CVegetationPanel::DoDataExchange(CDataExchange* pDX)
{
	CToolbarDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVegetationPanel)
	//}}AFX_DATA_MAP

	DDX_Control(pDX, IDC_RADIUS, m_radius);
	DDX_Control(pDX, IDC_PAINT, m_paintButton);	
	DDX_Control(pDX, IDC_REMOVEDUPLICATED, m_removeDuplicatedButton);
	DDX_Control(pDX, IDC_OBJECTS, m_objectsTree);
	DDX_Control(pDX, IDC_PROPERTIES, m_propertyCtrl);
	DDX_Control(pDX, IDC_INFO, m_info);
}


BEGIN_MESSAGE_MAP(CVegetationPanel, CToolbarDialog)
	//{{AFX_MSG_MAP(CVegetationPanel)
	//}}AFX_MSG_MAP
	//ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_RADIUS, OnReleasedcaptureRadius)
	ON_CONTROL( WMU_FS_CHANGED,IDC_RADIUS,OnBrushRadiusChange )
	ON_BN_CLICKED(IDC_PAINT, OnPaint)
	ON_BN_CLICKED(IDC_REMOVEDUPLICATED, OnRemoveDuplVegetation)
	ON_WM_SIZE()

	ON_NOTIFY(TVN_SELCHANGED, IDC_OBJECTS, OnObjectSelectionChanged)
	ON_NOTIFY(TVN_CHKCHANGE, IDC_OBJECTS, OnTvnHideObjects)
	ON_NOTIFY(TVN_KEYDOWN, IDC_OBJECTS, OnTvnKeydownObjects)
	ON_NOTIFY(TVN_BEGINLABELEDIT, IDC_OBJECTS, OnTvnBeginlabeleditObjects)
	ON_NOTIFY(TVN_ENDLABELEDIT, IDC_OBJECTS, OnTvnEndlabeleditObjects)
	ON_NOTIFY(NM_RCLICK, IDC_OBJECTS, OnObjectsRClick)

	ON_COMMAND( ID_PANEL_VEG_ADD,OnAdd )
	ON_COMMAND( ID_PANEL_VEG_ADDCATEGORY,OnNewCategory )
	ON_COMMAND( ID_PANEL_VEG_CLONE,OnClone )
	ON_COMMAND( ID_PANEL_VEG_REPLACE,OnReplace )
	ON_COMMAND( ID_PANEL_VEG_REMOVE,OnRemove )

	ON_COMMAND( ID_PANEL_VEG_CREATE_SEL, OnInstancesToCategory)

	ON_COMMAND( ID_PANEL_VEG_DISTRIBUTE,OnDistribute )
	ON_COMMAND( ID_PANEL_VEG_CLEAR,OnClear )
	ON_COMMAND( ID_PANEL_VEG_IMPORT,OnBnClickedImport )
	ON_COMMAND( ID_PANEL_VEG_EXPORT,OnBnClickedExport )
	ON_COMMAND( ID_PANEL_VEG_SCALE,OnBnClickedScale )

	ON_COMMAND( ID_PANEL_VEG_MERGE,OnBnClickedMerge )

	ON_COMMAND( ID_PANEL_VEG_RANDOM_ROTATE,OnBnClickedRandomRotate )
	ON_COMMAND( ID_PANEL_VEG_CLEAR_ROTATE,OnBnClickedClearRotate )
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CVegetationPanel message handlers

void CVegetationPanel::OnBrushRadiusChange() 
{
	int radius = m_radius.GetValue();
	m_tool->SetBrushRadius(radius);

	AfxGetMainWnd()->SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnPaint()
{
	bool paint = m_paintButton.GetCheck()==0;
	m_tool->SetMode( paint );
	if (m_paintButton.GetCheck() == 0)
		m_paintButton.SetCheck(1);
	else
		m_paintButton.SetCheck(0);
	AfxGetMainWnd()->SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnRemoveDuplVegetation()
{
	m_vegetationMap->RemoveDuplVegetation();
	UpdateAllObjectsInTree();
}

//////////////////////////////////////////////////////////////////////////
BOOL CVegetationPanel::OnInitDialog() 
{
	CToolbarDialog::OnInitDialog();

	m_toolbar.Create( this,WS_CHILD|WS_VISIBLE|CBRS_TOP|CBRS_TOOLTIPS|CBRS_FLYBY );
	m_toolbar.LoadToolBar24( IDR_PANEL_VEGETATION,14 );

	m_treeImageList.Create( IDB_VEGETATION_TREE, 12, 1, RGB (255,255,255));
	m_treeStateImageList.Create( IDB_VEGETATION_TREE_STATE, 12, 1, RGB(255,255,255) );
	m_objectsTree.SetImageList(&m_treeImageList,TVSIL_NORMAL);
	m_objectsTree.SetImageList( &m_treeStateImageList, TVSIL_STATE );
	m_objectsTree.SetIndent( 0 );


	//DWORD style = ::GetWindowLong(m_objectsTree.GetSafeHwnd(),GWL_STYLE);
	//::SetWindowLong( m_objectsTree.GetSafeHwnd(),GWL_STYLE,style|TVS_CHECKBOXES );

	m_paintButton.SetCheck(0);
	m_paintButton.SetToolTip( "Hold Ctrl to remove" );

	m_radius.SetRange( 1,300 );
	m_radius.SetValue( m_tool->GetBrushRadius() );

	ReloadObjects();

	/*
	CRect rc;
	GetDlgItem(IDC_PREVIEW)->GetWindowRect(rc);
	ScreenToClient( rc );
	GetDlgItem(IDC_PREVIEW)->ShowWindow(SW_HIDE);
	if (gSettings.bPreviewGeometryWindow)
		m_previewCtrl.Create( this,rc,WS_VISIBLE|WS_CHILD|WS_BORDER );
		*/

	SendToControls();
	UpdateInfo();

	SetResize( IDC_OBJECTS,SZ_HORRESIZE(1) );
	SetResize( IDC_PROPERTIES,SZ_HORRESIZE(1) );
	SetResize( IDC_INFO,SZ_HORRESIZE(1) );
	SetResize( IDC_PAINT,SZ_HORRESIZE(1) );
	SetResize( IDC_RADIUS,SZ_HORRESIZE(1) );

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	// resize splitter window.
	if (m_toolbar.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		m_toolbar.SetWindowPos(NULL, 0, 0, rc.right, 24, SWP_NOZORDER);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::GetObjectsInCategory( const CString &category,std::vector<CVegetationObject*> &objects )
{
	objects.clear();
	for (int i = 0; i < m_vegetationMap->GetObjectCount(); i++)
	{
		CVegetationObject *obj = m_vegetationMap->GetObject(i);
		if (category == obj->GetCategory())
		{
			objects.push_back(obj);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnNewCategory()
{
	CStringDlg dlg( "New Category" );
	if (dlg.DoModal() == IDOK)
	{
		if (m_categoryMap.find(dlg.GetString()) == m_categoryMap.end())
		{
			m_categoryMap[dlg.GetString()] = 0;
			ReloadObjects();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnDistribute()
{
	if(IDYES==MessageBox("Are you sure you want to Distribute?", "Vegetation Distribute", MB_YESNO | MB_ICONQUESTION))
	{
		CUndo undo( "Vegetation Distribute" );
		BeginWaitCursor();
		if (m_tool)
			m_tool->Distribute();
		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnDistributeMask()
{
	CUndo undo( "Vegetation Distribute Mask" );
	CString file;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_TEXTURE,file ))
	{
		BeginWaitCursor();
		if (m_tool)
			m_tool->DistributeMask( file );
		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnClear()
{
	if(IDYES==MessageBox("Are you sure you want to Clear?", "Vegetation Clear", MB_YESNO | MB_ICONQUESTION))
	{
		CUndo undo( "Vegetation Clear" );
		BeginWaitCursor();
		if (m_tool)
			m_tool->Clear();
		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnClearMask()
{
	CUndo undo( "Vegetation Clear Mask" );
	CString file;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_TEXTURE,file ))
	{
		BeginWaitCursor();
		if (m_tool)
			m_tool->ClearMask( file );
		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::SetObjectPanel( CVegetationObjectPanel *panel )
{
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnBnClickedScale()
{
	CUndo undo( "Vegetation Scale" );
	if (m_tool)
		m_tool->ScaleObjects();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnBnClickedImport()
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
void CVegetationPanel::OnBnClickedMerge()
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
	}
}



//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnBnClickedExport()
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
void CVegetationPanel::OnBnClickedRandomRotate()
{
	CUndo undo( "Vegetation Random Rotate" );
	if (m_tool)
		m_tool->DoRandomRotate();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnBnClickedClearRotate()
{
	CUndo undo( "Vegetation Clear Rotate" );
	if (m_tool)
		m_tool->DoClearRotate();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::ReloadObjects()
{
	std::map<CString,HTREEITEM>::iterator cit;
	
	m_objectsTree.DeleteAllItems();

	m_bIgnoreSelChange = true;
	// Clear all selections.
	for (int i = 0; i < m_vegetationMap->GetObjectCount(); i++)
	{
		m_vegetationMap->GetObject(i)->SetSelected(false);
	}
	m_bIgnoreSelChange = false;

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

	// Find object.
	TV_ITEM item;
	ZeroStruct(item);
	item.mask = TVIF_TEXT;
	item.pszText = "Default";
	HTREEITEM hItem = m_objectsTree.FindNextItem( &item,NULL );
	if (hItem)
		m_objectsTree.SetItemText(hItem, "_");

	m_objectsTree.SortChildren(TVI_ROOT);
	if (hItem)
		m_objectsTree.SetItemText(hItem, "Default");


	m_objectsTree.Invalidate();
	
	// Give focus back to main view.
	AfxGetMainWnd()->SetFocus();
}


void CVegetationPanel::AddObjectToTree( CVegetationObject *object,bool bExpandCategory )
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

	m_objectsTree.SortChildren(hRoot);

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

void CVegetationPanel::UpdateObjectInTree( CVegetationObject *object,bool bUpdateInfo )
{
	CString str;
	char filename[_MAX_PATH];

	// Find object.
	TV_ITEM item;
	ZeroStruct(item);
	item.mask = TVIF_PARAM;
	item.lParam = (LPARAM)object;
	HTREEITEM hItem = m_objectsTree.FindNextItem( &item,NULL );
	if (hItem)
	{
		// Add new category item.
		_splitpath( object->GetFileName(),0,0,filename,0 );
		str.Format( "%s (%d)",filename,object->GetNumInstances() );
		m_objectsTree.SetItemText( hItem,str );
	}

	if (bUpdateInfo)
		UpdateInfo();
}

void CVegetationPanel::UpdateAllObjectsInTree()
{
	for (int i = 0; i < m_vegetationMap->GetObjectCount(); i++)
	{
		UpdateObjectInTree( m_vegetationMap->GetObject(i),false );
	}
	UpdateInfo();
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::SelectObject( CVegetationObject *object,bool bAddToSelection )
{
	m_bIgnoreSelChange = true;
	// Find object.
	TV_ITEM item;
	ZeroStruct(item);
	item.mask = TVIF_PARAM;
	item.lParam = (LPARAM)object;
	HTREEITEM hItem = m_objectsTree.FindNextItem( &item,NULL );
	if (hItem)
	{
		bool bIsTreeSelected = m_objectsTree.GetItemState(hItem,TVIS_SELECTED);

		m_objectsTree.EnsureVisible(hItem);
		if (!bAddToSelection)
			m_objectsTree.SelectAll(FALSE);

		m_objectsTree.SetItemState(hItem,TVIS_SELECTED|TVIS_FOCUSED,TVIS_SELECTED|TVIS_FOCUSED);
	}
	if (!object->IsSelected())
	{
		object->SetSelected(true);
		SendToControls();
	}
	m_bIgnoreSelChange = false;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::UnselectAll()
{
	m_objectsTree.SelectAll(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::UpdateInfo()
{
	CString str;
	int numObjects = m_vegetationMap->GetObjectCount();
	int numInstance = m_vegetationMap->GetNumInstances();
	str.Format( "Total Objects: %d\nTotal Instances: %d",numObjects,numInstance );
	// Update info.
	m_info.SetWindowText( str );
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::RemoveObjectFromTree( CVegetationObject *object )
{
	// Find object.
	TV_ITEM item;
	ZeroStruct(item);
	item.mask = TVIF_PARAM;
	item.lParam = (LPARAM)object;
	HTREEITEM hItem = m_objectsTree.FindNextItem( &item,NULL );
	if (hItem)
	{
		m_objectsTree.DeleteItem(hItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnAdd()
{
	////////////////////////////////////////////////////////////////////////
	// Add another static object to the list
	////////////////////////////////////////////////////////////////////////
	std::vector<CString> files;
	if (!CFileUtil::SelectMultipleFiles( EFILE_TYPE_GEOMETRY,files ))
		return;

	CString category = m_category;

	CWaitCursor wait;

	CUndo undo( "Add VegObject(s)" );
	for (int i = 0; i < files.size(); i++)
	{
		// Create a new static object settings class
		CVegetationObject *obj = m_vegetationMap->CreateObject();
		if (!obj)
			continue;
		obj->SetFileName( files[i] );
		if (!category.IsEmpty())
			obj->SetCategory( m_category );

		AddObjectToTree( obj );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnClone()
{
	Selection objects;
	GetSelectedObjects( objects );

	CUndo undo( "Clone VegObject" );
	for (int i = 0; i < objects.size(); i++)
	{
		CVegetationObject *object = objects[i];
		CVegetationObject *newObject = m_vegetationMap->CreateObject( object );
		if (newObject)
			AddObjectToTree( newObject );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnReplace()
{
	HTREEITEM hItem = m_objectsTree.GetSelectedItem();
	if (hItem)
	{
		// Check if category selected.
		if (m_objectsTree.GetItemData(hItem) == 0)
		{
			m_category = m_objectsTree.GetItemText(hItem);
			// Rename category.
			CStringDlg dlg("Rename Category",this);
			dlg.m_strString = m_category;
			if (dlg.DoModal() == IDOK && m_category != dlg.GetString())
			{
				CString oldCategory = m_category;
				m_category = dlg.GetString();
				Selection objects;
				GetObjectsInCategory( oldCategory,objects );
				for (int i = 0; i < objects.size(); i++)
				{
					objects[i]->SetCategory( m_category );
				}
				m_categoryMap[m_category] = m_categoryMap[oldCategory];
				m_categoryMap.erase(oldCategory);
				ReloadObjects();
			}
			return;
		}
	}


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

	RemoveObjectFromTree( object );
	AddObjectToTree( object );
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnRemove()
{
	Selection objects;
	GetSelectedObjects( objects );

	if (objects.empty())
		return;

	// validate
	if (AfxMessageBox( _T("Delete Vegetation Object(s)?"), MB_YESNO ) != IDYES)
	{
		return;
	}

	// Unselect all instances.
	m_tool->ClearThingSelection();

	CWaitCursor wait;
	CUndo undo( "Remove VegObject(s)" );

	m_bIgnoreSelChange = true;
	for (int i = 0; i < objects.size(); i++)
	{
		RemoveObjectFromTree( objects[i] );
		m_vegetationMap->RemoveObject( objects[i] );
	}
	m_bIgnoreSelChange = false;
	//ReloadObjects();
	SendToControls();

	GetIEditor()->UpdateViews( eUpdateStatObj );
}


//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnInstancesToCategory()
{
	//CUndo undo( "Move instances" );
	if (m_tool->GetCountSelectedInstances())
	{
		CStringDlg dlg( "Enter Category name" );
		if (dlg.DoModal() == IDOK)
		{
			m_tool->MoveSelectedInstancesToCategory(dlg.GetString());
			//ReloadObjects();
			GetIEditor()->Notify( eNotify_OnVegetationPanelUpdate );
		}
		AfxGetMainWnd()->SetFocus();
	}
	else
		AfxMessageBox( "Select Instances first" );
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnObjectSelectionChanged(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;

	//if (pNMTreeView->action != TVC_BYKEYBOARD && pNMTreeView->action != TVC_BYMOUSE)
		//return;

	if (m_bIgnoreSelChange)
		return;

	static bool bNoRecurse = false;
	if (bNoRecurse)
		return;
	bNoRecurse = true;

	m_bIgnoreSelChange = true;
	// Clear all selections.
	for (int i = 0; i < m_vegetationMap->GetObjectCount(); i++)
	{
		m_vegetationMap->GetObject(i)->SetSelected(false);
	}

	HTREEITEM hItem = m_objectsTree.GetFirstSelectedItem();
	while (hItem)
	{
		CVegetationObject *object = (CVegetationObject*)m_objectsTree.GetItemData(hItem);
		if (object)
		{
			m_category = object->GetCategory();
			object->SetSelected(true);
		}
		else
		{
			// Category selected.
			m_category = m_objectsTree.GetItemText(hItem);

			Selection objects;
			GetObjectsInCategory( m_category,objects );
			for (int i = 0; i < objects.size(); i++)
			{
				objects[i]->SetSelected(true);
				//m_objectsTree.SelectChildren(hItem);
			}

			m_objectsTree.SelectChildren(hItem);
		}

		hItem = m_objectsTree.GetNextSelectedItem(hItem);
	}
	m_bIgnoreSelChange = false;

	SendToControls();
	bNoRecurse = false;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnTvnKeydownObjects(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTVKEYDOWN pTVKeyDown = reinterpret_cast<LPNMTVKEYDOWN>(pNMHDR);
	
	if (pTVKeyDown->wVKey == VK_DELETE)
	{
		// Delete current item.
		OnRemove();

		*pResult = TRUE;
		return;
	}
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnTvnBeginlabeleditObjects(NMHDR *pNMHDR, LRESULT *pResult)
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
void CVegetationPanel::OnTvnEndlabeleditObjects(NMHDR *pNMHDR, LRESULT *pResult)
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
void CVegetationPanel::OnTvnHideObjects(NMHDR *pNMHDR, LRESULT *pResult)
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

void CVegetationPanel::GetSelectedObjects( std::vector<CVegetationObject*> &objects )
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
void CVegetationPanel::SendToControls()
{
	Selection objects;
	GetSelectedObjects( objects );

	// Delete all var block.
	m_varBlock = 0;
	m_propertyCtrl.DeleteAllItems();

	if (objects.empty())
	{
		if (m_previewPanel)
			m_previewPanel->LoadFile( "" );
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
		if (m_previewPanel)
			m_previewPanel->LoadFile( object->GetFileName() );
		
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
		m_propertyCtrl.AddVarBlock( m_varBlock );
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
		if (m_previewPanel)
			m_previewPanel->LoadFile( object->GetFileName() );
	}
	else
	{
		if (m_previewPanel)
			m_previewPanel->LoadFile( "" );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::AddLayerVars( CVarBlock *pVarBlock,CVegetationObject *pObject )
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
		pBoolVar->AddOnSetCallback( functor(*this,&CVegetationPanel::OnLayerVarChange) );
		if (stl::find(layers,pType->GetName()))
			pBoolVar->Set( (bool)true );
		else
			pBoolVar->Set( (bool)false );

		pTable->AddChildVar( pBoolVar );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnLayerVarChange( IVariable *pVar )
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

	GetIEditor()->GetGameEngine()->ReloadSurfaceTypes();

	for (int i = 0; i < objects.size(); i++)
	{
		CVegetationObject *pObject = objects[i];
		pObject->SetEngineParams();
	}
	GetIEditor()->SetModifiedFlag();
}

#define MENU_HIDE_ALL 1
#define MENU_UNHIDE_ALL 2
#define MENU_GOTO_MATERIAL 3

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::OnObjectsRClick( NMHDR * pNotifyStruct, LRESULT *result )
{
	CMenu menu;
	menu.CreatePopupMenu();

	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_ADDCATEGORY,"Add Category" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_ADD,"Add Object" );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_CLONE,"Clone Object" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_REPLACE,"Replace Object" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_REMOVE,"Remove Object" );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_DISTRIBUTE,"Auto Distribute Object" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_CLEAR,"Clear All Instances" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_CREATE_SEL,"Selected Instances to Category" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_RANDOM_ROTATE,"Randomly Rotate All Instances" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_CLEAR_ROTATE,"Clear Rotation" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_SCALE,"Scale Selected Instances" );
	menu.AppendMenu( MF_STRING,MENU_GOTO_MATERIAL,"Go To Object Material" );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu( MF_STRING,MENU_HIDE_ALL,"Hide All Objects" );
	menu.AppendMenu( MF_STRING,MENU_UNHIDE_ALL,"Unhide All Objects" );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_IMPORT,"Import Objects" );
	menu.AppendMenu( MF_STRING,ID_PANEL_VEG_EXPORT,"Export Objects" );

	CPoint pos;
	GetCursorPos(&pos);
	int cmd = menu.TrackPopupMenu( TPM_RETURNCMD|TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY,pos.x,pos.y,this );
	if (cmd > 0)
	{
		switch (cmd) {
		case MENU_HIDE_ALL:
			m_vegetationMap->HideAllObjects(true);
			ReloadObjects();
			break;
		case MENU_UNHIDE_ALL:
			m_vegetationMap->HideAllObjects(false);
			ReloadObjects();
			break;
		case MENU_GOTO_MATERIAL:
			GotoObjectMaterial();
			break;
		default:
			SendMessage( WM_COMMAND,MAKEWPARAM(cmd,0),0 );
			break;
		}
	}
	*result = TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CVegetationPanel::GotoObjectMaterial()
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
void CVegetationPanel::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
	case eNotify_OnVegetationObjectSelection:
		if (!m_bIgnoreSelChange)
		{
			Selection objects;
			GetSelectedObjects( objects );

			m_bIgnoreSelChange = true;
			m_objectsTree.SelectAll(FALSE);
			for (int i = 0; i < objects.size(); i++)
			{
				SelectObject( objects[i],true );
			}
			m_bIgnoreSelChange = false;
			SendToControls();
		}
		break;

	case eNotify_OnVegetationPanelUpdate:
		ReloadObjects();
		break;
	}
}
