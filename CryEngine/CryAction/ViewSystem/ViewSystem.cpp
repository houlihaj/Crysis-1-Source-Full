/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 17:9:2004 : Created by Filippo De Luca
    24:11:2005: added movie system (Craig Tiller)
*************************************************************************/
#include "StdAfx.h"

#include <Cry_Camera.h>
#include "ViewSystem.h"
#include "GameObjects/GameObject.h"
#include "CryAction.h"
#include "IActorSystem.h"
#include "PNoise3.h"
#include <ISoundMoodManager.h>
#include <IAISystem.h>

#define VS_CALL_LISTENERS(func) \
{ \
	size_t count = m_listeners.size(); \
	if (count > 0) \
	{ \
		const size_t memSize = count*sizeof(IViewSystemListener*); \
		IViewSystemListener** pArray = (IViewSystemListener**) alloca(memSize); \
		memcpy(pArray, &*m_listeners.begin(), memSize); \
		while (count--) \
		{ \
			(*pArray)->func; ++pArray; \
		} \
	}	\
}

//------------------------------------------------------------------------
CViewSystem::CViewSystem(ISystem *pSystem) :
	m_pSystem(pSystem),
	m_activeViewId(0),
	m_nextViewIdToAssign(1000),
	m_preSequenceViewId(0),
	m_cutsceneViewId(0),
	m_cutsceneCount(0),
	m_cutsceneFacialAnimRadiusSave(30.0f),
	m_bOverridenCameraRotation(false),
	m_bActiveViewFromSequence(false),
	m_fBlendInPosSpeed(0.0f),
	m_fBlendInRotSpeed(0.0f),
	m_bPerformBlendOut(false)
{	
	pSystem->GetIConsole()->Register("cl_camera_noise",&m_fCameraNoise,-1,0,
		"Adds hand-held like camera noise to the camera view. \n The higher the value, the higher the noise.\n A value <= 0 disables it.");
	pSystem->GetIConsole()->Register("cl_camera_noise_freq",&m_fCameraNoiseFrequency,2.5326173f,0,
		"Defines camera noise frequency for the camera view. \n The higher the value, the higher the noise.\n");
	pSystem->GetIConsole()->Register("cl_camera_nearz",&m_fDefaultCameraNearZ,DEFAULT_NEAR,VF_CHEAT,
		"Overrides default near z-range for camera.\n");
}

//------------------------------------------------------------------------
CViewSystem::~CViewSystem()
{
	for (TViewMap::iterator it=m_views.begin();it!=m_views.end();++it)
	{
		CView *pView = it->second;

		delete pView;
	}		
}

