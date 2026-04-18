////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   ScriptBind_Sound.cpp
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ScriptBind_Sound.h"
// TODO: remove this filth once sound groups is working
#include "../ScriptTable.h"
#include <ISystem.h>
#include <IConsole.h>
#include <ILog.h>
#include <ISound.h>
#include <IMusicSystem.h>
#include <IReverbManager.h>
#include <../CrySoundSystem/IAudioDevice.h>
#include <ISoundMoodManager.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CScriptBind_Sound::CScriptBind_Sound(IScriptSystem *pScriptSystem, ISystem *pSystem)
{
	m_pSystem = 0;
	m_pSystem = pSystem;
  m_pMusicSystem = gEnv->pMusicSystem; 
	m_pSoundSystem = gEnv->pSoundSystem;

	bMenuMusicLoaded = false;

	CScriptableBase::Init(pScriptSystem,pSystem);
	SetGlobalName( "Sound" );

#undef SCRIPT_REG_CLASSNAME 
#define SCRIPT_REG_CLASSNAME &CScriptBind_Sound::

	SCRIPT_REG_TEMPLFUNC(Precache,"sGroupAndSoundName,nPrecacheFlags");
	SCRIPT_REG_TEMPLFUNC(Play,"sGroupAndSoundName,vPos,nSoundFlags");
	SCRIPT_REG_TEMPLFUNC(PlayEx,"sGroupAndSoundName");
  SCRIPT_REG_TEMPLFUNC(SetParameterValue, "SoundID,sParameterName,fParameterValue");

	SCRIPT_REG_FUNC(Silence);

	SCRIPT_REG_FUNC(DeactivateAudioDevice);
	SCRIPT_REG_FUNC(ActivateAudioDevice);
	SCRIPT_REG_FUNC(UnloadProjects);

	SCRIPT_REG_FUNC(IsPlaying);
	SCRIPT_REG_FUNC(LoadSound);
	SCRIPT_REG_FUNC(Load3DSound);
	SCRIPT_REG_FUNC(LoadStreamSound);
	SCRIPT_REG_FUNC(PlaySound);	
	SCRIPT_REG_FUNC(SetSoundVolume);
	SCRIPT_REG_FUNC(GetSoundVolume);
	SCRIPT_REG_FUNC(SetSoundLoop);
	SCRIPT_REG_FUNC(SetSoundFrequency);
	SCRIPT_REG_FUNC(SetSoundPitching);
	SCRIPT_REG_FUNC(SetSoundRelative);
	SCRIPT_REG_FUNC(SetSoundPaused);
	
	SCRIPT_REG_FUNC(StopSound);
	SCRIPT_REG_FUNC(SetSoundPosition);

	SCRIPT_REG_FUNC(SetSoundSpeed);	
	SCRIPT_REG_FUNC(SetMinMaxDistance);
	SCRIPT_REG_FUNC(SetFadeTime);
	//SCRIPT_REG_FUNC(SetLoopPoints);	
	SCRIPT_REG_FUNC(SetMasterVolumeScale);
	SCRIPT_REG_FUNC(SetMasterMusicEffectsVolume);
	SCRIPT_REG_TEMPLFUNC(SetPauseAllPlaying,"bPause");
	
	// group stuff
	SCRIPT_REG_FUNC(AddToScaleGroup);
	SCRIPT_REG_FUNC(RemoveFromScaleGroup);
	SCRIPT_REG_FUNC(SetGroupScale);

	SCRIPT_REG_FUNC(RegisterWeightedEaxEnvironment); 
	SCRIPT_REG_FUNC(UpdateWeightedEaxEnvironment); 
	SCRIPT_REG_FUNC(UnregisterWeightedEaxEnvironment); 
	SCRIPT_REG_FUNC(FXEnable);
	SCRIPT_REG_FUNC(SetFXSetParamEQ);
	SCRIPT_REG_FUNC(SetDirectionalAttenuation);
	SCRIPT_REG_FUNC(GetDirectionalAttenuationMaxScale);

	// Music
	SCRIPT_REG_FUNC(LoadMusic);
	SCRIPT_REG_FUNC(UnloadMusic);
	SCRIPT_REG_FUNC(SerializeMusicInternal);
	SCRIPT_REG_FUNC(SetMusicTheme);
	SCRIPT_REG_FUNC(EndMusicTheme);
	SCRIPT_REG_FUNC(SetMusicMood);
	SCRIPT_REG_FUNC(SetDefaultMusicMood);
	SCRIPT_REG_FUNC(GetMusicThemes);
	SCRIPT_REG_FUNC(GetMusicMoods);
	SCRIPT_REG_FUNC(AddMusicMoodEvent);
	SCRIPT_REG_FUNC(IsInMusicTheme);
	SCRIPT_REG_FUNC(IsInMusicMood);
	SCRIPT_REG_FUNC(GetSoundLength);
	SCRIPT_REG_FUNC(GetMusicStatus);
	SCRIPT_REG_FUNC(SetMenuMusic);
	SCRIPT_REG_FUNC(PlayStinger);	
	SCRIPT_REG_FUNC(PlayPattern);	

	// Misc
	SCRIPT_REG_FUNC(SetSoundRatio);	
	SCRIPT_REG_FUNC(SetWeatherCondition);
	
	SCRIPT_REG_TEMPLFUNC(RegisterSoundMood,"sSoundMoodName"); 
	SCRIPT_REG_TEMPLFUNC(UpdateSoundMood,"sSoundMoodName, fFade");
	SCRIPT_REG_TEMPLFUNC(GetSoundMoodFade,"sSoundMoodName");
	SCRIPT_REG_TEMPLFUNC(UnregisterSoundMood,"sSoundMoodName"); 

	// Multi Listener
	SCRIPT_REG_TEMPLFUNC(CreateListener,"");
	SCRIPT_REG_TEMPLFUNC(RemoveListener,"nListenerID");
	SCRIPT_REG_TEMPLFUNC(SetListener,"nListenerID, vPos, vVel, vForward, vTop, bActive, fRecord");
	//SCRIPT_REG_TEMPLFUNC(SetListener,""); // keep for Debug

	m_pSS->SetGlobalValue("SOUND_DEFAULT_3D",FLAG_SOUND_DEFAULT_3D);

	
	
	m_pSS->SetGlobalValue("SOUND_LOOP",FLAG_SOUND_LOOP);	
	m_pSS->SetGlobalValue("SOUND_2D",FLAG_SOUND_2D);
	m_pSS->SetGlobalValue("SOUND_3D",FLAG_SOUND_3D);
	m_pSS->SetGlobalValue("SOUND_STEREO",FLAG_SOUND_STEREO);
	//
	m_pSS->SetGlobalValue("SOUND_STREAM",FLAG_SOUND_STREAM);
	m_pSS->SetGlobalValue("SOUND_RELATIVE",FLAG_SOUND_RELATIVE);
	m_pSS->SetGlobalValue("SOUND_RADIUS",FLAG_SOUND_RADIUS);
	m_pSS->SetGlobalValue("SOUND_DOPPLER",FLAG_SOUND_DOPPLER);
	m_pSS->SetGlobalValue("SOUND_NO_SW_ATTENUATION",FLAG_SOUND_NO_SW_ATTENUATION);
	m_pSS->SetGlobalValue("SOUND_MUSIC",FLAG_SOUND_MUSIC);
	m_pSS->SetGlobalValue("SOUND_OUTDOOR",FLAG_SOUND_OUTDOOR);
	m_pSS->SetGlobalValue("SOUND_INDOOR",FLAG_SOUND_INDOOR);
	m_pSS->SetGlobalValue("SOUND_UNSCALABLE",FLAG_SOUND_UNSCALABLE);
	m_pSS->SetGlobalValue("SOUND_CULLING",FLAG_SOUND_CULLING);
	m_pSS->SetGlobalValue("SOUND_LOAD_SYNCHRONOUSLY",FLAG_SOUND_LOAD_SYNCHRONOUSLY);	
	//
	m_pSS->SetGlobalValue("SOUND_FADE_OUT_UNDERWATER",FLAG_SOUND_FADE_OUT_UNDERWATER);
	m_pSS->SetGlobalValue("SOUND_OBSTRUCTION",FLAG_SOUND_OBSTRUCTION);
	m_pSS->SetGlobalValue("SOUND_SELFMOVING",FLAG_SOUND_SELFMOVING);
	m_pSS->SetGlobalValue("SOUND_START_PAUSED",FLAG_SOUND_START_PAUSED);
	m_pSS->SetGlobalValue("SOUND_VOICE",FLAG_SOUND_VOICE);	
	m_pSS->SetGlobalValue("SOUND_EVENT",FLAG_SOUND_EVENT);	
	//

	// precache
	m_pSS->SetGlobalValue("SOUND_PRECACHE_LOAD_SOUND",FLAG_SOUND_PRECACHE_LOAD_SOUND);	
	m_pSS->SetGlobalValue("SOUND_PRECACHE_LOAD_GROUP",FLAG_SOUND_PRECACHE_LOAD_GROUP);	
	m_pSS->SetGlobalValue("SOUND_PRECACHE_LOAD_STAY_IN_MEMORY",FLAG_SOUND_PRECACHE_STAY_IN_MEMORY);	
	m_pSS->SetGlobalValue("SOUND_PRECACHE_LOAD_UNLOAD_AFTER_PLAY",FLAG_SOUND_PRECACHE_UNLOAD_AFTER_PLAY);	

	
	// group stuff
	m_pSS->SetGlobalValue("SOUNDSCALE_MASTER", SOUNDSCALE_MASTER);
	m_pSS->SetGlobalValue("SOUNDSCALE_SCALEABLE", SOUNDSCALE_SCALEABLE);
	m_pSS->SetGlobalValue("SOUNDSCALE_DEAFNESS", SOUNDSCALE_DEAFNESS);
	m_pSS->SetGlobalValue("SOUNDSCALE_UNDERWATER", SOUNDSCALE_UNDERWATER);
	m_pSS->SetGlobalValue("SOUNDSCALE_MISSIONHINT", SOUNDSCALE_MISSIONHINT);

	m_pSS->SetGlobalValue("SOUND_VOLUMESCALEMISSIONHINT", 0.45f);

	// Semantic
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_AMBIENCE", eSoundSemantic_Ambience);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_AMBIENCE_ONESHOT",	eSoundSemantic_Ambience_OneShot);

	m_pSS->SetGlobalValue("SOUND_SEMANTIC_PHYSICS_COLLISION",	eSoundSemantic_Physics_Collision);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_DIALOG",	eSoundSemantic_Dialog);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_MP_CHAT",	eSoundSemantic_MP_Chat);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_FOOTSTEP",	eSoundSemantic_Physics_Footstep);

	m_pSS->SetGlobalValue("SOUND_SEMANTIC_PHYSICS_GENERAL",	eSoundSemantic_Physics_General);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_HUD",	eSoundSemantic_HUD);
	//m_pSS->SetGlobalValue("SOUND_SEMANTIC_BULLET_IMPACT",	eSoundSemantic_Bullet_Impact);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_FLOWGRAPH",	eSoundSemantic_FlowGraph);

	//m_pSS->SetGlobalValue("SOUND_SEMANTIC_WEAPON_FIRE_FP",	eSoundSemantic_Weapon_Fire_FP);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_LIVING_ENTITY",	eSoundSemantic_Living_Entity);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_MECHANIC_ENTITY",	eSoundSemantic_Mechanic_Entity);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_NANOSUIT",	eSoundSemantic_NanoSuit);

	m_pSS->SetGlobalValue("SOUND_SEMANTIC_SOUNDSPOT",	eSoundSemantic_SoundSpot);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_PARTICLE",	eSoundSemantic_Particle);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_AI_PAIN_DEATH",	eSoundSemantic_AI_Pain_Death);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_AI_READABILITY",	eSoundSemantic_AI_Readability);

	m_pSS->SetGlobalValue("SOUND_SEMANTIC_AI_READABILITY_RESPONSE",	eSoundSemantic_AI_Readability_Response);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_TRACKVIEW",	eSoundSemantic_TrackView);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_PROJECTILE",	eSoundSemantic_Projectile);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_VEHICLE",	eSoundSemantic_Vehicle);

	m_pSS->SetGlobalValue("SOUND_SEMANTIC_WEAPON",	eSoundSemantic_Weapon);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_EXPLOSION",	eSoundSemantic_Explosion);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_PLAYER_FOLEY",	eSoundSemantic_Player_Foley);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_PLAYER_FOLEY_VOICE",	eSoundSemantic_Player_Foley_Voice);
	m_pSS->SetGlobalValue("SOUND_SEMANTIC_ANIMATION",	eSoundSemantic_Animation);
}

