////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   Roadpanel.h
//  Version:     v1.00
//  Created:     25/07/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __Roadpanel_h__
#define __Roadpanel_h__
#pragma once

#include "Controls\PickObjectButton.h"
#include "Controls\ToolButton.h"

class CRoadObject;

// CRoadPanel dialog
class CRoadPanel : public CDialog
{
	DECLARE_DYNAMIC(CRoadPanel)

public:
	CRoadPanel( CWnd* pParent = NULL);   // standard constructor
	virtual ~CRoadPanel();

// Dialog Data
	enum { IDD = IDD_PANEL_ROAD_SHAPE };

	void SetRoad( CRoadObject *Road );
	CRoadObject* GetRoad() const { return m_Road; }

	void SelectPoint( int index );
	void OnUpdateParams( CNumberCtrl *ctrl );

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedSelect();
	afx_msg void OnAlignHeightMap();
	afx_msg void OnDefaultWidth();

	virtual void OnOK() {};
	virtual void OnCancel() {};

	DECLARE_MESSAGE_MAP()

	CRoadObject *m_Road;
	CPickObjectButton m_pickButton;
	CCustomButton m_alignHmapButton;
	CToolButton m_editRoadButton;

	CNumberCtrl m_angle;
	CNumberCtrl m_width;
};

#endif // __Roadpanel_h__