//------------------------------------------------------------------------
void CViewSystem::Update(float frameTime)
{
	if ( GetISystem()->IsDedicated() )
		return;

	IView * pActiveView = GetActiveView();

	for (TViewMap::iterator it=m_views.begin();it!=m_views.end();++it)
	{
		CView *pView = it->second;
		bool isActive = (pView == pActiveView);

		pView->Update(frameTime,isActive);

		if (isActive)
		{
			CCamera cam = pView->GetCamera();

			SViewParams currentParams = *(pView->GetCurrentParams());

			cam.SetJustActivated(currentParams.justActivated);

			currentParams.justActivated = false;
			pView->SetCurrentParams(currentParams);

			if (m_bOverridenCameraRotation)
			{
				// When cammera rotation is overriden.
				Vec3 pos = cam.GetMatrix().GetTranslation();
				Matrix34 camTM(m_overridenCameraRotation);
				camTM.SetTranslation(pos);
				cam.SetMatrix(camTM);
			}
			else
			{
				// Normal setting of the camera

				if (m_fCameraNoise>0)
				{
					Matrix33 m = Matrix33(cam.GetMatrix());
					m.OrthonormalizeFast();
					Ang3 aAng1 = Ang3::GetAnglesXYZ(m);
					//Ang3 aAng2 = RAD2DEG(aAng1);

					Matrix34 camTM=cam.GetMatrix();
					Vec3 pos = camTM.GetTranslation(); 
					camTM.SetIdentity();

					const float fScale=0.1f;
					CPNoise3 *pNoise=m_pSystem->GetNoiseGen();
					float fRes=pNoise->Noise1D(m_pSystem->GetITimer()->GetCurrTime()*m_fCameraNoiseFrequency);
					aAng1.x+=fRes*m_fCameraNoise*fScale;
					pos.z-=fRes*m_fCameraNoise*fScale;
					fRes=pNoise->Noise1D(17+m_pSystem->GetITimer()->GetCurrTime()*m_fCameraNoiseFrequency);
					aAng1.y-=fRes*m_fCameraNoise*fScale;
					
					//aAng1.z+=fRes*0.025f; // left / right movement should be much less visible

					camTM.SetRotationXYZ(aAng1);
					camTM.SetTranslation(pos);				
					cam.SetMatrix(camTM);
				}
			}

			m_pSystem->SetViewCamera(cam);
		}
	}

	// perform dynamic color grading
	if (!IsPlayingCutScene())
	{
		SAIDetectionLevels detection;
		gEnv->pAISystem->GetDetectionLevels(NULL, detection);
		float factor = detection.puppetThreat;

		// marcok: magic numbers taken from Maciej's tweaked values (final - initial)
		{
			float percentage = (0.0851411f - 0.0509998f)/0.0509998f * factor;
			float amount = 0.0f;
			gEnv->p3DEngine->GetPostEffectParam("ColorGrading_GrainAmount", amount);
			gEnv->p3DEngine->SetPostEffectParam("ColorGrading_GrainAmount_Offset", amount*percentage);
		}
		{
			float percentage = (0.405521f - 0.256739f)/0.256739f * factor;
			float amount = 0.0f;
			gEnv->p3DEngine->GetPostEffectParam("ColorGrading_SharpenAmount", amount);
			//gEnv->p3DEngine->SetPostEffectParam("ColorGrading_SharpenAmount_Offset", amount*percentage);
		}
		{
			float percentage = (0.14f - 0.11f)/0.11f * factor;
			float amount = 0.0f;
			gEnv->p3DEngine->GetPostEffectParam("ColorGrading_PhotoFilterColorDensity", amount);
			gEnv->p3DEngine->SetPostEffectParam("ColorGrading_PhotoFilterColorDensity_Offset", amount*percentage);
		}
		{
			float percentage = (234.984f - 244.983f)/244.983f * factor;
			float amount = 0.0f;
			gEnv->p3DEngine->GetPostEffectParam("ColorGrading_maxInput", amount);
			//gEnv->p3DEngine->SetPostEffectParam("ColorGrading_maxInput_Offset", amount*percentage);
		}
		{
			float percentage = (239.984f - 247.209f)/247.209f * factor;
			float amount = 0.0f;
			gEnv->p3DEngine->GetPostEffectParam("ColorGrading_maxOutput", amount);
			//gEnv->p3DEngine->SetPostEffectParam("ColorGrading_maxOutput_Offset", amount*percentage);
		}
		{
			Vec4 dest(0.0f/255.0f, 22.0f/255.0f, 33.0f/255.0f, 1.0f);
			Vec4 initial(2.0f/255.0f, 154.0f/255.0f, 226.0f/255.0f, 1.0f);
			Vec4 percentage = (dest - initial)/initial * factor;
			Vec4 amount;
			gEnv->p3DEngine->GetPostEffectParamVec4("clr_ColorGrading_SelectiveColor", amount);
			//gEnv->p3DEngine->SetPostEffectParamVec4("clr_ColorGrading_SelectiveColor_Offset", amount*percentage);
		}
		{
			float percentage = (-5.0083 - -1.99999)/-1.99999 * factor;
			float amount = 0.0f;
			gEnv->p3DEngine->GetPostEffectParam("ColorGrading_SelectiveColorCyans", amount);
			//gEnv->p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorCyans_Offset", amount*percentage);
		}
		{
			float percentage = (10.0166 - -5.99999)/-5.99999 * factor;
			float amount = 0.0f;
			gEnv->p3DEngine->GetPostEffectParam("ColorGrading_SelectiveColorMagentas", amount);
			//gEnv->p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorMagentas_Offset", amount*percentage);
		}
		{
			float percentage = (10.0166 - 1.99999)/1.99999 * factor;
			float amount = 0.0f;
			gEnv->p3DEngine->GetPostEffectParam("ColorGrading_SelectiveColorYellows", amount);
			//gEnv->p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorYellows_Offset", amount*percentage);
		}
	}
}

//------------------------------------------------------------------------
IView *CViewSystem::CreateView()
{
	CView *newView = new CView(m_pSystem);

	if (newView)
	{
		m_views.insert(TViewMap::value_type(m_nextViewIdToAssign, newView));
		++m_nextViewIdToAssign;
	}

	return newView;
}

