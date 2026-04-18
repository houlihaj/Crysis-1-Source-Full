////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   AudioDeviceFmodEx400.cpp
//  Version:     v1.00
//  Created:     8/6/2005 by Tomas
//  Compilers:   Visual Studio.NET
//  Description: FmodEx 4.00 Implementation of a Audio Device
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#ifdef SOUNDSYSTEM_USE_FMODEX400

#include <CryThread.h>
#include <ISystem.h>
#include <IConsole.h>
#include <ICryPak.h> 
#include <CrySizer.h>
#include <IRenderer.h>
#include <ITimer.h>
#include <CryMemoryAllocator.h>
#include "IAudioDevice.h"
#include "AudioDeviceFmodEx400.h"
#include "SoundBufferFmodEx400.h"
#include "PlatformSoundFmodEx400.h"
#include "SoundBufferFmodEx400Event.h"
#include "PlatformSoundFmodEx400Event.h"
#include "SoundBufferFmodEx400Micro.h"
#include "SoundBufferFmodEx400Network.h"
#include "ISound.h"
#include "Sound.h"
#include "SoundSystem.h"
#include "FmodEx/inc/fmod_errors.h"
#include "FmodEx/inc/fmod_dsp.h"
#include "FmodEx/inc/fmod.hpp"
#include "FmodEx/inc/fmod_event.hpp"
#include "FmodEx/inc/fmod_event_net.h"
#include "SoundSystemCommon.h"
#include "ISoundMoodManager.h"
#include "IReverbManager.h"

static uint32 nMainThreadId = -1;

#if defined(USE_RSX_MEM)
	unsigned int PS3_MEM_FLAG = 0;
#endif

#if defined(USE_SPURS)
	extern unsigned long _binary_fmodex_spurs_elf_start[];
	extern unsigned long _binary_fmodex_spurs_mpeg_elf_start[];
#endif

//#include <windows.h> // used for get currentthread streaming
//////////////////////////////////////////////////////////////////////////
#define IS_FMODERROR (m_ExResult != FMOD_OK )
// this starts off not active
#define GETMEMORYSTATSEVERY 1000
// ****OPTIONAL**** set the size of the mixing buffer in milliSeconds 
#define FMOD_MIXING_BUFF_SIZE 200 // mixing buffer size in millaseconds ** NOT USED AT THIS TIME ******
 

// debugging memory crash

#ifdef FMOD_MEMORY_DEBUGGING
CAudioDeviceFmodEx400::tMemMap CAudioDeviceFmodEx400::MemMap;
CDebugLogger CAudioDeviceFmodEx400::MemLogger("FMOD_Memory.log");
char	CAudioDeviceFmodEx400::MemString[128];
static void *m_CS = NULL; // Critical section pointer.

class CSmartCriticalSection
{
private:
	void *m_pCS;
public:
	CSmartCriticalSection( void *cs ) { m_pCS = cs; CryEnterCriticalSection(m_pCS); }
	~CSmartCriticalSection() { CryLeaveCriticalSection(m_pCS); }
};

void CAudioDeviceFmodEx400::TraceMemory(void *ptr, int nSize, int nMemMode)
{

	CSmartCriticalSection SmartCriticalSection(m_CS);

	tMemMap::iterator It = MemMap.find(ptr);

	switch(nMemMode)
	{
	case 1:	// alloc
				 {
					 if (It == MemMap.end())
					 {
						 sprintf(MemString,"Alloc   : %d Size: %d", ptr, nSize);
						 MemLogger.LogString(MemString,true);
						 MemMap[ptr] = nSize;
					 }
					 else
					 {
						 sprintf(MemString,"Alloc   : %d Size: %d - Already found!", ptr, nSize);
						 MemLogger.LogString(MemString,true);
					 }
				 }
				 break;
	case 2: 	// free
				 {
					 if (It == MemMap.end())
					 {
						 sprintf(MemString,"Free    : %d Size: %d - Not found!", ptr, nSize);
						 MemLogger.LogString(MemString,true);
					 }
					 else
					 {
						 sprintf(MemString,"Free    : %d Size: %d", ptr, nSize);
						 MemLogger.LogString(MemString,true);
						 MemMap.erase(It);			
					 }
				 }
				 break;
	default:
		break;
	}
}

#endif
#ifdef FMOD_MEMORY_DEBUGGING_SIMPLE
uint32 CAudioDeviceFmodEx400::nFMODMemUsage = 0;
#endif


// [Alexey] I found biggest problems with WriteLocks in CryMalloc because of tons of FMOD allocations
// Ive decided made a special fmod allocator (thread safe) and after that this problem disappeared
// We still have a statistics because node_allocator uses CryMalloc with proper statistics counting
extern "C" 
{
	node_alloc<eCryDefaultMalloc, false, 524288> g_Allocator;
	//////////////////////////////////////////////////////////////////////////
	static void * F_CALLBACK CrySound_Alloc (unsigned int size, FMOD_MEMORY_TYPE memtype)
	{

#ifdef FMOD_MEMORY_DEBUGGING
		CAudioDeviceFmodEx400::TraceMemory(ptr, size, 1);
#endif

#ifdef FMOD_MEMORY_DEBUGGING_SIMPLE
		CAudioDeviceFmodEx400::nFMODMemUsage += size;
#endif
		void* ptr = g_Allocator.alloc(size + sizeof(unsigned int));//malloc(size);
		unsigned int * t = (unsigned int*)ptr;
		*t++ = size;

		return t;

	}

	//////////////////////////////////////////////////////////////////////////
	static void F_CALLBACK CrySound_Free (void* ptr, FMOD_MEMORY_TYPE memtype)
	{
#ifdef FMOD_MEMORY_DEBUGGING
		unsigned int size = _msize( ptr );
		CAudioDeviceFmodEx400::TraceMemory(ptr, size, 2);
#endif

#ifdef FMOD_MEMORY_DEBUGGING_SIMPLE
		unsigned int nSize = _msize( ptr );
		CAudioDeviceFmodEx400::nFMODMemUsage -= nSize;
#endif

		//free (ptr);
		unsigned int *t = (unsigned int*)ptr;
		unsigned size = *--t;
		size += sizeof(unsigned int);
		g_Allocator.dealloc(t, size);

	}

	//////////////////////////////////////////////////////////////////////////
	static void * F_CALLBACK CrySound_Realloc (void *ptr, unsigned int size, FMOD_MEMORY_TYPE memtype)
	{
		void *pRealloc = NULL;

#ifdef FMOD_MEMORY_DEBUGGING
		unsigned int nOldsize = _msize( ptr );
		CAudioDeviceFmodEx400::TraceMemory(ptr, nOldsize, 2);
#endif

#ifdef FMOD_MEMORY_DEBUGGING_SIMPLE
		unsigned int nOldsize2 = _msize( ptr );
		CAudioDeviceFmodEx400::nFMODMemUsage += (size-nOldsize2);
#endif

		//		pRealloc = realloc (ptr, size);

		//g_Allocator.dealloc(ptr);
		//ptr = g_Allocator.alloc(size);
		void * np = CrySound_Alloc(size, memtype);
		size_t oldsize = CryCrtSize(ptr);//((int *)ptr)[-1];
		memmove(np, ptr, oldsize);
		CrySound_Free(ptr, memtype);
		

#ifdef FMOD_MEMORY_DEBUGGING
		CAudioDeviceFmodEx400::TraceMemory(pRealloc, size, 1);
#endif

		return np;
	}
}


//extern "C" 
//{
//	//////////////////////////////////////////////////////////////////////////
//	static void * F_CALLBACK CrySound_Alloc (unsigned int size, FMOD_MEMORY_TYPE memtype)
//	{
//		void* ptr = malloc(size);
//
//#ifdef FMOD_MEMORY_DEBUGGING
//		CAudioDeviceFmodEx400::TraceMemory(ptr, size, 1);
//#endif
//
//#ifdef FMOD_MEMORY_DEBUGGING_SIMPLE
//		CAudioDeviceFmodEx400::nFMODMemUsage += size;
//#endif
//
//		return ptr;
//
//	}
//
//	//////////////////////////////////////////////////////////////////////////
//	static void F_CALLBACK CrySound_Free (void* ptr, FMOD_MEMORY_TYPE memtype)
//	{
//#ifdef FMOD_MEMORY_DEBUGGING
//		unsigned int size = _msize( ptr );
//		CAudioDeviceFmodEx400::TraceMemory(ptr, size, 2);
//#endif
//
//#ifdef FMOD_MEMORY_DEBUGGING_SIMPLE
//		unsigned int nSize = _msize( ptr );
//		CAudioDeviceFmodEx400::nFMODMemUsage -= nSize;
//#endif
//
//		free (ptr);
//	}
//
//	//////////////////////////////////////////////////////////////////////////
//	static void * F_CALLBACK CrySound_Realloc (void *ptr, unsigned int size, FMOD_MEMORY_TYPE memtype)
//	{
//		void *pRealloc = NULL;
//
//#ifdef FMOD_MEMORY_DEBUGGING
//		unsigned int nOldsize = _msize( ptr );
//		CAudioDeviceFmodEx400::TraceMemory(ptr, nOldsize, 2);
//#endif
//
//#ifdef FMOD_MEMORY_DEBUGGING_SIMPLE
//		unsigned int nOldsize2 = _msize( ptr );
//		CAudioDeviceFmodEx400::nFMODMemUsage += (size-nOldsize2);
//#endif
//
//		pRealloc = realloc (ptr, size);
//
//#ifdef FMOD_MEMORY_DEBUGGING
//		CAudioDeviceFmodEx400::TraceMemory(pRealloc, size, 1);
//#endif
//
//		return pRealloc;
//	}
//}

//////////////////////////////////////////////////////////////////////////
// File callbacks for FMODEX.
//////////////////////////////////////////////////////////////////////////
	//typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_OPENCALLBACK)     (const char *name, int unicode, unsigned int *filesize, void **handle, void **userdata);
	//typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_CLOSECALLBACK)    (void *handle, void *userdata);
	//typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_READCALLBACK)     (void *handle, void *buffer, unsigned int sizebytes, unsigned int *bytesread, void *userdata);
	//typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_SEEKCALLBACK)     (void *handle, unsigned int pos, void *userdata);

	static FMOD_RESULT F_CALLBACK CrySoundEx_fopen(const char *name, int unicode, unsigned int *filesize, void **handle, void **userdata)
	{
		assert(name);
		assert(filesize);
		assert(handle);
				
		//DWORD threadid = GetCurrentThreadId();

#ifdef WIN32
		static bool bThreadNamed = false;

		if (!bThreadNamed)
		{
			if (GetCurrentThreadId() != nMainThreadId)
			{
				CryThreadSetName( -1,"FMOD-Streaming" );
				bThreadNamed = true;
			}
			bThreadNamed = false;
		}
#endif
		//FILE *file = gEnv->pCryPak->FOpen( name,"rb", );
		// rbx open flags, x is a hint to not cache whole file in memory.
		FILE *file = gEnv->pCryPak->FOpen( name,"rbx",ICryPak::FOPEN_HINT_DIRECT_OPERATION );
		if (!file)
		{			
			CryWarning(VALIDATOR_MODULE_SOUNDSYSTEM,VALIDATOR_ERROR,"File %s open failed!", name);
			//GetISystem()->Warning( VALIDATOR_MODULE_SOUNDSYSTEM,VALIDATOR_WARNING,VALIDATOR_FLAG_SOUND, name, "File open failed!" );
			return FMOD_ERR_FILE_NOTFOUND;	
		}
		// File is opened for streaming.
		//uint32 nSize = gEnv->pCryPak->GetFileSize(file);
		uint32 nSize = gEnv->pCryPak->FGetSize(file);
		*handle		= file;
		*filesize = nSize;

		//if (gEnv->pSoundSystem)
		//{
		//	CSoundSystem* pSoundSystem = (CSoundSystem*) gEnv->pSoundSystem;
		//	//unsigned int nTemp = nSize/1024;
		//	string sTemp = "FMOD opens: ";
		//	//string sSize = sTemp + nTemp;
		//	sTemp += name;
		//	//sTemp += " (" + sSize;
		//	//sTemp += "KB :";
		//	if (pSoundSystem)
		//	{
		//		pSoundSystem->m_pDebugLogger->LogString(sTemp);
		//		pSoundSystem->m_FMODFilesMap[file] = name;
		//	}
		//}
		

		return FMOD_OK;

	}

	static FMOD_RESULT F_CALLBACK CrySoundEx_fclose(void *nFile, void *userdata)
	{
		assert(nFile);
		if (nFile)
		{
			FILE *file = (FILE*)nFile;

			//if (gEnv->pSoundSystem)
			//{
			//	CSoundSystem* pSoundSystem = (CSoundSystem*) gEnv->pSoundSystem;
			//	string sTemp = "FMOD closes: ";
			//	if (pSoundSystem)
			//	{
			//		pSoundSystem->m_pDebugLogger->LogString(sTemp + pSoundSystem->m_FMODFilesMap[file]);
			//		pSoundSystem->m_FMODFilesMap.erase(file);
			//	}
			//}

			if (!gEnv->pCryPak->FClose( file ))
				return FMOD_OK;
		}

		return FMOD_ERR_FILE_NOTFOUND;
	}

	static FMOD_RESULT F_CALLBACK CrySoundEx_fread(void *nFile, void *buffer, unsigned int sizebytes, unsigned int *bytesread, void *userdata)
	{
		assert(nFile);
		assert(buffer);
		assert(sizebytes);
		assert(bytesread);
		FILE *file = (FILE*)nFile;

		uint32 nBytesRead = gEnv->pCryPak->FReadRaw( buffer,1,sizebytes,file );
		*bytesread = nBytesRead;
		
		if (nBytesRead != sizebytes)
			return FMOD_ERR_FILE_EOF;
			
		//if (gEnv->pSoundSystem)
		//{
			//CSoundSystem* pSoundSystem = (CSoundSystem*) gEnv->pSoundSystem;
//			string sTemp = "FMOD reads: ";
			//if (pSoundSystem)
				//pSoundSystem->m_DebugLogger.LogString(sTemp + file->_tmpfname);
		//}

		return FMOD_OK;
	}

	static FMOD_RESULT F_CALLBACK CrySoundEx_fseek(void *nFile, unsigned int pos, void *userdata)
	{
		assert(nFile);
		FILE *file = (FILE*)nFile;
		// mode not supplied
		int mode = 0;

		if (gEnv->pCryPak->FSeek( file,pos,mode ))
			return FMOD_ERR_FILE_COULDNOTSEEK;
		else
			return FMOD_OK;
	}

