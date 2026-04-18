#ifndef __TVSHAKEKEYDIALOG_H__
#define __TVSHAKEKEYDIALOG_H__

#if _MSC_VER > 1000
#pragma once
#endif

struct IAnimTrack;
struct IAnimNode;

#include "IKeyDlg.h"
#include "TcbPreviewCtrl.h"

class CTVShakeKeyDialog : public IKeyDlg, public IEditorNotifyListener
{
	DECLARE_DYNAMIC(CTVShakeKeyDialog)

public:
	CTVShakeKeyDialog(CWnd* pParent = NULL);
	virtual ~CTVShakeKeyDialog();

	enum {IDD = IDD_TV_SHAKEKEYS};

	void SetKey(IAnimNode *node,IAnimTrack *track,int key);

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);
	void SetKeyControls(int nkey);

	DECLARE_MESSAGE_MAP()

	CNumberCtrl m_value[4];
	CNumberCtrl m_tcb[5];
	CTcbPreviewCtrl m_tcbPreview;

	Vec4 m_currValue;
	float m_currTens;
	float m_currCont;
	float m_currBias;
	float m_currEaseTo;
	float m_currEaseFrom;

	int m_nLButtonState;

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnUpdateValue();
	afx_msg void OnCtrlLBD();
	afx_msg void OnCtrlLBU();
};

#endif