CScriptBind_Sound::~CScriptBind_Sound()
{
}

// Precaches the sound data used by a sound definition
// Depending on additional flags and current settings may load the sound data into the memory for later usage.
// Precaching is optional, sound system can also play not cached sounds, but it can cause small delay before sound playing starts.
int CScriptBind_Sound::Precache( IFunctionHandler *pH, const char *sSoundOrEventName, uint32 nPrecacheFlags )
{
	bool bResult = false;
	
	if (m_pSoundSystem)
		bResult = (m_pSoundSystem->Precache( sSoundOrEventName, 0, nPrecacheFlags ) != ePrecacheResult_Error);

  return pH->EndFunction(bResult);
}

// Uses the sound definition of the Handle to play a sound at the position vPos .
// It returns a unique SoundID that is valid as long as the sound is played.
int CScriptBind_Sound::Play( IFunctionHandler *pH, const char *sSoundOrEventName, Vec3 vPos, uint32 nSoundFlags, uint32 nSemantic )
{
  // make sure sound group parameters loaded and create and begin loading buffer
  //m_pSoundSystem->Precache( sGroupAndEventName, 0 );
  
  // this is here for to support back words campatability (this was written by NickV)
//  _smart_ptr<ISound> pSound = m_pSoundSystem->LoadSound( sGroupAndSoundName, FLAG_SOUND_3D|nSoundFlags );
  //_smart_ptr<ISound> pSound = m_pSoundSystem->CreateSound(nHandle, nSoundFlags);
	_smart_ptr<ISound> pSound = NULL;

	if (m_pSoundSystem)
		pSound = m_pSoundSystem->CreateSound(sSoundOrEventName, nSoundFlags);

  if (pSound)
  {
		//pSound->SetMinMaxDistance(1.0, fDistance);
    pSound->SetPosition(vPos);
		pSound->SetSemantic((ESoundSemantic)nSemantic);
		pSound->Play();

    // Return sound id.
    return pH->EndFunction( ScriptHandle(pSound->GetId()) );
  }

  // Return nil if failed.
  return pH->EndFunction();
}