//static int F_CALLBACKAPI CrySound_ftell( void * nFile )
//  {
//  FILE *file = (FILE*)nFile;
//  return gEnv->pCryPak->FTell( file );
//  }


	void CAudioDeviceFmodEx400::FmodErrorOutput(const char * sDescription, IMiniLog::ELogType LogType)
	{
		switch(LogType) 
		{
		case IMiniLog::eWarning: 
			gEnv->pLog->Log("[Warning] <Sound> Sound-FmodEx-AudioDevice: %s (%d) %s\n", sDescription, m_ExResult, FMOD_ErrorString(m_ExResult));
			//gEnv->pLog->LogWarning("<Sound> Sound-FmodEx-AudioDevice: %s (%d) %s\n", sDescription, m_ExResult, FMOD_ErrorString(m_ExResult));
			break;
		case IMiniLog::eError: 
			gEnv->pLog->Log("[Error] <Sound> Sound-FmodEx-AudioDevice: %s (%d) %s\n", sDescription, m_ExResult, FMOD_ErrorString(m_ExResult));
			//gEnv->pLog->LogError("<Sound> Sound-FmodEx-AudioDevice: %s (%d) %s\n", sDescription, m_ExResult, FMOD_ErrorString(m_ExResult));
			break;
		case IMiniLog::eMessage: 
			gEnv->pLog->Log("<Sound> Sound-FmodEx-AudioDevice: %s (%d) %s\n", sDescription, m_ExResult, FMOD_ErrorString(m_ExResult));
			//gEnv->pLog->Log("<Sound> Sound-FmodEx-AudioDevice: %s (%d) %s\n", sDescription, m_ExResult, FMOD_ErrorString(m_ExResult));
			break;
		default:
			break;
		}
	}

CAudioDeviceFmodEx400::CAudioDeviceFmodEx400(void * hWnd)
{
  //GUARD_HEAP;

#ifdef FMOD_MEMORY_DEBUGGING
	m_CS = CryCreateCriticalSection();
#endif

	m_ExResult					= FMOD_OK;
	m_nSpeakerMode			= FMOD_SPEAKERMODE_RAW;
	m_pEventSystem			= NULL;
	m_pSoundSystem			= NULL;
	m_pAPISystem				= NULL;
	m_pAPISystemDSound	= NULL;
	m_pEventDumpLogger	= NULL;
	m_tLastUpdate				= gEnv->pTimer->GetFrameStartTime();

#ifdef WIN32
	nMainThreadId = GetCurrentThreadId();
#endif


	//m_ExResult = FMOD::System_SetDebugMode(FMOD::DEBUG_FILE);
	//m_ExResult = FMOD::System_SetDebugLevel(FMOD::LOG_NONE);
#if defined(XENON)
	#define XENON_FMOD_MEMBLOCK_SIZE  512*1024*100       // 51,2MB
	m_pMemBlock = XPhysicalAlloc(XENON_FMOD_MEMBLOCK_SIZE, MAXULONG_PTR, 0, PAGE_READWRITE | MEM_LARGE_PAGES);
	m_ExResult = FMOD::Memory_Initialize(m_pMemBlock, XENON_FMOD_MEMBLOCK_SIZE, 0, 0, 0);
#else
	m_ExResult = FMOD::Memory_Initialize(0, 0, CrySound_Alloc, CrySound_Realloc, CrySound_Free);
#endif

	//m_ExResult = FMOD::Memory_Initialize(malloc(64*1024*1024), 64*1024*1024, 0, 0, 0);

	if (IS_FMODERROR)
	{
		FmodErrorOutput("memory initialization failed! ", IMiniLog::eError);
	}

	//m_ExResult = FMOD::System_Create(&m_pFMODEX);
	//if (FmodExErrorCheck("system create failed! "))
		//return;

	m_InitSettings.nMaxHWchannels = 0;
	m_InitSettings.nMinHWChannels = 0;
	m_InitSettings.nSoftwareChannels = 0;
	m_InitSettings.nMPEGDecoders = 0;
	m_InitSettings.nXMADecoders = 0;
	m_InitSettings.nADPCMDecoders = 0;

  //CS_SetHWND(hWnd);
	m_nCurrentMemAlloc = 0;
	m_nMaxMemAlloc		 = 0;
//	m_nEaxStatusFMOD	 = 0;
	
	m_nHardware2DChannels = 0;
	m_nHardware3DChannels = 0;
	m_nTotalHardwareChannelsAvail = 0;

  // 0 - 1, 1 being max volume
  m_fMasterVolume = 1.0f;
	m_nFMODDriverCaps = 0;
	m_nCountProject = 0;
	m_nCountEvent = 0;
	m_nCountGroup = 0;

  // store active window
  m_pHWnd = hWnd; 

	m_ExResult = FMOD::EventSystem_Create(&m_pEventSystem);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("event system failed! ", IMiniLog::eError);
		return;
	}

	// have to do it again
	FMOD_DEBUGLEVEL nLevel = FMOD_DEBUG_LEVEL_NONE;
	m_ExResult = FMOD::Debug_SetLevel(nLevel);

	// Get FMOD-Ex System Ptr from EventSystem
	m_ExResult = m_pEventSystem->getSystemObject(&m_pAPISystem);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("get system object failed! ", IMiniLog::eError);
		return;
	}

}

#if defined(PS3)
	void InitExtraDriverData(FMOD_PS3_EXTRADRIVERDATA& rExtradriverdata)
	{
		memset(&rExtradriverdata, 0, sizeof(FMOD_PS3_EXTRADRIVERDATA));
#if defined(USE_SPURS)
		//implement me
		rExtradriverdata.spurs                             = GetIJobManSPU()->GetSPURS();
		rExtradriverdata.spu_mixer_elfname_or_spursdata    = _binary_fmodex_spurs_elf_start;
		rExtradriverdata.spu_streamer_elfname_or_spursdata = _binary_fmodex_spurs_mpeg_elf_start;
#else
		rExtradriverdata.spu_mixer_elfname_or_spursdata    = SYS_APP_HOME"/fmodex_spu.self";
		rExtradriverdata.spu_streamer_elfname_or_spursdata = SYS_APP_HOME"/fmodex_spu_mpeg.self";
#endif
		rExtradriverdata.spu_priority_mixer                = 16;  // Default
		rExtradriverdata.spu_priority_streamer             = 200; // Default
		rExtradriverdata.spu_priority_at3                  = 200; // Default
#if defined(USE_RSX_MEM)
		void* pRSXMem = NULL;
		const uint32 cAllocated = CryMallocRSX(32 * 1024 * 1024, pRSXMem);
		if(cAllocated > 0)
		{
			rExtradriverdata.rsx_pool													 = pRSXMem;
			rExtradriverdata.rsx_pool_size										 = cAllocated;
			PS3_MEM_FLAG = FMOD_LOADSECONDARYRAM;
			printf("%d KB allocated RSX memory for sound\n",cAllocated >> 10);
		}
#endif
	}
#endif

CAudioDeviceFmodEx400::~CAudioDeviceFmodEx400()
{
	ShutDownDevice();

	IWavebank *pWavebank = NULL;
	tmapWavebanks::iterator ItEnd = m_Wavebanks.end();
	for (tmapWavebanks::const_iterator It = m_Wavebanks.begin(); It!=ItEnd; ++It)
	{
		pWavebank = (*It).second;
		delete pWavebank;
	}

	int nEnd = m_UnusedPlatformSounds.size();
	for (int i=0; i<nEnd; ++i)
		delete m_UnusedPlatformSounds[i];

	nEnd = m_UnusedPlatformSoundEvents.size();
	for (int i=0; i<nEnd; ++i)
		delete m_UnusedPlatformSoundEvents[i];

#ifdef FMOD_MEMORY_DEBUGGING
	CryDeleteCriticalSection(m_CS);
	m_CS = NULL;
#endif

#if defined(XENON)
	XPhysicalFree(m_pMemBlock);
#endif
}

bool CAudioDeviceFmodEx400::InitDevice(CSoundSystem* pSoundSystem) 
{
  //GUARD_HEAP;

	if (!pSoundSystem) // check for valid system
		return (false);

#if defined(PS3)
	//extra driver data required for PS3 initialization, also used by event system
	FMOD_PS3_EXTRADRIVERDATA extradriverdata;
	InitExtraDriverData(extradriverdata);
#endif

	m_pSoundSystem = pSoundSystem;
  bool bTemp = true;
  m_nMemoryStatInc = 0;

  // system starts out not muted
  m_bMuteStatus = false;

  // make true when have new attributes for listener
  m_bHaveListenerAttributes = false;

  m_bSystemPaused = false; // starts out not paused
    
  // gets system stats when turned on
  m_bGetSystemStats = false;

	if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
	{
		if (!m_CommandPlayer.IsRecording())
			m_CommandPlayer.RecordFile("SoundCommands.xml");

		m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::EVENTSYSTEM_CREATE, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds());
	}

	//m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::EVENTSYSTEM_GETSYSTEMOBJECT, gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), m_pEventSystem, m_pAPISystem);

	// memory debugging
	//m_ExResult = m_pEventSystem->release();
	//if (IS_FMODERROR)
	//{
	//	FmodErrorOutput("event system release failed! ", IMiniLog::eError);
	//	bTemp = false;
	//}


	//m_ExResult = m_pEventSystem->release();
	//if (IS_FMODERROR)
	//{
	//	FmodErrorOutput("event system release failed! ", IMiniLog::eError);
	//	bTemp = false;
	//}

	// if not already there, create again
	if (!m_pEventSystem)
	{
		//m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::EVENTSYSTEM_CREATE, gEnv->pTimer->GetFrameStartTime().GetMilliSeconds());
		m_ExResult = FMOD::EventSystem_Create(&m_pEventSystem);
		if (IS_FMODERROR)
		{
			FmodErrorOutput("event system failed! ", IMiniLog::eError);
			return false;
		}
	}

	// and again..
	FMOD_DEBUGLEVEL nLevel = FMOD_DEBUG_LEVEL_NONE;
	m_ExResult = FMOD::Debug_SetLevel(nLevel);

	if (pSoundSystem->g_nNetworkAudition)
	{
		m_ExResult = FMOD::NetEventSystem_Init(m_pEventSystem);
		if (IS_FMODERROR)
		{
			FmodErrorOutput("event system network audition init failed! ", IMiniLog::eError);
			return false;
		}
	}

	// Get FMOD-Ex System Ptr from EventSystem
	if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
		m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::EVENTSYSTEM_GETSYSTEMOBJECT, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds());

	m_ExResult = m_pEventSystem->getSystemObject(&m_pAPISystem);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("get system object failed! ", IMiniLog::eError);
		return false;
	}

	// changing number of Mpeg codec instances
	FMOD_ADVANCEDSETTINGS settings;
	
	settings.cbsize = sizeof(FMOD_ADVANCEDSETTINGS);
	settings.ASIONumChannels = 0;
	m_ExResult = m_pAPISystem->getAdvancedSettings(&settings);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("get system advanced settings failed! ", IMiniLog::eError);
		return false;
	}

	settings.cbsize = sizeof(FMOD_ADVANCEDSETTINGS);
	settings.maxMPEGcodecs	= m_InitSettings.nMPEGDecoders;
	settings.maxADPCMcodecs = m_InitSettings.nADPCMDecoders;
	settings.maxXMAcodecs		= m_InitSettings.nXMADecoders;
	//settings.minHRTFAngle		= 90;
	////settings.maxHRTFAngle   = 0;
	//settings.minHRTFFreq    = 4000;
	//settings.maxHRTFFreq		= 22050;

	m_ExResult = m_pAPISystem->setAdvancedSettings(&settings);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("set system advanced settings failed! ", IMiniLog::eError);
		return false;
	}

	// software format
	int nSampleRate = 0;
	FMOD_SOUND_FORMAT Format = FMOD_SOUND_FORMAT_NONE;
	int nOutputChannels = 0;
	int nInputChannels = 0;
	FMOD_DSP_RESAMPLER Resampler = FMOD_DSP_RESAMPLER_NOINTERP;
	int nBits = 0;
	m_ExResult = m_pAPISystem->getSoftwareFormat(&nSampleRate, &Format, &nOutputChannels, &nInputChannels, &Resampler, &nBits);

	if (IS_FMODERROR)
	{
		FmodErrorOutput("get software format failed! ", IMiniLog::eError);
		return false;
	}

	if (pSoundSystem->g_nFormatSampleRate > 0 && pSoundSystem->g_nFormatSampleRate <= 96000)
		nSampleRate = pSoundSystem->g_nFormatSampleRate;

	if (pSoundSystem->g_nFormatType >= 0 && (FMOD_SOUND_FORMAT) pSoundSystem->g_nFormatType < FMOD_SOUND_FORMAT_MAX)
		Format = (FMOD_SOUND_FORMAT) pSoundSystem->g_nFormatType;

	if (pSoundSystem->g_nFormatResampler >= 0 && (FMOD_DSP_RESAMPLER) pSoundSystem->g_nFormatResampler < FMOD_DSP_RESAMPLER_MAX)
		Resampler = (FMOD_DSP_RESAMPLER) pSoundSystem->g_nFormatResampler;

	m_ExResult = m_pAPISystem->setSoftwareFormat(nSampleRate, Format, 0, 0, Resampler);

	if (IS_FMODERROR)
	{
		FmodErrorOutput("set software format failed! ", IMiniLog::eError);
		return false;
	}

	// output types
	FMOD_OUTPUTTYPE nOutput = FMOD_OUTPUTTYPE_AUTODETECT;

	if (pSoundSystem->g_nOutputConfig == 1)
	{
		nOutput = FMOD_OUTPUTTYPE_DSOUND;
		CryLogAlways("Sound - trying to initialize DirectSound output! \n");
	}
	if (pSoundSystem->g_nOutputConfig == 2)
	{
		nOutput = FMOD_OUTPUTTYPE_WAVWRITER;
		CryLogAlways("Sound - trying to initialize Wav-Writer output! \n");
	}
	if (pSoundSystem->g_nOutputConfig == 3)
	{
		nOutput = FMOD_OUTPUTTYPE_WAVWRITER_NRT;
		CryLogAlways("Sound - trying to initialize Wav-Writer_NRT output! \n");
	}

	if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
		m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::SYSTEM_SETOUTPUT, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), (void*)nOutput);

  m_ExResult = m_pAPISystem->setOutput(nOutput);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("set sound output failed! ", IMiniLog::eError);
		return false;
	}

	// Create DSound System object on Vista for SoftDec audio support
	m_ExResult = m_pAPISystem->getOutput(&nOutput);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("get sound output failed! ", IMiniLog::eError);
		return false;
	}

		switch (nOutput) 
	{
	case FMOD_OUTPUTTYPE_DSOUND:
		CryLogAlways("Sound - starting to initialize DirectSound output! \n");
		break;
	case FMOD_OUTPUTTYPE_WASAPI:
		CryLogAlways("Sound - starting to initialize Windows Audio Session API output! \n");
		break;
			case FMOD_OUTPUTTYPE_WINMM:
		CryLogAlways("Sound - starting to initialize Windows Multimedia output! \n");
		break;
			case FMOD_OUTPUTTYPE_XBOX360:
		CryLogAlways("Sound - starting to initialize XBox360 output! \n");
		break;
			case FMOD_OUTPUTTYPE_PS3:
		CryLogAlways("Sound - starting to initialize PS3 output! \n");
		break;
	default:
		break;
	}

