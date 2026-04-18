////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   MaterialBrowser.cpp
//  Version:     v1.00
//  Created:     26/8/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MaterialBrowser.h"
#include "IDataBaseItem.h"
#include "Material.h"
#include "MaterialManager.h"
#include "ViewManager.h"
#include "IObjectManager.h"
#include "Objects\BaseObject.h"
#include "Clipboard.h"
#include "NumberDlg.h"
#include "MaterialImageListCtrl.h"
#include "SourceControlDescDlg.h"

#include <ISourceControl.h>
#include "StringDlg.h"

#define IDC_MATERIAL_TREE 2

//////////////////////////////////////////////////////////////////////////
#define ITEM_IMAGE_SHARED_MATERIAL   0
#define ITEM_IMAGE_SHARED_MATERIAL_SELECTED 1
#define ITEM_IMAGE_FOLDER            2
#define ITEM_IMAGE_FOLDER_OPEN       3
#define ITEM_IMAGE_MATERIAL          4
#define ITEM_IMAGE_MATERIAL_SELECTED 5
#define ITEM_IMAGE_MULTI_MATERIAL    6
#define ITEM_IMAGE_MULTI_MATERIAL_SELECTED 7


#define ITEM_IMAGE_OVERLAY_CGF          8
#define ITEM_IMAGE_OVERLAY_INPAK        9
#define ITEM_IMAGE_OVERLAY_READONLY     10
#define ITEM_IMAGE_OVERLAY_ONDISK       11
#define ITEM_IMAGE_OVERLAY_LOCKED       12
#define ITEM_IMAGE_OVERLAY_CHECKEDOUT   13
#define ITEM_IMAGE_OVERLAY_NO_CHECKOUT  14
//////////////////////////////////////////////////////////////////////////

#define ITEM_OVERLAY_ID_CGF          1
#define ITEM_OVERLAY_ID_INPAK        2
#define ITEM_OVERLAY_ID_READONLY     3
#define ITEM_OVERLAY_ID_ONDISK       4
#define ITEM_OVERLAY_ID_LOCKED       5
#define ITEM_OVERLAY_ID_CHECKEDOUT   6
#define ITEM_OVERLAY_ID_NO_CHECKOUT  7

enum
{
	MENU_CUT = 1,
	MENU_COPY,
	MENU_COPY_NAME,
	MENU_PASTE,
	MENU_DUPLICATE,
	MENU_RENAME,
	MENU_DELETE,
	MENU_ASSIGNTOSELECTION,
	MENU_SELECTASSIGNEDOBJECTS,
	MENU_NUM_SUBMTL,
	MENU_ADDNEW,
	MENU_ADDNEW_MULTI,
	MENU_SUBMTL_MAKE,
	MENU_SUBMTL_CLEAR,
	MENU_SAVE_TO_FILE,
	MENU_SAVE_TO_FILE_MULTI,

	MENU_SCM_ADD,
	MENU_SCM_CHECK_IN,
	MENU_SCM_CHECK_OUT,
	MENU_SCM_UNDO_CHECK_OUT,
	MENU_SCM_GET_LATEST,
};



BEGIN_MESSAGE_MAP(CItemBrowserTreeCtrl, CTreeCtrl)
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()


//////////////////////////////////////////////////////////////////////////
BOOL CItemBrowserTreeCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if(nFlags)
	{
		HTREEITEM hItem = GetFirstVisibleItem();
		if(hItem)
		{
			HTREEITEM hTmpItem = 0;
			int d = abs(zDelta/WHEEL_DELTA)*2;
			for(int i = 0; i<d; i++)
			{
				if(zDelta<0)
					hTmpItem = GetNextVisibleItem(hItem);
				else
					hTmpItem = GetPrevVisibleItem(hItem);
				if(hTmpItem)
					hItem = hTmpItem;
			}
			if(hItem)
				SelectSetFirstVisible( hItem );
		}
	}
	return CTreeCtrl::OnMouseWheel(nFlags, zDelta, pt);
}


//////////////////////////////////////////////////////////////////////////
// CMaterialBrowserDlg implementation
//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMaterialBrowserDlg, CDialog)
	ON_COMMAND(IDC_LEVEL,OnRadioButton)
	ON_COMMAND(IDC_ALL,OnRadioButton)
	ON_COMMAND(IDC_RELOAD,OnReload)
	ON_EN_CHANGE(IDC_FILTER,OnFilterChange)
END_MESSAGE_MAP()

struct CMaterialBrowserCtrl::SItemInfo
{
	SItemInfo() { pMaterial = 0; hItem = NULL; bFolder = false; nSubMtlSlot = -1; }
	bool bFolder;
	int nSubMtlSlot;
	CString name;
	CString itemText;
	_smart_ptr<CMaterial> pMaterial;
	HTREEITEM hItem;
};

CMaterialBrowserDlg::CMaterialBrowserDlg()
{
	m_iSelection = 0;
}

//////////////////////////////////////////////////////////////////////////
CMaterialBrowserDlg::~CMaterialBrowserDlg()
{
}

