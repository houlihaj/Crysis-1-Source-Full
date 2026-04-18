////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SolidBrushSubObjPanel.h
//  Version:     v1.00
//  Created:     11/1/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: CSolidBrushSubObjPanel declaration file.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SolidBrushSubObjPanel_h__
#define __SolidBrushSubObjPanel_h__
#pragma once

#include "Controls\NumberCtrl.h"

class CSolidBrushObject;

// CSolidBrushSubObjPanel dialog
class CSolidBrushSubObjPanel : public CDialog
{
	DECLARE_DYNAMIC(CSolidBrushSubObjPanel)

public:
	CSolidBrushSubObjPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSolidBrushSubObjPanel();

	void SetObject( CSolidBrushObject *pObject );

	void SetCurrentMatId( int nMatId );

// Dialog Data
	enum { IDD = IDD_PANEL_SOLIDBRUSH_SUBOBJ };

protected:
	virtual void OnOK() {};
	virtual void OnCancel();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	afx_msg void OnVertexSnapToGrid();
	afx_msg void OnVertexDelete();
	afx_msg void OnFaceSplit();
	afx_msg void OnFaceDelete();
	afx_msg void OnSetMatId();
	afx_msg void OnSelectMatId();
	afx_msg void OnPivotToVertices();
	afx_msg void OnPivotToCenter();

	DECLARE_MESSAGE_MAP()

	_smart_ptr<CSolidBrushObject> m_pObject;
	
	CCustomButton m_btn[10];
	CNumberCtrl m_matIdCtrl;
};

#endif // __SolidBrushSubObjPanel_h__