#if !defined(PS3) && !defined(XENON) 
	if (nOutput != FMOD_OUTPUTTYPE_DSOUND)
	{
		m_ExResult = FMOD::System_Create(&m_pAPISystemDSound);
		if (IS_FMODERROR)
		{
			FmodErrorOutput("2nd system object failed! ", IMiniLog::eError);
			m_pAPISystemDSound = 0;
		}
		else
		{
			FMOD_DEBUGLEVEL nLevel = FMOD_DEBUG_LEVEL_NONE;
			m_ExResult = FMOD::Debug_SetLevel(nLevel);

			m_ExResult = m_pAPISystemDSound->setOutput(FMOD_OUTPUTTYPE_DSOUND);
			if (IS_FMODERROR)
			{
				FmodErrorOutput("set DSound on 2nd system object failed! ", IMiniLog::eError);
				m_pAPISystemDSound = 0;
			}
			else
			{
				if (m_pSoundSystem->g_nOutputConfig == 2 || m_pSoundSystem->g_nOutputConfig == 3)
					m_ExResult = m_pAPISystemDSound->init(5, FMOD_INIT_NORMAL, (void*)"audio_capture_dsound.wav");
				else
					m_ExResult = m_pAPISystemDSound->init(5, FMOD_INIT_NORMAL, m_pSoundSystem->g_nNetworkAudition?NULL:m_pHWnd);
				
				if (IS_FMODERROR)
				{
					FmodErrorOutput("init 2nd system object failed! ", IMiniLog::eError);
					m_pAPISystemDSound = NULL;
				}
			}
		}
	}
#endif

	int nNumDrivers = 0;
	m_ExResult = m_pAPISystem->getNumDrivers(&nNumDrivers);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("get number of drivers failed! ", IMiniLog::eError);
		//return false;

#if !defined(PS3) && !defined(XENON) 
		// enforce DSound
		m_ExResult = m_pAPISystem->setOutput(FMOD_OUTPUTTYPE_DSOUND);
		if (IS_FMODERROR)
		{
			FmodErrorOutput("set sound output (enforce DSound) failed! ", IMiniLog::eError);
			return false;
		}

		m_ExResult = m_pAPISystem->getNumDrivers(&nNumDrivers);
		if (IS_FMODERROR)
		{
			FmodErrorOutput("get number of drivers failed again! ", IMiniLog::eError);
			return false;
		}
#else
		return false;
#endif
	}
	
	{
		CryLogAlways("Sound - %d drivers found: \n", nNumDrivers);

		// prints out valid drivers
		for (int i=0; m_ExResult==FMOD_OK; ++i)
		{
			char sDriverName[MAXCHARBUFFERSIZE];
			m_ExResult = m_pAPISystem->getDriverInfo(i, sDriverName, MAXCHARBUFFERSIZE, 0);

			if (m_ExResult == FMOD_OK)
				CryLogAlways("Sound - available drivers: %d %s !\n", i, sDriverName);
		}
	}

	// setting driver
	if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
		m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::SYSTEM_SETDRIVER, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds());

	m_ExResult = m_pAPISystem->setDriver(0);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("set driver to default failed! ", IMiniLog::eError);
		return false;
	}

	// querying which driver
	int nDriver = 0;
	m_ExResult = m_pAPISystem->getDriver(&nDriver);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("get driver failed! ", IMiniLog::eError);
		return false;
	}

	// querying which driver
	char sDriverName[MAXCHARBUFFERSIZE];
	m_ExResult = m_pAPISystem->getDriverInfo(nDriver, sDriverName, MAXCHARBUFFERSIZE, 0);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("get driver name failed! ", IMiniLog::eError);
		return false;
	}

#if !defined(PS3) && !defined(XENON) 
	// Set HW and Softwarevoices
	m_ExResult = m_pAPISystem->setHardwareChannels(m_InitSettings.nMinHWChannels, m_InitSettings.nMaxHWchannels, m_InitSettings.nMinHWChannels, m_InitSettings.nMaxHWchannels);
	
	if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
		m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::SYSTEM_SETHARDWARECHANNELS, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), (void*) 0, (void*) 0, (void*) 0, (void*) 0);
	
	//m_ExResult = m_pAPISystem->setHardwareChannels(0, 0, 0, 0);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("set hardware voices limit failed! ", IMiniLog::eError);
		return false;
	}
#endif

	// overwrite file system
	m_ExResult = m_pAPISystem->setFileSystem(CrySoundEx_fopen, CrySoundEx_fclose, CrySoundEx_fread, CrySoundEx_fseek, -1);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("set file system callbacks failed! ", IMiniLog::eError);
		return false;
	}

	// need to call iseax before initt to set it up correctly (also speaker mode)
	IsEax();

	if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
		m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::SYSTEM_SETSPEAKERMODE, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), (void*) m_nSpeakerMode);

#if defined(PS3)
	m_nSpeakerMode = FMOD_SPEAKERMODE_7POINT1;
#endif
	m_ExResult = m_pAPISystem->setSpeakerMode(m_nSpeakerMode);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("set speaker mode failed! ", IMiniLog::eError);
		return false;
	}

	// TOMAS
	m_ExResult = m_pAPISystem->setSoftwareChannels(64);

	CryLogAlways("Sound - initializing FMOD-EX now!\n");
	
	if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
		m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::EVENTSYSTEM_INIT, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), (void*) m_InitSettings.nSoftwareChannels, (void*)FMOD_INIT_NORMAL, (void*) 0, (void*) FMOD_EVENT_INIT_NORMAL);

	FMOD_INITFLAGS InitFlags = FMOD_INIT_NORMAL;

	if (m_pSoundSystem->g_nObstruction == 1)
		InitFlags |= FMOD_INIT_SOFTWARE_OCCLUSION;

	if (m_pSoundSystem->g_nHRTF_DSP == 1)
		InitFlags |= FMOD_INIT_SOFTWARE_HRTF;

	if (m_pSoundSystem->g_nVol0TurnsVirtual == 1)
		InitFlags |= FMOD_INIT_VOL0_BECOMES_VIRTUAL;

	unsigned int nBlocksize = 0;
	int nNumblocks = 0;
	m_ExResult = m_pAPISystem->getDSPBufferSize(&nBlocksize, &nNumblocks);

#if !defined(PS3)
	if (m_pSoundSystem->g_nOutputConfig == 2 || m_pSoundSystem->g_nOutputConfig == 3)
	{
		m_ExResult = m_pEventSystem->init(m_InitSettings.nSoftwareChannels, InitFlags, (void*)"audio_capture.wav", FMOD_EVENT_INIT_NORMAL);
		if (IS_FMODERROR)
		{
			FmodErrorOutput("event system init failed pipeing into audio_capture.wav! ", IMiniLog::eError);
			return false;
		}
	}
	else