// for the new group sound param settings
int CScriptBind_Sound::PlayEx( IFunctionHandler *pH, const char *sSoundOrEventName, Vec3 vPos, uint32 nSoundFlags, float fVolume, float minRadius, float maxRadius, uint32 nSemantic  )
{
  // make sure sound group parameters loaded and create and begin loading buffer
	//m_pSoundSystem->Precache( sGroupAndSoundName, 0);


  _smart_ptr<ISound> pSound = NULL;
	if (m_pSoundSystem)
		pSound = m_pSoundSystem->CreateSound( sSoundOrEventName, nSoundFlags );
	
  if (pSound)
  {
    pSound->SetPosition(vPos);
		pSound->SetSemantic((ESoundSemantic)nSemantic);
    pSound->SetVolume( fVolume );
		
		if (minRadius>0 && maxRadius>0)
			pSound->SetMinMaxDistance( minRadius, maxRadius );
	  
		pSound->Play();

    // Return sound id.
    return pH->EndFunction( ScriptHandle(pSound->GetId()) );
  }

  // Return nil if failed.
  return pH->EndFunction();
}

int CScriptBind_Sound::SetParameterValue( IFunctionHandler *pH, ScriptHandle SoundID, const char *sParameterName, float fParameterValue )
{
	bool bResult = false;
	int nIndex = 0;

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	if (pSound)
	{
		if (!strcmp(sParameterName, "pitch"))
		{
			ptParamF32 fParam(fParameterValue);
			bResult = pSound->SetParam(spPITCH, &fParam);
			return pH->EndFunction(bResult);
		}

		// passing the parameter by string
		nIndex = pSound->SetParam(sParameterName, fParameterValue);
	}

	return pH->EndFunction(bResult);
}

/*! Silence to the whole SoundSystem
@return bool in case of success
*/
int CScriptBind_Sound::Silence(IFunctionHandler *pH)
{
	bool bSilenceSucceed = false;

	if (m_pSoundSystem)
	{
		bSilenceSucceed = m_pSoundSystem->Silence(true, false);
	}

	return pH->EndFunction(bSilenceSucceed);
}

/*! Deactivates Audiodevice so Wavebanks can be overwritten
@return bool in case of success
*/
int CScriptBind_Sound::DeactivateAudioDevice(IFunctionHandler *pH)
{
	bool bResult = false;

	if (m_pSoundSystem)
		bResult = m_pSoundSystem->DeactivateAudioDevice();

	return pH->EndFunction(bResult);
}

/*! Re-Activates the Audiodevice 
@return bool in case of success
*/
int CScriptBind_Sound::ActivateAudioDevice(IFunctionHandler *pH)
{
	bool bResult = false;

	if (m_pSoundSystem)
		bResult = m_pSoundSystem->ActivateAudioDevice();

	return pH->EndFunction(bResult);
}

/*! Unloads all projects so Wavebanks/Project files can be overwritten
@return bool in case of success
*/
int CScriptBind_Sound::UnloadProjects(IFunctionHandler *pH)
{
	bool bResult = false;

	if (m_pSoundSystem)
		bResult = m_pSoundSystem->GetIAudioDevice()->ResetAudio();

	return pH->EndFunction(bResult);
}



/* ! Retrieves the pointer to a sound object by SoundID or direct pointer
 @param index number and function handle where information is stored
 @return ISound pointer
*/
ISound* CScriptBind_Sound::GetSoundPtr(IFunctionHandler *pH,int index)
{
	ScriptVarType vType = pH->GetParamType(index);

	if (vType == svtPointer) // this is a script handle
	{
		ScriptHandle soundID;
		if (!pH->GetParam(index, soundID))
			return 0;

		if (m_pSoundSystem)
			return m_pSoundSystem->GetSound(soundID.n);
		else
			return NULL;
	}
	else if (vType != svtNull)
	{
		SmartScriptTable tbl;
		if (!pH->GetParam(index, tbl))
			return 0;

		void * ptr = ((CScriptTable*)tbl.GetPtr())->GetUserDataValue();
		if (!ptr)
			return 0;

		return *static_cast<ISound**>(ptr);
	}
	else
		return 0;
}

/*! Determine if a sound is still playing
	@param nID ID of the sound
	@return nil or not nil
*/
int CScriptBind_Sound::IsPlaying(IFunctionHandler *pH)
{
	bool bRes = false;
	
	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

 	if (pSound)
	{
 		if (pSound->IsPlaying())
 			bRes=true;
 		else
 			bRes=false;
 	}
  
	return pH->EndFunction(bRes);
}

/*! Load a 2D sound
	@param sSound filename of the sound
	@return nil or sound ID in case of success
*/
int CScriptBind_Sound::LoadSound(IFunctionHandler *pH)
{
	float fVolume=1.0f;

	if (pH->GetParamCount()<1 || pH->GetParamCount()>3)
	{
		m_pSS->RaiseError("Sound.LoadSound wrong number of arguments");
		return pH->EndFunction();
	}

	const char *sSound=NULL;
	ISound *pSound;
	uint32 nFlags=0;
	int pcount=pH->GetParamCount();

	if(!pH->GetParam(1,sSound))
	{
		m_pSS->RaiseError("LoaddSound: First parameter not a string");
		m_pSystem->GetILog()->LogError("LoadSound has bad input file name!!");
		return pH->EndFunction(-1);   // error in data being passed
	}

	if (pcount>1) 
	{
		if (!pH->GetParam(2, nFlags))
		{
			m_pSS->RaiseError("LoaddSound: second parameter not a valid flag value");
			return pH->EndFunction(-1);// error in data
		}
	}


	if (pcount>2)
	{
		if (!pH->GetParam(3, fVolume))
		{
			m_pSS->RaiseError("LoaddSound: third parameter not a valid volume value");
			return pH->EndFunction(-1);// error in data
		}
		else if (fVolume < 0.0f || fVolume > 1.0f)
		{
			fVolume = 1.0f;
			m_pSystem->GetILog()->LogError("Volume not in range of 0-1");
		}
	}

	if (m_pSoundSystem)
	{
		pSound = m_pSoundSystem->CreateSound(sSound,nFlags );
		if(pSound)
		{
			pSound->SetVolume(fVolume);

			pSound->AddRef(); // Worst line of code *EVER*

			// tell people not to use this anymore
//			m_pSS->RaiseError("Error: Sound LoadSound was called by script. Please remove and use Play() instead.");
			// m_pSystem->GetILog()->Log("Error: Sound LoadSound(%s) was called by script. Please remove and use Play() instead.", sSound);

			return pH->EndFunction(WrapSoundReturn(pH, pSound));
			//return pH->EndFunction( ScriptHandle(pSound->GetId()) );
		}
		else
		{
			m_pSS->RaiseError("Sound.LoadSound error loading %s",sSound);
		}
	}
	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Load a 3D sound
	@param sSound filename of the sound
	@param nFlags flags :) [optional]
	@param fVolume [optional]
	@param nMinDistance [optional]
	@param nMaxDistance [optional]
	@return nil or sound ID in case of success
