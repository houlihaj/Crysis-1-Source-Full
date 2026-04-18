////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   GravityVolumePanel.h
//  Version:     v1.00
//  Created:     22/05/2006 by Sergiy Shaykin.
//  Compilers:   Visual C++.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GravityVolumepanel_h__
#define __GravityVolumepanel_h__
#pragma once

#include "Controls\PickObjectButton.h"
#include "Controls\ToolButton.h"

class CGravityVolumeObject;

// CGravityVolumePanel dialog
class CGravityVolumePanel : public CDialog
{
	DECLARE_DYNAMIC(CGravityVolumePanel)

public:
	CGravityVolumePanel( CWnd* pParent = NULL);   // standard constructor
	virtual ~CGravityVolumePanel();

// Dialog Data
	enum { IDD = IDD_PANEL_GRAVITYVOLUME};

	void SetGravityVolume( CGravityVolumeObject *GravityVolume );
	CGravityVolumeObject* GetGravityVolume() const { return m_GravityVolume; }

	void SelectPoint( int index );

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedSelect();

	virtual void OnOK() {};
	virtual void OnCancel() {};

	DECLARE_MESSAGE_MAP()

	CGravityVolumeObject *m_GravityVolume;
	//CPickObjectButton m_pickButton;
	CToolButton m_editGravityVolumeButton;
};

#endif // __GravityVolumepanel_h__