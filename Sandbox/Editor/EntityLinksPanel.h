#if !defined(__entity_links_panel_h__)
#define __entity_links_panel_h__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// EntityLinksPanel.h : header file
//

#include "PickObjectTool.h"
#include "Controls\PickObjectButton.h"
#include <XTToolkitPro.h>

/////////////////////////////////////////////////////////////////////////////
// CEntityLinksPanel dialog

class CEntityLinksPanel : public CXTResizeDialog, public IPickObjectCallback
{
	// Construction
public:
	CEntityLinksPanel(CWnd* pParent = NULL);   // standard constructor

	// Dialog Data
	enum { IDD = IDD_PANEL_ENTITY_LINKS };
	
	CCustomButton	m_removeButton;
	CPickObjectButton m_pickButton;

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

	void	ReloadLinks();

	// Ovverriden from IPickObjectCallback
	afx_msg void OnBnClickedRemove();
	afx_msg void OnRclickLinks(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblClickLinks(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBeginLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);

	virtual void OnPick( CBaseObject *picked );
	virtual void OnCancelPick();

	DECLARE_MESSAGE_MAP()

	CEntity* m_entity;

	int m_currentLinkIndex;
	CListCtrl m_links;


};

#endif // !__entity_links_panel_h__