*/
int CScriptBind_Sound::Load3DSound(IFunctionHandler *pH)
{
	int nParamCount=pH->GetParamCount();
	if(nParamCount<1)
	{
		m_pSS->RaiseError("Sound.Load3DSound wrong number of arguments");
		return pH->EndFunction();
	};
	
	const char *sSound = 0;
	ISound *pSound;
	uint32 nFlags = 0;

	if (!pH->GetParam(1, sSound))
	{
		m_pSS->RaiseError("Load3dSound: First parameter not a string");
		return pH->EndFunction(-1);
	}

	float fMin = 1.0f;
	float	fClipDistance = 500.0f;
	float fMax = 100000.0f;
	float fVolume = 1.0f;
	int	nPriority = 0;

	if (nParamCount>1)
	{
		pH->GetParam(2, nFlags);
	}
	if (nParamCount>2)
	{
		pH->GetParam(3,fVolume);
	}

	if (nParamCount>3)
	{
		pH->GetParam(4,fMin);
	}

	if (nParamCount>4)
	{
		pH->GetParam(5,fClipDistance);
		if (fClipDistance>1000)
			fClipDistance=1000;
		//fMax=fClipDistance;
	}

	if (pH->GetParamCount()>5) 
	{
		pH->GetParam(6, nPriority);
	}

	if (m_pSoundSystem)
	{			
		pSound = m_pSoundSystem->CreateSound(sSound, FLAG_SOUND_3D | nFlags );

		if (pSound)
		{
			pSound->SetVolume(fVolume);
			pSound->SetMinMaxDistance(fMin,fClipDistance/2.0f);
			//				pSound->SetMaxSoundDistance(fClipDistance/2); // :) 				
			if (pH->GetParamCount()>6) 
			{
				int nGroups;
				pH->GetParam(7, nGroups);
				pSound->SetScaleGroup(nGroups);
			}

			pSound->AddRef();
			pSound->SetSoundPriority(nPriority);

			// tell people not to use this anymore
			//m_pSS->RaiseError("Error: Sound LoadSound was called by script. Please remove and use Play() instead.");
			// m_pSystem->GetILog()->Log("Error: Sound Load3DSound(%s) was called by script. Please remove and use Play() instead.", sSound);
			
			return pH->EndFunction(WrapSoundReturn(pH, pSound));
			//return pH->EndFunction( ScriptHandle(pSound->GetId()) );
		}
		else
		{
			//m_pSS->RaiseError("Sound.Load3DSound error loading %s",sSound);
			m_pSystem->Warning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,VALIDATOR_FLAG_SOUND,
				sSound,"Sound %s Failed to Load",sSound );
		}
	}

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Load a 3D sound
@param sSound filename of the sound
@param nFlags flags :) [optional]
@param nVolume [optional]
@param nMinDistance [optional]
@param nMaxDistance [optional]
@return nil or sound ID in case of success
*/
/*
int CScriptBind_Sound::Load3DSoundLocalized(IFunctionHandler *pH)
{
	int nParamCount=pH->GetParamCount();
	if(nParamCount<1)
	{
		m_pSS->RaiseError("Sound.Load3DSound wrong number of arguments");
		return pH->EndFunction();
	};

	const char *sSound=0;
	ISound *pSound;
	int nFlags=0;

	if (!pH->GetParam(1, sSound))
	{
		m_pSS->RaiseError("Load3dSound: First parameter not a string");
		return pH->EndFunction(-1);
	}

	float fMin=1,fClipDistance=500,fMax=100000,fVolume=128;
	int	nPriority=0;

	if (nParamCount>1)
	{
		pH->GetParam(2, nFlags);
	}
	if (nParamCount>2)
	{
		pH->GetParam(3,fVolume);
	}

	if (nParamCount>3)
	{
		pH->GetParam(4,fMin);
	}

	if (nParamCount>4)
	{
		pH->GetParam(5,fClipDistance);
		if (fClipDistance>1000)
			fClipDistance=1000;
		//fMax=fClipDistance;
	}

	if (pH->GetParamCount()>5) 
	{
		pH->GetParam(6, nPriority);
	}

	string szPath;

	ICVar *g_language = m_pSystem->GetIConsole()->GetCVar("g_language");
	assert(g_language);

	szPath = "LANGUAGES/";
	szPath += g_language->GetString();
	szPath += "/";
	szPath += sSound;


	if (m_pSoundSystem)
	{			
		pSound=m_pSoundSystem->LoadSound(szPath.c_str(), FLAG_SOUND_3D | nFlags);

		if (pSound)
		{
			pSound->SetVolume((int)fVolume);
			pSound->SetMinMaxDistance(fMin,fClipDistance/2.0f);
			//				pSound->SetMaxSoundDistance(fClipDistance/2); // :) 				
			if (pH->GetParamCount()>6) 
			{
				unsigned int nGroups;
				pH->GetParam(7, nGroups);
				pSound->SetScaleGroup(nGroups);
			}
			pSound->AddRef();
			pSound->SetSoundPriority(nPriority);
			return pH->EndFunction(ScriptHandle(pSound));
		}
		else
		{
			//m_pSS->RaiseError("Sound.Load3DSound error loading %s",sSound);
			m_pSystem->Warning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,VALIDATOR_FLAG_SOUND,
				sSound,"Sound %s Failed to Load",szPath.c_str() );
		}
	}

	return pH->EndFunction();
}
*/

/*!	Registers an EAX Preset Area with as being active
//! morphing of several EAX Preset Areas is done internally
//! Registering the same Preset multiple time will only overwrite the old one
@param nPreset one of the presets
*/
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::RegisterWeightedEaxEnvironment(IFunctionHandler *pH)
{
	if (pH->GetParamCount()<1)
	{
		SCRIPT_CHECK_PARAMETERS(1);
	}
	SmartScriptTable pObj(m_pSS,true);
	CScriptVector oVec(m_pSS,true);
	Vec3 vVec;
	int nEaxEnvironment=0;

	int nParamCount=pH->GetParamCount();
	if(nParamCount<1)
	{
		m_pSS->RaiseError("Sound.RegisterWeightedEaxEnvironment wrong number of arguments");
		return pH->EndFunction();
	}

	const char *sPresetName=0;

	if (!pH->GetParam(1, sPresetName))
	{
		m_pSS->RaiseError("RegisterWeightedEaxEnvironment: First parameter not a string");
		return pH->EndFunction(-1);
	}

//	float fWeight;
	int nFullEffectWhenInside = 0;
	uint32 nFlags = 0;

	if (nParamCount>1)
	{
		pH->GetParam(2, nFullEffectWhenInside);
	}
	if (nParamCount>2)
	{
		pH->GetParam(3, nFlags);
	}

	if (m_pSoundSystem)		
		if (m_pSoundSystem->GetIReverbManager())		
			m_pSoundSystem->GetIReverbManager()->RegisterWeightedReverbEnvironment(sPresetName, nFullEffectWhenInside ? true:false, 0);
	//m_pSoundSystem->GetIReverbManager()->RegisterWeightedReverbEnvironment(sPresetName, &pProps, nFullEffectWhenInside ? true:false, 0);
	
	return pH->EndFunction();
}

