////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002-2006.
// -------------------------------------------------------------------------
//  File name:   SelectAnimationDialog.h
//  Version:     v1.00
//  Created:     10/11/2006 by MichaelS.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __SELECTANIMATIONDIALOG_H__
#define __SELECTANIMATIONDIALOG_H__

#include "GenericSelectItemDialog.h"

struct ICharacterInstance;

class CSelectAnimationDialog : public CGenericSelectItemDialog
{
	DECLARE_DYNAMIC(CSelectAnimationDialog)
	CSelectAnimationDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSelectAnimationDialog() {}

	void SetCharacterInstance(ICharacterInstance* pCharacterInstance);
	CString GetSelectedItem();

protected:
	virtual BOOL OnInitDialog();

	// Derived Dialogs should override this
	virtual void GetItems(std::vector<SItem>& outItems);

	// Called whenever an item gets selected
	virtual void ItemSelected();

	ICharacterInstance* m_pCharacterInstance;
};

#endif //__SELECTANIMATIONDIALOG_H__
