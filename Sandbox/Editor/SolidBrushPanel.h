////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SolidBrushPanel.h
//  Version:     v1.00
//  Created:     11/1/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: CSolidBrushPanel declaration file.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SolidBrushPanel_h__
#define __SolidBrushPanel_h__
#pragma once

#include "Controls\NumberCtrl.h"
#include "Controls\ToolButton.h"

class CSolidBrushObject;

// CSolidBrushPanel dialog
class CSolidBrushPanel : public CDialog
{
	DECLARE_DYNAMIC(CSolidBrushPanel)

public:
	CSolidBrushPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSolidBrushPanel();

	void SetBrush( CSolidBrushObject *pObject );

// Dialog Data
	enum { IDD = IDD_PANEL_SOLIDBRUSH };

protected:
	virtual void OnOK() {};
	virtual void OnCancel();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()

	_smart_ptr<CSolidBrushObject> m_pObject;
	CCustomButton m_btn[10];
	CToolButton m_geomEdit;

public:
	afx_msg void OnSaveToCgf();
	afx_msg void OnBnClickedResetTransform();
	afx_msg void OnBnClickedMakeHollow();
	afx_msg void OnBnClickedCsgCombine();
	afx_msg void OnBnClickedCsgIntersect();
	afx_msg void OnBnClickedCsgSubAb();
	afx_msg void OnBnClickedCsgSubBa();
	afx_msg void OnBnClickedSnapToGrid();
};

#endif // __SolidBrushPanel_h__