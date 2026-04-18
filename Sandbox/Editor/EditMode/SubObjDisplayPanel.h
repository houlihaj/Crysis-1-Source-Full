
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SubObjDisplayPanel.h
//  Version:     v1.00
//  Created:     26/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SubObjDisplayPanel_h__
#define __SubObjDisplayPanel_h__
#pragma once

// CSubObjDisplayPanel dialog

class CSubObjDisplayPanel : public CDialog
{
	DECLARE_DYNAMIC(CSubObjDisplayPanel)

public:
	CSubObjDisplayPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSubObjDisplayPanel();

	// Dialog Data
	enum { IDD = IDD_PANEL_SUBSEL_DISPLAY };

protected:
	virtual void OnOK() {};
	virtual void OnCancel();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	virtual BOOL OnInitDialog();
	afx_msg void OnUpdateFromUI();

	DECLARE_MESSAGE_MAP()

	int m_displayType;
	BOOL m_bDisplayBackfacing;
	BOOL m_bDisplayNormals;
	float m_fNormalsLength;
	CNumberCtrl m_normalsLength;
};

#endif // __SubObjDisplayPanel_h__