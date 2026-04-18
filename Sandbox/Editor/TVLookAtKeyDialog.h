#pragma once

#include "IKeyDlg.h"

// CTVLookAtKeyDialog dialog

class CTVLookAtKeyDialog : public IKeyDlg
{
	DECLARE_DYNAMIC(CTVLookAtKeyDialog)

private:
	CComboBox m_name;
	CComboBox m_boneSet;
	CEdit m_id;
	CButton m_allowAdditionalTransforms;
	IAnimTrack* m_track;
	IAnimNode* m_node;
	int m_key;

public:
	CTVLookAtKeyDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTVLookAtKeyDialog();

// Dialog Data
	enum { IDD = IDD_TV_LOOKATKEY };

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	void SetKey( IAnimNode *node,IAnimTrack *track,int nkey );
	afx_msg void OnUpdateValue();
	afx_msg void OnCbnSelchangeName();
	afx_msg void OnUpdateBoneSet();
	afx_msg void OnCbnSelchangeBoneSet();
	afx_msg void OnChangeAllowAdditionalTransforms();
	std::map<ELookAtKeyBoneSet, CString> m_boneSetNamesMap;
	std::map<CString, ELookAtKeyBoneSet> m_boneSetNamesMapReverse;
};