#endif
	{
#if !defined(PS3)
		// test for windows advanced performance hardware acceleration slider
		if (m_nFMODDriverCaps & FMOD_CAPS_HARDWARE_EMULATED) // This is really bad for latency!. 
		{                                                   /* You might want to warn the user about this. */
			m_ExResult = m_pAPISystem->setDSPBufferSize(1024, 10);    /* At 48khz, the latency between issuing an fmod command and hearing it will now be about 213ms. */
			if (IS_FMODERROR)
			{
				FmodErrorOutput("system set DSP Buffer Size failed! ", IMiniLog::eError);
				//return false;
			}
		} 
		m_ExResult = m_pEventSystem->init(m_InitSettings.nSoftwareChannels, InitFlags, m_pSoundSystem->g_nNetworkAudition?NULL:m_pHWnd, FMOD_EVENT_INIT_NORMAL);
#else
		m_ExResult = m_pEventSystem->init(m_InitSettings.nSoftwareChannels, FMOD_EVENT_INIT_NORMAL/*InitFlags*/, (void *)&extradriverdata, FMOD_EVENT_INIT_NORMAL);
#endif
		if (IS_FMODERROR)
		{
			FmodErrorOutput("event system init failed! ", IMiniLog::eError);
			if (m_ExResult == FMOD_ERR_OUTPUT_CREATEBUFFER)
			{
				// this might be an invalid speaker mode (5.0 or 7.1), so report an error if possible and fallback to stereo
				m_ExResult = m_pAPISystem->setSpeakerMode(FMOD_SPEAKERMODE_STEREO);
				
				if (IS_FMODERROR)
				{
					FmodErrorOutput("set fallback stereo speaker mode failed! ", IMiniLog::eError);
					return false;
				}

				m_ExResult = m_pEventSystem->init(m_InitSettings.nSoftwareChannels, InitFlags, m_pSoundSystem->g_nNetworkAudition?NULL:m_pHWnd, FMOD_EVENT_INIT_NORMAL);
				
				if (IS_FMODERROR)
				{
					FmodErrorOutput("event system init with fallback stereo speaker mode failed! ", IMiniLog::eError);
					return false;
				}
				else
					CryLogAlways("Sound - initialized FMOD-EX with stereo fallback\n");

			}
			else
				return false;
		}
		else
			CryLogAlways("Sound - initialized FMOD-EX\n");
	}



	// Record driver query
	for (int i=0; m_ExResult==FMOD_OK; ++i)
	{
		char sRecordDriverName[MAXCHARBUFFERSIZE];
		m_ExResult = m_pAPISystem->getRecordDriverInfo(i, sRecordDriverName, MAXCHARBUFFERSIZE, 0);

		if (m_ExResult == FMOD_OK)
			CryLogAlways("Sound - available record drivers: %d %s !\n", i, sRecordDriverName);
	}

	// set Record Driver for Microphone first
	if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
		m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::SYSTEM_SETRECORDDRIVER, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), (void*) 0);

	m_ExResult = FMOD_OK;
	//m_ExResult = m_pAPISystem->setRecordDriver(0);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("init of microphone recording failed! ", IMiniLog::eError);
	}
	
	uint32 nVersion;
	m_ExResult = m_pAPISystem->getVersion(&nVersion);
	CryLogAlways("Sound - using FMOD version: %08X and internal %08X!\n", nVersion, FMOD_VERSION);

	if (IS_FMODERROR)
	{
		FmodErrorOutput("Version number not available!", IMiniLog::eError);
		return false;
	}

	if (nVersion < FMOD_VERSION)
	{
		m_ExResult = FMOD_ERR_VERSION;
		if (IS_FMODERROR)
		{
			FmodErrorOutput("Version number conflict!", IMiniLog::eError);
			return false;
		}
	}

	char szPath[512];
	sprintf(szPath, "..\\%s\\Sounds", PathUtil::GetGameFolder().c_str());
	
	if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
		m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::EVENTSYSTEM_SETMEDIAPATH, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), (void*) szPath);

	m_ExResult = m_pEventSystem->setMediaPath(szPath);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("invalid media path! ", IMiniLog::eError);
	}

	//m_ExResult = m_pFMODEX->setOutput(FMOD_OUTPUTTYPE_AUTODETECT);
	//m_ExResult = m_pFMODEX->setDriver(0);
	
	/*
	Set the distance units. (meters/feet etc).
	*/
	const float DISTANCEFACTOR = 1.0f;   // Units per meter.  I.e feet would = 3.28.  centimeters would = 100.
	// workaround for FMOD bug: distance attenuation was 1.0f, lowered to fix
	m_ExResult = m_pAPISystem->set3DSettings(1.0, DISTANCEFACTOR, 0.01f);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("distance factor failed! ", IMiniLog::eError);
	}

  // Set memory system to crytek mem manager
  //CS_SetMemorySystem(NULL,NULL,CrySound_Alloc,CrySound_Realloc,CrySound_Free);

  // Assign file access callbacks to fmod to our pak file system.
  //CS_File_SetCallbacks( CrySound_fopen,CrySound_fclose,CrySound_fread,CrySound_fseek,CrySound_ftell );
	
  // store the amount of software channels
  //ptParamINT32 softwareChannels(softwareChan);
  //bTemp = SetParam(adpSOFTWARECHANNELS ,&softwareChannels);
  
  //******  Init fmod system setting mixing/playing speed (44100)
  // also requesting 256 software channels
  // flags are -0- for now

  // After init called this sets up hardware channel info for 3d and 2d channels
  GetNumberSoundsPlaying();

  // report info to log file
	//int32 nNuMDriver;
	//char sDriverName[MAXCHARBUFFERSIZE];

	//m_ExResult = m_pCSEX->getDriver(&nNuMDriver);
	//m_ExResult = m_pCSEX->getDriverName(nNuMDriver, sDriverName, MAXCHARBUFFERSIZE);

	int nHW2D = 0;
	int nHW3D = 0;

	gEnv->pLog->Log("-------------CRYSOUND-EX VERSION =   %08X ------- \n", FMOD_VERSION);
	m_ExResult = m_pAPISystem->getHardwareChannels(&nHW2D, &nHW3D, &m_nTotalHardwareChannelsAvail);
	gEnv->pLog->Log("Total number of 2D hardware channels available: %d\n", nHW2D);
	gEnv->pLog->Log("Total number of 3D hardware channels available: %d\n", nHW3D);
	gEnv->pLog->Log("Total number of all hardware channels available: %d\n", m_nTotalHardwareChannelsAvail);

	//m_nHardware2DChannels = 5;
	//m_nHardware3DChannels = 5;
	//m_nTotalHardwareChannelsAvail = 10;

	IReverbManager *pReverbManager = m_pSoundSystem->GetIReverbManager();
	if (pReverbManager)
	{
		pReverbManager->Init(this, m_pSoundSystem->g_nReverbInstances);
		pReverbManager->SelectReverb(m_pSoundSystem->g_nReverbType);
	}
	
	/* After eventsystem create in your code */
	//FMOD::gDebugMode  = FMOD::DEBUG_STDOUT;
	//FMOD::gDebugLevel = FMOD::LOG_NONE;


	// DSP Debugging
	//char sDSPName[512];
	//int nNums = 0;
	//FMOD::DSP* pSoundCard = NULL;
	//m_ExResult = m_pAPISystem->getDSPHead(&pSoundCard);
	//m_ExResult = pSoundCard->getInfo(sDSPName, 0,0,0,0);
	//m_ExResult = pSoundCard->getNumInputs(&nNums);


	//FMOD::DSP* pTargetUnit = NULL;
	//m_ExResult = pSoundCard->getInput(0, &pTargetUnit);
	//m_ExResult = pTargetUnit->getInfo(sDSPName, 0,0,0,0);
	//m_ExResult = pTargetUnit->getNumInputs(&nNums);

	//FMOD::DSP* pFMODMaster = NULL;
	//FMOD::DSP* pMaster = NULL;
	//FMOD::DSP* pReverb = NULL;
	//		

	//m_ExResult = pTargetUnit->getNumInputs(&nNums);

	//m_ExResult = pTargetUnit->getInput(0, &pFMODMaster);
	//if (pFMODMaster)
	//{
	//	m_ExResult = pFMODMaster->getNumInputs(&nNums);
	//	m_ExResult = pFMODMaster->getInfo(sDSPName, 0,0,0,0);
	//}

	//m_ExResult = pTargetUnit->getInput(1, &pMaster);
	//if (pMaster)
	//{
	//	FMOD::DSP* pMusic = NULL;		
	//	m_ExResult = pMaster->getInfo(sDSPName, 0,0,0,0);
	//	m_ExResult = pMaster->getNumInputs(&nNums);
	//	m_ExResult = pMaster->getInput(0, &pMusic);

	//	if (pMusic)
	//	{
	//		m_ExResult = pMusic->getNumInputs(&nNums);
	//		m_ExResult = pMusic->getInfo(sDSPName, 0,0,0,0);
	//	}
	//}	




  return bTemp;
}


// returns ptr to the output device if possible, else NULL
// this might be a pointer to DirectX LPDIRECTSOUND or a WINMM handle
void CAudioDeviceFmodEx400::GetOutputHandle( void **pHandle, EOutputHandle *HandleType)
{
	if (!pHandle || !HandleType || !m_pAPISystem)
		return; 

	if (m_pAPISystem)
		m_ExResult = m_pAPISystem->getOutputHandle(pHandle);
	
	FMOD_OUTPUTTYPE OutputType;

	if (m_pAPISystem)
		m_ExResult = m_pAPISystem->getOutput(&OutputType);

	switch(OutputType) 
	{
	case FMOD_OUTPUTTYPE_DSOUND:
		*HandleType = eOUTPUT_DSOUND;

		// prevent CRI video codec from not play intro/tutorial videos, instead play them but without any sound
		if (m_nFMODDriverCaps & FMOD_CAPS_HARDWARE_EMULATED)
			*HandleType = eOUTPUT_NOSOUND;
		break;
	case FMOD_OUTPUTTYPE_WINMM:
		*HandleType = eOUTPUT_WINMM;
		break;
	case FMOD_OUTPUTTYPE_WASAPI:
		*HandleType = eOUTPUT_WASAPI;
		break;
	case FMOD_OUTPUTTYPE_OPENAL:
		*HandleType = eOUTPUT_OPENAL;
		break;
	case FMOD_OUTPUTTYPE_ASIO:
		*HandleType = eOUTPUT_ASIO;
		break;
	case FMOD_OUTPUTTYPE_OSS:
		*HandleType = eOUTPUT_OSS;
		break;
	case FMOD_OUTPUTTYPE_ESD:
		*HandleType = eOUTPUT_ESD;
		break;
	case FMOD_OUTPUTTYPE_ALSA:
		*HandleType = eOUTPUT_ALSA;
		break;	
	//case FMOD_OUTPUTTYPE_MAC:
	//	HandleType = eOUTPUT_MAC;
	//	break;	
	case FMOD_OUTPUTTYPE_XBOX:
		*HandleType = eOUTPUT_Xbox;
		break;
	case FMOD_OUTPUTTYPE_PS2:
		*HandleType = eOUTPUT_PS2;
		break;
	case FMOD_OUTPUTTYPE_GC:
		*HandleType = eOUTPUT_GC;
		break;
	case FMOD_OUTPUTTYPE_NOSOUND:
		*HandleType = eOUTPUT_NOSOUND;
		break;
	case FMOD_OUTPUTTYPE_WAVWRITER:
		*HandleType = eOUTPUT_WAVWRITER;
		break;
default:
	*HandleType = eOUTPUT_MAX;
	}

	// use secondary DSound system
	if (m_pAPISystemDSound)
	{
		*HandleType = eOUTPUT_DSOUND;
		m_ExResult = m_pAPISystemDSound->getOutputHandle(pHandle);
	}
}

void CAudioDeviceFmodEx400::GetInitSettings(AudioDeviceSettings *InitSettings)
{
	*InitSettings = m_InitSettings;
}

void CAudioDeviceFmodEx400::SetInitSettings(AudioDeviceSettings *InitSettings)
{
	m_InitSettings = *InitSettings;
}

bool CAudioDeviceFmodEx400::ShutDownDevice(void)
{
  bool bTemp=true;
  // this one dosn't need a param
  bTemp = SetParam(adpSTOPALL_CHANNELS, NULL);
	
	if (m_pEventSystem)
	{
		if (m_pSoundSystem->g_nNetworkAudition)
		{
			m_ExResult = FMOD::NetEventSystem_Shutdown();
			if (IS_FMODERROR)
			{
				FmodErrorOutput("event system network audition shutdown failed! ", IMiniLog::eError);
				bTemp = false;
			}
		}		
		
		m_ExResult = m_pEventSystem->unload();
		if (IS_FMODERROR)
		{
			FmodErrorOutput("event system unload failed! ", IMiniLog::eWarning);
			bTemp = false;
		}

		m_ExResult = m_pEventSystem->release();
		if (IS_FMODERROR)
		{
			FmodErrorOutput("event system release failed! ", IMiniLog::eError);
			bTemp = false;
		}

		m_pEventSystem = NULL;
		m_pAPISystem = NULL;

	}

	if (m_pAPISystemDSound)
	{
		m_ExResult = m_pAPISystemDSound->close();
		if (IS_FMODERROR)
		{
			FmodErrorOutput("2nd DSound system release failed! ", IMiniLog::eError);
			bTemp = false;
		}

		m_pAPISystemDSound = NULL;

	}

	//if (m_pFMODEX)
	//{
	//	m_ExResult = m_pFMODEX->close();
	//	if (IS_FMODERROR)
	//	{
	//		FmodErrorOutput("system object close failed! ", IMiniLog::eError);
	//		bTemp = false;
	//	}

	//	m_ExResult = m_pFMODEX->release();
	//	if (IS_FMODERROR)
	//	{
	//		FmodErrorOutput("system object release failed! ", IMiniLog::eError);
	//		bTemp = false;
	//	}

	//	m_pFMODEX = NULL;
	//}

	//m_LoadedProjectFiles.clear();
	m_VecLoadedProjectFiles.clear();

	// refresh categories after unloading projects
	if (m_pSoundSystem->GetIMoodManager())
		m_pSoundSystem->GetIMoodManager()->RefreshCategories();
	

  return bTemp;
}

bool CAudioDeviceFmodEx400::ResetAudio(void)
{
  bool bTemp = true;
  //GUARD_HEAP;

	assert (m_pEventSystem);

	if (m_pSoundSystem->g_nUnloadData)
	{
		//unload Projects *important* all handles turn invalid now!
		m_ExResult = m_pEventSystem->unload();
		if (IS_FMODERROR)
		{
			FmodErrorOutput("projects couldnt be unloaded! ", IMiniLog::eError);
			bTemp = false;
		}

		m_VecLoadedProjectFiles.clear();
	}

	// refresh categories after unloading projects
	if (m_pSoundSystem->GetIMoodManager())
		m_pSoundSystem->GetIMoodManager()->RefreshCategories();

  //m_nMemoryStatInc = 0;

	//ShutDownDevice();
	//m_ExResult = FMOD::System_Create(&m_pFMODEX);
	//if (IS_FMODERROR)
	//{
	//	FmodErrorOutput("system object create failed! ", IMiniLog::eError);
	//	bTemp = false;
	//}


	//InitAudio(m_pSoundSystem, m_nSoftwareChannels);

  //if (!CS_Init(m_nSystemFrequency , m_nSoftwareChannels, 0))
	//m_ExResult = m_pFMODEX->init(100, FMOD_INIT_NORMAL, m_pHWnd);

	//if (!InitDevice(m_pSoundSystem, m_nSoftwareChannels))
	//{
	//	gEnv->pLog->Log("System re-init of CRYSOUND FAILED\n");
	//	ShutDownDevice();
	//	return false;
	//}
  //gEnv->pLog->Log("--------------  RE-INIT  --------------------------CRYSOUND VERSION = %f\n",CS_GetVersion());
	//int32 nSoftwareChannels = 0;
	//m_pFMODEX->getSoftwareChannels(&nSoftwareChannels);
	//gEnv->pLog->Log("Total number of channels available: %d\n", nSoftwareChannels);


  //bTemp = (CS_SetHWND(m_pHWnd) ? true : false); // re_set active window

  return bTemp;
}

