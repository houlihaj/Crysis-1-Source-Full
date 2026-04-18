////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   OverwriteConfirmDialog.h
//  Version:     v1.00
//  Created:     14/7/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __OVERWRITECONFIRMDIALOG_H__
#define __OVERWRITECONFIRMDIALOG_H__

#include "resource.h"

class COverwriteConfirmDialog : public CDialog
{
public:
	enum { IDD = IDD_CONFIRM_OVERWRITE };

	COverwriteConfirmDialog(CWnd* pParentWindow, const char* szMessage, const char* szCaption);

private:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	afx_msg BOOL OnCommand(UINT uID);

	DECLARE_MESSAGE_MAP()

	string message;
	string caption;
	CEdit messageEdit;
};

#endif //__OVERWRITECONFIRMDIALOG_H__
