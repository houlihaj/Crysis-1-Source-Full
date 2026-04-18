////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   tcbkeydialog.h
//  Version:     v1.00
//  Created:     28/5/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __tcbkeydialog_h__
#define __tcbkeydialog_h__

#if _MSC_VER > 1000
#pragma once
#endif

struct IAnimTrack;
struct IAnimNode;

#include "ikeydlg.h"
#include "tcbpreviewctrl.h"

// CTcbKeyDialog dialog

class CTcbKeyDialog : public IKeyDlg, public IEditorNotifyListener
{
	DECLARE_DYNAMIC(CTcbKeyDialog)

public:
	CTcbKeyDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTcbKeyDialog();

// Dialog Data
	enum { IDD = IDD_TV_TCBKEY };

	void SetKey( IAnimNode *node,IAnimTrack *track,int key );

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	void SetKeyControls( int nkey );

	DECLARE_MESSAGE_MAP()

	CNumberCtrl m_value[3];
	CNumberCtrl m_tcb[5];
	CTcbPreviewCtrl m_tcbPreview;

	int m_paramId;
	Matrix34 m_dragStartM;
	Matrix34 m_dragStartInvM;
	Vec3 m_currValue;
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

#endif // __tcbkeydialog_h__