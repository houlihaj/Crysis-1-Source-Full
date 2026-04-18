////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   gridsettingsdialog.h
//  Version:     v1.00
//  Created:     26/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __gridsettingsdialog_h__
#define __gridsettingsdialog_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Controls\NumberCtrl.h"
#include <XTToolkitPro.h>

// CGridSettingsDialog dialog

class CGridSettingsDialog : public CDialog
{
	DECLARE_DYNAMIC(CGridSettingsDialog)

public:
	CGridSettingsDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CGridSettingsDialog();

// Dialog Data
	enum { IDD = IDD_GRID_SETTINGS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnBnUserDefined();
	afx_msg void OnBnGetFromObject();
	afx_msg void OnBnGetAngles();
	afx_msg void OnBnGetTranslation();

	void EnableGridPropertyControls(const bool isUserDefined,const bool isGetFromObject);

	DECLARE_MESSAGE_MAP()
private:
	CNumberCtrl m_gridSize;
	CNumberCtrl m_gridScale;
	
	CButton m_userDefined;
	CButton m_getFromObject;
	CNumberCtrl m_angleX;
	CNumberCtrl m_angleY;
	CNumberCtrl m_angleZ;
	CNumberCtrl m_translationX;
	CNumberCtrl m_translationY;
	CNumberCtrl m_translationZ;
	CButton m_getAnglesFromObject;
	CButton m_getTranslationFromObject;

	CNumberCtrl m_angleSnapScale;
	CNumberCtrl m_CPSize;
	CNumberCtrl m_snapMarkerSize;
	CButton m_angleSnap;
	CButton m_snapToGrid;
	CButton m_displayCP;
	CButton m_displaySnapMarker;
	CXTColorPicker m_snapMarkerColor;
};

#endif // __gridsettingsdialog_h__