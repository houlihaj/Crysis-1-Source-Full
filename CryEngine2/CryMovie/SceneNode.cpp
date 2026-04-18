////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   scenenode.cpp
//  Version:     v1.00
//  Created:     23/4/2002 by Lennert.
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SceneNode.h"
#include "AnimTrack.h"
#include "SelectTrack.h"
#include "EventTrack.h"
#include "ConsoleTrack.h"
#include "MusicTrack.h"
#include "SequenceTrack.h"
#include "ISystem.h"
#include "AnimCameraNode.h"
#include "Movie.h"

#include <ISound.h>
#include <IMusicSystem.h>
#include <IConsole.h>

#define s_nodeParamsInitialized s_nodeParamsInitializedScene
#define s_nodeParams s_nodeParamsSene
#define AddSupportedParam AddSupportedParamScene

namespace {
	bool s_nodeParamsInitialized = false;
	std::vector<IAnimNode::SParamInfo> s_nodeParams;

	void AddSupportedParam( const char *sName,int paramId,EAnimValue valueType )
	{
		IAnimNode::SParamInfo param;
		param.name = sName;
		param.paramId = paramId;
		param.valueType = valueType;
		s_nodeParams.push_back( param );
	}
}

//////////////////////////////////////////////////////////////////////////
CAnimSceneNode::CAnimSceneNode( IMovieSystem *sys )
: CAnimNode(sys)
{
	m_dwSupportedTracks = PARAM_BIT(APARAM_CAMERA) | PARAM_BIT(APARAM_EVENT) |
									PARAM_BIT(APARAM_SOUND1)|PARAM_BIT(APARAM_SOUND2)|PARAM_BIT(APARAM_SOUND3) |
									PARAM_BIT(APARAM_SEQUENCE) | PARAM_BIT(APARAM_CONSOLE) | PARAM_BIT(APARAM_MUSIC);
	m_pMovie=sys;
	for (int i=0;i<SCENE_SOUNDTRACKS;i++)
	{
		m_SoundInfo[i].nLastKey=-1;
		m_SoundInfo[i].pSound=NULL;
		m_SoundInfo[i].sLastFilename="";
	}
	m_bMusicMoodSet=false;
	m_lastCameraKey = -1;
	m_lastEventKey = -1;
	m_lastConsoleKey = -1;
	m_lastMusicKey = -1;
	m_lastSequenceKey = -1;
	m_currentCameraEntityId = 0;

	m_pParentSequence = NULL;

	if (!s_nodeParamsInitialized)
	{
		s_nodeParamsInitialized = true;
		AddSupportedParam( "Camera",APARAM_CAMERA,AVALUE_SELECT );
		AddSupportedParam( "Event",APARAM_EVENT,AVALUE_EVENT );
		AddSupportedParam( "Sound1",APARAM_SOUND1,AVALUE_SOUND );
		AddSupportedParam( "Sound2",APARAM_SOUND2,AVALUE_SOUND );
		AddSupportedParam( "Sound3",APARAM_SOUND3,AVALUE_SOUND );
		AddSupportedParam( "Sequence",APARAM_SEQUENCE,AVALUE_SEQUENCE );
		AddSupportedParam( "Console",APARAM_CONSOLE,AVALUE_CONSOLE );
		AddSupportedParam( "Music",APARAM_MUSIC,AVALUE_MUSIC );
	}
}

