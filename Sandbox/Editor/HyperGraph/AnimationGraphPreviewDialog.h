////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   AnimationGraphPreviewDialog.h
//  Version:     v1.00
//  Created:     5/5/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AnimationGraphPreviewDialog_h__
#define __AnimationGraphPreviewDialog_h__
#pragma once

#include "ToolbarDialog.h"

class CModelViewportCE;

//////////////////////////////////////////////////////////////////////////
class CAnimationGraphPreviewDialog : public CToolbarDialog
{
	DECLARE_DYNAMIC(CAnimationGraphPreviewDialog)
public:
	static void RegisterViewClass();

	CAnimationGraphPreviewDialog(const char* szCharacterName);
	~CAnimationGraphPreviewDialog();

	enum { IDD = IDD_DATABASE };

	void RedrawPreview();

	ICharacterInstance* GetCharacter();
	void SetCharacter(const char* szCharacterName);
	CModelViewportCE* GetModelViewport() {return m_pModelViewport;}

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);

private:
	CModelViewportCE* m_pModelViewport;
	string m_sCharacterName;
};

#endif // __FacialPreviewDialog_h__
