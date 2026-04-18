////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   BaseLibraryDialog.cpp
//  Version:     v1.00
//  Created:     22/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BaseLibraryDialog.h"

#include "StringDlg.h"

#include "BaseLibrary.h"
#include "BaseLibraryItem.h"
#include "BaseLibraryManager.h"
#include "Clipboard.h"
#include "ErrorReport.h"

IMPLEMENT_DYNAMIC(CBaseLibraryDialog,CToolbarDialog)

//////////////////////////////////////////////////////////////////////////
// CBaseLibraryDialog implementation.
//////////////////////////////////////////////////////////////////////////
CBaseLibraryDialog::CBaseLibraryDialog( UINT nID,CWnd *pParent )
	: CDataBaseDialogPage(nID, pParent)
{
	m_sortRecursionType = SORT_RECURSION_FULL;

	m_bIgnoreSelectionChange = false;

	m_pItemManager = 0;

	// Load cusrors.
	m_hCursorDefault = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	m_hCursorNoDrop = AfxGetApp()->LoadCursor(IDC_NODROP);
	m_hCursorCreate = AfxGetApp()->LoadCursor(IDC_HIT_CURSOR);
	m_hCursorReplace = AfxGetApp()->LoadCursor(IDC_HAND_INTERNAL);
	m_bLibsLoaded = false;

	GetIEditor()->RegisterNotifyListener(this);
}

CBaseLibraryDialog::~CBaseLibraryDialog()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

void CBaseLibraryDialog::DoDataExchange(CDataExchange* pDX)
{
	CToolbarDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CBaseLibraryDialog, CToolbarDialog)
	ON_COMMAND( ID_DB_ADDLIB,OnAddLibrary )
	ON_COMMAND( ID_DB_DELLIB,OnRemoveLibrary )
	ON_COMMAND( ID_DB_REMOVE,OnRemoveItem )
	ON_COMMAND( ID_DB_RENAME,OnRenameItem )
	ON_COMMAND( ID_DB_SAVE,OnSave )
	ON_COMMAND( ID_DB_EXPORTLIBRARY,OnExportLibrary )
	ON_COMMAND( ID_DB_LOADLIB,OnLoadLibrary )
	ON_COMMAND( ID_DB_RELOAD,OnReloadLib )
	ON_COMMAND( ID_DB_RELOADLIB,OnReloadLib )

	ON_CBN_SELENDOK( ID_DB_LIBRARY,OnChangedLibrary )
	ON_NOTIFY(TVN_SELCHANGED, IDC_LIBRARY_ITEMS_TREE, OnSelChangedItemTree)
	ON_NOTIFY(TVN_KEYDOWN, IDC_LIBRARY_ITEMS_TREE, OnKeyDownItemTree)

	ON_COMMAND( ID_DB_CUT,OnCut )
	ON_COMMAND( ID_DB_COPY,OnCopy )
	ON_COMMAND( ID_DB_PASTE,OnPaste )
	ON_COMMAND( ID_DB_CLONE,OnClone )
	ON_UPDATE_COMMAND_UI( ID_DB_COPY,OnUpdateSelected )
	ON_UPDATE_COMMAND_UI( ID_DB_CUT,OnUpdateSelected )
	ON_UPDATE_COMMAND_UI( ID_DB_CLONE,OnUpdateSelected )
	ON_UPDATE_COMMAND_UI( ID_DB_PASTE,OnUpdatePaste )

	ON_WM_SIZE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnDestroy()
{
	SelectItem( 0 );
	m_pLibrary = 0;
	m_pCurrentItem = 0;
	m_itemsToTree.clear();

	CToolbarDialog::OnDestroy();
}

