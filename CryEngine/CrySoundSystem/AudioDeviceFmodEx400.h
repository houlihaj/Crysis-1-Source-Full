////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   AudioDeviceFmodEx400.h
//  Version:     v1.00
//  Created:     8/6/2005 by Tomas
//  Compilers:   Visual Studio.NET
//  Description: FmodEx 4.00 Implementation of a Audio Device
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AUDIODEVICEFMODEX400_H__
#define __AUDIODEVICEFMODEX400_H__

#include "ISound.h"
#include "soundsystemcommon.h"
#include "IAudioDevice.h"
#include "IMiniLog.h"
#include "FmodEx/inc/fmod.hpp"
//#include "FmodEx/inc/fmod_event.hpp"

#include "DebugLogger.h"
#include "CommandPlayerFmodEx400.h"

//#define FMOD_MEMORY_DEBUGGING
//#define FMOD_MEMORY_DEBUGGING_SIMPLE

class CSoundSystem;
class CSoundAssetManager;
class CSoundBuffer;
class CSound;
class CPlatformSoundFmodEx400Event;
class CPlatformSoundFmodEx400;


class CAudioDeviceFmodEx400 : public IAudioDevice
{
public:
	CAudioDeviceFmodEx400::CAudioDeviceFmodEx400(void* hWnd);
	CAudioDeviceFmodEx400::~CAudioDeviceFmodEx400();

	virtual bool InitDevice(CSoundSystem* pSoundSystem);			// returns true if init went well 
	virtual bool ShutDownDevice(void);												// shut down fmod and free system
	virtual void* GetSoundLibrary() {return m_pAPISystem; }		// returns ptr to the sound library if possible, else NULL
	virtual void*	GetEventSystem() {return m_pEventSystem;}		// returns ptr to the event system if possible, else NULL
	virtual void  GetOutputHandle( void **pHandle, EOutputHandle *HandleType);	// returns ptr to the output handle if possible, else NULL and handletype

	virtual void GetInitSettings(AudioDeviceSettings *InitSettings);
	virtual void SetInitSettings(AudioDeviceSettings *InitSettings);

	bool ResetAudio(void);													// major system attrib has been changed del all sounds clean system start
	bool UpdateListeners(void);											// this is called every frame to update all listeners and must be called *before* SubSystemUpdate()
	bool Update(void);															// example would be CS_update()
	int  GetMemoryStats(void);
	int  GetNumberSoundsPlaying(void);							//
	virtual float GetCpuUsage(void);								// returns percent of cpu usage 1.0 is 100%
	bool SetFrequency(int newFreq);									// sets sound system global frequency
	int GetMemoryUsage(class ICrySizer* pSizer);		// compute memory-consumption, returns rough estimate in MB

	// accesses wavebanks
	virtual IWavebank* GetWavebank(int nIndex);
	virtual IWavebank* GetWavebank(const char* sWavebankName);
	virtual int GetWavebankCount() { return m_Wavebanks.size(); }

	CSoundBuffer*				CreateSoundBuffer(const SSoundBufferProps &BufferProps);
	IPlatformSound*			CreatePlatformSound(CSound* pSound, const char *sEventName);
	bool								RemovePlatformSound(IPlatformSound* pPlatformSound);

	// check for multiple record devices and expose them, write name to sName pointer
	bool GetRecordDeviceInfo(const int nRecordDevice, char* sName, const int nNameLength);


	// sound system settings
	bool GetParam(enumAudioDeviceParamSemantics enumtype, ptParam* pParam); // get enum for type of info and ptr type of info gotton
	// the bool returns if this info is available on this type
	bool SetParam(enumAudioDeviceParamSemantics enumtype, ptParam* pParam);

	bool IsEax(void);

	// return handle if Project is available 
	FMOD::EventProject* LoadProjectFile(const string &sProjectPath, const string &sProjectName);

	// writes output to screen in debug
	virtual void DrawInformation(IRenderer* pRenderer, float xpos, float ypos, int nSoundInfo); 

	static CFrameProfiler& GetFMODFrameProfiler();

	void FindLostEvent(); // hack for looping weapon sound MP bug

#ifdef FMOD_MEMORY_DEBUGGING
	// memory debugging
	static void TraceMemory(void *ptr, int nSize, int nMemMode);

	typedef std::pair<void*, int> tMemPair;

	typedef std::map<void*, int> tMemMap;
	static tMemMap MemMap;
	static CDebugLogger MemLogger;
	static char	MemString[128];
#endif

#ifdef FMOD_MEMORY_DEBUGGING_SIMPLE
	static uint32 CAudioDeviceFmodEx400::nFMODMemUsage;
#endif

