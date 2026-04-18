#ifndef __TVSEQUENCEKEYDIALOG_H__
#define __TVSEQUENCEKEYDIALOG_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include "IKeyDlg.h"

class CTVSequenceKeyDialog : public IKeyDlg
{
	DECLARE_DYNAMIC(CTVSequenceKeyDialog)

private:
	CComboBox m_name;
	CEdit m_id;
	CButton m_overrideTimes;
	CNumberCtrl m_startTime;
	CNumberCtrl m_endTime;
	IAnimTrack* m_track;
	IAnimNode* m_node;
	int m_key;

public:
	CTVSequenceKeyDialog(CWnd* pParent = NULL);
	virtual ~CTVSequenceKeyDialog();

	enum {IDD = IDD_TV_SEQUENCEKEY};

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);

	void EnableControls();

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	void SetKey(IAnimNode *node,IAnimTrack *track,int nkey);
	afx_msg void OnUpdateValue();
	afx_msg void OnCbnSelchangeName();
	afx_msg void OnBnClickedOverride();
	afx_msg void OnUpdateTimeValue();
};

#endif