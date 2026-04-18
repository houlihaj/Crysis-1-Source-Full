////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   ScriptBind_Sound.h
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ScriptBind_Sound_h__
#define __ScriptBind_Sound_h__
#pragma once


#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <IScriptSystem.h>

struct IMusicSystem;
struct ISoundSystem;
struct ISound;

/*
	<title Sound>
	Syntax: Sound

	In this class are all sound-related script-functions implemented.

	IMPLEMENTATIONS NOTES:
	These function will never be called from C-Code. They're script-exclusive.
*/
class CScriptBind_Sound : public CScriptableBase
{
public:
	CScriptBind_Sound(IScriptSystem *pScriptSystem, ISystem *pSystem);
	virtual ~CScriptBind_Sound();
	virtual void GetMemoryStatistics(ICrySizer *pSizer) { pSizer->Add(this); };

	//////////////////////////////////////////////////////////////////////////
	// Comment on new SoundGroupSystem: DO NOT USE FILE EXTENSIONS ANYMORE
	// example: Sounds/Weapons/Scar:reload
	//////////////////////////////////////////////////////////////////////////
	
	// <title Precache>
	// Syntax: Sound.Precache( const char *sGroupAndSoundName, int nPrecacheFlags )
	// Description:
	//    Precaches the sound data used by a sound definition
	//    Depending on additional flags and current settings may load the sound data into the memory for later usage.
	//    Precaching is optional, sound system can also play not cached sounds, but it can cause small delay before sound playing starts.
	// Arguments:
	//    sGroupAndSoundName - SoundGroup name of the sound definition or direct filename the sound data should be loaded
	//    nPrecacheFlags - Precache flags, specify sound file caching priority.
	int Precache( IFunctionHandler *pH, const char *sSoundOrEventName, uint32 nPrecacheFlags );

	// <title Play>
	// Syntax: Sound.Play( const char *sGroupAndSoundName,Vec3 vPos, int nSoundFlags )
	// Description:
	//    Uses the sound definition of the Handle to play a sound at the position vPos .
	//		It returns a unique SoundID that is valid as long as the sound is played.
	// Arguments:
	//    sGroupAndSoundName -  SoundGroup name of the sound definition or direct filename of a sound
	//    vPos - Position where to play sound.
	//    nSoundFlags - additional Sound flags, specify how to play sound.
	// Return:
	//    SoundID.
	int Play( IFunctionHandler *pH, const char *sSoundOrEventName, Vec3 vPos, uint32 nSoundFlags, uint32 nSemantic );

	// <title PlayEx>
	// Syntax: Sound.PlayEx( const char *sGroupAndSoundName,int nSoundFlags, float fVolume, Vec3 pos,float minRadius,float maxRadius )
	// Description:
	//    Play sound file.
	// Arguments:
	//    sGroupAndSoundName -  SoundGroup name of the sound definition or direct filename of a sound
	//    vPos - Position where to play sound.
	//    nSoundFlags - Sound flags, specify how to play sound.
	//    nSemantic - Semantical information what this sound is.
	// Return:
	//    SoundID.
	int PlayEx( IFunctionHandler *pH, const char *sSoundOrEventName, Vec3 vPos, uint32 nSoundFlags, float fVolume, float minRadius, float maxRadius, uint32 nSemantic );

	// <title SetParameterValue>
	// Syntax: Sound.SetParameterValue( int SoundID, const char *sParameterName, float fParameterValue )
	// Description:
	//    Changes a parameter value of a specific sound (if supported)
	// Arguments:
	//    SoundID - unique SoundID of the sound
	//    sParameterName - name of the parameter to be changed ("distance"/"pitch")
	//    fParameterValue - value of the parameter to be set, always float
	//    nSemantic - Semantical information what this sound is.
	// Return:
	//    bool - returns true if the SoundID was valid and the parameter value could be set
	int SetParameterValue( IFunctionHandler *pH, ScriptHandle SoundID, const char *sParameterName, float fParameterValue );

	int Silence(IFunctionHandler *pH); // int (return bool)
	int DeactivateAudioDevice(IFunctionHandler *pH); // int (return bool)
	int ActivateAudioDevice(IFunctionHandler *pH); // int (return bool)
	int UnloadProjects(IFunctionHandler *pH); // int (return bool)

	int IsPlaying(IFunctionHandler *pH); //int (return int)
	int LoadSound(IFunctionHandler *pH); //char * (return int)
	int Load3DSound(IFunctionHandler *pH); //char * (return int)
	int LoadStreamSound(IFunctionHandler *pH); //char * (return int)	
	int PlaySound(IFunctionHandler *pH); //int (return void)	
	int SetSoundVolume(IFunctionHandler *pH); //int float (return void)
	int GetSoundVolume(IFunctionHandler *pH); //int (return float)
	int SetSoundLoop(IFunctionHandler *pH); //int int (return void)
	int SetSoundFrequency(IFunctionHandler *pH); //int int (return void)
	int SetSoundPitching(IFunctionHandler *pH); //int float (return void)
	int SetSoundPaused(IFunctionHandler *pH); //int bool (return void)
	int SetSoundRelative(IFunctionHandler *pH); // int bool (return void) // (un)sets the RELATIVE Flag. If unset, a new valid position has to be set, or the sound stays at the last listener position
	int StopSound(IFunctionHandler *pH);										// nSoundHandle can be SoundID or Ptr
	int SetSoundPosition(IFunctionHandler *pH); // nSoundHandle can be SoundID or Ptr
	int SetSoundSpeed(IFunctionHandler *pH); //int vector(return void)	
	int SetMinMaxDistance(IFunctionHandler *pH); // int float float float (return void)
	//int SetLoopPoints(IFunctionHandler *pH); // int int int (return void)
	int SetFadeTime(IFunctionHandler *pH); //int int (TimeInMS);
	int AddSoundFlags(IFunctionHandler *pH); // int int (return void) /TODO why is it commented out in.cpp? TN
	int SetMasterVolumeScale(IFunctionHandler *pH); // int int (return void)
  int SetMasterMusicEffectsVolume(IFunctionHandler *pH);// float (return void)

