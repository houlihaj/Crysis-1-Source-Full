////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   FacialPreviewOptionsDialog.h
//  Version:     v1.00
//  Created:     11/6/2005 by Michael S.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __FACIALPREVIEWOPTIONSDIALOG_H__
#define __FACIALPREVIEWOPTIONSDIALOG_H__

#include "PropertiesPanel.h"

class CModelViewportCE;
class CFacialPreviewDialog;
class CFacialEdContext;

class CFacialPreviewOptionsDialog : public CDialog
{
	DECLARE_DYNAMIC(CFacialPreviewOptionsDialog)
public:
	enum { IDD = IDD_DATABASE };

	CFacialPreviewOptionsDialog();
	~CFacialPreviewOptionsDialog();

	void SetViewport(CFacialPreviewDialog* pPreviewDialog);
	void SetContext(CFacialEdContext* pContext);

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

	CPropertiesPanel* m_panel;
	CModelViewportCE* m_pModelViewportCE;
	CFacialPreviewDialog* m_pPreviewDialog;
	HACCEL m_hAccelerators;
	CFacialEdContext* m_pContext;
};

#endif //__FACIALPREVIEWOPTIONSDIALOG_H__