	//static tMemPair	m_Malloc[1000];
	//static int			m_MallocIndex;
	//static tMemPair m_Free[1000];
	//static int		m_FreeIndex;

	CCommandPlayerFmodEx400 m_CommandPlayer;

private:
	FMOD::System*				m_pAPISystemDSound;
	FMOD::System*				m_pAPISystem;
	FMOD::EventSystem*	m_pEventSystem;
	FMOD_RESULT					m_ExResult;
	CSoundSystem*				m_pSoundSystem;

	CTimeValue m_tLastUpdate;

	int					 m_nfModDriverType;
	int					 m_nCurrentMemAlloc;
	int					 m_nMaxMemAlloc;
	unsigned int m_nMemoryStatInc;
	FMOD_CAPS		m_nFMODDriverCaps;
	//int					m_nEaxStatusFMOD;

	// init vars
	AudioDeviceSettings m_InitSettings;

	int m_nHardware3DChannels;
	int m_nHardware2DChannels;
	int m_nTotalHardwareChannelsAvail;
	//int m_nSoftwareChannels;
	FMOD_SPEAKERMODE m_nSpeakerMode;
	int m_nTotalActiveSounds; // for fmod total hardware and software channels -- for xennon total voices
	int m_fMasterVolume;// volume between 0-1, 1 being max
	bool m_bSystemPaused;
	bool m_bGetSystemStats;
	bool m_bHaveListenerAttributes;
	int m_nSystemFrequency;
	bool m_bMuteStatus;
	bool m_bDopplerStatus;
	int m_nMixingBuffSize;

	CDebugLogger	 *m_pEventDumpLogger;

	struct SProjectFile
	{
		string							sProjectName;
		FMOD::EventProject	*ProjectHandle;
	};

	//typedef std::set<string>				ProjectFiles;
	//typedef ProjectFiles::iterator	ProjectFilesIter;
	//ProjectFiles										m_LoadedProjectFiles;

	typedef std::vector<SProjectFile> VecProjectFiles;
	typedef VecProjectFiles::iterator VecProjectFilesIter;
	VecProjectFiles										m_VecLoadedProjectFiles;

	typedef std::map<string, IWavebank* > tmapWavebanks;
	tmapWavebanks													m_Wavebanks;
	TFixedResourceName m_sFullWaveBankName;

	int32 m_nCountProject;
	int32 m_nCountGroup;
	int32 m_nCountEvent;

	void UpdateGroupEventCount();
	void GroupEventCount(FMOD::EventGroup* pGroup, FMOD::EventProject* pProject);

	std::vector<CPlatformSoundFmodEx400Event*>	m_UnusedPlatformSoundEvents;
	std::vector<CPlatformSoundFmodEx400*>				m_UnusedPlatformSounds;


	void *m_pHWnd; // for windows which window is active
	//float m_fpctCpuUsed;

	void FmodErrorOutput(const char * sDescription, IMiniLog::ELogType LogType = IMiniLog::eMessage);

};


class CWavebankFmodEx400 : public IWavebank
{
public:
	CWavebankFmodEx400::CWavebankFmodEx400(const char* sWavebankName) 
		{ 
			m_sWavebankName = sWavebankName; 
			m_WavebankInfo.nFileSize = 0;
	}
	CWavebankFmodEx400::~CWavebankFmodEx400();

	virtual const char* GetName() { return m_sWavebankName.c_str(); }
	virtual const char* GetPath() { return m_sWavebankPath.c_str(); }
	virtual void SetPath(const char* sWavebankPath) { m_sWavebankPath = sWavebankPath; }

	virtual SWavebankInfo* GetInfo() { return &m_WavebankInfo; }
	virtual void AddInfo(SWavebankInfo &WavebankInfo)
	{
		m_WavebankInfo.nFileSize = max(m_WavebankInfo.nFileSize, WavebankInfo.nFileSize);
		m_WavebankInfo.nTimesAccessed = max(m_WavebankInfo.nTimesAccessed, WavebankInfo.nTimesAccessed);
		m_WavebankInfo.nMemCurrentlyInByte = WavebankInfo.nMemCurrentlyInByte;
		m_WavebankInfo.nMemPeakInByte = max(m_WavebankInfo.nMemPeakInByte, WavebankInfo.nMemPeakInByte);
	}

private:
	string				m_sWavebankName;
	string				m_sWavebankPath;
	SWavebankInfo m_WavebankInfo;
};

#endif

