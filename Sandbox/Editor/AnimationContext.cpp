////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   animationcontext.cpp
//  Version:     v1.00
//  Created:     7/5/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AnimationContext.h"

#include "IMovieSystem.h"
#include "IMusicSystem.h"
#include "ITimer.h"
#include "GameEngine.h"

#include "Objects\SelectionGroup.h"
#include "Objects\Entity.h"

#include "IObjectManager.h"
#include "TrackViewDialog.h"

#include "Viewport.h"
#include "ViewManager.h"

//using namespace std;

extern CTrackViewDialog *g_pTrackViewDialog;

//////////////////////////////////////////////////////////////////////////
// Movie Callback.
//////////////////////////////////////////////////////////////////////////
class CMovieCallback : public IMovieCallback
{
protected:
	virtual void OnMovieCallback( ECallbackReason reason,IAnimNode *pNode )
	{
		switch (reason)
		{
		case CBR_ADDNODE:
			switch (pNode->GetType())
			{
			case ANODE_ENTITY:
			case ANODE_CAMERA:
				{
					// Find owner entity.
					IEntity *pIEntity = pNode->GetEntity();
					if (pIEntity)
					{
						// Find owner editor entity.
						CEntity *pEntity = CEntity::FindFromEntityId( pIEntity->GetId() );
						pEntity->SetAnimNode(pNode);
					}
					else
					{
						EntityGUID *pEntityGuid = pNode->GetEntityGuid();
						if (pEntityGuid != 0 && *pEntityGuid == 0)
						{
							// if entity guid is 0, try to find anim-node by node id.
							CBaseObject *pObject = GetIEditor()->GetObjectManager()->FindAnimNodeOwner(pNode);
							if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntity)))
							{
								CEntity *pEntity = (CEntity*)pObject;
								pEntity->SetAnimNode(pNode);
							}
						}
					}
				}
			}
			break;
		case CBR_REMOVENODE:
			{
				IAnimNodeOwner *pOwner = pNode->GetNodeOwner();
				if (pOwner)
				{
					CEntity *pEntity = static_cast<CEntity*>(pOwner);
					pEntity->SetAnimNode(0);
				}
				if (g_pTrackViewDialog)
					g_pTrackViewDialog->InvalidateSequence();
			}
			break;
		}
	}
	void OnSetCamera( const SCameraParams &Params )
	{
		// Only switch camera when in Play mode.
		CAnimationContext *ac = GetIEditor()->GetAnimation();
//		if (!ac->IsPlaying())
//			return;

		GUID camObjId = GUID_NULL;
		if (Params.cameraEntityId != 0)
		{
			// Find owner editor entity.
			CEntity *pEditorEntity = CEntity::FindFromEntityId( Params.cameraEntityId );
			if (pEditorEntity)
				camObjId = pEditorEntity->GetId();
		}
		// Switch camera in active rendering view.
		GetIEditor()->GetViewManager()->SetCameraObjectId( camObjId );
	};
};

static CMovieCallback s_movieCallback;