//! Updates an EAX Preset Area with the current blending ratio (0-1)
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::UpdateWeightedEaxEnvironment(IFunctionHandler *pH)
{
	if (pH->GetParamCount()<1)
	{
		SCRIPT_CHECK_PARAMETERS(1);
	}

	int nParamCount=pH->GetParamCount();
	if(nParamCount<1)
	{
		m_pSS->RaiseError("Sound.RegisterWeightedEaxEnvironment wrong number of arguments");
		return pH->EndFunction();
	}

	const char *sPresetName = NULL;

	if (!pH->GetParam(1, sPresetName))
	{
		m_pSS->RaiseError("RegisterWeightedEaxEnvironment: First parameter not a string");
		return pH->EndFunction(-1);
	}

	float fWeight = 0;

	if (nParamCount>1)
	{
		pH->GetParam(2,fWeight);
	}

	if (m_pSoundSystem)		
		if (m_pSoundSystem->GetIReverbManager())		
			m_pSoundSystem->GetIReverbManager()->UpdateWeightedReverbEnvironment(sPresetName, fWeight);

	return pH->EndFunction();
}

//! Unregisters an active EAX Preset Area 
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::UnregisterWeightedEaxEnvironment(IFunctionHandler *pH)
{
	if (pH->GetParamCount()<1)
	{
		SCRIPT_CHECK_PARAMETERS(1);
	}

	int nParamCount=pH->GetParamCount();
	if(nParamCount<1)
	{
		m_pSS->RaiseError("Sound.RegisterWeightedEaxEnvironment wrong number of arguments");
		return pH->EndFunction();
	}

	const char *sPresetName=NULL;

	if (!pH->GetParam(1, sPresetName))
	{
		m_pSS->RaiseError("RegisterWeightedEaxEnvironment: First parameter not a string");
		return pH->EndFunction(-1);
	}

	if (m_pSoundSystem)		
		if (m_pSoundSystem->GetIReverbManager())		
			m_pSoundSystem->GetIReverbManager()->UnregisterWeightedReverbEnvironment(sPresetName);

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Load a streaming sound
	@param sSound filename of the sound
	@return nil or sound ID in case of success
*/
int CScriptBind_Sound::LoadStreamSound(IFunctionHandler *pH)
{	
	if (pH->GetParamCount()<1 || pH->GetParamCount()>2)
	{  
		m_pSS->RaiseError("System.LoadStreamSound wrong number of arguments"); 
		return pH->EndFunction(); 
	};
 
/*
	ICVar *pMusic=m_pSystem->GetIConsole()->GetCVar("g_MusicEnable");
	if (pMusic && (pMusic->GetIVal()==0))
		return (pH->EndFunction());
*/
	const char *sSound;
//	int nID;
	ISound *pSound;
	pH->GetParam(1,sSound);

	uint32 nFlags = 0;
	if (pH->GetParamCount()>1) 
		pH->GetParam(2, nFlags);

  if (m_pSoundSystem)
  {
	  pSound = m_pSoundSystem->CreateSound(sSound,(FLAG_SOUND_STREAM | nFlags ));

		if (pSound)
		{
			pSound->AddRef();

			//return pH->EndFunction(WrapSoundReturn(pH,pSound));
			return pH->EndFunction( ScriptHandle(pSound->GetId()) );
  	}
  }else
	{
			m_pSS->RaiseError("Sound.LoadStreamSound error loading %s",sSound);
	}
	return pH->EndFunction();

}


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Load a streaming sound
@param sSound filename of the sound
@return nil or sound ID in case of success
*/
/*
int CScriptBind_Sound::LoadStreamSoundLocalized(IFunctionHandler *pH)
{	
	if (pH->GetParamCount()<1 || pH->GetParamCount()>2)
	{  
		m_pSS->RaiseError("System.LoadStreamSound wrong number of arguments"); 
		return pH->EndFunction(); 
	};

	const char *sSound;
	//	int nID;
	ISound *pSound;
	pH->GetParam(1,sSound);

	int nFlags=0;
	if (pH->GetParamCount()>1) 
		pH->GetParam(2, nFlags);

	string szPath;

	ICVar *g_language = m_pSystem->GetIConsole()->GetCVar("g_language");
	assert(g_language);

	szPath = "LANGUAGES/";
	szPath += g_language->GetString();
	szPath += "/";
	szPath += sSound;

	if (m_pSoundSystem)
	{
		pSound=m_pSoundSystem->LoadSound(szPath.c_str(),FLAG_SOUND_STREAM | nFlags);
		if (pSound)
		{
			pSound->AddRef();

			return pH->EndFunction(ScriptHandle(pSound));
		}
	}else
	{
		m_pSS->RaiseError("Sound.LoadStreamSound error loading %s",szPath.c_str());
	}
	return pH->EndFunction();

}
*/
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Start playing a sound, loops is if enabled
	@param nID ID of the sound
	@see CScriptBind_Sound::SetSoundLoop
*/
int CScriptBind_Sound::PlaySound(IFunctionHandler *pH)
{
	float fVolumeScale=1.0f;

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	if (pSound)
	{
		if (pH->GetParamCount()>1) 			
		{		
			if(!pH->GetParam(2,fVolumeScale))
			{
				fVolumeScale=1.0f;
			}
		}

		pSound->Play(fVolumeScale);
	}
	else
	{
		if(m_pSoundSystem)
			m_pSS->RaiseError("PlaySound NULL SOUND!!");
	}
	return pH->EndFunction();
}

/*! Set max sound hearable distance
	@param nID ID of the sound
	@param fSoundDistance max sound distance, in meters
*/
/////////////////////////////////////////////////////////////////////////////////
/*int CScriptBind_Sound::SetMaxSoundDistance(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	int		nCookie=0;
	float fDistance=500;
	ISound *pSound=NULL;
	pH->GetParamUDVal(1,(int &)pSound,nCookie);

	pH->GetParam(2,fDistance);

	if(pSound && (nCookie==USER_DATA_SOUND))
		pSound->SetMaxSoundDistance(fDistance);

	return pH->EndFunction();
}*/

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Change the volume of a sound
	@param nID ID of the sound
	@param fVolume volume between 0 and 1
*/
int CScriptBind_Sound::SetSoundVolume(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	int nCookie = 0;
	float fVolume = 0.0f;
	
	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);
	if (pSound)
	{
		pH->GetParam(2,fVolume);
		pSound->SetVolume(fVolume);
	}

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/*! retrieve the volume of a sound
@param nID ID of the sound
*/
int CScriptBind_Sound::GetSoundVolume(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	float fVolume = 0.0f;

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);
	if (pSound)
	{
		fVolume = pSound->GetVolume();
	}

	return pH->EndFunction(fVolume);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Change the looping status of a looped sound
	@param nID ID of the sound
	@param nFlag 1/0 to enable/disable looping
*/
int CScriptBind_Sound::SetSoundLoop(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	int nCookie = 0;
	uint32 nFlag = 0;

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2,nFlag);

	if (pSound)
	{
		pSound->SetLoopMode(nFlag?true:false);
	}

	return pH->EndFunction();
}
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Changes the pitch of a sound sound
	@param nID ID of the sound
	@param nFrequency Frequency, value range is between 0 and 1000
*/
int CScriptBind_Sound::SetSoundFrequency(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	int nCookie=0;
	int nFrequency=0;

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2,nFrequency);


  if (pSound)
  {
  	pSound->SetPitch(nFrequency);
  }
  
	return pH->EndFunction();
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::SetSoundPitching(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	int nCookie=0;
	float fPitching=0;

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2,fPitching);


	if (pSound)
	{
		pSound->SetPitching(fPitching);
	}

	return pH->EndFunction();
}


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Sets the Pause state of a sound
@param nID ID of the sound
@param bPaused State of Pause Mode
*/
int CScriptBind_Sound::SetSoundPaused(IFunctionHandler *pH) //int bool (return void)
{
	SCRIPT_CHECK_PARAMETERS(2);
	int nCookie=0;
	bool bPaused=false;

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2,bPaused);

	if (pSound)
	{
		pSound->SetPaused(bPaused);
	}

	return pH->EndFunction();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Stop playing a sound
	@param nID ID of the sound
