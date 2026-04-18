////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   brushpanel.h
//  Version:     v1.00
//  Created:     2/12/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __BrushPanel_h__
#define __BrushPanel_h__

#pragma once

#include <XTToolkitPro.h>
#include "Controls\ToolButton.h"

class CBrushObject;

// CBrushPanel dialog

class CBrushPanel : public CDialog
{
	DECLARE_DYNAMIC(CBrushPanel)

public:
	CBrushPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CBrushPanel();

	void SetBrush( CBrushObject *obj );

// Dialog Data
	enum { IDD = IDD_PANEL_BRUSH };

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	afx_msg void OnCbnSelendokSides();
	afx_msg void OnBnClickedReload();
	afx_msg void OnBnClickedSavetoCGF();
	afx_msg void OnBnClickedEditGeometry();

	DECLARE_MESSAGE_MAP()

	CBrushObject *m_brushObj;
	CCustomButton m_btn1;
	CCustomButton m_btn2;
	CToolButton m_subObjEdit;
};

#endif // __BrushPanel_h__