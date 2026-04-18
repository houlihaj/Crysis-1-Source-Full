#include "StdAfx.h"
#include "SceneController.h"


#include <IMovieSystem.h>
#include <IAnimatedCharacter.h>
#include <IActorSystem.h>
#include <IHardwareMouse.h>
#include "Launcher.h"
#include "LauncherWindow.h"


CSceneController::CSceneController()
{
	m_isInGameMode = false;
	m_pCamera = new CCamera();
}


CSceneController::~CSceneController()
{
}


void CSceneController::Initialize(CSceneStartup* pScene, EViewTarget eViewTarget, CViewPort* pViewPort)
{
	m_pScene = pScene;

	SetViewTarget(eViewTarget);
	m_pViewPort = pViewPort;

	// spectator setup

	SetPlayerPosAng(Vec3(584,264,20), Vec3(1,0,0));
	IActor * pClActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
	pClActor->ToggleThirdPerson();		

	// camera setup
	m_pCamera->SetPosition(Vec3(590,250,25));

	// prepare class to receive triggers from playing movie sequences
	m_pScene->GetGameFramework()->GetIViewSystem()->AddListener(this);
}


CCamera* CSceneController::GetCamera()
{
	return m_pCamera;
}


void CSceneController::SetViewTarget(EViewTarget eViewTarget)
{
	CMenu* menu = gTheApp.GetWindow()->GetMenu();

	menu->CheckMenuItem(ID_SPECTATOR_1STPERSON, MF_UNCHECKED);
	menu->CheckMenuItem(ID_VIEW_SPECTATOR, MF_UNCHECKED);
	menu->CheckMenuItem(ID_VIEW_CAMERA, MF_UNCHECKED);

	switch(eViewTarget)
	{
	case VT_FIRST_PERSON:
		menu->CheckMenuItem(ID_SPECTATOR_1STPERSON, MF_CHECKED);
		break;

	case VT_SPECTATOR:
		menu->CheckMenuItem(ID_VIEW_SPECTATOR, MF_CHECKED);
		break;

	case VT_CAMERA:
		menu->CheckMenuItem(ID_VIEW_CAMERA, MF_CHECKED);
		break;
	}

	m_eViewTarget = eViewTarget;
	ResetGameMode();
}


EViewTarget CSceneController::GetViewTarget()
{
	return m_eViewTarget;
}


void CSceneController::StartGameMode()
{
	if (IsInGameMode())
		StopGameMode();

	m_isInGameMode = true;

	gEnv->pHardwareMouse->SetGameMode(true);
	gEnv->pInput->SetExclusiveMode(eDI_Mouse, true, gTheApp.m_pMainWnd->m_hWnd);
	GetMouseDelta();

	if (GetViewTarget()==VT_SPECTATOR || GetViewTarget()==VT_FIRST_PERSON)
	{
		gEnv->pInput->EnableDevice(eDI_Mouse, true);
		gEnv->pInput->EnableDevice(eDI_Keyboard, true);
	}
	else
	{
		gEnv->pInput->EnableDevice(eDI_Mouse, false);
		gEnv->pInput->EnableDevice(eDI_Keyboard, false);
	}
}


void CSceneController::StopGameMode()
{
	if (!m_isInGameMode)
		return;

	m_isInGameMode = false;

	gEnv->pHardwareMouse->SetGameMode(false);	
	gEnv->pInput->EnableDevice(eDI_Mouse, false);
	gEnv->pInput->EnableDevice(eDI_Keyboard, false);
}


void CSceneController::ResetGameMode()
{
	if (!IsInGameMode())
		return;

	StopGameMode();
	StartGameMode();
}


bool CSceneController::IsInGameMode()
{
	return m_isInGameMode;
}


void CSceneController::SetPlayerPosAng(Vec3 pos,Vec3 viewDir)
{
	IActor * pClActor = gEnv->pGame->GetIGameFramework()->GetClientActor();

	if (pClActor)
	{
		// pos coming from editor is a camera position, we must convert it into the player position by subtructing eye height.
		IEntity *myPlayer = pClActor->GetEntity();

		if (myPlayer)
		{
			pe_player_dimensions dim;
			dim.heightEye = 0;
			if (myPlayer->GetPhysics())
			{
				myPlayer->GetPhysics()->GetParams( &dim );
				pos.z = pos.z - dim.heightEye;
			}
		}

		pClActor->GetEntity()->SetPosRotScale( pos,Quat::CreateRotationVDir(viewDir),Vec3(1,1,1),ENTITY_XFORM_EDITOR );
	}
}


