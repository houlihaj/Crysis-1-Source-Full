////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   BrushClipToolPanel.h
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain modification tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __BrushClipToolPanel_h__
#define __BrushClipToolPanel_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include <XTToolkitPro.h>

class CBrushClipTool;

//////////////////////////////////////////////////////////////////////////
class CBrushClipToolPanel : public CXTResizeDialog
{
	DECLARE_DYNAMIC(CBrushClipToolPanel)

public:
	CBrushClipToolPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CBrushClipToolPanel();

	void SetTool( CBrushClipTool *pTool );

	// Dialog Data
	enum { IDD = IDD_PANEL_BRUSH_CLIPTOOL };

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	
	afx_msg void OnClipFront();
	afx_msg void OnClipBack();
	afx_msg void OnClipSplit();
	afx_msg void OnSizeChange();
	afx_msg void OnAngleChange();
	afx_msg void OnPlaneTypeChange();

	DECLARE_MESSAGE_MAP()

	CBrushClipTool *m_pTool;
	CCustomButton m_btn[3];

	CNumberCtrl m_sizeCtrl;
	CNumberCtrl m_angleCtrl;
	int m_nPlaneType;
};

#endif // __BrushClipTool_h__