//////////////////////////////////////////////////////////////////////////
CAnimSceneNode::~CAnimSceneNode()
{
	ReleaseSounds();
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::CreateDefaultTracks()
{
	CreateTrack(APARAM_CAMERA);
};

//////////////////////////////////////////////////////////////////////////
int CAnimSceneNode::GetParamCount() const
{
	return s_nodeParams.size();
}

//////////////////////////////////////////////////////////////////////////
bool CAnimSceneNode::GetParamInfo( int nIndex, SParamInfo &info ) const
{
	if (nIndex >= 0 && nIndex < (int)s_nodeParams.size())
	{
		info = s_nodeParams[nIndex];
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CAnimSceneNode::GetParamInfoFromId( int paramId, SParamInfo &info ) const
{
	for (int i = 0; i < (int)s_nodeParams.size(); i++)
	{
		if (s_nodeParams[i].paramId == paramId)
		{
			info = s_nodeParams[i];
			return true;
		}
	}
	return false;
}

/*
//////////////////////////////////////////////////////////////////////////
IAnimBlock* CAnimSceneNode::CreateAnimBlock()
{
	// Assign tracks to this anim node.
	IAnimBlock *block = new CAnimBlock;
	CreateTrack(block, APARAM_CAMERA);
	return block;
};*/

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::Activate( bool bActivate )
{
	if (bActivate)
	{
		CAnimBlock *pAnimBlock = (CAnimBlock*)GetAnimBlock();

		if (pAnimBlock != NULL)
		{
			int trackCount = pAnimBlock->GetTrackCount();

			for (int i = 0; i < trackCount; i++)
			{
				int trackType;
				IAnimTrack *pTrack;

				if (!pAnimBlock->GetTrackInfo(i,trackType,&pTrack) || trackType != APARAM_SEQUENCE)
				{
					continue;
				}

				CSequenceTrack *pSequenceTrack = (CSequenceTrack *)pTrack;

				for (int currKey = 0; currKey < pSequenceTrack->GetNumKeys(); currKey++)
				{
					ISequenceKey key;

					pSequenceTrack->GetKey(currKey,&key);

					IAnimSequence *pSequence = GetMovieSystem()->FindSequence(key.szSelection);
					if (pSequence)
					{
						if (key.bOverrideTimes)
						{
							key.fDuration = key.fEndTime-key.fStartTime > 0.0f ? key.fEndTime-key.fStartTime : 0.0f;
						}
						else
						{
							key.fDuration = pSequence->GetTimeRange().Length();
						}
						pTrack->SetKey(currKey,&key);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::SyncUpdate( ESyncUpdateType type,SAnimContext &ac )
{
	CAnimBlock *anim = (CAnimBlock*)GetAnimBlock();
	if (!anim)
		return;

	if (ac.bResetting)
		return;

	if (m_lastSequenceKey < 0)
		return;

	int paramCount = anim->GetTrackCount();
	for (int paramIndex = 0; paramIndex < paramCount; paramIndex++)
	{
		int trackType;
		IAnimTrack *pTrack;
		if (!anim->GetTrackInfo( paramIndex,trackType,&pTrack ))
			continue;
		switch (trackType)
		{
			case APARAM_SEQUENCE:
				{
					CSequenceTrack * pSequenceTrack = (CSequenceTrack*)pTrack;
					ISequenceKey key;

					pSequenceTrack->GetKey(m_lastSequenceKey,&key);

					IAnimSequence *pSequence = GetMovieSystem()->FindSequence(key.szSelection);

					if (pSequence != NULL)
					{
						switch (type)
						{
							case eSUT_SyncParentWait:
								{
									GetMovieSystem()->SyncSequence(pSequence,type,0);
								}
								break;
							case eSUT_SyncParent:
								{
									float time = ac.time-key.time+(key.bOverrideTimes ? key.fStartTime : pSequence->GetTimeRange().start);
									float endTime = key.bOverrideTimes ? key.fEndTime : pSequence->GetTimeRange().end;

									if (time > endTime)
									{
										time = endTime;
									}

									GetMovieSystem()->SyncSequence(pSequence,type,time);

									if (ac.bSingleFrame && gEnv->pSystem->IsEditorMode())
									{
										SAnimContext acCurrent;
										acCurrent.dt = ac.dt;
										acCurrent.fps = ac.fps;
										acCurrent.time = time;
										acCurrent.bSingleFrame = ac.bSingleFrame;

										pSequence->Animate(acCurrent);
									}
								}
								break;
						}
					}
				}
				break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::Animate( SAnimContext &ec )
{
	CAnimBlock *anim = (CAnimBlock*)GetAnimBlock();
	if (!anim)
		return;

	if (ec.bResetting)
		return;

	CSelectTrack *cameraTrack = NULL;
	CEventTrack *pEventTrack = NULL;
	CSoundTrack *pSoundTrack[3] = { NULL,NULL,NULL };
	CSequenceTrack *pSequenceTrack = NULL;
	CConsoleTrack *pConsoleTrack = NULL;
	CMusicTrack *pMusicTrack = NULL;
	/*
	bool bTimeJump = false;
	if (ec.time < m_time)
		bTimeJump = true;
	*/

	int paramCount = anim->GetTrackCount();
	for (int paramIndex = 0; paramIndex < paramCount; paramIndex++)
	{
		int trackType;
		IAnimTrack *pTrack;
		if (!anim->GetTrackInfo( paramIndex,trackType,&pTrack ))
			continue;
		switch (trackType)
		{
		case APARAM_CAMERA: cameraTrack = (CSelectTrack*)pTrack; break;
		case APARAM_EVENT: pEventTrack = (CEventTrack*)pTrack; break;
		case APARAM_SOUND1: pSoundTrack[0] = (CSoundTrack*)pTrack; break;
		case APARAM_SOUND2: pSoundTrack[1] = (CSoundTrack*)pTrack; break;
		case APARAM_SOUND3: pSoundTrack[2] = (CSoundTrack*)pTrack; break;
		case APARAM_SEQUENCE: pSequenceTrack = (CSequenceTrack*)pTrack; break;
		case APARAM_CONSOLE: pConsoleTrack = (CConsoleTrack*)pTrack; break;
		case APARAM_MUSIC: pMusicTrack = (CMusicTrack*)pTrack; break;
		}
	}

	// Process entity track.
	if (cameraTrack)
	{
		ISelectKey key;
		int cameraKey = cameraTrack->GetActiveKey(ec.time,&key);
		if (cameraKey != m_lastCameraKey/* && cameraKey > m_lastCameraKey*/)
		{
//			if (!ec.bSingleFrame || key.time == ec.time) // If Single frame update key time must match current time.
				ApplyCameraKey( key,ec );
		}
		m_lastCameraKey = cameraKey;
	}

	if (pEventTrack)
	{
		IEventKey key;
		int nEventKey = pEventTrack->GetActiveKey(ec.time,&key);
		if (nEventKey != m_lastEventKey && nEventKey >= 0)
		{
//			if (!ec.bSingleFrame || key.time == ec.time) // If Single frame update key time must match current time.
				ApplyEventKey(key, ec);
		}
		m_lastEventKey = nEventKey;
	}

	if (pConsoleTrack)
	{
		IConsoleKey key;
		int nConsoleKey = pConsoleTrack->GetActiveKey(ec.time,&key);
		if (nConsoleKey != m_lastConsoleKey && nConsoleKey >= 0)
		{
			if (!ec.bSingleFrame || key.time == ec.time) // If Single frame update key time must match current time.
				ApplyConsoleKey(key, ec);
		}
		m_lastConsoleKey = nConsoleKey;
	}

	if (pMusicTrack)
	{
		IMusicKey key;
		int nMusicKey = pMusicTrack->GetActiveKey(ec.time,&key);
		if (nMusicKey != m_lastMusicKey && nMusicKey >= 0)
		{
			if (!ec.bSingleFrame || key.time == ec.time) // If Single frame update key time must match current time.
				ApplyMusicKey(key, ec);
		}
		m_lastMusicKey = nMusicKey;
	}

	if(m_pMovie->GetSystem()->GetISoundSystem())
	{
		for (int i=0;i<SCENE_SOUNDTRACKS;i++)
		{
			if (pSoundTrack[i])
			{
				ISoundKey key;
				int nSoundKey = pSoundTrack[i]->GetActiveKey(ec.time, &key);
				if (nSoundKey!=m_SoundInfo[i].nLastKey || key.time==ec.time || nSoundKey==-1 || ec.bSingleFrame)
				{
					//if (!ec.bSingleFrame || key.time == ec.time) // If Single frame update key time must match current time.
					{
						m_SoundInfo[i].nLastKey=nSoundKey;
						ApplySoundKey( pSoundTrack[i],nSoundKey,i, key, ec);
					}
				}
			}
		}
	}

	if (pSequenceTrack)
	{
		ISequenceKey key;
		int nSequenceKey = pSequenceTrack->GetActiveKey(ec.time,&key);
		IAnimSequence *pSequence = GetMovieSystem()->FindSequence(key.szSelection);

		if (nSequenceKey != m_lastSequenceKey || !GetMovieSystem()->IsPlaying(pSequence))
		{
//			if (!ec.bSingleFrame || key.time == ec.time) // If Single frame update key time must match current time.
				ApplySequenceKey(pSequenceTrack,m_lastSequenceKey,nSequenceKey,key, ec);
		}
		m_lastSequenceKey = nSequenceKey;
	}

	m_time = ec.time;
	if (m_pOwner)
	{
    m_pOwner->OnNodeAnimated(this);
	}else
	{
	}
}

void CAnimSceneNode::ReleaseSounds()
{
	// stop all sounds
	for (int i=0;i<SCENE_SOUNDTRACKS;i++)
	{
		if (m_SoundInfo[i].pSound)
			m_SoundInfo[i].pSound->Stop();
		m_SoundInfo[i].nLastKey=-1;
		m_SoundInfo[i].sLastFilename="";
		m_SoundInfo[i].pSound=NULL;
	}
	// enable music-event processing
	if (m_bMusicMoodSet)
	{
		IMusicSystem *pMusicSystem=GetMovieSystem()->GetSystem()->GetIMusicSystem();
		if (pMusicSystem)
			pMusicSystem->EnableEventProcessing(true);
		m_bMusicMoodSet=false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::Reset()
{
	// If camera from this sequence still active, remove it.
	// reset camera
	SCameraParams CamParams = m_pMovie->GetCameraParams();
	if (CamParams.cameraEntityId != 0 && CamParams.cameraEntityId == m_currentCameraEntityId)
	{
		CamParams.cameraEntityId = 0;
		CamParams.fFOV = 0;
		CamParams.justActivated = true;
		m_pMovie->SetCameraParams(CamParams);
	}

	if (m_lastSequenceKey >= 0)
	{
		CAnimBlock *pAnimBlock = (CAnimBlock*)GetAnimBlock();

		if (pAnimBlock != NULL)
		{
			int trackCount = pAnimBlock->GetTrackCount();

			for (int i = 0; i < trackCount; i++)
			{
				int trackType;
				IAnimTrack *pTrack;

				if (!pAnimBlock->GetTrackInfo(i,trackType,&pTrack) || trackType != APARAM_SEQUENCE)
				{
					continue;
				}

				CSequenceTrack *pSequenceTrack = (CSequenceTrack *)pTrack;
				ISequenceKey prevKey;

				pSequenceTrack->GetKey(m_lastSequenceKey,&prevKey);
				GetMovieSystem()->StopSequence(prevKey.szSelection);
			}
		}
	}

	m_lastCameraKey = -1;
	m_lastEventKey = -1;
	m_lastConsoleKey = -1;
	m_lastMusicKey = -1;
	m_lastSequenceKey = -1;
	m_currentCameraEntityId = 0;

	ReleaseSounds();
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::Pause()
{
	ReleaseSounds();
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplyCameraKey( ISelectKey &key,SAnimContext &ec )
{
	IAnimNode *cameraNode = GetMovieSystem()->FindNode(key.szSelection);

	SCameraParams CamParams;
	CamParams.cameraEntityId = 0;
	CamParams.fFOV = 0;
	CamParams.justActivated = true;

	IEntity *pEntity = gEnv->pEntitySystem->FindEntityByName(key.szSelection);
	if (pEntity)
		CamParams.cameraEntityId = pEntity->GetId();

	if (cameraNode && cameraNode->GetType() == ANODE_CAMERA)
	{
		float fov = 60.0f;
		cameraNode->GetParamValue( ec.time,APARAM_FOV,fov );
		CamParams.fFOV = DEG2RAD(fov);
	}
	else
	{
		/*
		if (strlen(key.szSelection) > 0)
		{
			m_pMovie->GetSystem()->Warning( VALIDATOR_MODULE_MOVIE,VALIDATOR_WARNING,0,0,
				"[CryMovie] Camera entity %s not found",(const char*)key.szSelection );
		}
		*/
	}
	m_currentCameraEntityId = CamParams.cameraEntityId;
	m_pMovie->SetCameraParams(CamParams);
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplyEventKey(IEventKey &key, SAnimContext &ec)
{
	char funcName[1024];
	strcpy(funcName, "Event_");
	strcat(funcName, key.event);
	m_pMovie->SendGlobalEvent(funcName);
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplySoundKey( IAnimTrack *pTrack,int nCurrKey,int nLayer, ISoundKey &key, SAnimContext &ec)
{
	if (m_SoundInfo[nLayer].nLastKey==-1)
	{
		if (m_SoundInfo[nLayer].pSound)
			m_SoundInfo[nLayer].pSound->Stop();
		m_SoundInfo[nLayer].pSound=NULL;
		return;
	}
	bool bNewSound = false;
	if (((strcmp(m_SoundInfo[nLayer].sLastFilename.c_str(), key.pszFilename) != 0) || (!m_SoundInfo[nLayer].pSound)))
	{
		int flags = 0;
		//if (key.bStream)
			//flags |= FLAG_SOUND_STREAM;
		//else
			//flags |= FLAG_SOUND_LOAD_SYNCHRONOUSLY; // Always synchronously for now.

		if (key.bLoop)
			flags |= FLAG_SOUND_LOOP;
		if (key.bVoice)
			flags |= FLAG_SOUND_VOICE;
		//if (key.b3DSound)
		{
			// Always 2D sound.
			flags |= FLAG_SOUND_2D|FLAG_SOUND_STEREO|FLAG_SOUND_16BITS;
		}

		// we have a different sound now
		if (m_SoundInfo[nLayer].pSound)
			m_SoundInfo[nLayer].pSound->Stop();
		
		if (gEnv->pSoundSystem)
			m_SoundInfo[nLayer].pSound = gEnv->pSoundSystem->CreateSound(key.pszFilename, flags|FLAG_SOUND_UNSCALABLE);

		m_SoundInfo[nLayer].sLastFilename = key.pszFilename;

		if (m_SoundInfo[nLayer].pSound)
		{
			if (key.bVoice)
				m_SoundInfo[nLayer].pSound->SetSemantic((ESoundSemantic)(eSoundSemantic_TrackView|eSoundSemantic_Dialog));
			else
				m_SoundInfo[nLayer].pSound->SetSemantic(eSoundSemantic_TrackView);

			m_SoundInfo[nLayer].nLength = m_SoundInfo[nLayer].pSound->GetLengthMs();
			key.fDuration = ((float)m_SoundInfo[nLayer].nLength) / 1000.0f;
			pTrack->SetKey( nCurrKey,&key ); // Update key duration.
		}
		bNewSound = true;
	}
	if (!m_SoundInfo[nLayer].pSound)
		return;
	
	if (bNewSound)
	{
		m_SoundInfo[nLayer].pSound->SetSoundPriority( MOVIE_SOUND_PRIORITY );
		m_SoundInfo[nLayer].pSound->SetVolume(key.fVolume);
		m_SoundInfo[nLayer].pSound->SetPan(key.nPan);
	}

	int nOffset=(int)((ec.time-key.time)*1000.0f);
	if (nOffset < m_SoundInfo[nLayer].nLength)
	{
		//return;
		m_SoundInfo[nLayer].pSound->SetCurrentSamplePos(nOffset, true);
	}
	if (m_SoundInfo[nLayer].nLength != 0 && nOffset > m_SoundInfo[nLayer].nLength)
	{
		// If time is outside of sound, do not start it.
		bNewSound = false;
	}
	
	if (bNewSound)
	{
		((CMovieSystem*)m_pMovie)->OnPlaySound( ec.sequence, m_SoundInfo[nLayer].pSound );
		if (!m_SoundInfo[nLayer].pSound->IsPlaying())
			m_SoundInfo[nLayer].pSound->Play();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplySequenceKey(  IAnimTrack *pTrack,int nPrevKey,int nCurrKey,ISequenceKey &key,SAnimContext &ec )
	{
		if (nPrevKey >= 0)
		{
			ISequenceKey prevKey;
			pTrack->GetKey(nPrevKey,&prevKey);
			GetMovieSystem()->StopSequence( prevKey.szSelection );
		}

	if (nCurrKey >= 0 && key.szSelection[0])
	{
		IAnimSequence *pSequence = GetMovieSystem()->FindSequence(key.szSelection);
		float startTime = -FLT_MAX;
		float endTime = -FLT_MAX;
		float currentTime = -FLT_MAX;
		if (pSequence)
		{
			if (key.bOverrideTimes)
			{
				key.fDuration = key.fEndTime-key.fStartTime > 0.0f ? key.fEndTime-key.fStartTime : 0.0f;
				startTime = key.fStartTime;
				endTime = key.fEndTime;
				currentTime = ec.time-key.time+key.fStartTime;
			}
			else
			{
				key.fDuration = pSequence->GetTimeRange().Length();
				currentTime = ec.time-key.time+pSequence->GetTimeRange().start;
			}
			pTrack->SetKey( nCurrKey,&key );
		}
		GetMovieSystem()->PlaySequence(key.szSelection,m_pParentSequence,true,true,currentTime,startTime,endTime);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplyConsoleKey(IConsoleKey &key, SAnimContext &ec)
{
	if (key.command[0])
	{
		GetMovieSystem()->GetSystem()->GetIConsole()->ExecuteString( key.command );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplyMusicKey(IMusicKey &key, SAnimContext &ec)
{
	IMusicSystem *pMusicSystem=GetMovieSystem()->GetSystem()->GetIMusicSystem();
	if (!pMusicSystem)
		return;
	switch (key.eType)
	{
		case eMusicKeyType_SetMood:
			m_bMusicMoodSet=true;
			pMusicSystem->EnableEventProcessing(false);
			pMusicSystem->SetMood(key.szMood);
			break;
		case eMusicKeyType_VolumeRamp:
			break;
	}
}

#undef s_nodeParamsInitialized
#undef s_nodeParams
#undef AddSupportedParam

