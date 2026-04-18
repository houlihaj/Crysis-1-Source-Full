////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SubObjSelection.h
//  Version:     v1.00
//  Created:     26/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SubObjSelection.h"

SSubObjSelOptions g_SubObjSelOptions;

/*

//////////////////////////////////////////////////////////////////////////
bool CSubObjSelContext::IsEmpty() const
{
	if (GetCount() == 0)
		return false;
	for (int i = 0; i < GetCount(); i++)
	{
		CSubObjectSelection *pSel = GetSelection(i);
		if (!pSel->IsEmpty())
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSubObjSelContext::ModifySelection( SSubObjSelectionModifyContext &modCtx )
{
	for (int n = 0; n < GetCount(); n++)
	{
		CSubObjectSelection *pSel = GetSelection(n);
		if (pSel->IsEmpty())
			continue;
		modCtx.pSubObjSelection = pSel;
		pSel->pGeometry->SubObjSelectionModify( modCtx );
	}
	if (modCtx.type == SO_MODIFY_MOVE)
	{
		OnSelectionChange();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSubObjSelContext::AcceptModifySelection()
{
	for (int n = 0; n < GetCount(); n++)
	{
		CSubObjectSelection *pSel = GetSelection(n);
		if (pSel->IsEmpty())
			continue;
		if (pSel->pGeometry)
			pSel->pGeometry->Update();
	}
}

*/