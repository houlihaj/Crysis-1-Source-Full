////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002-2006.
// -------------------------------------------------------------------------
//  File name:   SelectMissionObjectiveDialog.h
//  Version:     v1.00
//  Created:     14/03/2006 by AlexL.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SELECTMISSIONOBJECTIVEDIALOG_H__
#define __SELECTMISSIONOBJECTIVEDIALOG_H__
#pragma once

#include "GenericSelectItemDialog.h"

// CSelectSequence dialog

class CSelectMissionObjectiveDialog : public CGenericSelectItemDialog
{
	DECLARE_DYNAMIC(CSelectMissionObjectiveDialog)
	CSelectMissionObjectiveDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSelectMissionObjectiveDialog() {}

protected:
	virtual BOOL OnInitDialog();

	// Derived Dialogs should override this
	virtual void GetItems(std::vector<SItem>& outItems);

	// Called whenever an item gets selected
	virtual void ItemSelected();

	struct SObjective
	{
		CString shortText;
		CString longText;
	};

	typedef std::map<CString, SObjective, stl::less_stricmp<CString> > TObjMap;
	TObjMap m_objMap;
};

#endif // __SELECTMISSIONOBJECTIVEDIALOG_H__