*/
int CScriptBind_Sound::StopSound(IFunctionHandler *pH)
{
//	SCRIPT_CHECK_PARAMETERS(1);
	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	if (pSound)
	{
		pSound->Stop();
	}

	return pH->EndFunction();
}
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Change the position of a 3D sound
	@param nID ID of the sound
	@param v3d Three component vector cotaining the position
	@see CScriptBind_Sound::Load3DSound
*/
int CScriptBind_Sound::SetSoundPosition(IFunctionHandler *pH)
{
	//SCRIPT_CHECK_PARAMETERS(2);

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	Vec3 vPos;
	pH->GetParam(2, vPos);

	if (pSound)
	{
		// Prevent script from changing the position of relative sounds
		if (pSound->IsRelative())
			return pH->EndFunction();

		pSound->SetPosition(vPos);
	}

	return pH->EndFunction();
}
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/*! Change the velocity of a sound
	@param nID ID of the sound
	@param v3d Three component vector containing the velocity for each axis
*/
int CScriptBind_Sound::SetSoundSpeed(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	int nCookie=0;

	Vec3 v3d(0,0,0);

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2,v3d);

	if (pSound)
	{
		pSound->SetVelocity(v3d);
	}
  
	return pH->EndFunction();
}

/*! Set distance attenuation of a sound
	@param iSoundID ID of the sound
	@param fMinDist Minimum distance, normally 0
	@param fMaxDist Maximum distance at which the sound can be heared
*/
int CScriptBind_Sound::SetMinMaxDistance(IFunctionHandler *pH)
{
	int nCookie=0;
	float fMinDist;
	float fMaxDist;

	SCRIPT_CHECK_PARAMETERS(3);

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2, fMinDist);
	pH->GetParam(3, fMaxDist);

	if(fMinDist<1)
		fMinDist=1;

  if (pSound)
  		pSound->SetMinMaxDistance(fMinDist, fMaxDist);
  
	return pH->EndFunction();
}

/*! Set fade (in) time of a sound
@param iSoundID ID of the sound
@param nFadeTimeInMs fade time
*/
int CScriptBind_Sound::SetFadeTime(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	float fFadeGoal = 0;
	int nFadeTimeInMS = 0;

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2, fFadeGoal);

	fFadeGoal = min(1.0f, fFadeGoal);
	fFadeGoal = max(0.0f, fFadeGoal);

	pH->GetParam(3, nFadeTimeInMS);

	if(nFadeTimeInMS < 0)
		nFadeTimeInMS = 1;

	// SetFade(const float fFadeGoal, const int nFadeTimeInMS)

	if (pSound)
		pSound->SetFade(fFadeGoal, nFadeTimeInMS);

	return pH->EndFunction();
}

/*!
*/
/*
int CScriptBind_Sound::SetLoopPoints(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	int nID = 0, iLoopPt1, iLoopPt2;
	int nCookie=0;
	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2, iLoopPt1);
	pH->GetParam(3, iLoopPt2);
	if (pSound)
		pSound->SetLoopPoints(iLoopPt1, iLoopPt2);
	

	return pH->EndFunction();
}
*/
/*!
*/
/*
int CScriptBind_Sound::AddSoundFlags(IFunctionHandler *pH)
{
	int iFlags;
	int nCookie=0;
	ISound *pISound = NULL;
	SCRIPT_CHECK_PARAMETERS(2);

	pH->GetParamUDVal(1,(int &)pISound,nCookie);
	pH->GetParam(2, iFlags);

	if (pISound && (nCookie==USER_DATA_SOUND))
		pISound->AddFlags(iFlags);
	 	
	return pH->EndFunction();
}
*/

int CScriptBind_Sound::SetSoundRelative(IFunctionHandler *pH)
{
	int nCookie=0;
	bool bIsRelative;

	SCRIPT_CHECK_PARAMETERS(2);

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2, bIsRelative);

	if (pSound)
	{
		if (bIsRelative)
		{
			// set the RELATIVE Flag
			pSound->SetFlags(pSound->GetFlags() | FLAG_SOUND_RELATIVE);
		}
		else
		{
			// unset the RELATIVE Flag, Caution: The old Position is lost and has to be reset!
			pSound->SetFlags(pSound->GetFlags() & ~FLAG_SOUND_RELATIVE);
		}
	}

	return pH->EndFunction();
}


int CScriptBind_Sound::SetMasterVolumeScale(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	float fScale=1;
	if(pH->GetParam(1, fScale) && m_pSoundSystem)
		m_pSoundSystem->SetMasterVolumeScale(fScale);
	return pH->EndFunction();
}

// allows effects on music volume
int CScriptBind_Sound::SetMasterMusicEffectsVolume(IFunctionHandler *pH)
{
  SCRIPT_CHECK_PARAMETERS(1);

  float fNewMusicVol = 1.0f;

  if (pH->GetParam(1, fNewMusicVol) && m_pSoundSystem)
    m_pSoundSystem->SetMusicEffectsVolume(fNewMusicVol);

  return pH->EndFunction();
}

int	CScriptBind_Sound::SetPauseAllPlaying(IFunctionHandler *pH, bool bPause)
{
	if (m_pSoundSystem)
		m_pSoundSystem->Pause(bPause);

	return pH->EndFunction();
}

