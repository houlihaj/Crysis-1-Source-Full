////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   CloudGroupPanel.h
//  Version:     v1.00
//  Created:     10/02/2005 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CloudGroupPanel_h__
#define __CloudGroupPanel_h__

#pragma once

#include <XTToolkitPro.h>
#include "Controls\ToolButton.h"
#include "Objects\CloudGroup.h"



// CCloudGroupPanel dialog
class CCloudGroupPanel : public CXTResizeDialog
{
	DECLARE_DYNAMIC(CCloudGroupPanel)

public:
	CCloudGroupPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CCloudGroupPanel();

// Dialog Data
	enum { IDD = IDD_PANEL_CLOUDGROUP };

	void Init(CCloudGroup * cloud);
	void FillBox(CBaseObject * pBox);
	void SetFileBrowseText();


protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	afx_msg void OnCloudsGenerate();
	afx_msg void OnCloudsImport();
	afx_msg void OnCloudsExport();
	afx_msg void OnCloudsBrowse();

	DECLARE_MESSAGE_MAP()

private:
	CCloudGroup * m_cloud;
};

#endif // __CloudGroupPanel_h__