////////////////////////////////////////////////////////////////////////////
//
//  CRYTEK Source File.
//  Copyright (C), CRYTEK Studios, 2001-2007.
// -------------------------------------------------------------------------
//  Created:     31/10/2007 by Benjamin.
//  Compilers:   Visual Studio.NET
//
////////////////////////////////////////////////////////////////////////////
#pragma once


#include "SceneStartup.h"

class CSceneController;


//////////////////////////////////////////////////////////////////////////
// Rendering scene w.r.t. viewer chosen in CSceneController
// Called by CLauncherWindow in OnPaint function
class CViewPort : public CStatic
{
public:
	CViewPort();

	void						Initialize(CSceneStartup* scene, CSceneController* pController);

	//////////////////////////////////////////////////////////////////////////
	// Chooses which point of view to render from. and adds stats HUD.
	void						Refresh();

	void						SetBarHeight(float height);
	float						GetBarHeight();
	float						GetMaxBarHeight();

private:
	bool						SetupRenderContext();
	void						PaintHUD();
	void						DrawScreenBars(float yOffset, int width, int height);

private: 
	bool						m_initialized;
	IRenderer*			m_pRenderer;
	CSceneStartup*	m_pScene;
	CSceneController*	m_pController;
	float						m_fBarHeight;
	float						m_fMaxBarHeight;

protected:
	afx_msg void		OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()
};
