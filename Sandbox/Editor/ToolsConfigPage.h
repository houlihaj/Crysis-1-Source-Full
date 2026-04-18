////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   toolsconfigpage.h
//  Version:     v1.00
//  Created:     27/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __toolsconfigpage_h__
#define __toolsconfigpage_h__
#pragma once

#include <XTToolkitPro.h>

/** Tools configuration property page.
*/
class CToolsConfigPage : public CXTResizePropertyPage
{
	DECLARE_DYNAMIC(CToolsConfigPage)

public:
	CToolsConfigPage();
	virtual ~CToolsConfigPage();

// Dialog Data
	enum { IDD = IDD_TOOLSCONFIG };

protected:
	enum Ctrls {
		CTRL_COMMAND,
		CTRL_TOGGLE
	};
	void ReadFromControls( int ctrl );

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	
	afx_msg void OnSelchangeEditList();
	
	afx_msg void OnChangeEditorCmd();
	afx_msg void OnSelEditorCmd();

	afx_msg void OnChangeConsoleCmd();
	afx_msg void OnSelConsoleCmd();

	afx_msg void OnToggleVar();
	afx_msg void OnNewItem();

	DECLARE_MESSAGE_MAP()

	struct Tool
	{
		CString editorCommand;
		CString command;
		bool bToggle;
	};

	//////////////////////////////////////////////////////////////////////////
	// Vars.
	//////////////////////////////////////////////////////////////////////////
	CButton m_toggleVar;
	CXTEditListBox	m_editList;
	CComboBox m_editorCmd;
	CComboBox m_consoleCmd;

	std::vector<Tool*> m_tools;
};

#endif // __toolsconfigpage_h__