bool CSceneController::CheckVirtualKey( int virtualKey )
{
	GetAsyncKeyState(virtualKey);
	if (GetAsyncKeyState(virtualKey) & (1<<15))
		return true;
	return false;
}


void CSceneController::Update()
{
	float frameDelta = gEnv->pSystem->GetITimer()->GetFrameTime();

	// camera movement

	if (GetViewTarget()==VT_CAMERA && IsInGameMode())
	{
		// compute speed scale
		float speedScale = 10.0f * frameDelta;
		if (speedScale > 10) speedScale = 10;

		// get camera position	
		Ang3 angles = m_pCamera->GetAngles();
		Vec3 pos = m_pCamera->GetPosition();

		// compute orientation
		POINT delta = GetMouseDelta();
		Matrix34 mx = Matrix34::CreateTranslationMat(pos);
		Matrix34 rotZ = Matrix34::CreateRotationZ(angles.x - delta.x * frameDelta/2);
		Matrix34 rotX = Matrix34::CreateRotationX(angles.y - delta.y * frameDelta/2);
		mx = mx * rotZ * rotX;

		Matrix34 posY = Matrix34::CreateTranslationMat(Vec3(0, speedScale, 0));
		Matrix34 negY = Matrix34::CreateTranslationMat(Vec3(0, -speedScale, 0));
		Matrix34 posX = Matrix34::CreateTranslationMat(Vec3(speedScale,0 , 0));
		Matrix34 negX = Matrix34::CreateTranslationMat(Vec3(-speedScale, 0, 0));		

		// compute movement

		if ( CheckVirtualKey(VK_UP) || CheckVirtualKey('W'))
			mx = mx * posY;

		if ( CheckVirtualKey(VK_DOWN) || CheckVirtualKey('S'))
			mx = mx * negY;

		if ( CheckVirtualKey(VK_RIGHT) || CheckVirtualKey('D'))
			mx = mx * posX;

		if ( CheckVirtualKey(VK_LEFT) || CheckVirtualKey('A'))
			mx = mx * negX;

		// write back movement
		m_pCamera->SetMatrix(mx);
	}

	UpdateMovieSequenceHUD();
}


bool CSceneController::OnBeginCutScene(IAnimSequence* pSeq, bool bResetFX)
{
	m_currentCutScene = pSeq;
	return true;
}


bool CSceneController::OnEndCutScene(IAnimSequence* pSeq)
{
	m_currentCutScene = NULL;
	return true;
}


void CSceneController::OnPlayCutSceneSound(IAnimSequence* pSeq, ISound* pSound)
{
}


bool CSceneController::OnCameraChange(const SCameraParams& cameraParams)
{
	return true;
}


void CSceneController::UpdateMovieSequenceHUD()
{
	float frameDelta = gEnv->pSystem->GetITimer()->GetFrameTime();
	float barHeight = m_pViewPort->GetBarHeight();
	float maxBarHeight = m_pViewPort->GetMaxBarHeight();

	// show black bars on 16 to 9 sequences
	if (m_currentCutScene!=NULL && m_currentCutScene->IS_16TO9)
	{
		if (gEnv->pMovieSystem->IsPlaying(m_currentCutScene) && barHeight<maxBarHeight)
		{
			barHeight += MIN(frameDelta*maxBarHeight*2, maxBarHeight);
			m_pViewPort->SetBarHeight(barHeight);
		}
	}	
	else if (barHeight>0) // remove black bars if previously visible
	{
		barHeight -= MAX(frameDelta*maxBarHeight*2,0);
		m_pViewPort->SetBarHeight(barHeight);
	}
}


POINT CSceneController::GetMouseDelta()
{
	CRect rect;
	GetClientRect(gTheApp.GetWindow()->m_hWnd, &rect);
	int midX = rect.left+rect.right/2;
	int midY = rect.top+rect.bottom/2;

	POINT p, delta;
	GetCursorPos(&p);
	SetCursorPos(midX,midY);

	delta.x = p.x - midX;
	delta.y = p.y - midY;

	return delta;
}
