////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SolidBrushCreatePanel.h
//  Version:     v1.00
//  Created:     11/1/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: CSolidBrushCreatePanel declaration file.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SolidBrushCreatePanel_h__
#define __SolidBrushCreatePanel_h__
#pragma once

#include "Controls\NumberCtrl.h"

class CSolidBrushObject;

// CSolidBrushCreatePanel dialog
class CSolidBrushCreatePanel : public CDialog
{
	DECLARE_DYNAMIC(CSolidBrushCreatePanel)

public:
	CSolidBrushCreatePanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSolidBrushCreatePanel();

	void EnableNumSides( bool bEnable );

// Dialog Data
	enum { IDD = IDD_PANEL_SOLIDBRUSH_CREATE };

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	afx_msg void OnNumSidesChange();
	afx_msg void OnTypeChange();

	DECLARE_MESSAGE_MAP()

	CNumberCtrl m_numSidesCtrl;
	CComboBox m_typeCtrl;
};

#endif // __SolidBrushCreatePanel_h__