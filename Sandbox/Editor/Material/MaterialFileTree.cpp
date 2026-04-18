////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   MaterialFileTree.cpp
//  Version:     v1.00
//  Created:     26/8/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MaterialFileTree.h"

//////////////////////////////////////////////////////////////////////////
CMaterialFileTreeCtrl::CMaterialFileTreeCtrl()
{
	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
CMaterialFileTreeCtrl::~CMaterialFileTreeCtrl()
{
}

//////////////////////////////////////////////////////////////////////////
BOOL CMaterialFileTreeCtrl::Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd,UINT nID )
{
	BOOL res = CTreeCtrl::Create(dwStyle,rect,pParentWnd,nID);
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialFileTreeCtrl::ReloadItems()
{
	CWaitCursor wait;
	CFileUtil::FileArray files;
	files.reserve(5000); // Reserve space for 5000 files.
	CFileUtil::ScanDirectory( "","*.mtl",files );

	m_bIgnoreSelectionChange = true;

	SetRedraw(FALSE);
	DeleteAllItems();

	CString filter = "";
	bool bFilter = !filter.IsEmpty();

	HTREEITEM hLibItem = TVI_ROOT;

	CString itemName;
	CString itemLastName;
	std::map<CString,HTREEITEM,stl::less_stricmp<CString> > items;
	for (int i = 0; i < files.size(); i++)
	{
		CString name = files[i].filename;
		itemLastName = name;

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

			HTREEITEM hParentItem = stl::find_in_map(items,itemName,0);
			if (!hParentItem)
			{
				hRoot = InsertItem(strToken, 0, 1, hRoot );
				items[itemName] = hRoot;
			}
			else
				hRoot = hParentItem;
		}

		HTREEITEM hNewItem = InsertItem( itemLastName,2,3,hRoot );
	}

	SortChildren( hLibItem );
	Expand( hLibItem,TVE_EXPAND );
	SetRedraw(TRUE);

	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
CString CMaterialFileTreeCtrl::GetFullItemName( HTREEITEM hItem )
{
	CString name = GetItemText(hItem);
	HTREEITEM hParent = hItem;
	while (hParent)
	{
		hParent = GetParentItem(hParent);
		if (hParent)
		{
			name = GetItemText(hParent) + "/" + name;
		}
	}
	return name;
}