	int	SetPauseAllPlaying(IFunctionHandler *pH, bool bPause ); 
	
	// group stuff
	int AddToScaleGroup(IFunctionHandler *pH); // int int (return void)
	int RemoveFromScaleGroup(IFunctionHandler *pH); // int int (return void)
	int SetGroupScale(IFunctionHandler *pH); // int int (return bool)

	int	RegisterWeightedEaxEnvironment(IFunctionHandler *pH); // char * int bool int (return bool) 
	int	UpdateWeightedEaxEnvironment(IFunctionHandler *pH); // char * float (return bool) 
	int	UnregisterWeightedEaxEnvironment(IFunctionHandler *pH); // char * (return bool) 
	int FXEnable(IFunctionHandler *pH);	// pSound, int nEffectNumber (return void)
	int SetFXSetParamEQ(IFunctionHandler *pH);	// pSound, float fCenter,float fBandwidth,float fGain (return void)
	int	SetDirectionalAttenuation(IFunctionHandler *pH);
	int	GetDirectionalAttenuationMaxScale(IFunctionHandler *pH);
	int SetSoundRatio(IFunctionHandler *pH);
	int GetSoundLength(IFunctionHandler * pH);
	int SetWeatherCondition(IFunctionHandler * pH);

	int	RegisterSoundMood(IFunctionHandler *pH, const char *sParameterName); // char * (return bool) 
	int	UpdateSoundMood(IFunctionHandler *pH, const char *sParameterName, float fFade, uint32 nFadeTimeInMS = 0); // char * float (return bool) 
	int GetSoundMoodFade(IFunctionHandler *pH, const char *sSoundMoodName);
	int	UnregisterSoundMood(IFunctionHandler *pH, const char *sParameterName); // char * (return bool) 

	// Music
	int	LoadMusic(IFunctionHandler *pH); // string (return bool)
	int	UnloadMusic(IFunctionHandler *pH);
	int	SetMusicTheme(IFunctionHandler *pH);
	int	EndMusicTheme(IFunctionHandler *pH);
	int	SerializeMusicInternal(IFunctionHandler *pH);
	int	SetMusicMood(IFunctionHandler *pH);
	int	SetDefaultMusicMood(IFunctionHandler *pH);
	int	GetMusicThemes(IFunctionHandler *pH);
	int	GetMusicMoods(IFunctionHandler *pH);
	int AddMusicMoodEvent(IFunctionHandler *pH);
	int	IsInMusicTheme(IFunctionHandler *pH);
	int	IsInMusicMood(IFunctionHandler *pH);
	int GetMusicStatus(IFunctionHandler *pH);
	int SetMenuMusic(IFunctionHandler *pH);
	int PlayStinger(IFunctionHandler *pH);
	int PlayPattern(IFunctionHandler *pH);

		// Multi Listener

		// <title CreateListener>
		// Syntax: Sound.CreateListener( IFunctionHandler *pH )
		// Description:
		//    Creates a new Listener
		// Arguments:
		// Return:
		//    Listener ID.
		int CreateListener( IFunctionHandler *pH );

		// <title RemoveListener>
		// Syntax: Sound.RemoveListener( IFunctionHandler *pH )
		// Description:
		//    Removes a Listener
		// Arguments:
		//		nListenerID - ID of the Listener to be removed
		// Return:
		//    bResult - true/false if Listener was removed successfully
		int RemoveListener( IFunctionHandler *pH, int nListenerID );

		// <title SetListener>
		// Syntax: Sound.SetListener( IFunctionHandler *pH )
		// Description:
		//    Sets attributes to a Listener
		// Arguments:
		//		nListenerID - ID of the Listener to be removed
		//		vPos				- Position of the Listener
		//		vForward		- a Lookat vector of the Listener
		//		vTop				- a up vector of the Listener
		//		bActive			- state of Listener (active/unactive)
		//		fRecord			- input signal strength [0.0-1.0] to fade in and out
		//		vVel				- Velocity for Doppler, (0,0,0) to disable or leave out for automatic calculation
		// Return:
		//    bResult - true/false if Listener was successfully changed
		int SetListener( IFunctionHandler *pH, int nListenerID, Vec3 vPos, Vec3 vVel , Vec3 vForward, Vec3 vTop, bool bActive, float fRecord);
		//int SetListener( IFunctionHandler *pH); // keep for Debug


private: // -------------------------------------------------------
	ISound* GetSoundPtr(IFunctionHandler *pH,int index);
	SmartScriptTable WrapSoundReturn( IFunctionHandler *pH, ISound * pSound );

	ISystem *								m_pSystem;						//!<
	ISoundSystem *					m_pSoundSystem;				//!< might be 0 (e.g. dedicated server or Init() wasn't successful)
	IMusicSystem *					m_pMusicSystem;				//!< might be 0 (e.g. dedicated server or Init() wasn't successful)

	string m_sCurrentMusicTheme;
	string m_sCurrentMusicMood;

	bool bMenuMusicLoaded;

	static int ReleaseFunction( IFunctionHandler* pH, void *pBuffer, int nSize );
};

#endif // __ScriptBind_Sound_h__