// group stuff
//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::AddToScaleGroup(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	int nGroup;
	int nCookie = 0;
	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2, nGroup);
	if (pSound)
		pSound->AddToScaleGroup(nGroup);
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::RemoveFromScaleGroup(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	int nGroup;
	int nCookie=0;
	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2, nGroup);
	if (pSound)
		pSound->RemoveFromScaleGroup(nGroup);
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::SetGroupScale(IFunctionHandler *pH)
{
	if(!m_pSoundSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(2);
	int nGroup;
	float fScale;
	pH->GetParam(1, nGroup);
	pH->GetParam(2, fScale);

	return pH->EndFunction(m_pSoundSystem->SetGroupScale(nGroup, fScale));
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::FXEnable(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	int nCookie=0;
	int nEffectNumber;

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2,nEffectNumber);
	if (pSound)
		pSound->FXEnable(nEffectNumber);
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::SetFXSetParamEQ(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(4);

	int nCookie=0;
	float fCenter;
	float fBandwidth;
	float fGain;

	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2,fCenter);
	pH->GetParam(3,fBandwidth);
	pH->GetParam(4,fGain);
	if (pSound)
		pSound->FXSetParamEQ(fCenter, fBandwidth, fGain);
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::SetDirectionalAttenuation(IFunctionHandler *pH)
{
	if(!m_pSoundSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(3);
	Vec3 vPos(0,0,0);
	Vec3 vDir(0,0,0);
	float fConeInRadians;
	pH->GetParam(1,vPos);
	pH->GetParam(2,vDir);

	if (!pH->GetParam(3, fConeInRadians))
		fConeInRadians=0.0f;
	else
		fConeInRadians*=0.5f;	// we need the half-angle

	m_pSoundSystem->CalcDirectionalAttenuation(vPos, vDir, fConeInRadians);
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::GetDirectionalAttenuationMaxScale(IFunctionHandler *pH)
{
	if(!m_pSoundSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(0);
	return pH->EndFunction(m_pSoundSystem->GetDirectionalAttenuationMaxScale());
}


/*!	Registers a SoundMood
//! morphing of several SoundMoods is done internally
//! Registering the same SoundMood multiple times will only overwrite the old one
*/
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::RegisterSoundMood(IFunctionHandler *pH, const char *sSoundMoodName)
{
	if (m_pSoundSystem)		
		if (m_pSoundSystem->GetIMoodManager())	
			m_pSoundSystem->GetIMoodManager()->RegisterSoundMood(sSoundMoodName);

	return pH->EndFunction();
}

//! Updates a SoundMood current fade ratio (0-1)
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::UpdateSoundMood(IFunctionHandler *pH, const char *sSoundMoodName, float fFade, uint32 nFadeTimeInMS)
{
	if (m_pSoundSystem)		
		if (m_pSoundSystem->GetIMoodManager())		
			m_pSoundSystem->GetIMoodManager()->UpdateSoundMood(sSoundMoodName, fFade, nFadeTimeInMS);

	return pH->EndFunction();
}

//! Get the SoundMood fade value
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::GetSoundMoodFade(IFunctionHandler *pH, const char *sSoundMoodName)
{
	if (m_pSoundSystem)		
		if (m_pSoundSystem->GetIMoodManager())
			return pH->EndFunction(m_pSoundSystem->GetIMoodManager()->GetSoundMoodFade(sSoundMoodName));

	return pH->EndFunction(0.0f);
}

//! Unregisters a SoundMood 
/////////////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::UnregisterSoundMood(IFunctionHandler *pH, const char *sSoundMoodName)
{
	if (m_pSoundSystem)		
		if (m_pSoundSystem->GetIMoodManager())	
			m_pSoundSystem->GetIMoodManager()->UnregisterSoundMood(sSoundMoodName);

	return pH->EndFunction();
}



//////////////////////////////////////////////////////////////////////////
// MUSIC
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::LoadMusic(IFunctionHandler *pH)
{
	bool bRes = true;
	
	SCRIPT_CHECK_PARAMETERS(1);
	const char *pszFilename;
	pH->GetParam(1, pszFilename);

	bRes = m_pMusicSystem->LoadFromXML(pszFilename, true, false);
	
	if (!bRes)
		m_pSystem->GetILog()->Log("Unable to load music from %s !", pszFilename);
		
	return pH->EndFunction(bRes);
}

//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::UnloadMusic(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(0);
	m_pMusicSystem->Unload();
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::SetMusicTheme(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	if (pH->GetParamCount()<1)
	{
		SCRIPT_CHECK_PARAMETERS(1);
	}

	const char *pszTheme;
	bool bOverride = false;
	bool bForceChange = false;
	bool bKeepMood = false;
	int nDelayInSec = -1;
	
	if (!pH->GetParam(1, pszTheme))
		return pH->EndFunction(false);
	
	if (pH->GetParamCount()>=2)
		pH->GetParam(2, bForceChange);

	if (pH->GetParamCount()>=3)
		pH->GetParam(3, bKeepMood);

	if (pH->GetParamCount()>=4)
		pH->GetParam(4, nDelayInSec);

	bool bResult = m_pMusicSystem->SetTheme(pszTheme, bForceChange, bKeepMood, nDelayInSec);
	
	if (!bResult)
		m_pSystem->GetILog()->Log("Unable to set music-theme \"%s\" !", pszTheme);
	
	return pH->EndFunction(bResult);
}


//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::EndMusicTheme(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	int nThemeFadeType = 0;
	int nForceEndLimitInSec = 5;
	bool bEndEverything = false;

	if (pH->GetParamCount()>=1)
		pH->GetParam(1, nThemeFadeType);

	if (pH->GetParamCount()>=2)
		pH->GetParam(2, nForceEndLimitInSec);

	if (pH->GetParamCount()>=3)
		pH->GetParam(3, bEndEverything);

	bool bResult = m_pMusicSystem->EndTheme((EThemeFadeType)nThemeFadeType, nForceEndLimitInSec, bEndEverything);

	if (!bResult)
		m_pSystem->GetILog()->Log("Unable to end music-theme !");

	return pH->EndFunction(bResult);
}


//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::SerializeMusicInternal(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(1);

	bool bSave = false;
	if (!pH->GetParam(1, bSave))
		return pH->EndFunction(false);
	
	bool bRes = m_pMusicSystem->SerializeInternal(bSave);

	if (!bRes)
		m_pSystem->GetILog()->Log("Unable to serialize internal musicsystem !");

	return pH->EndFunction(bRes);
}

//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::SetMusicMood(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	//SCRIPT_CHECK_PARAMETERS(3);
	const char *pszMood;
	bool bPlayFromStart = true;
	bool bForceChange = false;

	bool bRes;
	
	if (!pH->GetParam(1, pszMood))
		return pH->EndFunction(false);

	if (pH->GetParamCount()>=2)
		pH->GetParam(2, bPlayFromStart);

	if (pH->GetParamCount()>=3)
		pH->GetParam(3, bForceChange);

	bRes = m_pMusicSystem->SetMood(pszMood, bPlayFromStart, bForceChange);

	if (!bRes)
		m_pSystem->GetILog()->Log("Unable to set music-mood \"%s\" !", pszMood);

	return pH->EndFunction(bRes);
}

//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::SetDefaultMusicMood(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(1);
	const char *pszMood;
	
	if (!pH->GetParam(1, pszMood))
		return pH->EndFunction(false);
	
	bool bRes = m_pMusicSystem->SetDefaultMood(pszMood);
	
	if (!bRes)
		m_pSystem->GetILog()->Log("Unable to set default music-mood \"%s\" !", pszMood);
	
	return pH->EndFunction(bRes);
}



//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::GetMusicThemes(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(0);
	IStringItVec *pVec=m_pMusicSystem->GetThemes();
	if (!pVec)
		return pH->EndFunction();
	SmartScriptTable pTable(m_pSS);
	int i=1;
	while (!pVec->IsEnd())
	{
		pTable->SetAt(i++, pVec->Next() );
	}
	pVec->Release();
	return pH->EndFunction(pTable);
}

//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::GetMusicMoods(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(1);
	const char *pszTheme;
	pH->GetParam(1, pszTheme);
	IStringItVec *pVec=m_pMusicSystem->GetMoods(pszTheme);
	if (!pVec)
		return pH->EndFunction();
	SmartScriptTable pTable(m_pSS);
	int i=1;
	while (!pVec->IsEnd())
	{
		pTable->SetAt(i++, pVec->Next());
	}
	pVec->Release();
	return pH->EndFunction(pTable);
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::AddMusicMoodEvent(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(2);
	const char *pszMood;	// name of mood
	float fTimeout=1.0f;	// minimum time this mood-event must be set in order to actually switch to the mood
	pH->GetParam(1, pszMood);
	pH->GetParam(2, fTimeout);

	return pH->EndFunction(m_pMusicSystem->AddMusicMoodEvent(pszMood, fTimeout));
}

