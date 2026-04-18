////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   VoxelObjectpanel.h
//  Version:     v1.00
//  Created:     19/07/2005 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __VoxelObjectPanel_h__
#define __VoxelObjectPanel_h__

#pragma once

#include <XTToolkitPro.h>

class CVoxelObject;

// CVoxelObjectPanel dialog

class CVoxelObjectPanel : public CXTResizeDialog
{
	DECLARE_DYNAMIC(CVoxelObjectPanel)

public:
	CVoxelObjectPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CVoxelObjectPanel();

	void SetVoxelObject( CVoxelObject *obj );

// Dialog Data
	enum { IDD = IDD_PANEL_VOXEL };

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClicked();
	afx_msg void OnBnSplit();
	afx_msg void OnUpdate();
	afx_msg void OnCopyHM();

	DECLARE_MESSAGE_MAP()

	CVoxelObject *m_voxelObj;
};

#endif // __VoxelObjectPanel_h__