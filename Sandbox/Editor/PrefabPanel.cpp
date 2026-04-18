////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   PrefabPanel.cpp
//  Version:     v1.00
//  Created:     14/11/2003 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PrefabPanel.h"

#include "Objects\ObjectManager.h"
#include "Objects\PrefabObject.h"
#include "Prefabs\PrefabItem.h"

IMPLEMENT_DYNCREATE(CPrefabPanel, CXTResizeDialog)

#define COLUMN_NAME 0
#define COLUMN_TYPE 1

CPrefabPanel::CPrefabPanel(CWnd* pParent /*=NULL*/)
: CXTResizeDialog(CPrefabPanel::IDD, pParent)
, m_type(0)
{
	m_object = 0;
	Create( IDD,pParent );
}

CPrefabPanel::~CPrefabPanel()
{
}

void CPrefabPanel::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EXTRACT_SELECTED, m_btns[0]);
	DDX_Control(pDX, IDC_EXTRACT_ALL, m_btns[1]);
	DDX_Control(pDX, IDC_OPEN, m_btnOpen);
	DDX_Control(pDX, IDC_CLOSE, m_btnClose);
	DDX_Control(pDX, IDC_PICK_NEW, m_pickButton);
	DDX_Control(pDX, IDC_RELOAD_PREFAB, m_btns[2]);
	DDX_Control(pDX, IDC_UPDATE_PREFAB, m_btns[3]);
	DDX_Control(pDX, IDC_REMOVE, m_btns[5]);
	DDX_Control(pDX, IDC_PREFAB, m_btns[6]);
	DDX_Control(pDX, IDC_PREFAB_NAME, m_prefabName );
	DDX_Control(pDX, IDC_OBJECTS, m_listCtrl );
	DDX_Control(pDX, IDC_NUM_OBJECTS, m_objectsText );
}

BOOL CPrefabPanel::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();

	CRect rc;
	GetDlgItem(IDC_OBJECTS)->GetClientRect( rc );
	int w1 = 2*rc.right/3 - 10;
	int w2 = rc.right/3 + 10;

	m_pickButton.SetPickCallback( this,"Pick Object To Attach" );

	m_listCtrl.SetExtendedStyle( LVS_EX_FLATSB|LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_HEADERDRAGDROP );
	m_listCtrl.InsertColumn( COLUMN_NAME+1,"Name",LVCFMT_LEFT,w1,0 );
	m_listCtrl.InsertColumn( COLUMN_TYPE+1,"Type",LVCFMT_LEFT,w2,1 );

	SetResize( IDC_OBJECTS,SZ_RESIZE(1) );
	SetResize( IDC_NUM_OBJECTS,SZ_RESIZE(1) );
	SetResize( IDC_STATIC1,SZ_RESIZE(1) );
	SetResize( IDC_OBJECT_INFO,SZ_RESIZE(1) );
	SetResize( IDC_PREFAB_NAME,SZ_RESIZE(1) );
	SetResize( IDC_PREFAB,SZ_REPOS(1) );

	SetResize( IDC_NAME,SZ_RESIZE(1) );
	SetResize( IDC_CLASS,SZ_RESIZE(1) );
	SetResize( IDC_TYPE,SZ_RESIZE(1) );

	return TRUE;  // return TRUE  unless you set the focus to a control
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CPrefabPanel, CXTResizeDialog)
	ON_BN_CLICKED(IDC_EXTRACT_SELECTED, OnBnClickedExtractSelected)
	ON_BN_CLICKED(IDC_EXTRACT_ALL, OnBnClickedExtractAll)
	ON_BN_CLICKED(IDC_PREFAB, OnBnClickedPrefab)
	ON_BN_CLICKED(IDC_OPEN, OnBnClickedOpen)
	ON_BN_CLICKED(IDC_CLOSE, OnBnClickedClose)
	ON_BN_CLICKED(IDC_REMOVE, OnBnClickedRemove)
	ON_BN_CLICKED(IDC_RELOAD_PREFAB, OnBnClickedReloadPrefab )
	ON_BN_CLICKED(IDC_UPDATE_PREFAB, OnBnClickedUpdatePrefab )
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_OBJECTS, OnItemchangedObjects)
	ON_NOTIFY(NM_DBLCLK, IDC_OBJECTS, OnDblclkObjects)
