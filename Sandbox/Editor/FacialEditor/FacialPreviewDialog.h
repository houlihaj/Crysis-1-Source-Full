////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FacialPreviewDialog.h
//  Version:     v1.00
//  Created:     5/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FacialPreviewDialog_h__
#define __FacialPreviewDialog_h__
#pragma once

#include "ToolbarDialog.h"
#include "IFacialEditor.h"
#include "FacialEdContext.h"

class CModelViewportCE;
class CFacialEdContext;
class CModelViewportFE;

//////////////////////////////////////////////////////////////////////////
class CFacialPreviewDialog : public CToolbarDialog, public IFacialEdListener
{
	DECLARE_DYNAMIC(CFacialPreviewDialog)
public:
	static void RegisterViewClass();

	CFacialPreviewDialog();
	~CFacialPreviewDialog();

	enum { IDD = IDD_DATABASE };

	void SetContext( CFacialEdContext *pContext );
	void RedrawPreview();

	CModelViewportCE* GetViewport();
	const CVarObject* GetVarObject() const { return &m_vars; }

	void SetForcedNeckRotation(const Quat& rotation);
	void SetForcedEyeRotation(const Quat& rotation, IFacialEditor::EyeType eye);

	void SetAnimateCamera(bool bAnimateCamera);
	bool GetAnimateCamera() const;

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK() {};
	virtual void OnCancel() {};

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

	afx_msg void OnCenterOnHead();

private:
	virtual void OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels);

	void OnLookIKChanged(IVariable* var);
	void OnLookIKEyesOnlyChanged(IVariable* var);
	void OnShowEyeVectorsChanged(IVariable* var);
	void OnLookIKOffsetChanged(IVariable* var);
	void OnProceduralAnimationChanged(IVariable* var);

	CModelViewportFE* m_pModelViewport;
	CXTPToolBar m_wndToolBar;

	CFacialEdContext *m_pContext;

	CVariable<bool> m_bLookIK;
	CVariable<bool> m_bLookIKEyesOnly;
	CVariable<bool> m_bShowEyeVectors;
	CVariable<float> m_fLookIKOffsetX;
	CVariable<float> m_fLookIKOffsetY;
	CVariable<bool> m_bProceduralAnimation;
	CVarObject m_vars;
	HACCEL m_hAccelerators;
};

#endif // __FacialPreviewDialog_h__
