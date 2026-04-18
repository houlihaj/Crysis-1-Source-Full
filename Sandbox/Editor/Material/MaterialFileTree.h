////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   MaterialFileTree.h
//  Version:     v1.00
//  Created:     26/8/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MaterialFileTree_h__
#define __MaterialFileTree_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
//
// CMaterialFileTreeCtrl is a special TreeCtrl that will scan game directory for all 
// files with extension .mtl that assumed to be material files, and will show them in tree form.
//
//////////////////////////////////////////////////////////////////////////
class CMaterialFileTreeCtrl : public CTreeCtrl
{
public:
	CMaterialFileTreeCtrl();
	~CMaterialFileTreeCtrl();

	BOOL Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd,UINT nID );
	CString CMaterialFileTreeCtrl::GetFullItemName( HTREEITEM hItem );

  void ReloadItems();

private:
	bool m_bIgnoreSelectionChange;
};

#endif // __MaterialFileTree_h__

