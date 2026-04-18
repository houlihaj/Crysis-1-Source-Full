////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ToolBoxBar.h
//  Version:     v1.00
//  Created:     18/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ToolBoxBar_h__
#define __ToolBoxBar_h__
#pragma once

#include <XTToolkitPro.h>

/////////////////////////////////////////////////////////////////////////////
// CToolBoxDialog dialog
class CToolBoxDialog : public CDialog, public IEditorNotifyListener
{
	// Construction
public:
	CToolBoxDialog();

	// Dialog Data
	enum { IDD = IDD_DATABASE };

	// Update tools.
	void UpdateTools();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	// Implementation
protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};

	int  AddIconFile( const char *filename );
	void UnselectAllExceptThisItem( CXTPTaskPanelGroupItem* pItem );
	void UnselectAllExceptThisItemID( int nID );
	void SelectCmd( int nCmdID );
	void SelectCurrentTool();

	//////////////////////////////////////////////////////////////////////////
	// IEditorNotifyListener
	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );
	//////////////////////////////////////////////////////////////////////////

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()

private:
	CXTPTaskPanel m_wndTaskPanel;
	CImageList m_imageList;

	enum EToolCmdType
	{
		ETOOL_CMD_EDITTOOL,
		ETOOL_CMD_USERCMD,
	};
	struct ToolCmd
	{
		EToolCmdType type;
		CString name;
	};

	typedef std::map<int,ToolCmd> CommandMap;
	CommandMap m_commandMap;
};

#endif //__ToolBoxBar_h__
