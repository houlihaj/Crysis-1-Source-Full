////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   LightmapCompilerDialog.h
//  Version:     v1.00
//  Created:     22/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __LightmapCompilerDialog_h__
#define __LightmapCompilerDialog_h__
#pragma once

#include "XTToolkitPro.h"
#include "Controls\PropertyCtrl.h"

#include "SceneContext.h"

class CLightmapCompilerDialog : public CDialog, public IEditorNotifyListener
{
	DECLARE_DYNCREATE(CLightmapCompilerDialog)
public:
	CLightmapCompilerDialog();
	~CLightmapCompilerDialog();

	static void RegisterViewClass();
	enum { IDD = IDD_DATABASE };

	void SetupAndGenerate( bool bOnlySelected );
	void Generate( bool bOnlySelected );
	void SetupLightmapQuality( bool bOnlySelected );
	void ShowTexelDensity( bool bOnlySelected );
	void GenerateLightList();
	void GenerateLightVolume();
	void RestoreDefaultParams();

protected:
	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );
	void UpdateParams();

	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void PostNcDestroy();
	void DoDataExchange(CDataExchange* pDX);
	BOOL OnInitDialog();
	void FillSceneContext(XmlNodeRef pNode);

	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);


	DECLARE_MESSAGE_MAP()

private:

	CXTPTaskPanel m_wndTaskPanel;
	CPropertyCtrl m_wndProps;

	class CLightmapParamsUI* m_pParamsUI;
};

#endif // __LightmapCompilerDialog_h__
