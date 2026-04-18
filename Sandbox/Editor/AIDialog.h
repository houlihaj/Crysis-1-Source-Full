////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   aidialog.h
//  Version:     v1.00
//  Created:     21/3/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __aidialog_h__
#define __aidialog_h__

#if _MSC_VER > 1000
#pragma once
#endif

// CAIDialog dialog

class CAIDialog : public CDialog
{
	DECLARE_DYNAMIC(CAIDialog)

public:
	CAIDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CAIDialog();

// Dialog Data
	enum { IDD = IDD_AIANCHORS };

	void SetAIBehavior( const CString &behavior );
	CString GetAIBehavior() { return m_aiBehavior; };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

protected:
	virtual BOOL OnInitDialog();

	void ReloadBehaviors();

	//////////////////////////////////////////////////////////////////////////
	// FIELDS
	//////////////////////////////////////////////////////////////////////////
	CListBox m_behaviors;

	CString m_aiBehavior;

	CEdit m_description;
	afx_msg void OnBnClickedEdit();
	afx_msg void OnBnClickedReload();
	afx_msg void OnLbnSelchange();
	afx_msg void OnLbnDblClk();
};

#endif // __aidialog_h__