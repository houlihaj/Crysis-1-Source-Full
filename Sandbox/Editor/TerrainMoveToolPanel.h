////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TerrainMoveToolPanel.h
//  Version:     v1.00
//  Created:     5/12/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain modification tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __TerrainMoveToolPanel_h__
#define __TerrainMoveToolPanel_h__


#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include "Resource.h"


/////////////////////////////////////////////////////////////////////////////
// CTerrainMoveToolPanel dialog
class CTerrainMoveTool;

class CTerrainMoveToolPanel : public CDialog
{
// Construction
public:
	CTerrainMoveToolPanel( CTerrainMoveTool *tool,CWnd* pParent = NULL);   // standard constructor

	void UpdateButtons();

// Dialog Data
	//{{AFX_DATA(CTerrainMoveToolPanel)
	enum { IDD = IDD_PANEL_MOVETOOL };
	CColorCheckBox	m_selectSource;
	CColorCheckBox	m_selectTarget;

	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTerrainMoveToolPanel)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};

	// Generated message map functions
	//{{AFX_MSG(CTerrainMoveToolPanel)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelectSource();
	afx_msg void OnSelectTarget();
	afx_msg void OnCopyButton();
	afx_msg void OnMoveButton();
	afx_msg void OnUpdateNumbers();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CNumberCtrl m_dymX;
	CNumberCtrl m_dymY;
	CNumberCtrl m_dymZ;

	CTerrainMoveTool*	m_tool;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // __TerrainMoveToolPanel_h__
