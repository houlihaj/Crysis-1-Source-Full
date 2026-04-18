#if !defined(AFX_ENTITYPANEL_H__E88E89D7_62BA_4E1F_82D8_17B58F96CA46__INCLUDED_)
#define AFX_ENTITYPANEL_H__E88E89D7_62BA_4E1F_82D8_17B58F96CA46__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// EntityPanel.h : header file
//

#include "PickObjectTool.h"
#include "Controls\PickObjectButton.h"
#include <XTToolkitPro.h>

/////////////////////////////////////////////////////////////////////////////
// CEntityPanel dialog

class CEntityPanel : public CXTResizeDialog
{
// Construction
public:
	CEntityPanel(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	enum { IDD = IDD_PANEL_ENTITY };

	void SetEntity( class CEntity *entity );

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEntityPanel)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK() {};
	virtual void OnCancel() {};

	// Generated message map functions
	afx_msg void OnEditScript();
	afx_msg void OnReloadScript();
	afx_msg void OnPrototype();
	afx_msg void OnBnClickedGetphysics();
	afx_msg void OnBnClickedResetphysics();
	afx_msg void OnBnClickedOpenFlowGraph();
	afx_msg void OnBnClickedRemoveFlowGraph();
	afx_msg void OnBnClickedListFlowGraphs();

	DECLARE_MESSAGE_MAP()

	CCustomButton	m_editScriptButton;
	CCustomButton	m_reloadScriptButton;
	CCustomButton	m_prototypeButton;
	CCustomButton m_flowGraphOpenBtn;
	CCustomButton m_flowGraphRemoveBtn;
	CCustomButton m_flowGraphListBtn;
	CCustomButton m_physicsBtn[2];
	CEntity* m_entity;
};

/////////////////////////////////////////////////////////////////////////////
// CEntityPanel dialog

class CEntityEventsPanel : public CXTResizeDialog, public IPickObjectCallback
{
	// Construction
public:
	CEntityEventsPanel(CWnd* pParent = NULL);   // standard constructor

	// Dialog Data
	enum { IDD = IDD_PANEL_ENTITY_EVENTS };
	CCustomButton	m_sendEvent;
	CCustomButton	m_runButton;
	CCustomButton	m_gotoMethodBtn;
	CCustomButton	m_addMethodBtn;
	CCustomButton	m_addMissionBtn;
	CCustomButton	m_removeButton;
	CTreeCtrl	m_eventTree;
	CPickObjectButton m_pickButton;
	CListBox	m_methods;
	CString	m_selectedMethod;

	void SetEntity( class CEntity *entity );

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEntityPanel)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	// Implementation
protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK() {};
	virtual void OnCancel() {};

	void	ReloadMethods();
	void	ReloadEvents();

	// Ovverriden from IPickObjectCallback
	virtual void OnPick( CBaseObject *picked );
	virtual void OnCancelPick();

	void GotoMethod( const CString &method );

	// Generated message map functions
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnDblclkMethods();
	afx_msg void OnGotoMethod();
	afx_msg void OnAddMethod();
	afx_msg void OnEventAdd();
	afx_msg void OnDestroy();
	afx_msg void OnEventRemove();
	afx_msg void OnSelChangedEventTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickEventTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblClickEventTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRunMethod();
	afx_msg void OnEventSend();
	afx_msg void OnBnAddMission();

	DECLARE_MESSAGE_MAP()

	CImageList m_treeImageList;

	StdMap<HTREEITEM,CString> m_sourceEventsMap;
	StdMap<HTREEITEM,int> m_targetEventsMap;

	CEntity* m_entity;
	CBrush m_grayBrush;
	class CPickObjectTool *m_pickTool;

	CString m_currentSourceEvent;
	int m_currentTrgEventId;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ENTITYPANEL_H__E88E89D7_62BA_4E1F_82D8_17B58F96CA46__INCLUDED_)
