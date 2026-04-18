
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SubObjSelectionPanel.h
//  Version:     v1.00
//  Created:     26/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SubObjSelectionPanel_h__
#define __SubObjSelectionPanel_h__
#pragma once

// CSubObjSelectionPanel dialog

class CSubObjSelectionPanel : public CDialog
{
	DECLARE_DYNAMIC(CSubObjSelectionPanel)

public:
	CSubObjSelectionPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSubObjSelectionPanel();

// Dialog Data
	enum { IDD = IDD_PANEL_SUB_SELECTION };

protected:
	virtual void OnOK() {};
	virtual void OnCancel();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support


	virtual BOOL OnInitDialog();
	afx_msg void OnUpdateFromUI();

	DECLARE_MESSAGE_MAP()

	BOOL m_bSelectByVertex;
	BOOL m_bIgnoreBackfacing;
	BOOL m_bSoftSelection;
	CNumberCtrl m_falloff;
};

#endif // __SubObjSelectionPanel_h__