END_MESSAGE_MAP()


//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::SetObject( CPrefabObject *object )
{
	assert( object );
	m_object = object;
	UpdateData(FALSE);
		
	ReloadObjects();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::RecursivelyGetAllPrefabChilds( CBaseObject *obj,std::vector<ChildRecord> &childs,int level )
{
	for (int i = 0; i < obj->GetChildCount(); i++)
	{
		CBaseObject *pChild = obj->GetChild(i);
		if (pChild->CheckFlags(OBJFLAG_PREFAB))
		{
			ChildRecord rec;
			rec.pObject = pChild;
			rec.level = level;
			rec.bSelected = false;
			childs.push_back(rec);
			if(!pChild->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
				RecursivelyGetAllPrefabChilds( pChild,childs,level+1 );
		}
	}
}

/*
//////////////////////////////////////////////////////////////////////////
static HTREEITEM AddPrefabTreeItem( CTreeCtrl &tree,CBaseObject *pObject,CBaseObject *pTopParent,std::map<CBaseObject*,HTREEITEM> &itemsMap )
{
	HTREEITEM hParent = TVI_ROOT;

	CBaseObject *pParent = pObject->GetParent();
	if (pParent && pParent != pTopParent)
	{
		hParent = stl::find_in_map( itemsMap,pParent, (HTREEITEM)0 );
		if (!hParent)
		{
			hParent = AddPrefabTreeItem( tree,pParent,pTopParent,itemsMap );
		}
	}
	CString str;
	//str = CString(pObject->GetName()) + " (" + pObject->GetTypeDescription() + ")";
	str = (pObject->GetName()) + " (" + pObject->GetClassDesc()->ClassName() + ")";
	HTREEITEM hItem = tree.InsertItem( str,hParent );
	itemsMap[pObject] = hItem;
	tree.SetItemData( hItem,(DWORD_PTR)pObject );
	tree.Expand( hItem,TVE_EXPAND );
	return hItem;
}
*/

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::ReloadListCtrl()
{
	m_listCtrl.SetRedraw(FALSE);
	m_listCtrl.DeleteAllItems();
	for (int i = 0; i < m_objects.size(); i++)
	{
		const ChildRecord &record = m_objects[i];

		LVITEM lvi;
		lvi.mask = LVIF_PARAM|LVIF_INDENT|LVIF_IMAGE|LVIF_TEXT;
		lvi.iItem = i;
		lvi.iIndent = record.level;
		lvi.iSubItem = COLUMN_NAME;
		lvi.stateMask = 0;
		lvi.state = 0;
		lvi.pszText = const_cast<char*>((const char*)record.pObject->GetName());
		lvi.iImage = 0;

		int id = m_listCtrl.InsertItem( &lvi );

		m_listCtrl.SetItem( id,COLUMN_TYPE,LVIF_TEXT|LVIF_STATE,(const char*)record.pObject->GetTypeDescription(),0,0,0,0 );
		/*
		if (obj->IsSelected())
		{
			m_list.SetItem( id,0,LVIF_STATE,0,0,LVIS_SELECTED,LVIS_SELECTED,0 );
		}
		*/
	}
	m_listCtrl.SetRedraw(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::ReloadObjects()
{
	m_objects.clear();
	RecursivelyGetAllPrefabChilds( m_object,m_objects,0 );

	ReloadListCtrl();

	CString text;
	int numObjects = m_objects.size();
	text.Format( "%d Object(s):",numObjects );

	//SetDlgItemText( IDC_NAME,SZ_RESIZE(1) );
	//SetResize( IDC_CLASS,SZ_RESIZE(1) );
	//SetResize( IDC_TYPE,SZ_RESIZE(1) );

	if (m_object)
	{
		if (m_object->IsOpen())
		{
			m_btnOpen.EnableWindow(FALSE);
			m_btnClose.EnableWindow(TRUE);
		}
		else
		{
			m_btnOpen.EnableWindow(TRUE);
			m_btnClose.EnableWindow(FALSE);
		}
	}

	m_objectsText.SetWindowText( text );
	CPrefabItem *pItem = m_object->GetPrefab();
	if (pItem)
	{
		m_prefabName.SetWindowText( pItem->GetFullName() );
	}
	else
	{
		m_prefabName.SetWindowText( "<Unknown>" );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnItemchangedObjects(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	if (pNMListView->iItem >= 0 && pNMListView->iItem < m_objects.size())
	{
		ChildRecord &record = m_objects[pNMListView->iItem];

		CBaseObject *pObject = record.pObject;
		if (pObject)
		{
			SetDlgItemText( IDC_NAME,pObject->GetName() );
			SetDlgItemText( IDC_CLASS,pObject->GetClassDesc()->ClassName() );
			SetDlgItemText( IDC_TYPE,pObject->GetTypeDescription() );
		}
	}


	for(int i = 0; i<m_listCtrl.GetItemCount(); i++)
	{
		if(m_listCtrl.GetItemState(i, LVIS_SELECTED))
			m_objects[i].bSelected = true;
		else
			m_objects[i].bSelected = false;
	}
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnDblclkObjects(NMHDR* pNMHDR, LRESULT* pResult) 
{
	*pResult = 0;
	// Select and Goto object.
	NMITEMACTIVATE *pNM = (NMITEMACTIVATE*)pNMHDR;
	if (pNM->iItem >= 0 && pNM->iItem < m_objects.size())
	{
		ChildRecord &record = m_objects[pNM->iItem];
		CBaseObject *pObject = record.pObject;
		if (pObject)
		{
			GetIEditor()->ClearSelection();
			GetIEditor()->SelectObject( pObject );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnBnClickedExtractSelected()
{
	if (!m_object)
		return;

	CUndo undo("Extract Object from Prefab");
	for (int i = 0; i < m_objects.size(); i++)
	{
		if (m_objects[i].bSelected)
		{
			// Extract selected object.
			m_object->ExtractObject(m_objects[i].pObject);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnBnClickedExtractAll()
{
	if (m_object)
	{
		CUndo undo("Extract All from Prefab");
		m_object->ExtractAll();
		GetIEditor()->DeleteObject(m_object);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnBnClickedPrefab()
{
	if (m_object)
	{
		GetIEditor()->OpenDataBaseLibrary( EDB_TYPE_PREFAB,m_object->GetPrefab() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnBnClickedOpen()
{
	if (m_object)
		m_object->Open();
	ReloadObjects();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnBnClickedClose()
{
	if (m_object)
		m_object->Close();
	ReloadObjects();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnBnClickedPick()
{
	
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnBnClickedRemove()
{
	if (!m_object)
		return;

	CUndo undo( "Remove Object(s) From Prefab" );
	bool bSomeRemoved = false;
	for (int i = 0; i < m_objects.size(); i++)
	{
		if (m_objects[i].bSelected)
		{
			if(!bSomeRemoved)
				m_object->StoreUndo( "Remove Object(s) From Prefab" );
			// Extract selected object.
			m_object->RemoveObjectFromPrefab( m_objects[i].pObject );
			bSomeRemoved = true;
		}
	}
	if (bSomeRemoved)
	{
		m_object->GetPrefab()->UpdateFromPrefabObject( m_object );
		ReloadObjects();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnBnClickedReloadPrefab()
{
	if (m_object)
	{
		if (m_object->GetPrefab())
		{
			m_object->SetPrefab( m_object->GetPrefab(),true );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnBnClickedUpdatePrefab()
{
	if (m_object)
	{
		m_object->UpdatePrefab();
	}
}


//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnPick( CBaseObject *picked )
{
	if (m_object)
	{
		CUndo undo("Attach Object");
		//m_object->AddEntity( (CEntity*)picked );
		m_object->AddObjectToPrefab( picked );
		ReloadObjects();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabPanel::OnPickFilter( CBaseObject *picked )
{
	if (picked->CheckFlags(OBJFLAG_PREFAB) || picked == m_object)
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabPanel::OnCancelPick()
{
}