// this is called every frame to update all listeners and must be called *before* SubSystemUpdate()
bool CAudioDeviceFmodEx400::UpdateListeners(void)
{
	if (!m_pAPISystem || !m_pEventSystem)
		return false;

	bool bResult = true;
	uint32 nNumActiveListeners = m_pSoundSystem->GetNumActiveListeners();
	FMOD_VECTOR vExPos;
	FMOD_VECTOR vExVel;
	FMOD_VECTOR vExForward;
	FMOD_VECTOR vExTop;


	CListener* pListener = NULL;
	ListenerID nListenerID = LISTENERID_INVALID;
	do 
	{
		pListener = (CListener*)m_pSoundSystem->GetNextListener(nListenerID);
		if (pListener)
		{
			nListenerID = pListener->GetID();

			if (pListener->GetActive())// && pListener->bRotated)
			{
				Vec3 vPos = pListener->GetPosition();
				Vec3 vForward = pListener->GetForward();
				Vec3 vTop = pListener->GetTop();
				vExPos.x = vPos.x;
				vExPos.y = vPos.z;
				vExPos.z = vPos.y;

				if (m_pSoundSystem->g_nDoppler)
				{
					vExVel.x = clamp(pListener->GetVelocity().x, -200.0f, 200.0f);
					vExVel.y = clamp(pListener->GetVelocity().z, -200.0f, 200.0f);
					vExVel.z = clamp(pListener->GetVelocity().y, -200.0f, 200.0f);
				}
				else
				{
					vExVel.x = 0;
					vExVel.y = 0;
					vExVel.z = 0;
				}

				vExForward.x = vForward.x;
				vExForward.y = vForward.z;
				vExForward.z = vForward.y;
				vExTop.x = vTop.x;
				vExTop.y = vTop.z;
				vExTop.z = vTop.y;

				// FMOD-EX
				//m_ExResult = m_pFMODEX->set3DNumListeners(nNumActiveListeners);
				//if (IS_FMODERROR)
				//{
				//	FmodErrorOutput("invalid number of listeners! ", IMiniLog::eWarning);
				//	bResult = false;
				//}

				//m_ExResult = m_pFMODEX->set3DListenerAttributes(nListenerID, &vExPos, &vExVel, &vExForward, &vExTop);
				//if (IS_FMODERROR)
				//{
				//	FmodErrorOutput("listener 3d update failed! ", IMiniLog::eWarning);
				//	bResult = false;
				//}

				// Event System

				{
					if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
						m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::EVENTSYSTEM_SET3DNUMLISTENERS, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), (void*)nNumActiveListeners);

					CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
					m_ExResult = m_pEventSystem->set3DNumListeners(nNumActiveListeners);
				}

				if (IS_FMODERROR)
				{
					FmodErrorOutput("event system invalid number of listeners! ", IMiniLog::eWarning);
					bResult = false;
				}

				//gEnv->pLog->Log("FMOD Update Listener Pos: %.2f,%.2f,%.2f", vPos.x, vPos.y, vPos.z);

				{	
					if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
						m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::EVENTSYSTEM_SET3DLISTENERSATTRIBUTES, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), (void*)nListenerID, (void*)&vExPos, (void*)&vExVel, (void*)&vExForward, (void*)&vExTop);

					CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
					m_ExResult = m_pEventSystem->set3DListenerAttributes(nListenerID, &vExPos, &vExVel, &vExForward, &vExTop);
				}

				if (IS_FMODERROR)
				{
					FmodErrorOutput("event system listener 3d update failed! ", IMiniLog::eWarning);
					bResult = false;
				}

				pListener->MarkAsSet();
			}
		}

	}	while (pListener);

	return bResult;
}

// must be called every game frame
bool CAudioDeviceFmodEx400::Update(void)
{
	//float position[3];
	//float velocity[3];
	bool bResult = true;

	//CTimeValue tTimeDiff = gEnv->pTimer->GetAsyncTime() - m_pSoundSystem->m_tUpdateAudioDevice;
	//float fMilliSecs = tTimeDiff.GetMilliSeconds();
	//if (fMilliSecs == 0)
	//	return bResult;

	if (m_pSoundSystem)
	{
		int nDebugLevel = m_pSoundSystem->g_nDebugSound;

		FMOD_DEBUGLEVEL nLevel = FMOD_DEBUG_LEVEL_NONE;

		if (nDebugLevel == SOUNDSYSTEM_DEBUG_FMOD_SIMPLE)
			nLevel = (FMOD_DEBUG_TYPE_EVENT | 
								FMOD_DEBUG_DISPLAY_TIMESTAMPS | 
								FMOD_DEBUG_LEVEL_ERROR);

		if (nDebugLevel == SOUNDSYSTEM_DEBUG_FMOD_COMPLEX)
			nLevel = (FMOD_DEBUG_TYPE_EVENT | 
								FMOD_DEBUG_DISPLAY_TIMESTAMPS | 
								FMOD_DEBUG_LEVEL_ERROR | 
								FMOD_DEBUG_LEVEL_WARNING);

		if (nDebugLevel == SOUNDSYSTEM_DEBUG_FMOD_ALL)
			nLevel = (FMOD_DEBUG_TYPE_EVENT | 
								FMOD_DEBUG_DISPLAY_TIMESTAMPS | 
								FMOD_DEBUG_LEVEL_ERROR | 
								FMOD_DEBUG_LEVEL_WARNING |
								FMOD_DEBUG_LEVEL_HINT | 
								FMOD_DEBUG_TYPE_MEMORY);

		m_ExResult = FMOD::Debug_SetLevel(nLevel);

		if (nDebugLevel == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
		{
			if (!m_CommandPlayer.IsRecording())
				m_CommandPlayer.RecordFile("SoundCommands.xml");
		}
		else
		{
			m_CommandPlayer.StopRecording();
		}

	}
	
	
	//DWORD threadid = GetCurrentThreadId();

	// every ? calls get stats (memory and cpu usage pct)
	//if (m_bGetSystemStats)
	//{
	//	if (++m_nMemoryStatInc >= GETMEMORYSTATSEVERY)
	//	{
	//		FRAME_PROFILER( "FMOD-GetMem_CPU", GetISystem(), PROFILE_SOUND );	
	//		GetCpuPctUsage();
	//		GetMemoryStats();
	//		m_nMemoryStatInc = 0;
	//	}
	//}

	int nUpdateLoops = 0;

	if (m_pSoundSystem->g_nOutputConfig == 3)
	{
		CTimeValue tCurrent = gEnv->pTimer->GetFrameStartTime();
		CTimeValue tTimeDiff = tCurrent - m_tLastUpdate;
		m_tLastUpdate = tCurrent;

		//CTimeValue tTimeDiff = (gEnv->pTimer->GetAsyncTime() - gEnv->pTimer->GetFrameStartTime()); //m_pSoundSystem->m_tUpdateAudioDevice;
		float fMS = tTimeDiff.GetMilliSeconds();

		if (fMS < 5000.0f) // prevent long pause (5 sec) of silence when activated at runtime
		{
			while (abs(fMS) > 21.33f)
			{
				++nUpdateLoops;
				fMS -= 21.33f;
			}
			tTimeDiff.SetMilliSeconds((int64)fMS);
			m_tLastUpdate -= tTimeDiff;
		}
	}
	else
		nUpdateLoops = 1;

	// update crysound must be called every frame
	while (m_pEventSystem && nUpdateLoops>0)
	{	
			--nUpdateLoops;

			
			{
				if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
					m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::EVENTSYSTEM_UPDATE, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds());

				FRAME_PROFILER( "FMOD-EventUpdate", GetISystem(), PROFILE_SOUND );	
				m_ExResult = m_pEventSystem->update();
			}	

			if (IS_FMODERROR) 
			{
				// disable output for VS2
				FmodErrorOutput("event system update failed! ", IMiniLog::eWarning);
				bResult = false;
			}

			if (m_pSoundSystem->g_nNetworkAudition)
			{
				CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
				m_ExResult = FMOD::NetEventSystem_Update();
				if (IS_FMODERROR)
				{
					FmodErrorOutput("event system network audition update failed! ", IMiniLog::eWarning);
					bResult = false;
				}
			}

			if (m_pSoundSystem->g_nProfiling > 0)
			{
				FMOD_EVENT_SYSTEMINFO SystemInfo;
				memset(&SystemInfo, 0, sizeof(FMOD_EVENT_SYSTEMINFO));

				{
					CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
					m_ExResult = m_pEventSystem->getInfo(&SystemInfo);
				}
				if (IS_FMODERROR)
				{
					FmodErrorOutput("event system get system info failed! ", IMiniLog::eWarning);
					bResult = false;
				}

				//nSizeInMB += Info.instancememory;

				for (int i=0; i<SystemInfo.numwavebanks; ++i)
				{
					tmapWavebanks::iterator It = m_Wavebanks.find(CONST_TEMP_STRING(SystemInfo.wavebankinfo[i].name));

					IWavebank *pWavebank = NULL;
					if (It == m_Wavebanks.end())
					{
						pWavebank = new CWavebankFmodEx400(SystemInfo.wavebankinfo[i].name);
						if (!pWavebank)
							return false;

						m_Wavebanks[SystemInfo.wavebankinfo[i].name] = pWavebank;
					}
					else
						pWavebank = (*It).second;

					IWavebank::SWavebankInfo BankInfo;
					BankInfo.nMemCurrentlyInByte = SystemInfo.wavebankinfo[i].streammemory + SystemInfo.wavebankinfo[i].samplememory;
					BankInfo.nMemPeakInByte = SystemInfo.wavebankinfo[i].streammemory + SystemInfo.wavebankinfo[i].samplememory;

					if (*pWavebank->GetPath())
					{
						CCryFile file;

						m_sFullWaveBankName = "";

						int nPathLength = strlen(pWavebank->GetPath());
						int nNameLength = strlen(pWavebank->GetName());
						if (nPathLength + nNameLength + 4 < 512)
						{
							m_sFullWaveBankName = pWavebank->GetPath();
							m_sFullWaveBankName += pWavebank->GetName();
							m_sFullWaveBankName += ".fsb";

							if (file.Open( m_sFullWaveBankName, "rb" ))
								BankInfo.nFileSize = file.GetLength();
						}

					}
					pWavebank->AddInfo(BankInfo);

					//nSizeInMB += Info.wavebankinfo[i].streammemory + Info.wavebankinfo[i].samplememory;
				}

				//nSizeInMB = nSizeInMB/(1024*1024);

				//m_Wavebanks

			}
			FindLostEvent(); // call to find and stop MP-looping-sound-bug
	}


	return bResult;
}

int CAudioDeviceFmodEx400::GetMemoryStats(void)
{

#ifdef FMOD_MEMORY_DEBUGGING_SIMPLE
	return nFMODMemUsage; // use own traced memory for now
#endif

	if (m_pAPISystem)
	{
		CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );	
		m_ExResult = FMOD::Memory_GetStats(&m_nCurrentMemAlloc, &m_nMaxMemAlloc);
	}
	if (IS_FMODERROR)
	{
		FmodErrorOutput("system get memory stats failed! ", IMiniLog::eWarning);
		return 0;
	}

	return m_nCurrentMemAlloc;
}

bool CAudioDeviceFmodEx400::IsEax(void)
{
	if (m_pAPISystem && !m_nFMODDriverCaps)
	{
		//int nDriver = 0;

		if (m_nSpeakerMode != FMOD_SPEAKERMODE_MAX)
			//m_ExResult = m_pFMODEX->getDriver(&nDriver);
			m_ExResult = m_pAPISystem->getDriverCaps(0, &m_nFMODDriverCaps, 0, 0, &m_nSpeakerMode);
		else
			m_ExResult = m_pAPISystem->getDriverCaps(0, &m_nFMODDriverCaps, 0, 0, 0);

		if (m_nFMODDriverCaps & FMOD_CAPS_HARDWARE_EMULATED)
			gEnv->pLog->Log(" *WARNING* Hardware acceleration turned off sound performance settings!");

		if (m_nFMODDriverCaps & FMOD_CAPS_HARDWARE)
			gEnv->pLog->Log(" Driver supports hardware 3D sound");
		
		if (m_nFMODDriverCaps & FMOD_CAPS_REVERB_EAX2)
			gEnv->pLog->Log(" Driver supports EAX 2.0 reverb");

		if (m_nFMODDriverCaps & FMOD_CAPS_REVERB_EAX3)
			gEnv->pLog->Log(" Driver supports EAX 3.0 reverb");

		if (m_nFMODDriverCaps & FMOD_CAPS_REVERB_EAX4)
			gEnv->pLog->Log(" Driver supports EAX 4.0 reverb");
	}

	return (m_nFMODDriverCaps & (FMOD_CAPS_REVERB_EAX2 | FMOD_CAPS_REVERB_EAX3 | FMOD_CAPS_REVERB_EAX4))?true:false;
}

int  CAudioDeviceFmodEx400::GetNumberSoundsPlaying(void)
{
	int nTemp = 0;
	ptParamINT32 Param(nTemp);
	GetParam(adpCHANNELS_PLAYING, &Param);
	Param.GetValue(nTemp);

	return nTemp;
}

// percent of cpu usage by CrySound mixer,dsound, and streams
float CAudioDeviceFmodEx400::GetCpuUsage(void)
{
	float fDSP = 0.0f;
	float fStream = 0.0f;
	float fUpdate = 0.0f;
	float fTotal = 0.0f;

	if (m_pAPISystem)
	{
		CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
		m_ExResult = m_pAPISystem->getCPUUsage(&fDSP,&fStream,&fUpdate,&fTotal);
	}

	if (IS_FMODERROR)
	{
		FmodErrorOutput("system get CPU usage failed! ", IMiniLog::eWarning);
		return 0.0f;
	}

	return fTotal;
}

