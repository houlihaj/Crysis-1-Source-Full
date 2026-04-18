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


#include <IHardwareMouse.h>
#include "ViewPort.h"
#include <IViewSystem.h>

enum EViewTarget
{
	VT_FIRST_PERSON,
	VT_SPECTATOR,
	VT_CAMERA
};


//////////////////////////////////////////////////////////////////////////
// Enables switching between different interactively controlled entities 
// which act as scene observers.
class CSceneController : public IViewSystemListener
{
public:
	CSceneController();
	~CSceneController();

	//////////////////////////////////////////////////////////////////////////
	// Sets up spectator and camera
	void						Initialize(CSceneStartup* scene, EViewTarget eViewTarget, CViewPort* pViewPort);

	//////////////////////////////////////////////////////////////////////////
	// Makes control of mouse and keyboard accessible in case of actor is an object controlled by CryEngine
	// Catches mouse cursor
	void						StartGameMode();

	//////////////////////////////////////////////////////////////////////////
	// Disables controls in CryEngine; Releases mouse cursor
	void						StopGameMode();

	void						ResetGameMode();
	bool						IsInGameMode();

	//////////////////////////////////////////////////////////////////////////
	// Called by CLauncherWindow; Computes camera movement
	void						Update();

	CCamera*				GetCamera();
	void						SetViewTarget(EViewTarget eViewTarget);
	EViewTarget			GetViewTarget();

private:
	void						SetPlayerPosAng(Vec3 pos,Vec3 viewDir);
	bool						CheckVirtualKey( int virtualKey );
	POINT						GetMouseDelta();
	void						UpdateMovieSequenceHUD();

private:
	bool OnBeginCutScene(IAnimSequence* pSeq, bool bResetFX);
	bool OnEndCutScene(IAnimSequence* pSeq);
	void OnPlayCutSceneSound(IAnimSequence* pSeq, ISound* pSound);
	bool OnCameraChange(const SCameraParams& cameraParams);

private:
	IAnimSequence*	m_currentCutScene;
	bool						m_isInGameMode;
	IEntity*				m_pEntity;
	CViewPort*			m_pViewPort;
	CCamera*				m_pCamera;
	EViewTarget			m_eViewTarget;
	CSceneStartup*	m_pScene;
};