//////////////////////////////////////////////////////////////////////////
BOOL CMaterialBrowserDlg::OnInitDialog()
{
	BOOL res = __super::OnInitDialog();
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserDlg::SetListType( int type )
{
	m_iSelection = type;
	UpdateData(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserDlg::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FILTER, m_filterCtrl);
	DDX_Control(pDX, IDC_RELOAD, m_reloadButton);
	DDX_Radio(pDX, IDC_LEVEL, m_iSelection);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserDlg::OnRadioButton()
{
	UpdateData(TRUE);
	switch (m_iSelection)
	{
	case 0:
		//m_pMtlBrowser->ReloadLevelItems();
		break;
	case 1:
		//m_pMtlBrowser->ReloadAllItems();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserDlg::OnFilterChange()
{
	CString filter;
	m_filterCtrl.GetWindowText(filter);
	m_pMtlBrowser->SetFilter(filter);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserDlg::OnReload()
{
	m_pMtlBrowser->ReloadItems(m_pMtlBrowser->GetViewType());
}

//////////////////////////////////////////////////////////////////////////
// CBrowserTreeCtrl implementation
//////////////////////////////////////////////////////////////////////////
#define NM_RBUTTONDOWN -1000
BOOL CItemBrowserTreeCtrl::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_RBUTTONDOWN)
	{
		//lResult = SendMessage(      // returns LRESULT in lResult     (HWND) hWndControl,      // handle to destination control     (UINT) WM_NOTIFY,      // message ID     (WPARAM) wParam,      // = (WPARAM) (int) idCtrl;    (LPARAM) lParam      // = (LPARAM) (LPNMHDR) pnmh; );  
		//lResult = SendMessage( GetParent()->GetSafeHwnd(),(UINT) WM_NOTIFY,(WPARAM)wParam,(LPARAM)lParam );
		NMHDR nmhdr;
		nmhdr.hwndFrom = GetSafeHwnd();
		nmhdr.idFrom = GetDlgCtrlID();
		nmhdr.code = NM_RBUTTONDOWN;
		GetParent()->SendMessage( (UINT)WM_NOTIFY,(WPARAM)nmhdr.idFrom,(LPARAM)&nmhdr );
	}
	return CTreeCtrl::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMaterialBrowserCtrl, CWnd)
	ON_WM_SIZE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_NOTIFY(TVN_BEGINDRAG, IDC_MATERIAL_TREE, OnBeginDrag)
	ON_NOTIFY(NM_RCLICK , IDC_MATERIAL_TREE, OnTreeRClick)
	ON_NOTIFY(NM_RBUTTONDOWN, IDC_MATERIAL_TREE, OnTreeRButtonDown)
	ON_NOTIFY(TVN_SELCHANGED, IDC_MATERIAL_TREE, OnSelChangedItemTree)
	ON_NOTIFY(TVN_GETDISPINFO,IDC_MATERIAL_TREE, OnTreeGetDispInfo)
	ON_NOTIFY(TVN_ITEMEXPANDING,IDC_MATERIAL_TREE, OnTreeItemExpending)
	ON_NOTIFY(TVN_KEYDOWN, IDC_MATERIAL_TREE, OnTreeKeyDown)
	ON_COMMAND(ID_MATERIAL_BROWSER_RELOAD,OnReloadItems)
	ON_COMMAND(ID_MATERIAL_BROWSER_FILTER,OnToggleFilter)
	ON_UPDATE_COMMAND_UI(ID_MATERIAL_BROWSER_FILTER,OnFilterUpdateUI)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
// CMaterialBrowserCtrl
//////////////////////////////////////////////////////////////////////////
CMaterialBrowserCtrl::CMaterialBrowserCtrl() : CToolbarDialog(IDD,NULL)
{
	m_bIgnoreSelectionChange = false;

	m_dragImage = 0;
	m_hDropItem = 0;
	m_hDraggedItem = 0;
	m_hCurrentItem = 0;

	// Load cursors.
	m_hCursorDefault = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	m_hCursorNoDrop = AfxGetApp()->LoadCursor(IDC_NODROP);
	m_hCursorCreate = AfxGetApp()->LoadCursor(IDC_HIT_CURSOR);
	m_hCursorReplace = AfxGetApp()->LoadCursor(IDC_HAND_INTERNAL);

	m_pMatMan = GetIEditor()->GetMaterialManager();
	m_pMatMan->AddListener(this);
	m_pListener = NULL;

	m_viewType = VIEW_LEVEL;
	m_pMaterialImageListCtrl = NULL;
	m_pLastActiveMultiMaterial = 0;
}

//////////////////////////////////////////////////////////////////////////
CMaterialBrowserCtrl::~CMaterialBrowserCtrl()
{
	if (m_pCurrentItem != NULL && m_pCurrentItem->IsModified())
		m_pCurrentItem->Save();

	m_pMaterialImageListCtrl = NULL;
	m_pMatMan->RemoveListener(this);
	ClearItems();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	CRect rcClient;
	GetClientRect(rcClient);
	if (!m_treeCtrl.m_hWnd)
		return;

	DWORD dwMode = LM_HORZ|LM_HORZDOCK|LM_STRETCH|LM_COMMIT;
	CSize sz = m_wndToolBar.CalcDockingLayout(32000, dwMode);

	CRect rctb = rcClient;
	rctb.bottom = rctb.top + sz.cy;
	m_wndToolBar.MoveWindow(rctb);

	rcClient.top = rctb.bottom + 1;

	CRect rctree = rcClient;
	m_treeCtrl.MoveWindow( rctree );

}

//////////////////////////////////////////////////////////////////////////
BOOL CMaterialBrowserCtrl::Create( const RECT& rect,CWnd* pParentWnd,UINT nID )
{
	BOOL res = CToolbarDialog::Create( IDD,pParentWnd );

	VERIFY(m_wndToolBar.CreateToolBar(WS_VISIBLE|WS_CHILD|CBRS_TOOLTIPS|CBRS_GRIPPER, this, AFX_IDW_TOOLBAR));
	VERIFY(m_wndToolBar.LoadToolBar(IDR_MATERIAL_BROWSER));
	m_wndToolBar.SetFlags(xtpFlagAlignTop|xtpFlagStretched);
	m_wndToolBar.EnableCustomization(FALSE);

	{
		CXTPControl *pCtrl = m_wndToolBar.GetControls()->FindControl(xtpControlButton, ID_MATERIAL_BROWSER_FILTER_TEXT, TRUE, FALSE);
		if (pCtrl)
		{
			int nIndex = pCtrl->GetIndex();
			CXTPControlEdit *pEdit = (CXTPControlEdit*)m_wndToolBar.GetControls()->SetControlType(nIndex,xtpControlEdit);
			pEdit->SetFlags(xtpFlagManualUpdate);
			pEdit->SetWidth(80);
		}
	}

	//VERIFY( m_wndToolBar.CreateEx(this, TBSTYLE_FLAT|TBSTYLE_WRAPABLE,WS_CHILD|WS_VISIBLE|CBRS_TOP|CBRS_TOOLTIPS|CBRS_FLYBY|CBRS_SIZE_DYNAMIC) );
	//VERIFY( m_wndToolBar.LoadToolBar24(IDR_LAYERS_BAR,14) );

/*
	LPCSTR lpzClass = AfxRegisterWndClass( CS_VREDRAW|CS_HREDRAW,::LoadCursor(NULL, IDC_ARROW),(HBRUSH)GetStockObject(BLACK_BRUSH),NULL );
	BOOL res = CreateEx( 0,lpzClass,NULL,WS_CHILD|WS_VISIBLE|WS_TABSTOP,rect,pParentWnd,nID );
	VERIFY(res);
*/

	CRect rc;
	GetClientRect(rc);

	CMFCUtils::LoadTrueColorImageList( m_imageList,IDB_MATERIAL_TREE,20,RGB(255,0,255) );
	CMFCUtils::LoadTrueColorImageList( m_imageList,IDB_FILE_STATUS,20,RGB(255,0,255) );

	m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_CGF,ITEM_OVERLAY_ID_CGF );
	m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_INPAK,ITEM_OVERLAY_ID_INPAK );
	m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_READONLY,ITEM_OVERLAY_ID_READONLY );
	m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_ONDISK,ITEM_OVERLAY_ID_ONDISK );
	m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_LOCKED,ITEM_OVERLAY_ID_LOCKED );
	m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_CHECKEDOUT,ITEM_OVERLAY_ID_CHECKEDOUT );
	m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_NO_CHECKOUT,ITEM_OVERLAY_ID_NO_CHECKOUT );

	// Create Use materials tree control.
	// Attach it to the control
	GetClientRect(rc);
	m_treeCtrl.Create( WS_VISIBLE|WS_CHILD|WS_TABSTOP|WS_BORDER|TVS_HASBUTTONS|TVS_SHOWSELALWAYS|TVS_LINESATROOT|TVS_HASLINES|
		TVS_FULLROWSELECT|TVS_NOHSCROLL|TVS_INFOTIP|TVS_TRACKSELECT,rc,this,IDC_MATERIAL_TREE );
	if (!gSettings.gui.bWindowsVista)
		m_treeCtrl.SetItemHeight(18);

	// TreeCtrl must be already created.
	m_treeCtrl.SetImageList(&m_imageList,TVSIL_NORMAL);

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnReloadItems()
{
	ReloadItems( m_viewType );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnToggleFilter()
{
	if (m_filterText.IsEmpty())
	{
		CXTPControlEdit *pEdit = (CXTPControlEdit*)m_wndToolBar.GetControls()->FindControl(xtpControlEdit, ID_MATERIAL_BROWSER_FILTER_TEXT, TRUE, FALSE);
		if (pEdit)
		{
			CString str = pEdit->GetEditText();
			if (str != m_filterText)
			{
				m_filterText = str;
				ReloadItems(m_viewType);
			}
		}
	}
	else
	{
		m_filterText = "";
		ReloadItems(m_viewType);
	}
}

void CMaterialBrowserCtrl::OnFilterUpdateUI( CCmdUI *pCmdUI )
{
	if (m_filterText.IsEmpty())
		pCmdUI->SetCheck(0);
	else
		pCmdUI->SetCheck(1);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::ClearItems()
{
	m_hCurrentItem = 0;
	m_bIgnoreSelectionChange = true;
	//////////////////////////////////////////////////////////////////////////
	for (Items::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		SItemInfo *pItem = *it;
		delete pItem;
	}
	m_items.clear();
	if(m_treeCtrl.m_hWnd)
	{
		m_treeCtrl.SetRedraw(FALSE);
		m_treeCtrl.DeleteAllItems();
		m_nameMap.clear();
		m_treeCtrl.SetRedraw(TRUE);
	}

	if (m_pMaterialImageListCtrl)
		m_pMaterialImageListCtrl->DeleteAllItems();
	m_pLastActiveMultiMaterial = NULL;

	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::ReloadItems( EViewType viewType )
{
	CWaitCursor wait;

	CString prevSelection;
	if (m_hCurrentItem)
	{
		prevSelection = GetFullItemName(m_hCurrentItem);
	}

	ClearItems();

	m_viewType = viewType;
	switch (m_viewType) {
	case VIEW_LEVEL:
		ReloadLevelItems();
		break;
	case VIEW_ALL:
	case VIEW_MATLIB:
	default:
		ReloadAllItems();
		break;
	}

	if (!prevSelection.IsEmpty())
	{
		HTREEITEM hItem = stl::find_in_map(m_nameMap,prevSelection,0);
		if (hItem)
			SetSelectedItem(hItem);
	}
	else
	{
		SelectItem( m_pMatMan->GetCurrentMaterial() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::ReloadLevelItems()
{
	HTREEITEM hLibItem = TVI_ROOT;

	m_bIgnoreSelectionChange = true;
	m_treeCtrl.SetRedraw(FALSE);

	IDataBaseItemEnumerator *pEnum = m_pMatMan->GetItemEnumerator();
	for (IDataBaseItem *pItem = pEnum->GetFirst(); pItem != NULL; pItem = pEnum->GetNext())
	{
		AddItemToTree( pItem,false );
	}
	pEnum->Release();

	m_treeCtrl.SortChildren( hLibItem );
	m_treeCtrl.Expand( hLibItem,TVE_EXPAND );
	m_treeCtrl.SetRedraw(TRUE);

	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::SetFilter( const CString &filter )
{
	if (m_filterText != filter)
	{
		m_filterText = filter;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::AddItemToTree( IDataBaseItem *pItem,bool bLockRedraw )
{
	if(!pItem)
		return;

	const char *sName = pItem->GetName();
	
	// Hide standard materials for which modifications could result in a crash
	if( strcmp( sName, "Materials/Ocean/ocean_bottom" ) == 0 ) return;

	if (!m_filterText.IsEmpty())
	{
		if (strstri(sName,m_filterText) == 0)
			return;
	}

	m_bIgnoreSelectionChange = true;
	if (bLockRedraw)
		m_treeCtrl.SetRedraw(FALSE);

	HTREEITEM hRoot = TVI_ROOT;
	char *token;
	char itempath[1024];
	strcpy( itempath,sName );

	token = strtok( itempath,"\\/." );

	CString itemName;
	while (token)
	{
		CString strToken = token;

		token = strtok( NULL,"\\/." );
		if (!token)
			break;

		itemName += strToken+"/";

		HTREEITEM hParentItem = stl::find_in_map(m_nameMap,itemName,0);
		if (!hParentItem)
		{
			hRoot = m_treeCtrl.InsertItem(strToken, ITEM_IMAGE_FOLDER,ITEM_IMAGE_FOLDER, hRoot );
			m_nameMap[itemName] = hRoot;
		}
		else
			hRoot = hParentItem;
	}

	HTREEITEM hNewItem = InsertItemToTree( pItem,hRoot );

	if (bLockRedraw)
		m_treeCtrl.SetRedraw(TRUE);
	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CMaterialBrowserCtrl::InsertItemToTree( IDataBaseItem *pItem,HTREEITEM hParent,int nSubMtlSlot )
{
	CMaterial *pMtl = (CMaterial*)pItem;

	CString itemText;
	if (pMtl)
		itemText = pMtl->GetShortName();
	else
		itemText = "";
	HTREEITEM hItem;
	if (pMtl && pMtl->IsMultiSubMaterial())
	{
		hItem = m_treeCtrl.InsertItem( itemText,ITEM_IMAGE_MULTI_MATERIAL,ITEM_IMAGE_MULTI_MATERIAL_SELECTED,hParent );
		
		for (int i = 0; i < pMtl->GetSubMaterialCount(); i++)
		{
			CMaterial *pSubMtl = pMtl->GetSubMaterial(i);
			InsertItemToTree( pSubMtl,hItem,i );
		}
	}
	else
	{
		int nImage = ITEM_IMAGE_MATERIAL;
		int nImageSel = ITEM_IMAGE_MATERIAL_SELECTED;
		if (nSubMtlSlot >= 0)
		{
			if (pMtl)
				itemText = pMtl->GetName();
			CString name = itemText;
			itemText.Format( "[%d] %s",nSubMtlSlot+1,(const char*)name );

			if (pMtl && !pMtl->IsPureChild())
			{
				nImage = ITEM_IMAGE_SHARED_MATERIAL;
				nImageSel = ITEM_IMAGE_SHARED_MATERIAL_SELECTED;
			}
		}
		hItem = m_treeCtrl.InsertItem( itemText,nImage,nImageSel,hParent );
	}
	//m_treeCtrl.SetItemState( hItem, TVIS_BOLD,TVIS_BOLD );

	SItemInfo *pItemInfo = new SItemInfo;
	pItemInfo->hItem = hItem;
	if (pMtl)
		pItemInfo->name = pMtl->GetName();
	pItemInfo->itemText = itemText;
	pItemInfo->pMaterial = pMtl;
	pItemInfo->nSubMtlSlot = nSubMtlSlot;
	m_items.insert(pItemInfo);
	m_treeCtrl.SetItemData( hItem,(DWORD_PTR)pItemInfo );

	return hItem;
}

//////////////////////////////////////////////////////////////////////////
CMaterialBrowserCtrl::SItemInfo* CMaterialBrowserCtrl::GetItemInfo( HTREEITEM hItem )
{
	if (!hItem)
		return 0;
	SItemInfo *pItemInfo = (SItemInfo*)m_treeCtrl.GetItemData(hItem);
	return pItemInfo;
}

//////////////////////////////////////////////////////////////////////////
CMaterialBrowserCtrl::SItemInfo* CMaterialBrowserCtrl::GetParentItemInfo( HTREEITEM hItem )
{
	if (!hItem)
		return 0;
	hItem = m_treeCtrl.GetParentItem(hItem);
	if (!hItem)
		return 0;
	SItemInfo *pItemInfo = (SItemInfo*)m_treeCtrl.GetItemData(hItem);
	return pItemInfo;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::ReloadTreeSubMtls( HTREEITEM hItem )
{
	// Delete all of the children of hmyItem.
	if (m_treeCtrl.ItemHasChildren(hItem))
	{
		HTREEITEM hNextItem;
		HTREEITEM hChildItem = m_treeCtrl.GetChildItem(hItem);

		while (hChildItem != NULL)
		{
			hNextItem = m_treeCtrl.GetNextItem(hChildItem, TVGN_NEXT);
			RemoveItemFromTree(hChildItem);
			hChildItem = hNextItem;
		}
	}

	SItemInfo *pItemInfo = (SItemInfo*)m_treeCtrl.GetItemData(hItem);
	if (pItemInfo)
	{
		CMaterial *pMtl = pItemInfo->pMaterial;
		if (pMtl)
		{
			for (int i = 0; i < pMtl->GetSubMaterialCount(); i++)
			{
				CMaterial *pSubMtl = pMtl->GetSubMaterial(i);
				InsertItemToTree( pSubMtl,hItem,i );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::RemoveItemFromTree( HTREEITEM hItem )
{
	if (hItem == m_hCurrentItem)
		SetSelectedItem(0);
	m_hCurrentItem = 0;

	SItemInfo *pItemInfo = GetItemInfo(hItem);

	HTREEITEM hParent = m_treeCtrl.GetParentItem(hItem);
	m_treeCtrl.DeleteItem(hItem);

	if (hParent)
	{
		SItemInfo *pParentItemInfo = GetItemInfo(hParent);
		if (!pParentItemInfo || pParentItemInfo->bFolder)
			RemoveEmptyParent(hParent);
	}

	m_items.erase(pItemInfo);
	delete pItemInfo;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::RemoveEmptyParent( HTREEITEM hItem )
{
	if (!m_treeCtrl.ItemHasChildren(hItem))
	{
		CString pathName = GetFullItemName(hItem);
		m_nameMap.erase(pathName);

		// This is empty node.
		HTREEITEM hParent = m_treeCtrl.GetParentItem(hItem);
		m_treeCtrl.DeleteItem(hItem);
		if (hParent)
			RemoveEmptyParent(hParent);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::ReloadAllItems()
{
	CFileUtil::FileArray files;
	files.reserve(5000); // Reserve space for 5000 files.

	// MichaelS - as well as .mtl files, we now load .binmtl files. To perform
	// the search for both these filetypes, we search for *.*mtl. This will of
	// course catch files with other extensions, although there shouldnt be any
	// in the game. However, to handle these we check the extension when iterating
	// over the files.
	if (m_viewType == VIEW_ALL)
	{
		CFileUtil::ScanDirectory( "","*.*mtl",files );
	}
	else if (m_viewType == VIEW_MATLIB)
	{
		for (int i = 0; i < gSettings.searchPaths[EDITOR_PATH_MATERIALS].size(); i++)
		{
			CString filespec = gSettings.searchPaths[EDITOR_PATH_MATERIALS][i];
			filespec = Path::AddBackslash(filespec) + "*.*mtl";
			CFileUtil::ScanDirectory( "",filespec,files );
		}
	}

	// First add all items already loaded in the level.
	ReloadLevelItems();
	
	m_bIgnoreSelectionChange = true;
	m_treeCtrl.SetRedraw(FALSE);

	bool bFilter = !m_filterText.IsEmpty();

	HTREEITEM hLibItem = TVI_ROOT;

	CMaterialManager *pMatMan = (CMaterialManager*)m_pMatMan;

	// MichaelS - since there are now two possible filenames that map to a single material name,
	// we need to make sure that we only load each material name once. Therefore we keep a set
	// of all the names loaded so far. Note that it is possible that the first time the material
	// name is encountered, the file loaded will not be the same as the file that is being scanned,
	// but this causes no problems.
	std::set<CString> loadedMaterialNames;

	CString itemName;
	CString itemLastName;
	for (int i = 0; i < files.size(); i++)
	{
		// Make sure we don't accidentally load incorrect extensions.
		CString extension = PathUtil::GetExt(files[i].filename.GetString());
		if (extension != "mtl" && extension != "binmtl")
			continue;
		if (extension == "binmtl")
			extension = extension;

		CString name = pMatMan->FilenameToMaterial( files[i].filename );

		// Check if this item already loaded.
		if (loadedMaterialNames.find(name) != loadedMaterialNames.end())
			continue;
		if (m_pMatMan->FindItemByName(name) != NULL)
			continue;

		itemLastName = name;

		// Remember that this material has been loaded, to make sure that it doesn't get added to the tree view twice.
		loadedMaterialNames.insert(name);

		if (bFilter)
		{
			if (strstri(name,m_filterText) == 0)
				continue;
		}

		HTREEITEM hRoot = hLibItem;
		char *token;
		char itempath[1024];
		strcpy( itempath,name );

		token = strtok( itempath,"\\/." );

		itemName = "";
		while (token)
		{
			CString strToken = token;

			token = strtok( NULL,"\\/." );
			if (!token)
			{
				itemLastName = strToken;
				break;
			}

			itemName += strToken+"/";

			HTREEITEM hParentItem = stl::find_in_map(m_nameMap,itemName,0);
			if (!hParentItem)
			{
				hRoot = m_treeCtrl.InsertItem(strToken, ITEM_IMAGE_FOLDER, ITEM_IMAGE_FOLDER, hRoot );
				m_nameMap[itemName] = hRoot;
			}
			else
				hRoot = hParentItem;
		}

		HTREEITEM hNewItem = m_treeCtrl.InsertItem( itemLastName,ITEM_IMAGE_MATERIAL,ITEM_IMAGE_MATERIAL_SELECTED,hRoot );
		//HTREEITEM hNewItem = m_treeCtrl.InsertItem( LPSTR_TEXTCALLBACK,2,3,hRoot );

		SItemInfo *pItemInfo = new SItemInfo;
		pItemInfo->hItem = hNewItem;
		pItemInfo->name = name;
		pItemInfo->itemText = itemLastName;
		m_items.insert(pItemInfo);
		m_treeCtrl.SetItemData( hNewItem,(DWORD_PTR)pItemInfo );
	}



	m_treeCtrl.SortChildren( hLibItem );

	SortTreeCtrl(hLibItem);

	m_treeCtrl.Expand( hLibItem,TVE_EXPAND );
	m_treeCtrl.SetRedraw(TRUE);

	m_bIgnoreSelectionChange = false;
}


//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::SortTreeCtrl(HTREEITEM hItem)
{
	HTREEITEM hChildItem = m_treeCtrl.GetChildItem(hItem);

	while(hChildItem != NULL)
	{
		m_treeCtrl.SortChildren( hChildItem );

		if(m_treeCtrl.ItemHasChildren(hChildItem))
			SortTreeCtrl(hChildItem);

		hChildItem = m_treeCtrl.GetNextItem(hChildItem, TVGN_NEXT);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnTreeGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult)
{	
	NMTVDISPINFO *pDI = (NMTVDISPINFO*)pNMHDR;
	if (pDI->item.mask & TVIF_TEXT)
	{
		SItemInfo *pItem = (SItemInfo*)pDI->item.lParam;
		strcpy( pDI->item.pszText,pItem->itemText );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::SetItemStateFromFileAttributes( HTREEITEM hItem,uint32 attr,bool bFromCGF )
{
	if (bFromCGF)
	{
		// From CGF.
		m_treeCtrl.SetItemState( hItem, INDEXTOOVERLAYMASK(ITEM_OVERLAY_ID_CGF), TVIS_OVERLAYMASK );
	}
	else if (attr&SCC_FILE_ATTRIBUTE_INPAK)
	{
		// Inside pak file.
		m_treeCtrl.SetItemState( hItem, INDEXTOOVERLAYMASK(ITEM_OVERLAY_ID_INPAK), TVIS_OVERLAYMASK );
	}
	else if (attr&SCC_FILE_ATTRIBUTE_CHECKEDOUT) 
	{
		// Checked out.
		m_treeCtrl.SetItemState( hItem, INDEXTOOVERLAYMASK(ITEM_OVERLAY_ID_CHECKEDOUT), TVIS_OVERLAYMASK );
	}
	else if (attr&SCC_FILE_ATTRIBUTE_MANAGED)
	{
		// Checked out.
		m_treeCtrl.SetItemState( hItem, INDEXTOOVERLAYMASK(ITEM_OVERLAY_ID_LOCKED), TVIS_OVERLAYMASK );
	}
	else if (attr&SCC_FILE_ATTRIBUTE_READONLY)
	{
		// Readonly.
		m_treeCtrl.SetItemState( hItem, INDEXTOOVERLAYMASK(ITEM_OVERLAY_ID_READONLY), TVIS_OVERLAYMASK );
	}
	else
	{
		// Remove overlay mask by using index zero.
		m_treeCtrl.SetItemState( hItem, INDEXTOOVERLAYMASK(0), TVIS_OVERLAYMASK );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnTreeItemExpending(NMHDR* pNMHDR, LRESULT* pResult)
{	
	NMTREEVIEW *pNM = (NMTREEVIEW*)pNMHDR;
	HTREEITEM hItem = pNM->itemNew.hItem;

	// If folder
	SItemInfo *pItemInfo = GetItemInfo(hItem);
	if (!pItemInfo || pItemInfo->bFolder)
	{
		if (pNM->action == TVE_EXPAND)
		{
			m_treeCtrl.SetItemImage(hItem,ITEM_IMAGE_FOLDER_OPEN,ITEM_IMAGE_FOLDER_OPEN);
		}
		else
		{
			m_treeCtrl.SetItemImage(hItem,ITEM_IMAGE_FOLDER,ITEM_IMAGE_FOLDER);
		}
	}

	if (m_treeCtrl.ItemHasChildren(hItem))
	{
		HTREEITEM hChildItem = m_treeCtrl.GetChildItem(hItem);
		while (hChildItem != NULL)
		{
			SItemInfo *pItemInfo = GetItemInfo(hChildItem);
			if (pItemInfo)
			{
				m_bIgnoreSelectionChange = true;
				if (!pItemInfo->pMaterial)
				{
					// Load this materials.
					if (!pItemInfo->name.IsEmpty())
					{
						pItemInfo->pMaterial = m_pMatMan->LoadMaterial(pItemInfo->name,false);
						
						if (pItemInfo->pMaterial != NULL && pItemInfo->pMaterial->IsMultiSubMaterial())
						{
							m_treeCtrl.SetItemImage(hChildItem,ITEM_IMAGE_MULTI_MATERIAL,ITEM_IMAGE_MULTI_MATERIAL_SELECTED);
							ReloadTreeSubMtls(hChildItem);
						}
					}
				}
				if (pItemInfo->pMaterial != NULL)
				{
					// Update overlay status.
					uint32 attr = pItemInfo->pMaterial->GetFileAttributes();
					SetItemStateFromFileAttributes( hChildItem,attr,pItemInfo->pMaterial->IsFromEngine() );
				}
				m_bIgnoreSelectionChange = false;
			}
			hChildItem = m_treeCtrl.GetNextItem(hChildItem, TVGN_NEXT);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CString CMaterialBrowserCtrl::GetFullItemName( HTREEITEM hItem )
{
	CString name = m_treeCtrl.GetItemText(hItem);
	HTREEITEM hParent = hItem;
	while (hParent)
	{
		hParent = m_treeCtrl.GetParentItem(hParent);
		if (hParent)
		{
			name = m_treeCtrl.GetItemText(hParent) + "/" + name;
		}
	}
	return name;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if(GetAsyncKeyState( VK_CONTROL ))
	{
		NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
		*pResult = 0;

		HTREEITEM hItem = pNMTreeView->itemNew.hItem;

		SItemInfo *pDraggedItem = (SItemInfo*)pNMTreeView->itemNew.lParam;
		if (!pDraggedItem)
			return;

		CMaterial *pMtl = pDraggedItem->pMaterial;
		if (!pMtl)
		{
			return;
		}

		m_treeCtrl.Select( hItem,TVGN_CARET );

		// Calculate the offset to the hotspot
		CPoint offsetPt(-10,-10);   // Initialize a default offset

		/*
		CPoint dragPt = pNMTreeView->ptDrag;    // Get the Drag point
		UINT nHitFlags = 0;
		HTREEITEM htiHit = m_treeCtrl.HitTest(dragPt, &nHitFlags);
		if (NULL != htiHit)
		{
			// The drag point has Hit an item in the tree
			CRect itemRect;

			// Get the text bounding rectangle 
			if (m_treeCtrl.GetItemRect(htiHit, &itemRect, TRUE)) 
			{ 
				// Calculate the new offset 
				offsetPt.y = dragPt.y - itemRect.top; 
				offsetPt.x = dragPt.x - (itemRect.left - m_treeCtrl.GetIndent()); 
			}
		}
		*/

		m_hDropItem = 0;
		m_dragImage = m_treeCtrl.CreateDragImage( hItem );
		if (m_dragImage)
		{
			m_hDraggedItem = hItem;
			m_hDropItem = hItem;

			//m_dragImage->BeginDrag(0, CPoint(-10, -10));
			m_dragImage->BeginDrag(0, offsetPt);

			POINT pt = pNMTreeView->ptDrag;
			ClientToScreen( &pt );
			m_dragImage->DragEnter(AfxGetMainWnd(), pt);
			SetCapture();

			GetIEditor()->EnableUpdate( false );
		}
	}

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_dragImage)
	{
		CPoint p;

		p = point;
		ClientToScreen( &p );
		m_treeCtrl.ScreenToClient( &p );

		SetCursor( m_hCursorDefault );
		TVHITTESTINFO hit;
		ZeroStruct(hit);
		hit.pt = p;
		HTREEITEM hHitItem = m_treeCtrl.HitTest( &hit );
		if (hHitItem)
		{
			if (m_hDropItem != hHitItem)
			{
				if (m_hDropItem)
					m_treeCtrl.SetItem( m_hDropItem,TVIF_STATE,0,0,0,0,TVIS_DROPHILITED,0 );
				// Set state of this item to drop target.
				m_treeCtrl.SetItem( hHitItem,TVIF_STATE,0,0,0,TVIS_DROPHILITED,TVIS_DROPHILITED,0 );
				m_hDropItem = hHitItem;
				//m_treeCtrl.Invalidate();
			}
		}
		
		// Check if this is not SubMaterial.
		SItemInfo *pItemInfo = GetItemInfo(m_hDraggedItem);
		CMaterial *pMtl = GetMaterial(m_hDraggedItem);
		if (!pMtl || pItemInfo->nSubMtlSlot >= 0)
		{
			SetCursor( m_hCursorNoDrop );
		}
		else
		{
			CRect rc;
			AfxGetMainWnd()->GetWindowRect( rc );
			p = point;
			ClientToScreen( &p );
			p.x -= rc.left;
			p.y -= rc.top;
			m_dragImage->DragMove( p );

			// Check if can drop here.
			{
				CPoint p;
				GetCursorPos( &p );
				CViewport* viewport = GetIEditor()->GetViewManager()->GetViewportAtPoint( p );
				if (viewport)
				{
					CPoint vp = p;
					viewport->ScreenToClient(&vp);
					HitContext hit;
					if (viewport->HitTest( vp,hit ))
					{
						if (hit.object)
						{
							SetCursor( m_hCursorReplace );
						}
					}
					else
					{
						if (viewport->CanDrop(vp,pMtl))
							SetCursor( m_hCursorReplace );
					}
				}
			}
		}
	}

	__super::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	//CXTResizeDialog::OnLButtonUp(nFlags, point);

	if (m_hDropItem)
	{
		m_treeCtrl.SetItem( m_hDropItem,TVIF_STATE,0,0,0,0,TVIS_DROPHILITED,0 );
		m_hDropItem = 0;
	}

	if (m_dragImage)
	{
		CPoint p;
		GetCursorPos( &p );

		GetIEditor()->EnableUpdate( true );

		m_dragImage->DragLeave( AfxGetMainWnd() );
		m_dragImage->EndDrag();
		delete m_dragImage;
		m_dragImage = 0;
		ReleaseCapture();

		CPoint treepoint = p;
		m_treeCtrl.ScreenToClient( &treepoint );

		TVHITTESTINFO hit;
		ZeroStruct(hit);
		hit.pt = treepoint;
		HTREEITEM hHitItem = m_treeCtrl.HitTest( &hit );
		if (hHitItem)
		{
			DropToItem( hHitItem,m_hDraggedItem );
			m_hDraggedItem = 0;
			return;
		}

		// Check if this is not SubMaterial.
		SItemInfo *pItemInfo = GetItemInfo(m_hDraggedItem);
		if (pItemInfo->nSubMtlSlot < 0)
		{
			CWnd *wnd = WindowFromPoint( p );

			CUndo undo( "Assign Material" );

			CViewport* viewport = GetIEditor()->GetViewManager()->GetViewportAtPoint( p );
			if (viewport)
			{
				CPoint vp = p;
				viewport->ScreenToClient(&vp);
				// Drag and drop into one of views.
				// Start object creation.
				HitContext hit;
				if (viewport->HitTest( vp,hit ))
				{
					if (hit.object)
					{
						CMaterial *pMtl = GetMaterial(m_hDraggedItem);
						if (pMtl && !pMtl->IsPureChild())
						{
							hit.object->SetMaterial(pMtl);
						}
					}
				}
				else
				{
					CMaterial *pMtl = GetMaterial(m_hDraggedItem);
					viewport->Drop(vp,pMtl);
				}
			}
			else
			{
			}
		}
	}
	m_hDraggedItem = 0;

	__super::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnTreeRButtonDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	CPoint point;
	UINT uFlags;
	GetCursorPos( &point );
	m_treeCtrl.ScreenToClient( &point );
	HTREEITEM hItem = m_treeCtrl.HitTest(point,&uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		SItemInfo *pItemInfo = GetItemInfo(hItem);
		if (pItemInfo)
		{
			SetSelectedItem( hItem );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnTreeRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	// Show helper menu.
	_smart_ptr<CMaterial> pMtl;

	// Find node under mouse.
	CPoint point;
	GetCursorPos( &point );
	m_treeCtrl.ScreenToClient( &point );
	// Select the item that is at the point myPoint.
	UINT uFlags;
	SItemInfo *pItemInfo = 0;
	HTREEITEM hItem = m_treeCtrl.HitTest(point,&uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		pItemInfo = GetItemInfo(hItem);
		if (pItemInfo)
			pMtl = pItemInfo->pMaterial;
	}
	
	GetCursorPos( &point );

	// Create pop up menu.
	CMenu menu;
	menu.CreatePopupMenu();

	bool bCanPaste = CanPaste();
	int pasteFlags = 0;
	if (!bCanPaste)
		pasteFlags |= MF_GRAYED;

	if (!pItemInfo || pItemInfo->bFolder)
	{
		menu.AppendMenu( MF_STRING|pasteFlags,MENU_PASTE,"Paste\tCtrl+V" );
		menu.AppendMenu( MF_SEPARATOR,0,"" );
		menu.AppendMenu( MF_STRING,MENU_ADDNEW,"Add New Material" );
		menu.AppendMenu( MF_STRING,MENU_ADDNEW_MULTI,"Add New Multi Material" );

		menu.AppendMenu( MF_SEPARATOR,0,"" );
		menu.AppendMenu( MF_STRING|MF_GRAYED,0,"  Source Control" );
		menu.AppendMenu( MF_STRING,MENU_SCM_ADD,"Add To Source Control" );
		menu.AppendMenu( MF_STRING,MENU_SCM_CHECK_OUT,"Check Out" );
		menu.AppendMenu( MF_STRING,MENU_SCM_CHECK_IN,"Check In" );
		menu.AppendMenu( MF_STRING,MENU_SCM_UNDO_CHECK_OUT,"Undo Check Out" );
		menu.AppendMenu( MF_STRING,MENU_SCM_GET_LATEST,"Get Latest Version" );
	}
	else
	{
		SetSelectedItem( hItem );


		if (pItemInfo->nSubMtlSlot >= 0)
		{
			int nFlags = 0;
			SItemInfo *pParentItemInfo = GetItemInfo(m_treeCtrl.GetParentItem(hItem));
			if (pParentItemInfo && pParentItemInfo->pMaterial != NULL)
			{
				uint32 nFileAttr = pParentItemInfo->pMaterial->GetFileAttributes();
				if (nFileAttr&SCC_FILE_ATTRIBUTE_READONLY)
					nFlags |= MF_GRAYED;
			}

			menu.AppendMenu( MF_STRING|nFlags,MENU_SUBMTL_MAKE,"Make Sub Material" );
			menu.AppendMenu( MF_STRING|nFlags,MENU_SUBMTL_CLEAR,"Clear Sub Material" );
			if (pMtl)
				menu.AppendMenu( MF_SEPARATOR,0,"" );
		}
		if (pMtl)
		{
			bool bDummMtl = false;
			if (!pMtl->IsPureChild() && (pMtl->GetFilename().IsEmpty() || pMtl->IsDummy()))
			{
				bDummMtl = true;
				menu.AppendMenu( MF_STRING,MENU_SAVE_TO_FILE,"Create Material From This" );
				menu.AppendMenu( MF_STRING,MENU_SAVE_TO_FILE_MULTI,"Create Multi Material From This" );
				menu.AppendMenu( MF_SEPARATOR,0,"" );
			}

			if (pMtl->IsMultiSubMaterial())
			{
				menu.AppendMenu( MF_STRING,MENU_NUM_SUBMTL,"Set Number of Sub-Materials" );
				menu.AppendMenu( MF_SEPARATOR,0,"" );
			}
			menu.AppendMenu( MF_STRING,MENU_CUT,"Cut\tCtrl+X" );
			menu.AppendMenu( MF_STRING,MENU_COPY,"Copy\tCtrl+C" );
			menu.AppendMenu( MF_STRING|pasteFlags,MENU_PASTE,"Paste\tCtrl+V" );
			menu.AppendMenu( MF_STRING,MENU_COPY_NAME,"Copy Name to Clipboard" );
			menu.AppendMenu( MF_SEPARATOR,0,"" );
			menu.AppendMenu( MF_STRING,MENU_DUPLICATE,"Duplicate\tCtrl+D" ); 
			menu.AppendMenu( MF_STRING,MENU_RENAME,"Rename\tF2" );
			menu.AppendMenu( MF_STRING,MENU_DELETE,"Delete\tDel" );
			menu.AppendMenu( MF_SEPARATOR,0,"" );
			menu.AppendMenu( MF_STRING,MENU_ASSIGNTOSELECTION,"Assign to Selected Objects" );
			menu.AppendMenu( MF_STRING,MENU_SELECTASSIGNEDOBJECTS,"Select Assigned Objects" );
			menu.AppendMenu( MF_SEPARATOR,0,"" );
			menu.AppendMenu( MF_STRING,MENU_ADDNEW,"Add New Material" );
			menu.AppendMenu( MF_STRING,MENU_ADDNEW_MULTI,"Add New Multi Material" );
			menu.AppendMenu( MF_SEPARATOR,0,"" );
			
			uint32 nFileAttr = pMtl->GetFileAttributes();
			if (nFileAttr & SCC_FILE_ATTRIBUTE_INPAK)
			{
				menu.AppendMenu( MF_STRING|MF_GRAYED,0,"  Material In Pak (Read Only)" );
				if (nFileAttr&SCC_FILE_ATTRIBUTE_MANAGED)
					menu.AppendMenu( MF_STRING,MENU_SCM_GET_LATEST,"Get Latest Version" );
			}
			else
			{
				menu.AppendMenu( MF_STRING|MF_GRAYED,0,"  Source Control" );
				if (!(nFileAttr&SCC_FILE_ATTRIBUTE_MANAGED))
				{
					// If not managed.
					menu.AppendMenu( MF_STRING,MENU_SCM_ADD,"Add To Source Control" );
				}
				else
				{
					// If managed.
					if (nFileAttr&SCC_FILE_ATTRIBUTE_READONLY)
						menu.AppendMenu( MF_STRING,MENU_SCM_CHECK_OUT,"Check Out" );
					if (nFileAttr&SCC_FILE_ATTRIBUTE_CHECKEDOUT)
					{
						menu.AppendMenu( MF_STRING,MENU_SCM_CHECK_IN,"Check In" );
						menu.AppendMenu( MF_STRING,MENU_SCM_UNDO_CHECK_OUT,"Undo Check Out" );
					}
					if (nFileAttr&SCC_FILE_ATTRIBUTE_MANAGED)
						menu.AppendMenu( MF_STRING,MENU_SCM_GET_LATEST,"Get Latest Version" );
				}
			}
		}
	}
	int cmd = menu.TrackPopupMenu( TPM_RETURNCMD|TPM_LEFTALIGN|TPM_LEFTBUTTON,point.x,point.y,this );
	switch(cmd)
	{
	case MENU_CUT: OnCut(); break;
	case MENU_COPY: OnCopy(); break;
	case MENU_COPY_NAME: OnCopyName(); break;
	case MENU_PASTE: OnPaste(); break;
	case MENU_DUPLICATE: OnClone(); break;
	case MENU_RENAME: OnRenameItem(); break;
	case MENU_DELETE: DeleteItem(hItem); break;
	case MENU_ASSIGNTOSELECTION:
		GetIEditor()->ExecuteCommand("Material.AssignToSelection");
		break;
	case MENU_SELECTASSIGNEDOBJECTS:
		GetIEditor()->ExecuteCommand("Material.SelectAssignedObjects");
		break;
	case MENU_NUM_SUBMTL: OnSetSubMtlCount(hItem); break;
	case MENU_ADDNEW: OnAddNewMaterial(); break;
	case MENU_ADDNEW_MULTI: OnAddNewMultiMaterial(); break;

	case MENU_SUBMTL_MAKE: OnMakeSubMtlSlot(hItem); break;
	case MENU_SUBMTL_CLEAR: OnClearSubMtlSlot(hItem); break;
	
	case MENU_SCM_ADD: DoSourceControlOp(hItem,ESCM_IMPORT); break;
	case MENU_SCM_CHECK_OUT: DoSourceControlOp(hItem,ESCM_CHECKOUT); break;
	case MENU_SCM_CHECK_IN: DoSourceControlOp(hItem,ESCM_CHECKIN); break;
	case MENU_SCM_UNDO_CHECK_OUT: DoSourceControlOp(hItem,ESCM_UNDO_CHECKOUT); break;
	case MENU_SCM_GET_LATEST: DoSourceControlOp(hItem,ESCM_GETLATEST); break;

	case MENU_SAVE_TO_FILE: OnSaveToFile(false); break;
	case MENU_SAVE_TO_FILE_MULTI: OnSaveToFile(true); break;
	//case MENU_MAKE_SUBMTL: OnAddNewMultiMaterial(); break;
	}
	
	RefreshSelected();
	if (m_pListener)
		m_pListener->OnBrowserSelectItem(m_pCurrentItem,true);
}


//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::DropToItem( HTREEITEM hItem,HTREEITEM hSrcItem )
{
	m_treeCtrl.Invalidate();
	SItemInfo *pTrgItem = GetItemInfo(hItem);
	SItemInfo *pSrcItem = GetItemInfo(hSrcItem);
	if (pTrgItem && pSrcItem)
	{
		// Get multisub parent.
		HTREEITEM hTrgParent = m_treeCtrl.GetParentItem(hItem);
		SItemInfo *pTrgParentItem = GetItemInfo(hTrgParent);

		HTREEITEM hSrcParent = m_treeCtrl.GetParentItem(hSrcItem);
		SItemInfo *pSrcParentItem = GetItemInfo(hSrcParent);

		_smart_ptr<CMaterial> pTrgMtl = pTrgItem->pMaterial;
		_smart_ptr<CMaterial> pSrcMtl = pSrcItem->pMaterial;
		if (pSrcMtl != NULL && !pSrcMtl->IsMultiSubMaterial())
		{
			if (pTrgParentItem == pSrcParentItem)
			{
				// We are possibly droping withing same multi sub object.
				if (pTrgItem->nSubMtlSlot >= 0 && pSrcItem->nSubMtlSlot >= 0)
				{
					if (pTrgParentItem && pTrgParentItem->pMaterial != NULL && pTrgParentItem->pMaterial->IsMultiSubMaterial())
					{
						// Swap materials.
						_smart_ptr<CMaterial> pParentMultiMaterial = pTrgParentItem->pMaterial;
						int nSrcSlot = pSrcItem->nSubMtlSlot;
						int nTrgSlot = pTrgItem->nSubMtlSlot;
						// This invalidates item infos.
						SetSubMaterial( hTrgParent,nSrcSlot,pTrgMtl,false );
						SetSubMaterial( hTrgParent,nTrgSlot,pSrcMtl,true );
						//pParentMultiMaterial->SetSubMaterial(nSrcSlot,pSrcMtl);
						//pParentMultiMaterial->SetSubMaterial(nTrgSlot,pTrgMtl);
						return;
					}
				}
			}
			else if (!pSrcMtl->IsPureChild())
			{
				// Not in same branch.
				if (pTrgItem->nSubMtlSlot >= 0)
				{
					if (pTrgParentItem && pTrgParentItem->pMaterial != NULL && pTrgParentItem->pMaterial->IsMultiSubMaterial())
					{
						SetSubMaterial( hTrgParent,pTrgItem->nSubMtlSlot,pSrcMtl,true );
						return;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialBrowserCtrl::GetMaterial( HTREEITEM hItem )
{
	SItemInfo *pItemInfo = GetItemInfo(hItem);
	if (!pItemInfo)
		return 0;

	if (pItemInfo->pMaterial)
		return pItemInfo->pMaterial;
	return 0;
	/*

	CMaterial *pMtl;
	if (!pItemInfo->name.IsEmpty())
	{
		pMtl = m_pMatMan->LoadMaterial(pItemInfo->name,false);
	}
	return pMtl;
	*/
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnSelChangedItemTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_bIgnoreSelectionChange)
		return;
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	if (m_treeCtrl)
	{
		HTREEITEM hItem = pNMTreeView->itemNew.hItem;
		SetSelectedItem( hItem );
	}

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::SetSelectedItem( HTREEITEM hItem )
{
	if (m_bIgnoreSelectionChange)
		return;

	m_bIgnoreSelectionChange = true;

	SItemInfo *pItemInfo = GetItemInfo(hItem);

	CMaterial* pMtl = NULL;
	if (pItemInfo)
	{
		pMtl = pItemInfo->pMaterial;
		/*
		if (pMtl == NULL && !pItemInfo->name.IsEmpty())
		{
			pMtl = m_pMatMan->LoadMaterial(pItemInfo->name,false);
		}
		*/
	}

	m_pCurrentItem = pMtl;
	m_hCurrentItem = hItem;
	m_treeCtrl.SelectItem(hItem);
	m_treeCtrl.EnsureVisible(hItem);

	if (m_pListener)
		m_pListener->OnBrowserSelectItem(pMtl,false);

	RefreshSelected();

	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::SelectItem( IDataBaseItem *pItem )
{
	if (m_bIgnoreSelectionChange)
		return;

	m_bIgnoreSelectionChange = true;

	CMaterial *pNewItem = (CMaterial*)pItem;
	m_pCurrentItem = pNewItem;

	if (m_pCurrentItem)
	{
		SItemInfo *pFirstFound = 0;
		// First search in not subitems.
		for (Items::iterator it = m_items.begin(); it != m_items.end(); ++it)
		{
			SItemInfo *pItemInfo = *it;
			if (pItemInfo->pMaterial == pItem)
			{
				if (!pFirstFound)
					pFirstFound = pItemInfo;
				if (pItemInfo->nSubMtlSlot < 0)
				{
					// This is the best item we can find.
					pFirstFound = pItemInfo;
					break;
				}
			}
		}

		if (pFirstFound)
		{
			m_treeCtrl.SelectItem(pFirstFound->hItem);
			m_treeCtrl.EnsureVisible(pFirstFound->hItem);
			if (m_pListener)
				m_pListener->OnBrowserSelectItem( pFirstFound->pMaterial,false );
		}
	}
	else
	{
		m_treeCtrl.SelectItem(NULL);
	}
	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnClone()
{
	CMaterial *pSrcMtl = m_pCurrentItem;
	if (pSrcMtl != 0 && !pSrcMtl->IsPureChild())
	{
		XmlNodeRef node = CreateXmlNode( "Material" );
		CBaseLibraryItem::SerializeContext ctx( node,false );
		ctx.bCopyPaste = true;
		pSrcMtl->Serialize( ctx );

		CString name = m_pMatMan->MakeUniqItemName( pSrcMtl->GetName() );
		// Create a new material.
		_smart_ptr<CMaterial> pMtl = m_pMatMan->CreateMaterial( name,node,pSrcMtl->GetFlags() );
		if (pMtl)
		{
			pMtl->Save();
			SelectItem( pMtl );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnCut()
{
	if (m_hCurrentItem)
	{
		OnCopy();
		DeleteItem(m_hCurrentItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnCopyName()
{
	CMaterial *pMtl = m_pCurrentItem;
	if (pMtl)
	{
		CClipboard clipboard;
		clipboard.PutString( pMtl->GetName(),"Material Name" );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnCopy()
{
	CMaterial *pMtl = m_pCurrentItem;
	if (pMtl)
	{
		CClipboard clipboard;
		XmlNodeRef node = CreateXmlNode( "Material" );
		node->setAttr("Name",pMtl->GetName());
		CBaseLibraryItem::SerializeContext ctx( node,false );
		ctx.bCopyPaste = true;
		pMtl->Serialize( ctx );
		clipboard.Put( node );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialBrowserCtrl::CanPaste()
{
	CClipboard clipboard;
	if (clipboard.IsEmpty())
		return false;
	XmlNodeRef node = clipboard.Get();
	if (!node)
		return false;

	if (strcmp(node->getTag(),"Material") == 0)
	{
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnPaste()
{
	CClipboard clipboard;
	if (clipboard.IsEmpty())
		return;
	XmlNodeRef node = clipboard.Get();
	if (!node)
		return;

	if (strcmp(node->getTag(),"Material") == 0)
	{
		GetIEditor()->ExecuteCommand( "Material.Create" );
		CMaterial *pMtl = m_pMatMan->GetCurrentMaterial();
		if (pMtl)
		{
			// This is material node.
			CBaseLibraryItem::SerializeContext serCtx( node,true );
			serCtx.bCopyPaste = true;
			serCtx.bUniqName = true;
			pMtl->Serialize( serCtx );

			SelectItem( pMtl );
			pMtl->Save();
			pMtl->Reload();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnAddNewMaterial()
{
	GetIEditor()->ExecuteCommand( "Material.Create" );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnAddNewMultiMaterial()
{
	GetIEditor()->ExecuteCommand( "Material.CreateMulti" );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::DeleteItem()
{
	DeleteItem(m_hCurrentItem);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::DeleteItem( HTREEITEM hItem )
{
	SItemInfo *pItemInfo = GetItemInfo(hItem);
	if (pItemInfo)
	{
		CMaterial *pMtl = pItemInfo->pMaterial;
		if(pMtl)
		{
			CSourceControlDescDlg dlg;
			if(!(pMtl->GetFileAttributes() & SCC_FILE_ATTRIBUTE_MANAGED) || dlg.DoModal()==IDOK)
			{
				if (pMtl->GetFileAttributes() & SCC_FILE_ATTRIBUTE_MANAGED)
				{
					if(!GetIEditor()->GetSourceControl()->Delete(pMtl->GetFilename(), dlg.m_sDesc))
					{
						MessageBox("Could not delete file from source control!", "Error", MB_OK | MB_ICONERROR);
					}
				}
				if (pItemInfo->nSubMtlSlot!=-1)
					OnClearSubMtlSlot(hItem);
				else
					GetIEditor()->ExecuteCommand("Material.Delete");
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnRenameItem()
{
	CMaterial *pMtl = m_pCurrentItem;
	if (!pMtl)
		return;

	if (pMtl->IsPureChild())
	{
		CStringDlg dlg("Enter New Material Name",this);
		dlg.SetString( pMtl->GetName() );
		if (dlg.DoModal() == IDOK)
		{
			pMtl->SetName( dlg.GetString() );
			pMtl->Save();
			pMtl->Reload();
		}
	}
	else
	{
		CString startPath = GetIEditor()->GetSearchPath(EDITOR_PATH_MATERIALS);
		if (pMtl)
			startPath = Path::GetPath( pMtl->GetFilename() );

		CString filename;
		if (!CFileUtil::SelectSaveFile( "Material Files (*.mtl)|*.mtl","mtl",startPath,filename ))
		{
			return;
		}
		CString itemName = m_pMatMan->FilenameToMaterial( Path::GetRelativePath(filename) );
		if (itemName.IsEmpty())
			return;

		if (m_pMatMan->FindItemByName( itemName ))
		{
			Warning( "Material with name %s already exist",(const char*)itemName );
			return;
		}

		if (pMtl->GetFileAttributes() & SCC_FILE_ATTRIBUTE_MANAGED)
		{
			if(!GetIEditor()->GetSourceControl()->Rename( pMtl->GetFilename(),filename, "Rename" ))
			{
				MessageBox("Could not rename file in Source Control.", "Error", MB_OK | MB_ICONERROR);
			}
		}

		// Delete file on disk.
		if (!pMtl->GetFilename().IsEmpty())
		{
			::DeleteFile( pMtl->GetFilename() );
		}
		pMtl->SetName( itemName );
		pMtl->Save();
		OnDataBaseItemEvent( pMtl,EDB_ITEM_EVENT_DELETE );
		OnDataBaseItemEvent( pMtl,EDB_ITEM_EVENT_ADD );
		SelectItem( pMtl );
	}
	/*
	CMaterial * pMat = GetIEditor()->GetMaterialManager()->GetCurrentMaterial();
	if(pMat)
	{
		if(!GetIEditor()->GetSourceControl()->Rename(pMat->GetFilename(), "Newname"))
		{
			MessageBox("Operation could not be completed.", "Error", MB_OK | MB_ICONERROR);
		}
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnTreeKeyDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	GetAsyncKeyState(VK_CONTROL);
	bool bCtrl = GetAsyncKeyState(VK_CONTROL) != 0;
	// Key press in items tree view.
	NMTVKEYDOWN *nm = (NMTVKEYDOWN*)pNMHDR;
	if (bCtrl && (nm->wVKey == 'c' || nm->wVKey == 'C'))
	{
		OnCopy();	// Ctrl+C
	}
	if (bCtrl && (nm->wVKey == 'v' || nm->wVKey == 'V'))
	{
		OnPaste(); // Ctrl+V
	}
	if (bCtrl && (nm->wVKey == 'x' || nm->wVKey == 'X'))
	{
		OnCut(); // Ctrl+X
	}
	if (bCtrl && (nm->wVKey == 'd' || nm->wVKey == 'D'))
	{
		OnClone(); // Ctrl+D
	}
	if (nm->wVKey == VK_DELETE)
	{
		DeleteItem(m_hCurrentItem);
	}
	if (nm->wVKey == VK_F2)
	{
		OnRenameItem();
	}
	if (nm->wVKey == VK_INSERT)
	{
		//OnAddItem();
	}
	if (nm->wVKey == VK_F2)
	{
		OnRenameItem();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnSetSubMtlCount( HTREEITEM hItem )
{
	CMaterial *pMtl = GetMaterial(hItem);
	if (!pMtl)
		return;
	
	if (!pMtl->IsMultiSubMaterial())
		return;

	int num = pMtl->GetSubMaterialCount();
	CNumberDlg dlg(0,num,"Number of Sub Materials");
  dlg.SetRange(0.0, (float) MAX_SUB_MATERIALS);
  dlg.SetInteger(true);
	if (dlg.DoModal() == IDOK)
	{
		num = dlg.GetValue();
		if (num != pMtl->GetSubMaterialCount())
		{
			CUndo undo( "Set SubMtl Count" );
			pMtl->SetSubMaterialCount(num);

			for (int i = 0; i < num; i++)
			{
				if (pMtl->GetSubMaterial(i) == 0)
				{
					// Allocate pure childs for all empty slots.
					CString name;
					name.Format("SubMtl%d",i+1);
					_smart_ptr<CMaterial> pChild = new CMaterial( m_pMatMan,name,MTL_FLAG_PURE_CHILD );
					pMtl->SetSubMaterial(i,pChild);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::DoSourceControlOp( HTREEITEM hItem,ESourceControlOp scmOp )
{
	if (!hItem)
		return;
	
	CMaterial *pMtl = NULL;

	SItemInfo *pItemInfo = GetItemInfo(hItem);
	if (pItemInfo)
		pMtl = pItemInfo->pMaterial;

	if(pMtl && pMtl->IsPureChild())
		pMtl = pMtl->GetParent();

	CString path;
	if (pMtl)
		path = pMtl->GetFilename();
	else
	{
		CString pathName = GetFullItemName( hItem );
		path = Path::AddBackslash( Path::GamePathToFullPath(pathName) );
	}

	if (pMtl && pMtl->IsModified())
		pMtl->Save();


	// Collecting texture pathes
	CString multipath;
	bool bFirst = true;
	if(pMtl)
	{
		SInputShaderResources sr = pMtl->GetShaderResources();
		for (int i = 0; i < EFTT_MAX; i++)
		{
			if(strlen(sr.m_Textures[i].m_Name)>0)
			{
				CString name = sr.m_Textures[i].m_Name;
				CString ext = name.Right(4);
				if(!stricmp(ext, ".tif"))
				{
					CString path = Path::GamePathToFullPath(name);
					if(!path.IsEmpty())
					{
						if(bFirst)
							bFirst=false;
						else
							multipath.Append(";");
						multipath.Append(path);
					}
				}
			}
		}
		if(pMtl->IsMultiSubMaterial())
		{
			for(int i=0; i<pMtl->GetSubMaterialCount(); i++)
			{
				CMaterial * pSubMtl = pMtl->GetSubMaterial(i);
				if(pSubMtl)
				{
					SInputShaderResources sr = pSubMtl->GetShaderResources();
					for (int i = 0; i < EFTT_MAX; i++)
					{
						if(strlen(sr.m_Textures[i].m_Name)>0)
						{
							CString name = sr.m_Textures[i].m_Name;
							CString ext = name.Right(4);
							if(!stricmp(ext, ".tif"))
							{
								CString path = Path::GamePathToFullPath(name);
								if(!path.IsEmpty())
								{
									if(bFirst)
										bFirst=false;
									else
										multipath.Append(";");
									multipath.Append(path);
								}
							}
						}
					}
				}
			}
		}
	}

	bool bRes = true;

	switch (scmOp)
	{
	case ESCM_IMPORT:
		{
			CSourceControlDescDlg dlg;
			if(dlg.DoModal()==IDOK)
			{
				if(!multipath.IsEmpty())
					GetIEditor()->GetSourceControl()->Add( multipath, dlg.m_sDesc );
				bRes = GetIEditor()->GetSourceControl()->Add( path, dlg.m_sDesc );
			}
			else
				bRes=true;
		}
		break;
	case ESCM_CHECKIN:
		{
			CSourceControlDescDlg dlg;
			if(dlg.DoModal()==IDOK)
			{
				if(!multipath.IsEmpty())
				{
					GetIEditor()->GetSourceControl()->Add( multipath, dlg.m_sDesc );
					GetIEditor()->GetSourceControl()->CheckIn( multipath, dlg.m_sDesc );
				}
				bRes = GetIEditor()->GetSourceControl()->CheckIn( path, dlg.m_sDesc );
			}
			else
				bRes=true;
		}
		break;
	case ESCM_CHECKOUT:
		bRes = GetIEditor()->GetSourceControl()->CheckOut( path );
		break;
	case ESCM_UNDO_CHECKOUT:
		bRes = GetIEditor()->GetSourceControl()->UndoCheckOut( path );
		break;
	case ESCM_GETLATEST:
		bRes = GetIEditor()->GetSourceControl()->GetLatestVersion( path );
		break;
	}

	if (bRes)
	{
		if (pItemInfo != NULL && pMtl != NULL)
			pMtl->Reload();

		if (pMtl && pMtl->IsMultiSubMaterial())
		{
			ReloadItems(m_viewType);
		}
	}
	else
	{
		MessageBox("Source Control Operation Failed.\r\nCheck if Source Control Provider correctly setup and working directory is correct.", "Error", MB_OK | MB_ICONERROR);
		ReloadItems(m_viewType);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnMakeSubMtlSlot( HTREEITEM hItem )
{
	SItemInfo *pItemInfo = GetItemInfo(hItem);
	if (pItemInfo && pItemInfo->nSubMtlSlot >= 0)
	{
		SItemInfo *pParentInfo = GetItemInfo(m_treeCtrl.GetParentItem(hItem));
		if (pParentInfo && pParentInfo->pMaterial != NULL && pParentInfo->pMaterial->IsMultiSubMaterial())
		{
			CString str;
			str.Format( _T("Making new material will override material currently assigned to the slot %d of %s\r\nMake new sub material?"),
				pItemInfo->nSubMtlSlot,pParentInfo->pMaterial->GetName() );
			if (MessageBox(str,_T("Confirm Override"),MB_YESNO|MB_ICONQUESTION) == IDYES)
			{
				CString name;
				name.Format("SubMtl%d",pItemInfo->nSubMtlSlot+1);
				_smart_ptr<CMaterial> pMtl = new CMaterial( m_pMatMan,name,MTL_FLAG_PURE_CHILD );
				pParentInfo->pMaterial->SetSubMaterial(pItemInfo->nSubMtlSlot,pMtl);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnClearSubMtlSlot( HTREEITEM hItem )
{
	SItemInfo *pItemInfo = GetItemInfo(hItem);
	if (pItemInfo && pItemInfo->nSubMtlSlot >= 0 && pItemInfo->pMaterial != NULL)
	{
		SItemInfo *pParentInfo = GetItemInfo(m_treeCtrl.GetParentItem(hItem));
		if (pParentInfo && pParentInfo->pMaterial != NULL && pParentInfo->pMaterial->IsMultiSubMaterial())
		{
			CString str;
			str.Format( _T("Clear Sub-Material Slot %d of %s?"),pItemInfo->nSubMtlSlot,pParentInfo->pMaterial->GetName() );
			if (MessageBox(str,_T("Clear Sub-Material"),MB_YESNO|MB_ICONQUESTION) == IDYES)
			{
				CUndo undo("Material Change");
				SetSubMaterial( pParentInfo->hItem,pItemInfo->nSubMtlSlot,0,true );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::SetSubMaterial( HTREEITEM hMultiSubMtl,int nSlot,CMaterial *pSubMaterial,bool bSelectSubMtl )
{
	SItemInfo *pItemInfo = GetItemInfo(hMultiSubMtl);
	if (pItemInfo && pItemInfo->pMaterial != NULL && pItemInfo->pMaterial->IsMultiSubMaterial())
	{
		_smart_ptr<CMaterial> pMultiMat = pItemInfo->pMaterial;
		pMultiMat->SetSubMaterial(nSlot,pSubMaterial);

		int i = 0;
		HTREEITEM hChildItem = m_treeCtrl.GetChildItem(hMultiSubMtl);
		while (hChildItem != NULL)
		{
			if (i == nSlot)
			{
				SetSelectedItem(hChildItem);
				break;
			}
			hChildItem = m_treeCtrl.GetNextItem(hChildItem, TVGN_NEXT);
			i++;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::UpdateItemState( HTREEITEM hItem )
{
	SItemInfo *pItem = GetItemInfo(hItem);
	if (pItem)
	{
		uint32 attr = 0;
		bool bFromCGF = false;
		if (pItem->pMaterial != NULL)
		{
			attr = pItem->pMaterial->GetFileAttributes();
			bFromCGF = pItem->pMaterial->IsFromEngine();
		}
		SetItemStateFromFileAttributes( hItem,attr,bFromCGF );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnDataBaseItemEvent( IDataBaseItem *pItem,EDataBaseItemEvent event )
{
	if (m_bIgnoreSelectionChange)
		return;
	CMaterial *pMtl = (CMaterial*)pItem;
	switch(event)
	{
	case EDB_ITEM_EVENT_ADD:
		if (pMtl && !pMtl->IsPureChild())
			AddItemToTree(pMtl);
		break;
	case EDB_ITEM_EVENT_DELETE:
		{
			Items::iterator next;
			for (Items::iterator it = m_items.begin(); it != m_items.end(); it = next)
			{
				next = it; next++;
				SItemInfo *pItemInfo = *it;
				if (pItemInfo->pMaterial == pItem)
				{
					RemoveItemFromTree(pItemInfo->hItem);
				}
			}
		}
		break;
	case EDB_ITEM_EVENT_CHANGED:
		{
			Items::iterator next;
			for (Items::iterator it = m_items.begin(); it != m_items.end(); it = next)
			{
				next = it; next++;
				SItemInfo *pItemInfo = *it;
				if (pItemInfo->pMaterial == pMtl)
				{
					if (pMtl->IsMultiSubMaterial())
						ReloadTreeSubMtls(pItemInfo->hItem);

					UpdateItemState( pItemInfo->hItem );
					// One time enough.
					break;
				}
			}
			if (pItem == m_pCurrentItem)
			{
				if (pMtl->IsMultiSubMaterial())
					m_pLastActiveMultiMaterial = NULL;
				RefreshSelected();
			}
		}
		break;
	case EDB_ITEM_EVENT_SELECTED:
		{
			SelectItem(pItem);
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::SetImageListCtrl( CMaterialImageListCtrl *pCtrl )
{
	m_pMaterialImageListCtrl = pCtrl;
	if (m_pMaterialImageListCtrl)
		m_pMaterialImageListCtrl->SetSelectMaterialCallback( functor(*this,&CMaterialBrowserCtrl::OnImageListCtrlSelect) );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnImageListCtrlSelect( CImageListCtrlItem *pMtlItem )
{
	int nSlot = (INT_PTR)pMtlItem->pUserData;
	if (nSlot < 0)
	{
		//if (pMtlItem->pMaterial != NULL)
			//SelectItem(pMtlItem->pMaterial);

		return;
	}

	if (!m_hCurrentItem)
		return;

	SItemInfo *pItemInfo = GetItemInfo(m_hCurrentItem);
	if (!pItemInfo)
		return;

	SItemInfo *pParentInfo = 0;
	if (pItemInfo->nSubMtlSlot < 0)
	{
		// Current selected material is not a sub slot.
		pParentInfo = pItemInfo;
	}
	else
	{
		// Current selected material is a sub slot.
		pParentInfo = GetParentItemInfo(m_hCurrentItem);
	}

	if (!pParentInfo)
		return;

	if (pParentInfo->pMaterial == NULL || !pParentInfo->pMaterial->IsMultiSubMaterial())
		return; // must be multi sub material.

	HTREEITEM hChildItem = m_treeCtrl.GetChildItem(pParentInfo->hItem);
	while (hChildItem != NULL)
	{
		SItemInfo *pChildInfo = GetItemInfo(hChildItem);
		if (pChildInfo->nSubMtlSlot == nSlot)
		{
			SetSelectedItem( hChildItem );
			return;
		}
		hChildItem = m_treeCtrl.GetNextItem(hChildItem, TVGN_NEXT);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnSaveToFile( bool bMulti )
{
	if (m_pCurrentItem)	
	{
		CString startPath = GetIEditor()->GetSearchPath(EDITOR_PATH_MATERIALS);
		CString filename;
		if (!CFileUtil::SelectSaveFile( "Material Files (*.mtl)|*.mtl","mtl",startPath,filename ))
		{
			return;
		}
		filename = Path::GetRelativePath(filename);
		CString itemName = Path::RemoveExtension(filename);
		itemName = Path::MakeGamePath(itemName);

		if (m_pMatMan->FindItemByName( itemName ))
		{
			Warning( "Material with name %s already exist",(const char*)itemName );
			return;
		}
		int flags = m_pCurrentItem->GetFlags();
		if (bMulti)
			flags |= MTL_FLAG_MULTI_SUBMTL;

		m_pCurrentItem->SetFlags( flags );

		if (m_pCurrentItem->IsDummy())
		{
			m_pCurrentItem->ClearMatInfo();
			m_pCurrentItem->SetDummy( false );
		}
		m_pCurrentItem->SetModified(true);
		m_pCurrentItem->Save();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::RefreshSelected()
{
	if (!m_hCurrentItem)
		return;

	HTREEITEM hItem = m_hCurrentItem;

	UpdateItemState( hItem );

	SItemInfo *pItemInfo = GetItemInfo(hItem);
	CMaterial *pMtl = NULL;

	if (pItemInfo)
		pMtl = pItemInfo->pMaterial;

	if (m_pMaterialImageListCtrl)
	{
		m_pMaterialImageListCtrl->InvalidateMaterial(pMtl);
		if (pMtl || (pItemInfo && pItemInfo->nSubMtlSlot >= 0))
		{
			if (pItemInfo->nSubMtlSlot < 0)
			{
				// Not sub item.
				if (pMtl && pMtl->IsMultiSubMaterial())
				{
					if (m_pLastActiveMultiMaterial != pMtl)
					{
						m_pLastActiveMultiMaterial = pMtl;
						m_pMaterialImageListCtrl->DeleteAllItems();
						for (int i = 0; i < pMtl->GetSubMaterialCount(); i++)
						{
							m_pMaterialImageListCtrl->AddMaterial( pMtl->GetSubMaterial(i),(void*)i );
						}
						m_pMaterialImageListCtrl->ResetSelection();
					}
				}
				else
				{
					// Normal material.
					m_pLastActiveMultiMaterial = NULL;
					m_pMaterialImageListCtrl->DeleteAllItems();
					m_pMaterialImageListCtrl->AddMaterial( pMtl,(void*)-1 );
					m_pMaterialImageListCtrl->SelectMaterial( pMtl );
				}
			}
			else
			{
				// Sub item, get parent material.
				SItemInfo *pParentInfo = GetItemInfo( m_treeCtrl.GetParentItem(hItem) );
				if (pParentInfo)
				{
					CMaterial *pMultiMtl = pParentInfo->pMaterial;
					assert( pMultiMtl );

					if (m_pLastActiveMultiMaterial != pMultiMtl)
					{
						m_pLastActiveMultiMaterial = pMultiMtl;
						m_pMaterialImageListCtrl->DeleteAllItems();
						for (int i = 0; i < pMultiMtl->GetSubMaterialCount(); i++)
						{
							m_pMaterialImageListCtrl->AddMaterial( pMultiMtl->GetSubMaterial(i),(void*)i );
						}
					}
				}
				m_pMaterialImageListCtrl->SelectItem( pItemInfo->nSubMtlSlot );
			}
		}
		else
		{
			m_pMaterialImageListCtrl->DeleteAllItems();
			m_pLastActiveMultiMaterial = NULL;
		}
	}
}