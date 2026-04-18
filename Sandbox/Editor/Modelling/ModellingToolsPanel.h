////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek, 2007.
// -------------------------------------------------------------------------
//  File name:   ModellingToolsPanel.h
//  Version:     v1.00
//  Created:     18/1/2007 by Timur.
//  Description: Polygon creation edit tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ModellingToolsPanel_h__
#define __ModellingToolsPanel_h__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


/////////////////////////////////////////////////////////////////////////////
// CModellingToolsPanel dialog

class CModellingToolsPanel : public CDialog
{
// Construction
public:
	CModellingToolsPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CModellingToolsPanel();

	enum { IDD = IDD_MAINTOOLS };

	void InitDefaultModellingPanels();
	void	UncheckAll();

// Overrides
public:
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);

// Implementation
protected:
	void CreateButtons();
	void ReleaseButtons();
	void OnButtonPressed( int i );
	void StartTimer();
	void StopTimer();

	virtual void OnOK() {};
	virtual void OnCancel() {};

	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();

	DECLARE_MESSAGE_MAP()

	std::vector<CColorCheckBox*> m_buttons;
	std::map<CString,CString> m_commandMap;
	int m_lastPressed;
	int m_nTimer;

	CString m_currentToolName;

	class CSubObjSelectionTypePanel *m_pTypePanel;
	int m_selectionTypePanelId;
	int m_selectionPanelId;
	int m_displayPanelId;
};


#endif // __ModellingToolsPanel_h__