// sets whole sound system's frequency
bool CAudioDeviceFmodEx400::SetFrequency(int newFreq)
{
  bool bTemp = true;

  m_nSystemFrequency = newFreq;

  //bTemp = (CS_SetFrequency(CS_ALL,newFreq) ? true : false);
  return bTemp;
}

// compute memory-consumption, returns rough estimate in MB
int CAudioDeviceFmodEx400::GetMemoryUsage(class ICrySizer* pSizer)
{
	// TODO MAJOR REVIEW HERE !

	int nSizeInByte = 0;
	int nWavebank = 0;

	if (pSizer)
	{
		if (!pSizer->Add(*this))
			return 0;

		//SIZER_COMPONENT_NAME(pSizer, "FMOD");
	}

	FMOD_EVENT_SYSTEMINFO SystemInfo;
	memset(&SystemInfo, 0, sizeof(FMOD_EVENT_SYSTEMINFO));

	m_ExResult = m_pEventSystem->getInfo(&SystemInfo);
	if (IS_FMODERROR)
	{
		FmodErrorOutput("event system get system info failed! ", IMiniLog::eWarning);
		return 0;
	}

	//CSoundBuffer *pBuffer=m_pSoundBuffer;
	int nFMODCurrent = 0;
	int nMax = 0;
	m_ExResult = FMOD::Memory_GetStats(&nFMODCurrent, &nMax);
	
	if (IS_FMODERROR)
	{
		FmodErrorOutput("system get memory stats failed! ", IMiniLog::eWarning);
		return 0;
	}

	nSizeInByte += nFMODCurrent;

	int nTempMemory = 0;

	if (pSizer)
	{
#ifdef FMOD_MEMORY_DEBUGGING_SIMPLE
		nSizeInByte = nFMODMemUsage;
		pSizer->AddObject(m_pAPISystem, nFMODMemUsage);
#else
		pSizer->AddObject(m_pAPISystem, nFMODCurrent);

#endif
		//pSizer->AddObject(m_pAPISystem, nFMODCurrent); // use own traced memory
		//pSizer->AddObject(m_pEventSystem, Info.instancememory);
	}

	nTempMemory = SystemInfo.eventmemory + SystemInfo.instancememory + SystemInfo.dspmemory;

	for (int i=0; i<SystemInfo.numwavebanks; ++i)
	{
		nWavebank += SystemInfo.wavebankinfo[i].streammemory + SystemInfo.wavebankinfo[i].samplememory;
		nTempMemory += SystemInfo.wavebankinfo[i].streammemory + SystemInfo.wavebankinfo[i].samplememory;
		//if (pSizer)
			//pSizer->AddObject(&(Info.wavebankinfo[i]), Info.wavebankinfo[i].streammemory + Info.wavebankinfo[i].samplememory);
	}

	//nSizeInByte += nWavebank;
#ifdef FMOD_MEMORY_DEBUGGING
	int nNum = MemMap.size();
	int nSum = 0;
	tMemMap::iterator ItEnd = MemMap.end();
	for (tMemMap::iterator It = MemMap.begin(); It!=ItEnd; ++It)
	{
		nSum += (*It).second;
	}
#endif

	return nSizeInByte;

}

// accesses wavebanks
IWavebank* CAudioDeviceFmodEx400::GetWavebank(int nIndex)
{
	IWavebank *pWavebank = NULL;

	int nIdx=0;
	tmapWavebanks::iterator ItEnd = m_Wavebanks.end();
	for (tmapWavebanks::const_iterator It = m_Wavebanks.begin(); It!=ItEnd; ++It)
	{
		if (nIndex == nIdx)
			pWavebank = (*It).second;
		++nIdx;
	}

	return pWavebank;
}

// accesses wavebanks
IWavebank* CAudioDeviceFmodEx400::GetWavebank(const char* sWavebankName)
{
	IWavebank *pWavebank = NULL;

	int nIdx=0;
	tmapWavebanks::iterator It = m_Wavebanks.find(CONST_TEMP_STRING(sWavebankName));
	if (It != m_Wavebanks.end())
	{
			pWavebank = (*It).second;
	}

	return pWavebank;
}

//MVD
// creates a new platform dependent SoundBuffer
CSoundBuffer* CAudioDeviceFmodEx400::CreateSoundBuffer(const SSoundBufferProps &BufferProps)
{
	CSoundBuffer* pBuf = NULL;

	if (BufferProps.eBufferType == btEVENT)
		pBuf = new CSoundBufferFmodEx400Event(BufferProps, m_pAPISystem);

	if (BufferProps.eBufferType == btMICRO)
		pBuf = new CSoundBufferFmodEx400Micro(BufferProps, m_pAPISystem);

	if (BufferProps.eBufferType == btNETWORK)
		pBuf = new CSoundBufferFmodEx400Network(BufferProps, m_pAPISystem);

	if (!pBuf)
		pBuf = new CSoundBufferFmodEx400(BufferProps, m_pAPISystem);

	return pBuf;
}

// creates a new platform dependent PlatformSound
IPlatformSound* CAudioDeviceFmodEx400::CreatePlatformSound(CSound* pSound, const char *sEventName)
{
	IPlatformSound* pPlatform = NULL;

	if (sEventName[0])
	{
		if (m_UnusedPlatformSoundEvents.empty())
			pPlatform = new CPlatformSoundFmodEx400Event(pSound, m_pAPISystem, sEventName);
		else
		{
			pPlatform = m_UnusedPlatformSoundEvents[m_UnusedPlatformSoundEvents.size()-1];
			pPlatform->Reset(pSound, sEventName);
			m_UnusedPlatformSoundEvents.pop_back();
		}
	}
	else
	{
		if (m_UnusedPlatformSounds.empty())
			pPlatform = new CPlatformSoundFmodEx400(pSound, m_pAPISystem);
		else
		{
			pPlatform = m_UnusedPlatformSounds[m_UnusedPlatformSounds.size()-1];
			pPlatform->Reset(pSound, "");
			m_UnusedPlatformSounds.pop_back();
		}
	}

	return pPlatform;
}

bool CAudioDeviceFmodEx400::RemovePlatformSound(IPlatformSound* pPlatformSound)
{
	if (pPlatformSound)
	{
		if (pPlatformSound->GetClass() == pscEVENT)
			m_UnusedPlatformSoundEvents.push_back((CPlatformSoundFmodEx400Event*)pPlatformSound);
		else
			m_UnusedPlatformSounds.push_back((CPlatformSoundFmodEx400*)pPlatformSound);
	}

	return true;
}

// check for multiple record devices and expose them, write name to sName pointer
bool CAudioDeviceFmodEx400::GetRecordDeviceInfo(const int nRecordDevice, char* sName, const int nNameLength)
{
	if (m_pAPISystem)
	{
		CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
		m_pAPISystem->getRecordDriverInfo(nRecordDevice, sName, nNameLength, 0);
		if (IS_FMODERROR)
		{
			FmodErrorOutput("retrieve microphone record information failed! ", IMiniLog::eWarning);
			return false;
		}
	}
	return true;
}