//------------------------------------------------------------------------
void CViewSystem::SetActiveView(IView *pView)
{
	m_activeViewId = GetViewId(pView);
	m_bActiveViewFromSequence = false;
}

//------------------------------------------------------------------------
void CViewSystem::SetActiveView(unsigned int viewId)
{
	if (GetView(viewId))
	{
		m_activeViewId = viewId;
		m_bActiveViewFromSequence = false;
	}
}

//------------------------------------------------------------------------
IView *CViewSystem::GetView(unsigned int viewId)
{
	TViewMap::iterator it = m_views.find(viewId);

	if (it != m_views.end())
	{
		return it->second;
	}

	return NULL;
}

//------------------------------------------------------------------------
IView *CViewSystem::GetActiveView()
{
	return GetView(m_activeViewId);
}

//------------------------------------------------------------------------
unsigned int CViewSystem::GetViewId(IView *pView)
{
	for (TViewMap::iterator it=m_views.begin();it!=m_views.end();++it)
	{
		IView *tView = it->second;
    
		if (tView == pView)
			return it->first;
	}

	return 0;
}

//------------------------------------------------------------------------
unsigned int CViewSystem::GetActiveViewId()
{
	// cutscene can override the games id of the active view
	if (m_cutsceneCount && m_cutsceneViewId)
		return m_cutsceneViewId;
	return m_activeViewId;
}

