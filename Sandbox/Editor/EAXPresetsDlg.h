#pragma once

#include "BaseLibraryDialog.h"
#include "Controls\PropertyCtrl.h"

class CEAXPresetMgr;

class CEAXPresetsDlg : public CBaseLibraryDialog
{
	DECLARE_DYNAMIC(CEAXPresetsDlg)

protected:
	CEAXPresetMgr *m_pEAXPresetMgr;
	CListBox m_wndPresets;
	CPropertyCtrl m_wndParams;

	int nWidthPresets;

protected:
	void InitToolbar( UINT nToolbarResID );
	void Update();

protected:
	virtual BOOL OnInitDialog();
	//virtual void PostNcDestroy() { delete this; };

public:
	CEAXPresetsDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CEAXPresetsDlg();
	void OnParamsChanged(XmlNodeRef pNode);

	void OnCopy(){}
	void OnPaste() {}
	void SetActive( bool bActive ){}

// Dialog Data
	enum { IDD = IDD_EAXPRESETS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg LRESULT OnUpdateProperties(WPARAM wParam, LPARAM lParam);
	afx_msg void OnLbnSelchangePresets();
	afx_msg void OnSavePreset();
	afx_msg void OnAddPreset();
	afx_msg void OnDelPreset();
	afx_msg void OnPreviewPreset();
	afx_msg void OnClose();
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
};
