
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   SelectGameTokenDialog.cpp
//  Version:     v1.00
//  Created:     14/12/2005 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SelectGameTokenDialog.h"
#include <IDataBaseItem.h>
#include <IDataBaseManager.h>
#include <IDataBaseLibrary.h>
#include <GameTokens/GameTokenItem.h>
#include <GameTokens/GameTokenManager.h>

// CSelectGameTokenDialog dialog

IMPLEMENT_DYNAMIC(CSelectGameTokenDialog, CXTResizeDialog)
CSelectGameTokenDialog::CSelectGameTokenDialog(CWnd* pParent /*=NULL*/)
	: CXTResizeDialog(CSelectGameTokenDialog::IDD, pParent)
{
}

CSelectGameTokenDialog::~CSelectGameTokenDialog()
{
}

void CSelectGameTokenDialog::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
	DDX_Control( pDX,IDC_TREE,m_tree );
}


BEGIN_MESSAGE_MAP(CSelectGameTokenDialog, CXTResizeDialog)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE, OnTvnSelchangedTree)
	ON_NOTIFY(NM_DBLCLK, IDC_TREE, OnTvnDoubleClick)
END_MESSAGE_MAP()


// CSelectGameTokenDialog message handlers

//////////////////////////////////////////////////////////////////////////
BOOL CSelectGameTokenDialog::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();
	
	// Create the list
	//m_imageList.Create(IDB_TREE_VIEW, 16, 1, RGB (255, 0, 255));
	CMFCUtils::LoadTrueColorImageList( m_imageList,IDB_ENTITY_TREE,16,RGB(255,0,255) );

	// Attach it to the control
	m_tree.SetImageList(&m_imageList, TVSIL_NORMAL);
	//m_tree.SetImageList(&m_imageList, TVSIL_STATE);

	m_tree.SetIndent( 0 );
	m_tree.SetBkColor( RGB(0xE0,0xE0,0xE0) );
	
	SetResize( IDC_TREE,SZ_RESIZE(1) );
	SetResize( IDOK,SZ_REPOS(1) );
	SetResize( IDCANCEL,SZ_REPOS(1) );

	ReloadGameTokens();

	AutoLoadPlacement( "Dialogs\\SelGameToken" );

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CSelectGameTokenDialog::ReloadGameTokens()
{
	StdMap<CString,HTREEITEM> treeItems;

	CGameTokenManager *pMgr = GetIEditor()->GetGameTokenManager();
	IDataBaseItemEnumerator *pEnum = pMgr->GetItemEnumerator();
	IDataBaseItem *pItem = pEnum->GetFirst();

	std::map<CString, CGameTokenItem*> items;

	while (pItem)
	{
		CGameTokenItem *pToken = (CGameTokenItem*) pItem;
		items[pToken->GetFullName()] = pToken;
		pItem = pEnum->GetNext();
	}
	pEnum->Release();

	// items now sorted, make the tree
	std::map<CString, CGameTokenItem*>::iterator iter = items.begin();
	HTREEITEM hSelected = 0;

	while (iter != items.end())
	{
		HTREEITEM hRoot = TVI_ROOT;

		CString libName;
		IDataBaseLibrary* pLib = (*iter).second->GetLibrary();
		if (pLib != 0)
			libName = pLib->GetName();
		const CString& groupName = (*iter).second->GetGroupName();

		// for now circumvent a bug in the database GetShortName when the item name
		// contains a ".". then short name returns only the last piece of it
		// which is wrong (say group: GROUP1 and name "NAME.SUBNAME" then
		// short name returns only SUBNAME. 
		CString shortName = (*iter).second->GetName(); // incl. group
		int i = shortName.Find(groupName+".");
		if (i>= 0)
			shortName = shortName.Mid(groupName.GetLength()+1);

		if (!libName.IsEmpty())
		{
			if (!treeItems.Find(libName,hRoot))
			{
				hRoot = m_tree.InsertItem(libName, 0, 1, hRoot);
				treeItems.Insert(libName, hRoot);
			}
		}
		if (!groupName.IsEmpty())
		{
			CString combinedName = libName;
			if (!libName.IsEmpty())
				combinedName += ".";
			combinedName += groupName;
			if (!treeItems.Find(combinedName,hRoot))
			{
				hRoot = m_tree.InsertItem(groupName, 0, 1, hRoot);
				treeItems.Insert(combinedName, hRoot);
			}			
		}

		HTREEITEM hNewItem = m_tree.InsertItem(shortName, 2, 3, hRoot );
		m_tree.SetItemState(hNewItem, TVIS_BOLD, TVIS_BOLD);
		m_itemsMap[hNewItem] = (*iter).second;
		if (!m_preselect.IsEmpty() && m_preselect.CompareNoCase((*iter).first) == 0)
			hSelected = hNewItem;

		++iter;
	}
	m_selectedItem = 0;

	if (hSelected != 0)
	{
		// all parent nodes have to be expanded first
		HTREEITEM hParent = hSelected;
		while ( hParent = m_tree.GetParentItem( hParent ) )
			m_tree.Expand( hParent, TVE_EXPAND );

		m_tree.Select( hSelected, TVGN_CARET );
		m_tree.EnsureVisible( hSelected );
	}
}

//////////////////////////////////////////////////////////////////////////
void CSelectGameTokenDialog::OnTvnSelchangedTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	std::map<HTREEITEM,IDataBaseItem*>::const_iterator found;
	found = m_itemsMap.find(pNMTreeView->itemNew.hItem);
	if (found != m_itemsMap.end())
		m_selectedItem = (*found).second;
	else
		m_selectedItem = 0;
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CSelectGameTokenDialog::OnTvnDoubleClick(NMHDR *pNMHDR, LRESULT *pResult)
{
	if (m_selectedItem != 0)
	{
		EndDialog(IDOK);
	}
}

//////////////////////////////////////////////////////////////////////////
CString CSelectGameTokenDialog::GetSelectedGameToken()
{
	if (m_selectedItem != 0)
		return m_selectedItem->GetFullName();
	return "";
}

