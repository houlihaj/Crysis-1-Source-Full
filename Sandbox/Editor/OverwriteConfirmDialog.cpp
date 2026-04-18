////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   OverwriteConfirmDialog.cpp
//  Version:     v1.00
//  Created:     14/7/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "OverwriteConfirmDialog.h"

BEGIN_MESSAGE_MAP(COverwriteConfirmDialog, CDialog)
	ON_COMMAND_EX(IDYES, OnCommand)
	ON_COMMAND_EX(IDNO, OnCommand)
	ON_COMMAND_EX(ID_YES_ALL, OnCommand)
	ON_COMMAND_EX(ID_NO_ALL, OnCommand)
END_MESSAGE_MAP()

COverwriteConfirmDialog::COverwriteConfirmDialog(CWnd* pParentWindow, const char* szMessage, const char* szCaption)
	:	CDialog(IDD, pParentWindow), message(szMessage), caption(szCaption)
{
}

BOOL COverwriteConfirmDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	messageEdit.SetWindowText(message.c_str());
	SetWindowText(message.c_str());

	return TRUE;
}

void COverwriteConfirmDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_MESSAGE, messageEdit);
}

BOOL COverwriteConfirmDialog::OnCommand(UINT uID)
{
	EndDialog(uID);

	return TRUE;
}