//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::IsInMusicTheme(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(1);
	const char *pszTheme;
	pH->GetParam(1, pszTheme);
	SMusicSystemStatus *pStatus=m_pMusicSystem->GetStatus();
	if (pStatus)
		return pH->EndFunction(stricmp(pStatus->sTheme.c_str(), pszTheme)==0);
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::IsInMusicMood(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(1);
	const char *pszMood;
	pH->GetParam(1, pszMood);
	SMusicSystemStatus *pStatus=m_pMusicSystem->GetStatus();
	if (pStatus)
		return pH->EndFunction(stricmp(pStatus->sMood.c_str(), pszMood)==0);
	return pH->EndFunction();
}

int CScriptBind_Sound::GetSoundLength(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	int nCookie=0;
	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	if (pSound)
		return pH->EndFunction( (float)pSound->GetLengthMs()/1000.f);

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::GetMusicStatus(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(0);
	SMusicSystemStatus *pStatus=m_pMusicSystem->GetStatus();
	ILog *pLog=m_pSystem->GetILog();
	assert(pLog);
	pLog->LogToConsole("--- MusicSystem Status Info ---");
	pLog->LogToConsole("  Streaming: %s", pStatus->bPlaying ? "Yes" : "No");
	pLog->LogToConsole("  Theme: %s", pStatus->sTheme.c_str());
	pLog->LogToConsole("  Mood: %s", pStatus->sMood.c_str());
	pLog->LogToConsole("  Playing patterns:");
	for (TPatternStatusVecIt It=pStatus->m_vecPlayingPatterns.begin();It!=pStatus->m_vecPlayingPatterns.end();++It)
	{
		SPlayingPatternsStatus &PatternStatus=(*It);
		pLog->LogToConsole("    %s (Layer: %s; Volume: %f)", PatternStatus.sName.c_str(), (PatternStatus.nLayer==MUSICLAYER_MAIN) ? "Main" : ((PatternStatus.nLayer==MUSICLAYER_RHYTHMIC) ? "Rhythmic" : "Incidental"), (float)PatternStatus.fVolume);
	}
	return pH->EndFunction();
}
//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::SetMenuMusic(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(3);
	bool bPlayMenuMusic;
	const char *pszTheme;
	const char *pszMood;
	pH->GetParam(1, bPlayMenuMusic);
	pH->GetParam(2, pszTheme);
	pH->GetParam(3, pszMood);

	if (bPlayMenuMusic)
	{
		m_pMusicSystem->SerializeInternal(true);

		if (!bMenuMusicLoaded)
		{
			m_pMusicSystem->LoadFromXML("music/common_music.xml", true, false);
			bMenuMusicLoaded = true;
		}

		m_pMusicSystem->SetTheme(pszTheme, false);
		m_pMusicSystem->SetMood(pszMood);
	}
	else
	{
		m_pMusicSystem->SerializeInternal(false);
	}
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::PlayStinger(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	m_pMusicSystem->PlayStinger();
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int	CScriptBind_Sound::PlayPattern(IFunctionHandler *pH)
{
	if(!m_pMusicSystem)
		return pH->EndFunction();

	SCRIPT_CHECK_PARAMETERS(3);
	const char *pszPattern;
	bool bStopPrevious = false;
	bool bPlaySynched = false;
	bool bRes = true;

	if (!pH->GetParam(1, pszPattern))
		return pH->EndFunction(false);

	pH->GetParam(2, bStopPrevious);
	pH->GetParam(3, bPlaySynched);

	m_pMusicSystem->PlayPattern(pszPattern, bStopPrevious, bPlaySynched);

	return pH->EndFunction(bRes);
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::SetSoundRatio(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	int nCookie=0;
	float fRatio;
	_smart_ptr<ISound> pSound = GetSoundPtr(pH,1);

	pH->GetParam(2, fRatio);
	if (pSound)
		pSound->SetRatio(fRatio);
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::SetWeatherCondition(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	int nParamCount=pH->GetParamCount();

	float fWeatherTemperatureInCelsius;
	float fWeatherHumidityAsPercent;
	float fWeatherInversion;
	
	pH->GetParam(1, fWeatherTemperatureInCelsius);
	pH->GetParam(2, fWeatherHumidityAsPercent);
	pH->GetParam(3, fWeatherInversion);

	if (m_pSoundSystem)		
		return pH->EndFunction(m_pSoundSystem->SetWeatherCondition(fWeatherTemperatureInCelsius, fWeatherHumidityAsPercent, fWeatherInversion));
	
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::CreateListener( IFunctionHandler *pH )
{
	int nListenerID = LISTENERID_INVALID;
		
	if (m_pSoundSystem)		
			nListenerID = m_pSoundSystem->CreateListener();

	return pH->EndFunction(nListenerID);
}

int CScriptBind_Sound::RemoveListener( IFunctionHandler *pH, int nListenerID )
{
	//SCRIPT_CHECK_PARAMETERS(1);
	//int nParamCount = pH->GetParamCount();
	bool bResult = false;

	//int nListenerID = LISTENERID_INVALID;
	//pH->GetParam(1, nListenerID);

	if (m_pSoundSystem)		
		bResult = m_pSoundSystem->RemoveListener(nListenerID);

	return pH->EndFunction(bResult);

}

int CScriptBind_Sound::SetListener( IFunctionHandler *pH, int nListenerID, Vec3 vPos, Vec3 vVel, Vec3 vForward, Vec3 vTop, bool bActive, float fRecord)
//int CScriptBind_Sound::SetListener( IFunctionHandler *pH )
{
	bool bResult = false;
	IListener* pListener = NULL;
	Matrix34 Transformation;

	// Debug
	//int nListenerID = 1;
	//Vec3 vPos(137,724,54);
	//Vec3 vVel(0,0,0);
	//Vec3 vForward(0,1,0);
	//Vec3 vTop(0,0,1);
	//float fRecord=1.0f;
	//bool bActive=true;

	if (m_pSoundSystem)		
		pListener = m_pSoundSystem->GetListener(nListenerID);

	if (pListener)
	{
		vForward.normalize();
		vTop.normalize();
		Vec3 vSide = vTop.Cross(vForward);
		vSide.normalize();

		Transformation.SetFromVectors(vForward, vSide, vTop, vPos);
		pListener->SetMatrix(Transformation);
		pListener->SetVelocity(vVel);
		pListener->SetRecordLevel(fRecord);
		pListener->SetActive(bActive);
	}

	return pH->EndFunction();
}

// TODO: remove this filth once sound groups is working
SmartScriptTable CScriptBind_Sound::WrapSoundReturn( IFunctionHandler *pH, ISound * pSound )
{
	static const int cookie = 0;

	SmartScriptTable userData = m_pSS->CreateUserData( &pSound, sizeof(pSound) );

	SmartScriptTable metaTable = SmartScriptTable(m_pSS);

	IScriptTable::SUserFunctionDesc fd;
	fd.pUserDataFunc = ReleaseFunction;
	fd.sFunctionName = "__gc";
	fd.nDataSize = sizeof(ISound*);
	fd.pDataBuffer = &pSound;
	fd.sGlobalName = "<gc-sound>";
	fd.sFunctionParams = "()";
	metaTable->AddFunction( fd );

	((CScriptTable*)userData.GetPtr())->SetMetatable( metaTable );

	return userData;
}

// TODO: remove this filth once sound groups is working
int CScriptBind_Sound::ReleaseFunction( IFunctionHandler* pH, void *pBuffer, int nSize )
{
	assert( nSize == sizeof(ISound*) );

	if (gEnv->pSoundSystem)
		(*static_cast<ISound**>(pBuffer))->Release();

	return pH->EndFunction();
}
