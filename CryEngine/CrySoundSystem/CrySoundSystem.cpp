// CrySoundSystem.cpp : Defines the entry point for the DLL application.
//

#include "StdAfx.h"
#include "platform_impl.h"

#include "Sound.h"
#include "DummySound.h"	
#include "SoundSystem.h"
#include "IAudioDevice.h"

#ifdef SOUNDSYSTEM_USE_FMODEX400
	#include "AudioDeviceFmodEx400.h"
#endif

#ifdef SOUNDSYSTEM_USE_XENON_XAUDIO
#include "AudioDeviceXenon.h"
#endif


//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_Sound : public ISystemEventListener
{
public:
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
	{
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_RELOAD:
			{
				STLALLOCATOR_CLEANUP;
				break;
			}
		}
	}
};

static CSystemEventListner_Sound g_system_event_listener_sound;

//////////////////////////////////////////////////////////////////////////////////////////////
// dll interface	


///////////////////////////////////////////////
extern "C" ISoundSystem* CreateSoundSystem(struct ISystem* pISystem, void* pInitData)
{
	ModuleInitISystem(pISystem);

	CSoundSystem* pSoundSystem = NULL;
  IAudioDevice *pAudioDevice = NULL;
  bool bInitDeviceResult = false;
	bool bSoundSystemResult = false;


#ifdef SOUNDSYSTEM_USE_FMODEX400
	// creating a FMOD AudioDevice for a Win32/64 platform and hook it into the SoundSystem
	pAudioDevice	= new CAudioDeviceFmodEx400(pInitData);
#endif

#ifdef SOUNDSYSTEM_USE_XENON_XAUDIO
	// creating a XENON AudioDevice and hook it into the SoundSystem
	pAudioDevice		= new CAudioDeviceXenon();
#endif

	if (pAudioDevice)
	{
		// We have created the audio device now lets create sound system
		pSoundSystem	= new CSoundSystem(pInitData, pAudioDevice);

		if (pSoundSystem)
		{
			// time to init device if everything ok with soundsystem
			if (pSoundSystem->IsOK())
			{
				CryLogAlways("Sound - initializing AudioDevice now!\n");
				bInitDeviceResult = pAudioDevice->InitDevice(pSoundSystem);
			}

			if (bInitDeviceResult)
			{
				CryLogAlways("Sound - initializing SoundSystem now!\n");
				bSoundSystemResult = pSoundSystem->Init();
			}
			else
				CryLogAlways("Error: Sound - Cannot initialize SoundSystem!\n");
		}
		else
			CryLogAlways("Error: Sound - Cannot create SoundSystem!\n");
	}

	if (!pSoundSystem || !bSoundSystemResult || !pAudioDevice || !bInitDeviceResult)
	{
		CryLogAlways("Error: Sound - Something went wrong. Creating DummySoundSystem now! \n");
		//if the sound system cannot be created or initialized,
		// or the console variable s_DummySound = 1 then
		//create the dummy sound system (NULL sound system, same as for
		//dedicated server)

		// Cleanup what was created
		if (pSoundSystem)
		{
			pSoundSystem->Release();
			pSoundSystem = NULL;
			pAudioDevice = NULL;
		}

		if (pAudioDevice)
		{
			pAudioDevice->ShutDownDevice();
			delete pAudioDevice;
			pAudioDevice = NULL;
		}

    // Use dummy sound system if wanted or something went wrong
    pSoundSystem = (CSoundSystem*) new CDummySoundSystem(pInitData);
	}

	pISystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_sound);
	return pSoundSystem;
}

#include <CrtDebugStats.h>