//////////////////////////////////////////////////////////////////////////
CAnimationContext::CAnimationContext()
{
	m_paused = 0;
	m_playing = false;
	m_recording = false;
	m_timeRange.Set(0,0);
	m_timeMarker.Set(0,0);
	m_currTime = 0;
	m_bSyncWait = false;
	m_bSyncMusic = false;
	m_bForceUpdateInNextFrame = false;
	m_resetTime = 0;
	m_fTimeScale = 1.0f;
	m_sequence = 0;
	m_bLooping = false;
	m_bAutoRecording = false;
	m_fRecordingTimeStep = 0;
	m_bEncodeAVI = false;
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::Init()
{
	gEnv->pMovieSystem->SetCallback( &s_movieCallback );
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::SetSequence( IAnimSequence *seq )
{
	if (m_sequence)
	{
		//SetTime(0);
		m_sequence->Deactivate();

		if (m_playing)
		{
			IMovieUser *pMovieUser = GetIEditor()->GetMovieSystem()->GetUser();

			if (pMovieUser)
			{
				pMovieUser->EndCutScene(m_sequence,m_sequence->GetCutSceneFlags(true));
			}

			if (gEnv->pMusicSystem != NULL && m_sequence->GetParentSequence() == NULL && strcmp(m_sequence->GetMusicPatternName(),""))
			{
				gEnv->pMusicSystem->StopLastPattern(m_sequence->GetMusicPatternName());
			}

			m_bSyncWait = false;
			m_bSyncMusic = false;
		}
	}
	m_sequence = seq;

	if (m_sequence)
	{
		if (m_playing)
		{
			IMovieUser *pMovieUser = GetIEditor()->GetMovieSystem()->GetUser();

			if (pMovieUser)
			{
				pMovieUser->BeginCutScene(m_sequence,m_sequence->GetCutSceneFlags(),true);
			}

			if (gEnv->pMusicSystem != NULL && m_sequence->GetParentSequence() == NULL && strcmp(m_sequence->GetMusicPatternName(),"") && !m_paused)
			{
				gEnv->pMusicSystem->PlayPattern(m_sequence->GetMusicPatternName(),false,false);
				gEnv->pMusicSystem->SeekLastPattern(m_sequence->GetMusicPatternName(),m_currTime);
				m_bSyncWait = true;
			}
		}

		m_timeRange = m_sequence->GetTimeRange();
		m_timeMarker=m_timeRange;
		m_sequence->Activate();
		ForceAnimation();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::SetTime( float t )
{
	if (t < m_timeRange.start)
		t = m_timeRange.start;

	if (t > m_timeRange.end)
		t = m_timeRange.end;

	if (fabs(m_currTime-t) < 0.001f)
		return;

	m_currTime = t;
	m_fRecordingCurrTime = t;
	ForceAnimation();

	if (m_sequence)
	{
		if (m_playing)
		{
			if (gEnv->pMusicSystem != NULL && m_sequence->GetParentSequence() == NULL && strcmp(m_sequence->GetMusicPatternName(),""))
			{
				gEnv->pMusicSystem->SeekLastPattern(m_sequence->GetMusicPatternName(),m_currTime);
				m_bSyncWait = true;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::Pause()
{
	assert( m_paused >= 0 );
	m_paused++;
	
	if (m_recording)
		GetIEditor()->GetMovieSystem()->SetRecording(false);
	
	GetIEditor()->GetMovieSystem()->Pause();
	if (m_sequence)
	{
		m_sequence->Pause();

		if (gEnv->pMusicSystem != NULL && m_sequence->GetParentSequence() == NULL && strcmp(m_sequence->GetMusicPatternName(),""))
		{
			gEnv->pMusicSystem->StopLastPattern(m_sequence->GetMusicPatternName());
		}

		m_bSyncWait = false;
		m_bSyncMusic = false;
	}

	if (m_bEncodeAVI)
		PauseAVIEncoding(true);
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::Resume()
{
	assert( m_paused > 0 );
	m_paused--;
	if (m_recording && m_paused == 0)
		GetIEditor()->GetMovieSystem()->SetRecording(true);
	GetIEditor()->GetMovieSystem()->Resume();
	if (m_sequence)
	{
		m_sequence->Resume();

		if (gEnv->pMusicSystem != NULL && m_sequence->GetParentSequence() == NULL && strcmp(m_sequence->GetMusicPatternName(),""))
		{
			gEnv->pMusicSystem->PlayPattern(m_sequence->GetMusicPatternName(),false,false);
			gEnv->pMusicSystem->SeekLastPattern(m_sequence->GetMusicPatternName(),m_currTime);
			m_bSyncWait = true;
		}
	}

	if (m_bEncodeAVI)
		PauseAVIEncoding(false);
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::SetRecording( bool recording )
{
	if (recording == m_recording)
		return;
	m_paused = 0;
	m_recording = recording;
	m_playing = false;

	if (!recording && m_fRecordingTimeStep != 0)
		SetAutoRecording( false,0 );

	// If started recording, assume we have modified the document.
	GetIEditor()->SetModifiedFlag();

	GetIEditor()->GetMovieSystem()->SetRecording(recording);

	if (m_bEncodeAVI)
		StopAVIEncoding();

	if (!recording)
	{
		GUID guid;
		ZeroStruct(guid);
		GetIEditor()->GetViewManager()->SetCameraObjectId( guid );
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CAnimationContext::SetPlaying( bool playing )
{
	if (playing == m_playing)
		return;

	m_paused = 0;
	m_playing = playing;
	m_recording = false;
	GetIEditor()->GetMovieSystem()->SetRecording(false);

	if (playing)
	{
		IMovieSystem *pMovieSystem = GetIEditor()->GetMovieSystem();

		pMovieSystem->Resume();
		if (m_sequence)
			m_sequence->Resume();

		if (m_sequence)
		{
			IMovieUser *pMovieUser = pMovieSystem->GetUser();

			if (pMovieUser)
			{
				pMovieUser->BeginCutScene(m_sequence,m_sequence->GetCutSceneFlags(),true);
			}

			if (gEnv->pMusicSystem != NULL && m_sequence->GetParentSequence() == NULL && strcmp(m_sequence->GetMusicPatternName(),""))
			{
				gEnv->pMusicSystem->PlayPattern(m_sequence->GetMusicPatternName(),false,false);
				gEnv->pMusicSystem->SeekLastPattern(m_sequence->GetMusicPatternName(),m_currTime);
				m_bSyncWait = true;
			}
		}
		pMovieSystem->ResumeCutScenes();

		if (m_bEncodeAVI)
			StartAVIEncoding();
	}
	else
	{
		IMovieSystem *pMovieSystem = GetIEditor()->GetMovieSystem();

		pMovieSystem->Pause();
/*		GetIEditor()->GetMovieSystem()->SetSequenceStopBehavior( IMovieSystem::ONSTOP_GOTO_START_TIME );
		GetIEditor()->GetMovieSystem()->StopAllSequences();
		GetIEditor()->GetMovieSystem()->SetSequenceStopBehavior( IMovieSystem::ONSTOP_GOTO_END_TIME );*/
		if (m_sequence)
			m_sequence->Pause();

		pMovieSystem->PauseCutScenes();
		if (m_sequence)
		{
			IMovieUser *pMovieUser = pMovieSystem->GetUser();

			if (pMovieUser)
			{
				pMovieUser->EndCutScene(m_sequence,m_sequence->GetCutSceneFlags(true));
			}

			if (gEnv->pMusicSystem != NULL && m_sequence->GetParentSequence() == NULL && strcmp(m_sequence->GetMusicPatternName(),""))
			{
				gEnv->pMusicSystem->StopLastPattern(m_sequence->GetMusicPatternName());
			}

			m_bSyncWait = false;
			m_bSyncMusic = false;
		}

		if (m_bEncodeAVI)
			StopAVIEncoding();
	}
	/*
	if (!playing && m_sequence != 0)
		m_sequence->Reset();
	*/

/*	if (!playing)
	{
		GUID guid;
		ZeroStruct(guid);
		GetIEditor()->GetViewManager()->SetCameraObjectId( guid );
	}*/
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::Update()
{
	ITimer *pTimer = GetIEditor()->GetSystem()->GetITimer();

	if (m_bForceUpdateInNextFrame)
	{
		ForceAnimation();
		m_bForceUpdateInNextFrame = false;
	}

	if (m_paused > 0 || !(m_playing || m_bAutoRecording))
	{
		if (m_sequence)
		{
			SAnimContext ac;
			ac.dt = 0;
			ac.fps = pTimer->GetFrameRate();
			ac.time = m_currTime;
			ac.bSingleFrame = true;

			m_sequence->SyncUpdate(eSUT_NonPlaying,ac);
		}

		if (!m_recording)
			GetIEditor()->GetMovieSystem()->SyncUpdate();

		return;
	}

	if (!m_bAutoRecording)
	{
		float dt = 0.0f;
		bool bJustSynced = false;

		if (!m_bSyncWait || gEnv->pMusicSystem == NULL)
		{
			dt = pTimer->GetRealFrameTime(false);
			m_currTime += dt * m_fTimeScale;
		}

		if (gEnv->pMusicSystem != NULL)
		{
			if (!gEnv->pMusicSystem->GetStatus()->bPlaying)
			{
				m_bSyncMusic = true;
			}
			else if (m_bSyncMusic == true)
			{
				gEnv->pMusicSystem->SeekLastPattern(m_sequence->GetMusicPatternName(),m_currTime);

				m_bSyncMusic = false;
				m_bSyncWait = true;
			}
		}

		if (m_bSyncWait)
		{
			if (gEnv->pMusicSystem != NULL)
			{
				CTimeValue startTime;

				if (gEnv->pMusicSystem->GetLastPatternStartTime(m_sequence->GetMusicPatternName(),startTime))
				{
					if (startTime.GetValue() != 0 && (pTimer->GetAsyncTime()-startTime).GetSeconds() > m_currTime)
					{
						m_currTime = (pTimer->GetAsyncTime()-startTime).GetSeconds();
						m_bSyncWait = false;
						bJustSynced = true;
					}
				}
				else
				{
					m_bSyncWait = false;
					bJustSynced = true;
				}
			}
			else
			{
				m_bSyncWait = false;
			}
		}

		if (m_sequence != NULL)
		{
			SAnimContext ac;
			ac.dt = 0;
			ac.fps = pTimer->GetFrameRate();
			ac.time = m_currTime;
			ac.bSingleFrame = (!m_playing) || (m_fTimeScale!=1.0f);
			m_sequence->Animate( ac );

			if (m_bSyncWait)
			{
				m_sequence->SyncUpdate(eSUT_SyncParentWait,ac);
			}

			if (bJustSynced)
			{
				m_sequence->SyncUpdate(eSUT_SyncParent,ac);
			}
		}

		if (!m_recording)
			GetIEditor()->GetMovieSystem()->Update(dt);
	}
	else
	{
		float dt = pTimer->GetFrameTime();
		m_fRecordingCurrTime += dt*m_fTimeScale;
		if (fabs(m_fRecordingCurrTime-m_currTime) > m_fRecordingTimeStep)
			m_currTime += m_fRecordingTimeStep;
	}

	if (m_currTime > m_timeMarker.end)
	{
		if (m_bAutoRecording)
		{
			SetAutoRecording(false,0);
		}
		else
		{
			if (m_bLooping)
				m_currTime = m_timeMarker.start;
			else
				SetPlaying(false);
		}
	}

	if (m_bAutoRecording)
	{
		// This is auto recording mode.
		// Send sync with physics event to all selected entities.
		GetIEditor()->GetSelection()->SendEvent( EVENT_PHYSICS_GETSTATE );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::ForceAnimation()
{
	// Before animating node, pause recording.
	if (m_bAutoRecording)
		Pause();
	if (m_sequence)
	{
		SAnimContext ac;
		ac.dt = 0;
		ac.fps = GetIEditor()->GetSystem()->GetITimer()->GetFrameRate();
		ac.time = m_currTime;
		ac.bSingleFrame = true;
		m_sequence->Animate( ac );

		// Update child sequences
		m_sequence->SyncUpdate(eSUT_SyncParent,ac);
	}
	if (m_bAutoRecording)
		Resume();
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::ResetAnimations( bool bPlayOnLoad,bool bSeekAllToStart )
{
	if (m_sequence)
	{
		SetTime(m_resetTime);
		m_resetTime = 0;
		m_bForceUpdateInNextFrame = true;
	}
	GetIEditor()->GetMovieSystem()->Reset(bPlayOnLoad,bSeekAllToStart);
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::SetAutoRecording( bool bEnable,float fTimeStep )
{
	if (bEnable)
	{
		m_bAutoRecording = true;
		m_fRecordingTimeStep = fTimeStep;
		// Turn on fixed time step.
		ICVar *fixed_time_step = GetIEditor()->GetSystem()->GetIConsole()->GetCVar( "fixed_time_step" );
		if (fixed_time_step)
		{
			//fixed_time_step->Set( m_fRecordingTimeStep );
		}
		// Enables physics/ai.
		GetIEditor()->GetGameEngine()->SetSimulationMode(true);
		SetRecording(bEnable);
	}
	else
	{
		m_bAutoRecording = false;
		m_fRecordingTimeStep = 0;
		// Turn off fixed time step.
		ICVar *fixed_time_step = GetIEditor()->GetSystem()->GetIConsole()->GetCVar( "fixed_time_step" );
		if (fixed_time_step)
		{
			//fixed_time_step->Set( 0 );
		}
		// Disables physics/ai.
		GetIEditor()->GetGameEngine()->SetSimulationMode(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::PlayToAVI( bool bEnable,const char *aviFilename )
{
	if (bEnable && aviFilename)
	{
		m_bEncodeAVI = true;
		m_aviFilename = aviFilename;
	}
	else
	{
		m_bEncodeAVI = false;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAnimationContext::IsPlayingToAVI() const
{
	return m_bEncodeAVI;
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::StartAVIEncoding()
{
	CViewport* pViewport = GetIEditor()->GetViewManager()->GetActiveViewport();
	if (pViewport)
	{
		pViewport->StartAVIRecording( m_aviFilename );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::StopAVIEncoding()
{
	CViewport* pViewport = GetIEditor()->GetViewManager()->GetActiveViewport();
	if (pViewport)
	{
		pViewport->StopAVIRecording();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimationContext::PauseAVIEncoding( bool bPause )
{
	CViewport* pViewport = GetIEditor()->GetViewManager()->GetActiveViewport();
	if (pViewport)
	{
		pViewport->PauseAVIRecording( bPause );
	}
}