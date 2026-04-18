#if !defined(AFX_MODELVIEWPANEL_H__3C441EB6_B02A_4D2F_A5E9_587E334E9F48__INCLUDED_)
#define AFX_MODELVIEWPANEL_H__3C441EB6_B02A_4D2F_A5E9_587E334E9F48__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Controls\AnimSequences.h"

/////////////////////////////////////////////////////////////////////////////
// CharPanel_BAttach dialog
/////////////////////////////////////////////////////////////////////////////
class CharPanel_BAttach : public CDialog
{

public:
	CharPanel_BAttach( class CModelViewport *vp,CWnd* pParent ) : CDialog(CharPanel_BAttach::IDD, pParent)
	{
		m_ModelViewPort = vp;
		Create(MAKEINTRESOURCE(IDD),pParent);
	}

	enum { IDD = IDD_CHARPANEL_BATTACH };

	void ClearBones();
	void AddBone( const CString &bone );
	void SelectBone( const CString &bone );
	void SetFileName( const CString &filename );
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK() {};
	virtual void OnCancel() {};
	int GetCurSel();
	virtual BOOL OnInitDialog();

	afx_msg void OnBnClickedBrowseObject();
	afx_msg void OnBnClickedAttachObject();
	afx_msg void OnBnClickedDetachObject();
	afx_msg void OnBnClickedDetachAll();
	DECLARE_MESSAGE_MAP()

	CModelViewport* m_ModelViewPort;

	CCustomButton m_browseObjectBtn;
	CCustomButton m_attachBtn;
	CCustomButton m_detachBtn;
	CCustomButton m_detachAllBtn;
	CComboBox m_boneName;
	CEdit m_objectName;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MODELVIEWPANEL_H__3C441EB6_B02A_4D2F_A5E9_587E334E9F48__INCLUDED_)