//------------------------------------------------------------------------
IView *CViewSystem::GetViewByEntityId(unsigned int id, bool forceCreate)
{
	for (TViewMap::iterator it=m_views.begin();it!=m_views.end();++it)
	{
		IView *tView = it->second;
    
		if (tView && tView->GetLinkedId() == id)
			return tView;
	}

	if (forceCreate)
	{
		if (IEntity * pEntity = gEnv->pEntitySystem->GetEntity(id))
		{
			if (CGameObject * pGameObject = (CGameObject*) pEntity->GetProxy(ENTITY_PROXY_USER))
			{
				if (IView * pNew = CreateView())
				{
					pNew->LinkTo(pGameObject);
					return pNew;
				}
			}
			else
			{
				if (IView * pNew = CreateView())
				{
					pNew->LinkTo(pEntity);
					return pNew;
				}
			}
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
void CViewSystem::SetActiveCamera( const SCameraParams &params )
{
	IView * pView = NULL;
	if (params.cameraEntityId)
	{
		pView = GetViewByEntityId( params.cameraEntityId, true );
		if (pView)
		{
			SViewParams viewParams = *pView->GetCurrentParams();
			viewParams.fov = params.fFOV;

			//char szDebug[256];
			//sprintf(szDebug,"pos=%0.2f,%0.2f,%0.2f,rot=%0.2f,%0.2f,%0.2f\n",viewParams.position.x,viewParams.position.y,viewParams.position.z,viewParams.rotation.v.x,viewParams.rotation.v.y,viewParams.rotation.v.z,viewParams.rotation.w);
			//OutputDebugString(szDebug);

			if (m_bActiveViewFromSequence==false && m_preSequenceViewId == 0)
			{
				m_preSequenceViewId = m_activeViewId;
				IView* pPrevView = GetView(m_activeViewId);
				if (pPrevView && m_fBlendInPosSpeed > 0.0f && m_fBlendInRotSpeed > 0.0f)
				{
					viewParams.blendPosSpeed = m_fBlendInPosSpeed;
					viewParams.blendRotSpeed = m_fBlendInRotSpeed;
					viewParams.BlendFrom(*pPrevView->GetCurrentParams());
				}
			}

			if (m_activeViewId != GetViewId(pView) && params.justActivated)
			{
				viewParams.justActivated = true;
			}

			pView->SetCurrentParams(viewParams);
			// make this one the active view
			SetActiveView(pView);
			m_bActiveViewFromSequence = true;
		}
	}
	else
	{
		if (m_preSequenceViewId != 0)
		{
			IView* pActiveView = GetView(m_activeViewId);
			IView* pNewView = GetView(m_preSequenceViewId);
			if (pActiveView && pNewView && m_bPerformBlendOut)
			{
				SViewParams activeViewParams = *pActiveView->GetCurrentParams();
				SViewParams newViewParams = *pNewView->GetCurrentParams();
				newViewParams.BlendFrom(activeViewParams);

				if (m_activeViewId != m_preSequenceViewId && params.justActivated)
				{
					newViewParams.justActivated = true;
				}

				pNewView->SetCurrentParams(newViewParams);
			}
			else if (m_activeViewId != m_preSequenceViewId && params.justActivated)
			{
				SViewParams activeViewParams = *pActiveView->GetCurrentParams();
				activeViewParams.justActivated = true;
				pNewView->SetCurrentParams(activeViewParams);
			}
			SetActiveView(m_preSequenceViewId);
			m_preSequenceViewId = 0;
			m_bActiveViewFromSequence = false;
		}
	}
	m_cutsceneViewId = GetViewId(pView);

	VS_CALL_LISTENERS(OnCameraChange(params));
}

//------------------------------------------------------------------------
void CViewSystem::BeginCutScene(IAnimSequence* pSeq, unsigned long dwFlags,bool bResetFX)
{
	m_cutsceneCount++;

	IConsole *pCon=gEnv->pConsole;

	if (IAnimSequence::NO_PLAYER&dwFlags)
	{
		HidePlayer(true);
	}

	if(IAnimSequence::NO_PHYSICS&dwFlags)
	{
		ICVar *pPhys=pCon->GetCVar("es_UpdatePhysics");
		if(pPhys)
			pPhys->Set(0);
	}

	if(IAnimSequence::NO_AI&dwFlags)
	{
		ICVar *pAIUpdate=pCon->GetCVar("ai_systemupdate");
		if(pAIUpdate)
			pAIUpdate->Set(0);
	}

	if (IAnimSequence::NO_TRIGGERS & dwFlags)
	{
		ICVar *pDisableTriggers=pCon->GetCVar("es_disabletriggers");
		if(pDisableTriggers)
			pDisableTriggers->Set(1);
	}

	if (m_cutsceneCount == 1)
	{	
		ICVar *pFacialAnimationRadius=pCon->GetCVar("ca_FacialAnimationRadius");
		if (pFacialAnimationRadius)
		{
			m_cutsceneFacialAnimRadiusSave = pFacialAnimationRadius->GetFVal();
			pFacialAnimationRadius->Set(30.0f);
		}

		gEnv->p3DEngine->ResetPostEffects();
	}

	VS_CALL_LISTENERS(OnBeginCutScene(pSeq, bResetFX));

/* TODO: how do we pause the game
	IScriptSystem *pSS=m_pGame->GetScriptSystem();
	_SmartScriptObject pClientStuff(pSS,true);
	if(pSS->GetGlobalValue("ClientStuff",pClientStuff)){
		pSS->BeginCall("ClientStuff","OnPauseGame");
		pSS->PushFuncParam(pClientStuff);
		pSS->EndCall();
	}
*/

/* TODO: how do we block keys
	// do not allow the player to mess around with player's keys
	// during a cutscene	
	GetISystem()->GetIInput()->GetIKeyboard()->ClearKeyState();
	m_pGame->m_pIActionMapManager->SetActionMap("player_dead");
	m_pGame->AllowQuicksave(false);
*/


/* TODO: how do we pause sounds?
	// Sounds are not stopped or cut automatically, code needs to listen to cutscene begin/end event
	// Cutscenes have dedicated foley sounds so to lower the volume of the rest a soundmood is used
*/

	if (gEnv->pSoundSystem && (IAnimSequence::NO_GAMESOUNDS & dwFlags))
	{
		gEnv->pSoundSystem->SetMovieFadeoutVolume(0.0f);
	}

/* TODO: how do we reset FX?
	if (bResetFx)
	{
		m_pGame->m_p3DEngine->ResetScreenFx();
		ICVar *pResetScreenEffects=pCon->GetCVar("r_ResetScreenFx");
		if(pResetScreenEffects)
		{
		pResetScreenEffects->Set(1);
		}
	}
*/

  if(gEnv->p3DEngine)
    gEnv->p3DEngine->ProposeContentPrecache();
}

//------------------------------------------------------------------------
void CViewSystem::EndCutScene(IAnimSequence* pSeq, unsigned long dwFlags)
{
	m_cutsceneCount-=(m_cutsceneCount>0);

	IConsole *pCon=gEnv->pConsole;

	if (IAnimSequence::NO_PLAYER & dwFlags)
	{
	HidePlayer(false);
	}

	if (IAnimSequence::NO_PHYSICS & dwFlags)
	{
		ICVar *pPhys=pCon->GetCVar("es_UpdatePhysics");
		if(pPhys)
			pPhys->Set(1);
	}

	if (IAnimSequence::NO_AI & dwFlags)
	{
		ICVar *pAIUpdate=pCon->GetCVar("ai_systemupdate");
		if(pAIUpdate)
			pAIUpdate->Set(1);
	}

	if (IAnimSequence::NO_TRIGGERS & dwFlags)
	{
		ICVar *pDisableTriggers=pCon->GetCVar("es_disabletriggers");
		if(pDisableTriggers)
			pDisableTriggers->Set(0);
	}

	if (m_cutsceneCount == 0)
	{	
		ICVar *pFacialAnimationRadius=pCon->GetCVar("ca_FacialAnimationRadius");
		if (pFacialAnimationRadius)
		{
			pFacialAnimationRadius->Set(m_cutsceneFacialAnimRadiusSave);
		}

		gEnv->p3DEngine->ResetPostEffects();
	}

	VS_CALL_LISTENERS(OnEndCutScene(pSeq));

// TODO: reimplement
//	m_pGame->AllowQuicksave(true);

/* TODO: how to resume game
	if (!m_pGame->IsServer())
	{
		IScriptSystem *pSS=m_pGame->GetScriptSystem();
		_SmartScriptObject pClientStuff(pSS,true);
		if(pSS->GetGlobalValue("ClientStuff",pClientStuff)){
			pSS->BeginCall("ClientStuff","OnResumeGame");
			pSS->PushFuncParam(pClientStuff);
			pSS->EndCall();
		}
	}
*/

	/* TODO: how do we pause sounds?
	// Sounds are not stopped or cut automatically, code needs to listen to cutscene begin/end event
	// Cutscenes have dedicated foley sounds so to lower the volume of the rest a soundmood is used

	if (m_bSoundsPaused)
	{
	}
*/
	if (gEnv->pSoundSystem && (IAnimSequence::NO_GAMESOUNDS & dwFlags))
	{
		gEnv->pSoundSystem->SetMovieFadeoutVolume(1.0f);
	}

/* TODO: resolve input difficulties
	m_pGame->m_pIActionMapManager->SetActionMap("default");
	GetISystem()->GetIInput()->GetIKeyboard()->ClearKeyState();	
*/

/* TODO: weird game related stuff
	// we regenerate stamina fpr the local payer on cutsceen end - supposendly he was idle long enough to get it restored
	if (m_pGame->GetMyPlayer())
	{
		CPlayer *pPlayer;
		if (m_pGame->GetMyPlayer()->GetContainer()->QueryContainerInterface(CIT_IPLAYER,(void**)&pPlayer))
		{
			pPlayer->m_stats.stamina = 100;
		}
	}

	// reset subtitles
	m_pGame->m_pClient->ResetSubtitles();
*/
}

void CViewSystem::HidePlayer( bool hide )
{
	if (IActor * pActor = CCryAction::GetCryAction()->GetClientActor())
		if (IEntity * pEntity = pActor->GetEntity())
			pEntity->Hide(hide);
	ICVar *pAIIgnorePlayer=gEnv->pConsole->GetCVar("ai_ignoreplayer");
	if(pAIIgnorePlayer)
		pAIIgnorePlayer->Set(hide);
}

void CViewSystem::SendGlobalEvent(const char *pszEvent)
{
	// TODO: broadcast to flowgraph/script system
}

void CViewSystem::PlaySubtitles( IAnimSequence* pSeq, ISound *pSound )
{
	VS_CALL_LISTENERS(OnPlayCutSceneSound(pSeq, pSound));
	// TODO: support subtitles
}

//////////////////////////////////////////////////////////////////////////
void CViewSystem::SetOverrideCameraRotation( bool bOverride,Quat rotation )
{
	m_bOverridenCameraRotation = bOverride;
	m_overridenCameraRotation = rotation;
}

void CViewSystem::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "ViewSystem");
	s->Add(*this);
	s->AddContainer(m_views);
	for (TViewMap::iterator iter = m_views.begin(); iter != m_views.end(); ++iter)
	{
		iter->second->GetMemoryStatistics(s);
	}
}

void CViewSystem::Serialize(TSerialize ser)
{
	TViewMap::iterator iter = m_views.begin();
	TViewMap::iterator iterEnd = m_views.end();
	while (iter != iterEnd)
	{
		iter->second->Serialize(ser);
		++iter;
	}
}