/*
adpSOFTWARECHANNELS,
adpHARDWARECHANNELS,
adpTOTALCHANNELS,
adpEAX_STATUS,
adpMUTE_STATUS,
adpLISTENER_POSITION,
adpLISTENER_VELOCITY,
adpLISTENER_FORWARD,
adpLISTENER_TOP,
adpWINDOW_HANDLE,
adpSYSTEM_FREQUENCY,
adpMASTER_VOLUME,
adpMIXING_RATE,
adpSPEAKER_MODE,
adpPAUSED_STATUS,
adpSNDSYSTEM_MEM_USAGE_NOW,
adpSNDSYSTEM_MEM_USAGE_HIGHEST,
adpSNDSYSTEM_OUTPUTTYPE,
adpSNDSYSTEM_DRIVER
adpDOPPLER
*/
bool CAudioDeviceFmodEx400::GetParam(enumAudioDeviceParamSemantics enumtype, ptParam* pParam)
{
	int   nTemp = 0;
	float fTemp = 0.0f;
	bool  bTemp = false;;
	Vec3	vTemp;
	string sTemp;  
	void * vpTemp = NULL;
	float tempVect[3];
	bool result=true;


	switch(enumtype) 
	{
    case adpDOPPLER: // turn doppler on or off with a bool
      if (!(pParam->GetValue(bTemp))) 
				return (false);
      bTemp = m_bDopplerStatus;
      pParam->SetValue(bTemp);
      break;
    case adpHARDWARECHANNELS:// number of hardware channels available
      if (!(pParam->GetValue(nTemp))) 
				return (false);
      GetNumberSoundsPlaying();
      nTemp = m_nTotalHardwareChannelsAvail;
      pParam->SetValue(nTemp);
      break;
    case adpTOTALCHANNELS:// total number of channels available
      if (!(pParam->GetValue(nTemp))) 
				return (false);
      GetNumberSoundsPlaying();
      nTemp = m_nTotalHardwareChannelsAvail + m_InitSettings.nSoftwareChannels;
      pParam->SetValue(nTemp);
      break;
    case adpEAX_STATUS:// is eax available on this machine
      if (!(pParam->GetValue(bTemp))) 
				return (false);
      bTemp = IsEax();
      pParam->SetValue(bTemp);
      break;
    case adpMUTE_STATUS:// returns bool telling caller if system wide mute on=true or off=false
      if (!(pParam->GetValue(bTemp))) 
				return (false);
      bTemp = m_bMuteStatus;
      pParam->SetValue(bTemp);
      break;
    case adpLISTENER_POSITION:// gets active Listeners position
      if (!(pParam->GetValue(vTemp))) 
				return (false);
      tempVect[0] = tempVect[1] = tempVect[2] = 0.0f;
      //CS_3D_Listener_GetAttributes(tempVect, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
      vTemp(tempVect[0], tempVect[2], tempVect[1]);
      pParam->SetValue(vTemp);
      break;
    case adpLISTENER_VELOCITY:// gets active Listeners velocity
      if (!(pParam->GetValue(vTemp))) 
				return (false);
      tempVect[0] = tempVect[1] = tempVect[2] = 0.0f;
      //CS_3D_Listener_GetAttributes(NULL, tempVect, NULL, NULL, NULL, NULL, NULL, NULL);
      vTemp(tempVect[0], tempVect[2], tempVect[1]);
      pParam->SetValue(vTemp);
      break;
    case adpLISTENER_FORWARD:// gets active Listeners direction or forward
      if (!(pParam->GetValue(vTemp))) 
				return (false);
      tempVect[0] = tempVect[1] = tempVect[2] = 0.0f;
      //CS_3D_Listener_GetAttributes(NULL, NULL, &tempVect[0], &tempVect[1], &tempVect[2], NULL, NULL, NULL);
      vTemp(tempVect[0], tempVect[2], tempVect[1]);
      pParam->SetValue(vTemp);
      break;
    case adpLISTENER_TOP:// gets active Listeners top
      if (!(pParam->GetValue(vTemp))) 
				return (false);
      tempVect[0] = tempVect[1] = tempVect[2] = 0.0f;
      //CS_3D_Listener_GetAttributes(NULL, NULL, NULL, NULL, NULL, &tempVect[0], &tempVect[1], &tempVect[2]);
      vTemp(tempVect[0], tempVect[2], tempVect[1]);
      pParam->SetValue(vTemp);
      break;
    case adpWINDOW_HANDLE:// returns handle of active window which is void * type
      if (!(pParam->GetValue(vpTemp))) 
				return (false);
      vpTemp = m_pHWnd;
      pParam->SetValue(vpTemp);
      break;
    case adpSYSTEM_FREQUENCY:// gets the mixing speed
      if (!(pParam->GetValue(nTemp))) 
				return (false);
      //nTemp = CS_GetFrequency(CS_ALL);
      pParam->SetValue(nTemp);
			break;
		case adpMASTER_VOLUME: // value from 0-1, 1 being max fmod master volume
			{
				if (!(pParam->GetValue(fTemp))) 
					return (false);

				CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
				FMOD::ChannelGroup *MasterChannelGroup = 0;
				m_ExResult = m_pAPISystem->getMasterChannelGroup(&MasterChannelGroup);
				if (!IS_FMODERROR)
				{
					m_ExResult = MasterChannelGroup->getVolume(&fTemp);
				}
				pParam->SetValue(nTemp);
				break;
			}
		case adpMASTER_PAUSE: // value true/false
			{
				if (!(pParam->GetValue(bTemp))) 
					return (false);

				CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
				FMOD::ChannelGroup *MasterChannelGroup = 0;
				m_ExResult = m_pAPISystem->getMasterChannelGroup(&MasterChannelGroup);
				if (!IS_FMODERROR)
				{
					m_ExResult = MasterChannelGroup->getPaused(&bTemp);
				}
				pParam->SetValue(nTemp);
				break;
			}
    case adpSPEAKER_MODE:// gets active speaker mode
      if (!(pParam->GetValue(nTemp))) 
				return (false);
			switch (m_nSpeakerMode)
			{
			case FMOD_SPEAKERMODE_MONO :
				nTemp = 1;
				break;
			case FMOD_SPEAKERMODE_STEREO :
				nTemp = 2;
				break;
			case FMOD_SPEAKERMODE_QUAD :
				nTemp = 4;
				break;
			case FMOD_SPEAKERMODE_5POINT1 :
				nTemp = 5;
				break;
			case FMOD_SPEAKERMODE_7POINT1 :
				nTemp = 7;
				break;
			case FMOD_SPEAKERMODE_PROLOGIC :
				nTemp = 9;
				break;
			}
      pParam->SetValue(nTemp);
      break;
    case adpPAUSED_STATUS:// is fmod paused or not
      if (!(pParam->GetValue(bTemp))) 
				return (false);
      bTemp = m_bSystemPaused;
      pParam->SetValue(bTemp);
      break;
    case adpSNDSYSTEM_MEM_USAGE_NOW:// returns how much memory fmod is using RIGHT NOW
      if (!(pParam->GetValue(nTemp))) 
				return (false);
      GetMemoryStats();
      nTemp = m_nCurrentMemAlloc;
      pParam->SetValue(nTemp);
      break;
    case adpSNDSYSTEM_MEM_USAGE_HIGHEST:// returns the amount of memory fmod used at its PEEK
      if (!(pParam->GetValue(nTemp))) 
				return (false);
      GetMemoryStats();
      nTemp = m_nMaxMemAlloc;
      pParam->SetValue(nTemp);
      break;
    case adpSNDSYSTEM_OUTPUTTYPE:// returns a enum in int form of output type
      /*
      CS_OUTPUT_NOSOUND,     NoSound driver, all calls to this succeed but do nothing.
      CS_OUTPUT_WINMM,     Windows Multimedia driver. 
      CS_OUTPUT_DSOUND,    DirectSound driver.  You need this to get EAX2 or EAX3 support, or FX api support. 
      CS_OUTPUT_A3D,       A3D driver. 

      CS_OUTPUT_OSS,        Linux/Unix OSS (Open Sound System) driver, i.e. the kernel sound drivers. 
      CS_OUTPUT_ESD,        Linux/Unix ESD (Enlightment Sound Daemon) driver. 
      CS_OUTPUT_ALSA,       Linux Alsa driver. 

      CS_OUTPUT_ASIO,       Low latency ASIO driver 
      CS_OUTPUT_XBOX,       Xbox driver 
      CS_OUTPUT_PS2,        PlayStation 2 driver 
      CS_OUTPUT_MAC,        Mac SoundManager driver 
      CS_OUTPUT_GC,         Gamecube driver 
      CS_OUTPUT_NOSOUND_NONREALTIME   This is the same as nosound, but the sound generation is driven by CS_Update 
      */
      if (!(pParam->GetValue(nTemp))) 
				return (false);
      //nTemp = CS_GetOutput();
      pParam->SetValue(nTemp);
      break;
    case adpSNDSYSTEM_DRIVER:// sets a string name of driver
      if (!(pParam->GetValue(sTemp))) 
				return (false);
      //sTemp = CS_GetDriverName(CS_GetOutput());
      pParam->SetValue(sTemp);
      break;
    case adpMIXING_BUFFER_SIZE:// ****** OPTIONAL ****** if set returns mixing buffer size in milliseconds (fmod does automaticly)
      if (!(pParam->GetValue(nTemp))) 
				return (false);
      nTemp = m_nMixingBuffSize;
      pParam->SetValue(nTemp);
			break;
		case adpCHANNELS_PLAYING:// returns number of channels being used
			{
				if (!(pParam->GetValue(nTemp)) || !m_pAPISystem) 
					return (false);

				CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
				m_ExResult = m_pAPISystem->getChannelsPlaying(&nTemp);
				if (IS_FMODERROR)
				{
					FmodErrorOutput("system get channels playing failed! ", IMiniLog::eWarning);
					return false;
				}

				pParam->SetValue(nTemp);
				return false; // disable FMOD-EX polling because event system is ignored (BUG)
				break;
			}
		case adpSTOPALL_CHANNELS:// ******* NOT ACTIVE !!!!!
			break;
		case adpEVENTCOUNT:
			if (!(pParam->GetValue(nTemp))) 
				return (false);
			nTemp = m_nCountEvent;
			pParam->SetValue(nTemp);
			break;
		case adpGROUPCOUNT:
			if (!(pParam->GetValue(nTemp))) 
				return (false);
			nTemp = m_nCountGroup;
			pParam->SetValue(nTemp);
			break;

    default:
      break;
    }
    return result;
	}
	bool CAudioDeviceFmodEx400::SetParam(enumAudioDeviceParamSemantics enumtype, ptParam* pParam)
	{
		int   nTemp = 0;
		float fTemp = 0.0f;
		bool  bTemp = false;
		Vec3	vTemp;
		string sTemp;  
		void * vpTemp = NULL;
		bool result = true;

		switch(enumtype) 
		{
		case adpHARDWARECHANNELS:
		case adpTOTALCHANNELS:
		case adpEAX_STATUS:          // these values are not used at this time
    case adpCHANNELS_PLAYING:
    case adpSNDSYSTEM_MEM_USAGE_NOW:
    case adpSNDSYSTEM_MEM_USAGE_HIGHEST:
      result = false;
      break;
    case adpMIXING_BUFFER_SIZE:// ****OPTIONAL**** set the size of the mixing buffer in milliSeconds 
      if (!(pParam->GetValue(nTemp))) 
				return (false);
      m_nMixingBuffSize = nTemp;
      //result = (CS_SetBufferSize(nTemp) ? true : false);
      break;
    case adpMUTE_STATUS:// changes the system mute status ,bTemp true mutes the system false reactivates sound system
      if (!(pParam->GetValue(bTemp))) 
				return (false);
      m_bMuteStatus = bTemp;
      //result = (CS_SetMute(CS_ALL, (signed char)bTemp) ? true : false);
			break;

		case adpLISTENER_POSITION:// sets listener position
			return (false);

			break;
		case adpLISTENER_VELOCITY:// sets 3d listener velocity
			return (false);

			break;
		case adpLISTENER_FORWARD:// sets 3d listener forward value
			return (false);

			break;
		case adpLISTENER_TOP:// sets 3d listener top value
			return (false);

      break;
    case adpWINDOW_HANDLE:// sets fmod window output handle
      if (!(pParam->GetValue(vpTemp))) 
				return (false);
      //result = (CS_SetHWND(vpTemp) ? true : false);
      m_pHWnd = vpTemp;
      break;
    case adpSYSTEM_FREQUENCY:// sets fmod global mixing speed
      if (!(pParam->GetValue(nTemp))) 
				return (false);
      m_nSystemFrequency = nTemp;
			//result = (CS_SetFrequency(CS_ALL, nTemp) ? true : false);
			break;
		case adpMASTER_VOLUME:// sets the fmod master volume 0 - 1, 1 being highest volume
			{
				if (!(pParam->GetValue(fTemp)) || !m_pAPISystem) 
					return (false);

				if (fTemp < 0.0f)
					fTemp = 0.0f;
				else if (fTemp > 1.0f)
					fTemp = 1.0f;

				CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
				FMOD::ChannelGroup *pChannelGroup = 0;
				m_ExResult = m_pAPISystem->getMasterChannelGroup(&pChannelGroup);

				if (IS_FMODERROR)
				{
					FmodErrorOutput("system get master channel group failed! ", IMiniLog::eWarning);
					return false;
				}
					
				m_ExResult = pChannelGroup->setVolume(fTemp);
				
				if (IS_FMODERROR)
				{
					FmodErrorOutput("master channel group set volume failed! ", IMiniLog::eWarning);
					return false;
				}

				break;
			}
		case adpMASTER_PAUSE: // value true/false
			{
				if (!(pParam->GetValue(bTemp))) 
					return (false);

				CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
				FMOD::ChannelGroup *MasterChannelGroup = 0;
				m_ExResult = m_pAPISystem->getMasterChannelGroup(&MasterChannelGroup);
				
				if (IS_FMODERROR)
				{
					FmodErrorOutput("system get master channel group failed! ", IMiniLog::eWarning);
					return false;
				}

				m_ExResult = MasterChannelGroup->setPaused(bTemp);
				if (IS_FMODERROR)
				{
					FmodErrorOutput("master channel group set pause failed! ", IMiniLog::eWarning);
					return false;
				}

				if (m_pEventSystem)
				{
					FMOD::EventCategory *pMasterCategory = NULL;
					m_ExResult = m_pEventSystem->getCategory("master", &pMasterCategory);

					if (IS_FMODERROR)
					{
						FmodErrorOutput("get master category failed! ", IMiniLog::eWarning);
						return false;
					}

					m_ExResult = pMasterCategory->setPaused(bTemp);
					if (IS_FMODERROR)
					{
						FmodErrorOutput("master category set pause failed! ", IMiniLog::eWarning);
						return false;
					}
				}

				break;
			}
		case adpPITCH:
			{
				if (!(pParam->GetValue(fTemp)) || !m_pEventSystem) 
					return (false);

				if (fTemp < -4.0f)
					fTemp = -4.0f;
				else if (fTemp > 4.0f)
					fTemp = 4.0f;

				FMOD::EventCategory *pPlatformCategory = NULL;
				m_ExResult = m_pEventSystem->getCategory("master", &pPlatformCategory);

				if (IS_FMODERROR)
				{
					FmodErrorOutput("system get master category failed! ", IMiniLog::eWarning);
					return false;
				}

				m_ExResult = pPlatformCategory->setPitch(fTemp);

				if (IS_FMODERROR)
				{
					FmodErrorOutput("master category set pitch failed! ", IMiniLog::eWarning);
					return false;
				}

				break;
			}

		case adpSPEAKER_MODE:// sets fmod speaker enum
			
		//	typedef enum
		//	{
		//	FMOD_SPEAKERMODE_RAW,              /* There is no specific speakermode.  Sound channels are mapped in order of input to output.  See remarks for more information. */
		//	FMOD_SPEAKERMODE_MONO,             /* The speakers are monaural. */
		//	FMOD_SPEAKERMODE_STEREO,           /* The speakers are stereo (default value). */
		//	FMOD_SPEAKERMODE_4POINT1,          /* 4.1 speaker setup.  This includes front, center, left, rear and a subwoofer. Also known as a "quad" speaker configuration. */
		//	FMOD_SPEAKERMODE_5POINT1,          /* 5.1 speaker setup.  This includes front, center, left, rear left, rear right and a subwoofer. */
		//	FMOD_SPEAKERMODE_7POINT1,          /* 7.1 speaker setup.  This includes front, center, left, rear left, rear right, side left, side right and a subwoofer. */
		//	FMOD_SPEAKERMODE_PROLOGIC,         /* Stereo output, but data is encoded in a way that is picked up by a Prologic/Prologic2 decoder and split into a 5.1 speaker setup. */

		//	FMOD_SPEAKERMODE_MAX,              /* Maximum number of speaker modes supported. */
		//	FMOD_SPEAKERMODE_FORCEINT = 65536  /* Makes sure this enum is signed 32bit. */
		//} FMOD_SPEAKERMODE;
	
				//" 0: Control Panel Settings\n"
				//"	1: Mono\n"
				//"	2: Stereo\n"
				//"	3: Headphone\n"
				//"	4: 4Point1\n"
				//"	5: 5Point1\n"
				////"	6: Surround\n"
				//"	7: 7Point1\n"
				//"	9: Prologic\n"

      // ***** Setspeakermode also sets the PAN separation setting to -0- if set to mono and -1- if stereo
      if (!(pParam->GetValue(nTemp))) 
				return (false);
			switch (nTemp)
			{
			case 0 :
				IsEax(); // polls the state of the control panel;
			 break;
			case 1 :
				m_nSpeakerMode = FMOD_SPEAKERMODE_MONO;
				break;
			case 2 :
				m_nSpeakerMode = FMOD_SPEAKERMODE_STEREO;
				break;
			case 3 :
				m_nSpeakerMode = FMOD_SPEAKERMODE_STEREO;
				break;
			case 4 :
				m_nSpeakerMode = FMOD_SPEAKERMODE_QUAD;
				break;
			case 5 :
				m_nSpeakerMode = FMOD_SPEAKERMODE_5POINT1;
				break;
			case 6 :
				m_nSpeakerMode = FMOD_SPEAKERMODE_PROLOGIC;
				break;
			case 7 :
				m_nSpeakerMode = FMOD_SPEAKERMODE_7POINT1;
				break;
			}
			//ResetAudio();
			//m_ExResult = m_pFMODEX->setSpeakerMode(m_nSpeakerMode);
      break;
    case adpPAUSED_STATUS:// pauses or un pauses system
      if (!(pParam->GetValue(bTemp))) 
				return (false);
      m_bSystemPaused = bTemp;
      //result = (CS_SetPaused(CS_ALL, bTemp) ? true : false);
      break;
/*
      CS_OUTPUT_NOSOUND,     NoSound driver, all calls to this succeed but do nothing.
        CS_OUTPUT_WINMM,     Windows Multimedia driver. 
        CS_OUTPUT_DSOUND,    DirectSound driver.  You need this to get EAX2 or EAX3 support, or FX api support. 
        CS_OUTPUT_A3D,       A3D driver. 

        CS_OUTPUT_OSS,        Linux/Unix OSS (Open Sound System) driver, i.e. the kernel sound drivers. 
        CS_OUTPUT_ESD,        Linux/Unix ESD (Enlightment Sound Daemon) driver. 
        CS_OUTPUT_ALSA,       Linux Alsa driver. 

        CS_OUTPUT_ASIO,       Low latency ASIO driver 
        CS_OUTPUT_XBOX,       Xbox driver 
        CS_OUTPUT_PS2,        PlayStation 2 driver 
        CS_OUTPUT_MAC,        Mac SoundManager driver 
        CS_OUTPUT_GC,         Gamecube driver 
        CS_OUTPUT_NOSOUND_NONREALTIME   This is the same as nosound, but the sound generation is driven by CS_Update 
        */
    case adpSNDSYSTEM_OUTPUTTYPE:// **** OPTIONAL ***** (-1 lets fmod pick best one) sets the fmod mixer driver type 
    case adpSNDSYSTEM_DRIVER: 
      if (!(pParam->GetValue(nTemp))) 
				return (false);
      
      // -1 tells the fmod system to pick the best one -- these are only valid choices
      if ((nTemp == -1) || (nTemp == FMOD_OUTPUTTYPE_NOSOUND) || (nTemp == FMOD_OUTPUTTYPE_WINMM) || (nTemp == FMOD_OUTPUTTYPE_DSOUND))
        //result = (CS_SetOutput(nTemp) ? true : false);
      break;

      case adpSTOPALL_CHANNELS:// stops all channels
				//m_pFMODEX->
        //result = (CS_StopSound(CS_ALL) ? true : false);  
        break;

      case adpDOPPLER: // turn doppler on or off with a bool
        if (!(pParam->GetValue(bTemp))) 
					return (false);
        m_bDopplerStatus = bTemp;
        break;

    default:
      break;
    }
  return result;
  }

	// return handle if Project is available 
	FMOD::EventProject* CAudioDeviceFmodEx400::LoadProjectFile(const string &sProjectPath, const string &sProjectName)
	{
		int nNum = m_VecLoadedProjectFiles.size();

		if (m_pEventSystem)
		{
			string sFull = sProjectPath + "/" + sProjectName;
			
			VecProjectFilesIter IterEnd = m_VecLoadedProjectFiles.end();
			for (VecProjectFilesIter Iter = m_VecLoadedProjectFiles.begin(); Iter!=IterEnd; ++Iter)
			{
				if (sFull == (*Iter).sProjectName)
					return (*Iter).ProjectHandle;
			}
			
			if (true)
			{
				SProjectFile NewProject;
				NewProject.sProjectName = sFull;
				NewProject.ProjectHandle = NULL;

				{
					if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
						m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::EVENTSYSTEM_SETMEDIAPATH, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), (void*) sProjectPath.c_str());

					CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
					m_ExResult = m_pEventSystem->setMediaPath((char*)sProjectPath.c_str());
				}
				if (IS_FMODERROR)
				{
					FmodErrorOutput("set event media path failed! ", IMiniLog::eWarning);
					return NULL;
				}

				FMOD_EVENT_LOADINFO Info;
				Info.size = sizeof(Info);
				Info.encryptionkey = 0;
				Info.loadfrommemory_length = 0;
				Info.sounddefentrylimit = m_pSoundSystem->g_fVariationLimiter;
				
				{
					CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
 					m_ExResult = m_pEventSystem->load((char*)sProjectName.c_str(), &Info, &NewProject.ProjectHandle);
				}
				
				if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_RECORD_COMMANDS)
					m_CommandPlayer.LogCommand(CCommandPlayerFmodEx400::EVENTSYSTEM_LOAD, (int)gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), (void*) sProjectName.c_str(), NewProject.ProjectHandle);

				if (IS_FMODERROR)
				{
					FmodErrorOutput("load event project failed! " + sProjectName, IMiniLog::eError);
					return NULL;
				}
        
				m_VecLoadedProjectFiles.push_back(NewProject);

				// refresh categories after loading a new project
				if (m_pSoundSystem->GetIMoodManager())
					m_pSoundSystem->GetIMoodManager()->RefreshCategories();

				// update group and event counting
				UpdateGroupEventCount();
				return NewProject.ProjectHandle;
			}
		}

		return NULL;
	}

	void CAudioDeviceFmodEx400::UpdateGroupEventCount()
	{
		m_nCountProject = 0;
		m_nCountGroup = 0;
		m_nCountEvent = 0;
		int nProjectIndex = 0;
		int nGroupIndex = 0;
		m_ExResult = FMOD_OK;
		FMOD::EventProject* pProject = NULL;
		FMOD::EventGroup*		pGroup = NULL;

		if (m_pSoundSystem->g_nDumpEventStructure)
		{
			if (m_pEventDumpLogger)
				delete m_pEventDumpLogger;

			m_pEventDumpLogger = (CDebugLogger*) new CDebugLogger("EventStructure.txt");
		}

		while(m_ExResult == FMOD_OK)
		{
			{
				CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
				m_ExResult = m_pEventSystem->getProjectByIndex(nProjectIndex, &pProject);
			}
			if (IS_FMODERROR)
			{
				//FmodErrorOutput("event system get project by index failed! ", IMiniLog::eWarning);
			}

			if (pProject && (m_ExResult == FMOD_OK))
			{
				
				while(m_ExResult == FMOD_OK)
				{
					{
						CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
						m_ExResult = pProject->getGroupByIndex(nGroupIndex, false, &pGroup);
					}

					if (IS_FMODERROR)
					{
						//FmodErrorOutput("project get group by index failed! ", IMiniLog::eWarning);
					}

					if (pGroup && (m_ExResult == FMOD_OK))
					{
						++nGroupIndex;
						++m_nCountGroup;
						GroupEventCount(pGroup, pProject);
					}
				}
				nGroupIndex = 0;
				m_ExResult = FMOD_OK;
				++nProjectIndex;
				++m_nCountProject;
			}
		}
	}

	void CAudioDeviceFmodEx400::GroupEventCount(FMOD::EventGroup* pParentGroup, FMOD::EventProject* pProject)
	{
		int nGroupIndex = 0;
		int nEventIndex = 0;
		m_ExResult = FMOD_OK;
		FMOD::EventGroup* pGroup = NULL;
		FMOD::Event* pEvent = NULL;

		char *sGroupName = 0; 
		char *sProjectName = 0; 
		char *sEventtName = 0;

		if (m_pSoundSystem->g_nDumpEventStructure)
		{
			pProject->getInfo(0, &sProjectName);
		}

		// counting Groups
		while(m_ExResult == FMOD_OK)
		{
			{
				CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
				m_ExResult = pParentGroup->getGroupByIndex(nGroupIndex, false, &pGroup);
			}
			if (IS_FMODERROR)
			{
				//FmodErrorOutput("group get group by index failed! ", IMiniLog::eWarning);
			}

			if (pGroup && (m_ExResult == FMOD_OK))
			{
				++nGroupIndex;
				++m_nCountGroup;
				GroupEventCount(pGroup, pProject);
			}
		}

		// counting Events
		int nNumEvents = 0;
		{
			CFrameProfilerSection frameProfilerSection( &CAudioDeviceFmodEx400::GetFMODFrameProfiler() );
			m_ExResult = pParentGroup->getNumEvents( &nNumEvents);
		}

		if (IS_FMODERROR)
		{
			//FmodErrorOutput("group get num. events failed! ", IMiniLog::eWarning);
		}

		if (m_pSoundSystem->g_nDumpEventStructure)
		{
			pParentGroup->getInfo(0, &sGroupName);
			for (int i=0; i<nNumEvents; ++i)
			{
				m_ExResult = pParentGroup->getEventByIndex(i, FMOD_EVENT_INFOONLY, &pEvent);
				
				if (IS_FMODERROR)
				{
					FmodErrorOutput("group get event by index failed! ", IMiniLog::eWarning);
				}

				if (pEvent)
				{
					m_ExResult = pEvent->getInfo(0, &sEventtName, 0);
					if (m_ExResult == FMOD_OK && m_pEventDumpLogger)
					{
						TFixedResourceName sTemp;
						sTemp = "Sounds/";
						sTemp += sProjectName;
						sTemp += ":";
						sTemp += sGroupName;
						sTemp += ":";
						sTemp += sEventtName;
						m_pEventDumpLogger->LogString(sTemp.c_str());
					}


				}

			}
			
		}

		if (!IS_FMODERROR)
			m_nCountEvent += nNumEvents;

	}

	// writes output to screen in debug
	void CAudioDeviceFmodEx400::DrawInformation(IRenderer* pRenderer, float xpos, float ypos, int nSoundInfo)
	{
		float fColor[4] = {1.0f, 1.0f, 1.0f, 0.7f};
		float fWhite[4] = {1.0f, 1.0f, 1.0f, 0.7f};
		float fBlue[4] = {0.0f, 0.0f, 1.0f, 0.7f};
		float fCyan[4] = {0.0f, 1.0f, 1.0f, 0.7f};

		if (nSoundInfo == 6)
		{
			uint32 nUsedMem = 0;

			IWavebank *pWavebank = NULL;

			ypos += 10;

			tmapWavebanks::iterator ItEnd = m_Wavebanks.end();
			for (tmapWavebanks::const_iterator It = m_Wavebanks.begin(); It!=ItEnd; ++It)
			{
				pWavebank = (*It).second;
				nUsedMem += pWavebank->GetInfo()->nMemCurrentlyInByte;
				if (pWavebank->GetInfo()->nMemCurrentlyInByte > 0)
				{
					pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColor, false, "%s Mem: %dKB File: %dKB", pWavebank->GetName(), pWavebank->GetInfo()->nMemCurrentlyInByte/1024, pWavebank->GetInfo()->nFileSize/1024);
					ypos += 10;
				}
			}

			ypos += 10;
			pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColor, false, "Total Wavebank Memory: %.1f MB", (nUsedMem/1024)/1024.0f);
			ypos += 10;
		}

		if (nSoundInfo == 1)
		{
			pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColor, false, "Channel sounds: %d", m_UnusedPlatformSounds.size() );
			ypos += 10;
			pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColor, false, "Event sounds: %d", m_UnusedPlatformSoundEvents.size() );
			ypos += 10;

			if (m_pEventSystem)
			{
				int nNum = 64;
				FMOD_EVENT_SYSTEMINFO SystemInfo;
				memset(&SystemInfo, 0, sizeof(FMOD_EVENT_SYSTEMINFO));

				SystemInfo.numplayingevents = nNum;
				FMOD_EVENT* PlayingEvents[64];
				SystemInfo.playingevents = PlayingEvents;
				
				m_ExResult = m_pEventSystem->getInfo( &SystemInfo );
				if (IS_FMODERROR)
				{
					FmodErrorOutput("event system get system info failed! ", IMiniLog::eWarning);
					return;
				}

				pRenderer->Draw2dLabel(xpos, ypos, 1, fColor, false, "----- Event Names %d -----", SystemInfo.numplayingevents );
				//pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColor, false, "Playing Events: %d", );
				ypos += 10;

				for (int i=0; i<SystemInfo.numplayingevents; ++i)
				{
					char *sName; 
					FMOD::Event* pEvent = (FMOD::Event*)SystemInfo.playingevents[i];
					FMOD_EVENT_INFO EventInfo;
					memset(&EventInfo, 0, sizeof(FMOD_EVENT_INFO));

					m_ExResult = pEvent->getInfo(0, &sName, &EventInfo);

					if (IS_FMODERROR)
					{
						FmodErrorOutput("event get info failed! ", IMiniLog::eWarning);
					}
					
					pRenderer->Draw2dLabel(xpos, ypos, 1.2f, fColor, false, "%s in %s", sName, EventInfo.wavebanknames[0]);
					ypos += 10;
				}
			}
		}




	}

	CFrameProfiler& CAudioDeviceFmodEx400::GetFMODFrameProfiler()
	{
		static CFrameProfiler g_FMODFrameProfiler( gEnv->pSystem,"FMOD-Other",PROFILE_SOUND );

		return g_FMODFrameProfiler;
	}


	// hack for looping weapon sound MP bug
	void CAudioDeviceFmodEx400::FindLostEvent()
	{
		// Go through all FMOD events and check if their handle is known to the soundsystem
		// in all active looping sounds. Oneshots will end eventually anyways.

		if (m_pEventSystem)
		{
			int nNum = 64;
			FMOD_EVENT_SYSTEMINFO SystemInfo = {0};

			SystemInfo.numplayingevents = nNum;
			FMOD_EVENT* PlayingEvents[64];
			SystemInfo.playingevents = PlayingEvents;

			m_ExResult = m_pEventSystem->getInfo( &SystemInfo );
			if (IS_FMODERROR)
			{
				FmodErrorOutput("event system get system info failed! ", IMiniLog::eWarning);
				return;
			}

			for (int i=0; i<SystemInfo.numplayingevents; ++i)
			{
				FMOD::Event* pEvent = (FMOD::Event*)SystemInfo.playingevents[i];
				FMOD_EVENT_INFO EventInfo = {0};

				m_ExResult = pEvent->getInfo(0, 0, &EventInfo);

				if (IS_FMODERROR)
				{
					FmodErrorOutput("event get info failed! ", IMiniLog::eWarning);
				}
				else
				{
					// oneshot sounds will end eventually, 
					//so only check for looping sounds which are not controlled by the sound system
					bool bLost = (EventInfo.lengthms == -1) && !(m_pSoundSystem->IsEventUsedInPlatformSound((tSoundHandle)pEvent));

					if (bLost)
					{
						//gEnv->pLog->Log("[Warning] <Sound> Lost event %s found and stopped. \n", sName );
						m_ExResult = pEvent->stop(false);
					}
				}
			}
		}
	}



#endif
