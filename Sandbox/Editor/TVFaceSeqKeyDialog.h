#pragma once

#include "IKeyDlg.h"

// CTVFaceSeqKeyDialog dialog

class CTVFaceSeqKeyDialog : public IKeyDlg
{
	DECLARE_DYNAMIC(CTVFaceSeqKeyDialog)

private:
	CComboBox m_name;
	IAnimTrack* m_track;
	IAnimNode* m_node;
	int m_key;

public:
	CTVFaceSeqKeyDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTVFaceSeqKeyDialog();

// Dialog Data
	enum { IDD = IDD_TV_FACESEQKEY };

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
	afx_msg void OnOpenInFE();
};
