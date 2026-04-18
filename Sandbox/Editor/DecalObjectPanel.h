/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 07:11:2005   11:00 : Created by Carsten Wenzel

*************************************************************************/

#ifndef _DECALOBJECTPANEL_H_
#define _DECALOBJECTPANEL_H_

#pragma once

#include <XTToolkitPro.h>
#include "Controls\ToolButton.h"


class CDecalObject;


class CDecalObjectPanel : public CXTResizeDialog
{
	DECLARE_DYNAMIC( CDecalObjectPanel )

public:
	CDecalObjectPanel( CWnd* pParent = 0 );
	virtual ~CDecalObjectPanel();

	void SetDecalObject( CDecalObject* pObj );

	enum { IDD = IDD_PANEL_DECAL };

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange( CDataExchange* pDX );
	virtual BOOL OnInitDialog();

	afx_msg void OnUpdateProjection();

	DECLARE_MESSAGE_MAP()

	CDecalObject* m_pDecalObj;
	CToolButton m_decalReorientateButton;
};


#endif // _DECALOBJECTPANEL_H_