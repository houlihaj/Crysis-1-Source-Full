////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002-2006.
// -------------------------------------------------------------------------
//  File name:   SelectSequenceDialog.h
//  Version:     v1.00
//  Created:     14/03/2006 by AlexL.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SELECTSEQUENCEDIALOG_H__
#define __SELECTSEQUENCEDIALOG_H__
#pragma once

#include "GenericSelectItemDialog.h"

// CSelectSequence dialog

class CSelectSequenceDialog : public CGenericSelectItemDialog
{
	DECLARE_DYNAMIC(CSelectSequenceDialog)
	CSelectSequenceDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSelectSequenceDialog() {}

protected:
	virtual BOOL OnInitDialog();

	// Derived Dialogs should override this
	virtual void GetItems(std::vector<SItem>& outItems);
};

#endif // __SELECTSEQUENCEDIALOG_H__