// CTVSelectKeyDialog message handlers
BOOL CBaseLibraryDialog::OnInitDialog()
{
	// Initialize command bars.
	VERIFY(InitCommandBars());
	GetCommandBars()->GetCommandBarsOptions()->bShowExpandButtonAlways = FALSE;
	GetCommandBars()->EnableCustomization(FALSE);

	__super::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::InitLibraryToolbar()
{
	CRect rc;
	GetClientRect(rc);
	// Create Library toolbar.
	CXTPToolBar *pLibToolBar = GetCommandBars()->Add( _T("ToolBar"),xtpBarTop );
	//pLibToolBar->EnableCustomization(FALSE);
	VERIFY(pLibToolBar->LoadToolBar( IDR_DB_LIBRARY_BAR ));

	// Create library control.
	{
		CXTPControl *pCtrl = pLibToolBar->GetControls()->FindControl(xtpControlButton, ID_DB_LIBRARY, TRUE, FALSE);
		if (pCtrl)
		{
			CRect rc(0,0,150,16);
			m_libraryCtrl.Create( WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|CBS_SORT,rc,this,ID_DB_LIBRARY );
			//m_libraryCtrl.SetParent( this );
			//m_libraryCtrl.SetFont( CFont::FromHandle((HFONT)gSettings.gui.hSystemFont) );

			CXTPControlCustom* pCustomCtrl = (CXTPControlCustom*)pLibToolBar->GetControls()->SetControlType(pCtrl->GetIndex(),xtpControlCustom);
			pCustomCtrl->SetSize( CSize(150,16) );
			pCustomCtrl->SetControl( &m_libraryCtrl );
			pCustomCtrl->SetFlags(xtpFlagManualUpdate);
		}
	}


	//////////////////////////////////////////////////////////////////////////
	// Standard toolbar.
	CXTPToolBar *pStdToolBar = GetCommandBars()->Add( _T("StandartToolBar"),xtpBarTop );
	pStdToolBar->EnableCustomization(FALSE);
	VERIFY(pStdToolBar->LoadToolBar( IDR_DB_STANDART ));
	DockRightOf( pStdToolBar,pLibToolBar );

	//////////////////////////////////////////////////////////////////////////
	// Create Item toolbar.
	CXTPToolBar *pItemToolBar = GetCommandBars()->Add( _T("ItemToolBar"),xtpBarTop );
	pItemToolBar->EnableCustomization(FALSE);
	VERIFY(pItemToolBar->LoadToolBar( IDR_DB_LIBRARY_ITEM_BAR ));
	DockRightOf( pItemToolBar,pLibToolBar );
	//////////////////////////////////////////////////////////////////////////

}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::InitItemToolbar()
{
}

//////////////////////////////////////////////////////////////////////////
// Create the toolbar
void CBaseLibraryDialog::InitToolbar( UINT nToolbarResID )
{
	// Tool bar must be already created in derived class.
	ASSERT( m_toolbar.m_hWnd );

	// Resize the toolbar
	CRect rc;
	GetClientRect(rc);
	m_toolbar.SetWindowPos(NULL, 0, 0, rc.right, 70, SWP_NOZORDER);
	CSize sz = m_toolbar.CalcDynamicLayout(TRUE,TRUE);

	//////////////////////////////////////////////////////////////////////////
	int index;
	index = m_toolbar.CommandToIndex(ID_DB_LIBRARY);
	if (index >= 0)
	{
		m_toolbar.SetButtonInfo(index,ID_DB_LIBRARY, TBBS_SEPARATOR, 150);
		m_toolbar.GetItemRect(index,&rc);
		rc.top++;
		rc.bottom += 400;
	}
	m_libraryCtrl.Create( WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|CBS_SORT,rc,this,ID_DB_LIBRARY );
	m_libraryCtrl.SetParent( &m_toolbar );
	m_libraryCtrl.SetFont( CFont::FromHandle((HFONT)gSettings.gui.hSystemFont) );
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnSize(UINT nType, int cx, int cy)
{
	CToolbarDialog::OnSize(nType, cx, cy);
	
	// resize splitter window.
	if (m_toolbar.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		//m_toolbar.SetWindowPos(NULL, 0, 0, rc.right, 70, SWP_NOZORDER);
		m_toolbar.MoveWindow(0, 0, rc.right, 70, FALSE);
		RecalcLayout();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
	case eNotify_OnBeginNewScene:
		m_bLibsLoaded = false;
		// Clear all prototypes and libraries.
		SelectItem(0);
		if (m_libraryCtrl)
			m_libraryCtrl.ResetContent();
		m_selectedLib = "";
		break;

	case eNotify_OnEndSceneOpen:
		m_bLibsLoaded = false;
		ReloadLibs();
		break;

	case eNotify_OnCloseScene:
		m_bLibsLoaded = false;
		SelectLibrary( "" );
		SelectItem( 0 );
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CBaseLibraryDialog::FindLibrary( const CString &libraryName )
{
	return (CBaseLibrary*)m_pItemManager->FindLibrary( libraryName );
}

CBaseLibrary* CBaseLibraryDialog::NewLibrary( const CString &libraryName )
{
	return (CBaseLibrary*)m_pItemManager->AddLibrary( libraryName, true );
}

void CBaseLibraryDialog::DeleteLibrary( CBaseLibrary *pLibrary )
{
	m_pItemManager->DeleteLibrary( pLibrary->GetName() );
}

void CBaseLibraryDialog::DeleteItem( CBaseLibraryItem *pItem )
{
	m_pItemManager->DeleteItem( pItem );
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::ReloadLibs()
{
	if (!m_pItemManager)
		return;

	SelectItem(0);

	CString selectedLib;
	if (m_libraryCtrl)
		m_libraryCtrl.ResetContent();
	bool bFound = false;
	for (int i = 0; i < m_pItemManager->GetLibraryCount(); i++)
	{
		CString library = m_pItemManager->GetLibrary(i)->GetName();
		if (selectedLib.IsEmpty())
			selectedLib = library;
		if (m_libraryCtrl)
			m_libraryCtrl.AddString( library );
	}
	m_selectedLib = "";
	SelectLibrary( selectedLib );
	m_bLibsLoaded = true;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::ReloadItems()
{
	m_bIgnoreSelectionChange = true;
	m_treeCtrl.SetRedraw(FALSE);
	m_selectedGroup = "";
	m_pCurrentItem = 0;
	if (m_pItemManager)
		m_pItemManager->SetSelectedItem(0);
	m_itemsToTree.clear();
	m_treeCtrl.DeleteAllItems();
	m_bIgnoreSelectionChange = false;

	if (!m_pLibrary)
	{
		m_treeCtrl.SetRedraw(TRUE);
		return;
	}

	CString filter = "";
	bool bFilter = !filter.IsEmpty();

	HTREEITEM hLibItem = TVI_ROOT;

	m_bIgnoreSelectionChange = true;

	std::map<CString,HTREEITEM,stl::less_stricmp<CString> > items;
	for (int i = 0; i < m_pLibrary->GetItemCount(); i++)
	{
		CBaseLibraryItem *pItem = (CBaseLibraryItem*)m_pLibrary->GetItem(i);
		CString name = pItem->GetName();

		if (bFilter)
		{
			if (strstri(name,filter) == 0)
				continue;
		}

		HTREEITEM hRoot = hLibItem;
		char *token;
		char itempath[1024];
		strcpy( itempath,name );

		token = strtok( itempath,"\\/." );

		CString itemName;
		while (token)
		{
			CString strToken = token;

			token = strtok( NULL,"\\/." );
			if (!token)
				break;

			itemName += strToken+"/";

			HTREEITEM hParentItem = stl::find_in_map(items,itemName,0);
			if (!hParentItem)
			{
				hRoot = m_treeCtrl.InsertItem(strToken, 0, 1, hRoot );
				items[itemName] = hRoot;
			}
			else
				hRoot = hParentItem;
		}

		HTREEITEM hNewItem = InsertItemToTree( pItem,hRoot );
	}

	SortItems(m_sortRecursionType);
	m_treeCtrl.Expand( hLibItem,TVE_EXPAND );
	m_treeCtrl.SetRedraw(TRUE);

	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CBaseLibraryDialog::InsertItemToTree( CBaseLibraryItem *pItem,HTREEITEM hParent )
{
	assert( pItem );
	HTREEITEM hItem = m_treeCtrl.InsertItem( pItem->GetShortName(),2,3,hParent );
	//m_treeCtrl.SetItemState( hItem, TVIS_BOLD,TVIS_BOLD );
	m_treeCtrl.SetItemData( hItem,(DWORD_PTR)pItem );
	m_itemsToTree[pItem] = hItem;
	return hItem;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::SelectLibrary( const CString &library )
{
	CWaitCursor wait;
	if (m_selectedLib != library)
	{
		SelectItem(0);
		m_pLibrary = FindLibrary( library );
		if (m_pLibrary)
		{
			m_selectedLib = library;
		}
		else
		{
			m_selectedLib = "";
		}
		ReloadItems();
	}
	if (m_libraryCtrl)
		m_libraryCtrl.SelectString( -1,m_selectedLib );
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnChangedLibrary()
{
	CString library;
	if (m_libraryCtrl)
		m_libraryCtrl.GetWindowText(library);
	if (library != m_selectedLib)
	{
		SelectLibrary( library );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnAddLibrary()
{
	CStringDlg dlg( _T("New Library Name"),this );
	if (dlg.DoModal() == IDOK)
	{
		if (!dlg.GetString().IsEmpty())
		{
			SelectItem(0);
			// Make new library.
			CString library = dlg.GetString();
			NewLibrary( library );
			ReloadLibs();
			SelectLibrary( library );
			GetIEditor()->SetModifiedFlag();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnExportLibrary()
{
	if (!m_pLibrary)
		return;

	CString filename;
	if (CFileUtil::SelectSaveFile( "Library XML Files (*.xml)|*.xml","xml",Path::GetGameFolder()+"/Materials",filename ))
	{
		XmlNodeRef libNode = CreateXmlNode( "MaterialLibrary" );
		m_pLibrary->Serialize( libNode,false );
		SaveXmlNode( libNode,filename );
	}
}

bool CBaseLibraryDialog::SetItemName( CBaseLibraryItem *item,const CString &groupName,const CString &itemName )
{
	assert( item );
	// Make prototype name.
	CString name;
	if (!groupName.IsEmpty())
		name = groupName + ".";
	name += itemName;
	CString fullName = name;
	if (item->GetLibrary())
		fullName = item->GetLibrary()->GetName() + "." + name;
	IDataBaseItem *pOtherItem = m_pItemManager->FindItemByName( fullName );
	if (pOtherItem && pOtherItem != item)
	{
		// Ensure uniqness of name.
		Warning( "Duplicate Item Name %s",(const char*)name );
		return false;
	}
	else
	{
		item->SetName( name );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnAddItem()
{
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnRemoveItem()
{
	if (m_pCurrentItem)
	{
		// Remove prototype from prototype manager and library.
		CString str;
		str.Format( _T("Delete %s?"),(const char*)m_pCurrentItem->GetName() );
		if (MessageBox(str,_T("Delete Confirmation"),MB_YESNO|MB_ICONQUESTION) == IDYES)
		{
			TSmartPtr<CBaseLibraryItem> pCurrent = m_pCurrentItem;
			DeleteItem( pCurrent );
			HTREEITEM hItem = stl::find_in_map( m_itemsToTree,pCurrent,(HTREEITEM)0 );
			if (hItem)
			{
				m_treeCtrl.DeleteItem( hItem );
				m_itemsToTree.erase( pCurrent );
			}
			GetIEditor()->SetModifiedFlag();
			SelectItem(0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnRenameItem()
{
	if (m_pCurrentItem)
	{
		CStringGroupDlg dlg;
		dlg.SetGroup( m_pCurrentItem->GetGroupName() );
		dlg.SetString( m_pCurrentItem->GetShortName() );
		if (dlg.DoModal() == IDOK)
		{
			TSmartPtr<CBaseLibraryItem> curItem = m_pCurrentItem;
			SetItemName( curItem,dlg.GetGroup(),dlg.GetString() );
			ReloadItems();
			SelectItem( curItem,true );
			curItem->SetModified();
			//m_pCurrentItem->Update();
		}
		GetIEditor()->SetModifiedFlag();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnRemoveLibrary()
{
	CString library = m_selectedLib;
	if (library.IsEmpty())
		return;
	if (m_pLibrary->IsModified())
	{
		CString ask;
		ask.Format( "Save changes to the library %s?",(const char*)library );
		if (AfxMessageBox( ask,MB_YESNO|MB_ICONQUESTION ) == IDYES)
		{
			OnSave();
		}
	}
	CString ask;
	ask.Format( "When removing library All items contained in this library will be deleted.\r\nAre you sure you want to remove libarary %s?\r\n(Note: Library file will not be deleted from the disk)",(const char*)library );
	if (AfxMessageBox( ask,MB_YESNO|MB_ICONQUESTION ) == IDYES)
	{
		SelectItem(0);
		DeleteLibrary( m_pLibrary );
		m_selectedLib = "";
		m_pLibrary = 0;
		ReloadLibs();
		GetIEditor()->SetModifiedFlag();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnSelChangedItemTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_bIgnoreSelectionChange)
		return;
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	bool bSelected = false;
	m_pCurrentItem = 0;
	if (m_treeCtrl)
	{
		HTREEITEM hItem = pNMTreeView->itemNew.hItem;
		if (hItem != 0 && hItem != TVI_ROOT)
		{
			// Change currently selected item.
			CBaseLibraryItem *prot = (CBaseLibraryItem*)m_treeCtrl.GetItemData(hItem);
			if (prot)
			{
				SelectItem( prot );
				bSelected = true;
			}
			else
			{
				m_selectedGroup = m_treeCtrl.GetItemText(hItem);
			}
		}
	}
	if (!bSelected)
		SelectItem( 0 );
	
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnKeyDownItemTree(NMHDR* pNMHDR, LRESULT* pResult)
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
		OnRemoveItem();
	}
	if (nm->wVKey == VK_INSERT)
	{
		OnAddItem();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBaseLibraryDialog::CanSelectItem( CBaseLibraryItem *pItem )
{
	assert( pItem );
	// Check if this item is in dialogs manager.
	if (m_pItemManager->FindItem(pItem->GetGUID()) == pItem)
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::SelectItem( CBaseLibraryItem *item,bool bForceReload )
{
	if (item == m_pCurrentItem && !bForceReload)
		return;

	if (item)
	{
		// Selecting item from different library.
		if (item->GetLibrary() != m_pLibrary && item->GetLibrary())
		{
			// Select library first.
			SelectLibrary( item->GetLibrary()->GetName() );
		}
	}

	m_pCurrentItem = item;

	if (item)
	{
		m_selectedGroup = item->GetGroupName();
	}
	else
		m_selectedGroup = "";

	m_pCurrentItem = item;

	// Set item visible.
	HTREEITEM hItem = stl::find_in_map( m_itemsToTree,item,(HTREEITEM)0 );
	if (hItem)
	{
		m_treeCtrl.SelectItem(hItem);
		m_treeCtrl.EnsureVisible(hItem);
	}

	// [MichaelS 17/4/2007] Select this item in the manager so that other systems
	// can find out the selected item without having to access the ui directly.
	m_pItemManager->SetSelectedItem(item);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::Update()
{
	// do nothing here.
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnSave()
{
	m_pItemManager->SaveAllLibs();
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::Reload()
{
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::SetActive( bool bActive )
{
	if (bActive && !m_bLibsLoaded)
	{
		ReloadLibs();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnUpdateSelected( CCmdUI* pCmdUI )
{
	if (m_pCurrentItem)
		pCmdUI->Enable( TRUE );
	else
		pCmdUI->Enable( FALSE );
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnUpdatePaste( CCmdUI* pCmdUI )
{
	CClipboard clipboard;
	if (clipboard.IsEmpty())
		pCmdUI->Enable( FALSE );
	else
		pCmdUI->Enable( TRUE );
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnClone()
{
	OnCopy();
	OnPaste();
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnCut()
{
	if (m_pCurrentItem)
	{
		OnCopy();
		OnRemoveItem();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnReloadLib()
{
	if (!m_pLibrary)
		return;

	CString libname = m_pLibrary->GetName();
	CString file = m_pLibrary->GetFilename();
	if (m_pLibrary->IsModified())
	{
		CString str;
		str.Format( "Layer %s was modified.\nReloading layer will discard all modifications to this library!",
			(const char*)libname );
		if (AfxMessageBox( str,MB_OKCANCEL|MB_ICONQUESTION ) != IDOK)
			return;
	}

	m_pItemManager->DeleteLibrary( libname );
	m_pItemManager->LoadLibrary( file );
	ReloadLibs();
	SelectLibrary( libname );
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::OnLoadLibrary()
{
	assert(m_pItemManager);

	CErrorsRecorder errorRecorder;

	// Load new material library.
	CString file;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_ANY,file,"Library XML Files (*.xml)|*.xml",m_pItemManager->GetLibsPath() ))
	{
		if (!file.IsEmpty())
		{
			IDataBaseLibrary *matLib = m_pItemManager->LoadLibrary( file );
			ReloadLibs();
			if (matLib)
				SelectLibrary( matLib->GetName() );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CString CBaseLibraryDialog::GetItemGroupName( CTreeCtrl &tree,HTREEITEM hItem )
{
	CString name = tree.GetItemText(hItem);
	HTREEITEM hParent = hItem;
	while (hParent)
	{
		hParent = tree.GetParentItem(hParent);
		if (hParent)
		{
			name = tree.GetItemText(hParent) + "/" + name;
		}
	}
	return name;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryDialog::SortItems(SortRecursionType recursionType)
{
	switch (recursionType)
	{
	case SORT_RECURSION_NONE:
		{
			m_treeCtrl.SortChildren(TVI_ROOT);
		}
		break;

	case SORT_RECURSION_FULL:
		{
			class Recurser
			{
			public:

				static void Recurse(CTreeCtrl& treeCtrl, HTREEITEM hItem)
				{
					if (hItem)
					{
						CBaseLibraryItem* pItem1 = (CBaseLibraryItem*) treeCtrl.GetItemData(hItem);
						treeCtrl.SortChildren(hItem);

						if (treeCtrl.ItemHasChildren(hItem))
						{
							HTREEITEM hChildItem = treeCtrl.GetChildItem(hItem);

							while (hChildItem != NULL)
							{
								HTREEITEM hNextItem = treeCtrl.GetNextItem(hChildItem, TVGN_NEXT);
								Recurser::Recurse(treeCtrl, hChildItem);
								hChildItem = hNextItem;
							}
						}
					}
				}
			};

			m_treeCtrl.SortChildren(TVI_ROOT);
			HTREEITEM hItem = m_treeCtrl.GetRootItem();
			while (hItem != NULL)
			{
				HTREEITEM hNextItem = m_treeCtrl.GetNextItem(hItem, TVGN_NEXT);
				Recurser::Recurse(m_treeCtrl, hItem);
				hItem = hNextItem;
			}
		}
		break;
	}
}
