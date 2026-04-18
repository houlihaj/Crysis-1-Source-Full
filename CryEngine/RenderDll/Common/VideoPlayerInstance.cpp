#include "StdAfx.h"

#include "VideoPlayerInstance.h"


#ifndef EXCLUDE_CRI_SDK


#include <ISound.h>
#include <StringUtils.h>


#if defined(WIN32)
#	if defined(WIN64)
#		pragma message (">>> include lib: cri_adxpcx64.lib")
#		pragma comment ( lib, "cri_adxpcx64.lib" )
#		pragma message (">>> include lib: cri_mwsfdpcx64.lib")
#		pragma comment ( lib, "cri_mwsfdpcx64.lib" )
#	else
#		pragma message (">>> include lib: cri_adxpcx86.lib")
#		pragma comment ( lib, "cri_adxpcx86.lib" )
#		pragma message (">>> include lib: cri_mwsfdpcx86.lib")
#		pragma comment ( lib, "cri_mwsfdpcx86.lib" )
#	endif // !defined(WIN64)
# pragma message (">>> include lib: ksuser.lib")
#	pragma comment ( lib, "ksuser.lib" )
#endif // defined(WIN32)


static void SfdLogError(void* /*pObj*/, char* pErrMsg)
{
	gEnv->pLog->LogError("[From CRI MW] %s", pErrMsg);
}


class CExtTimer
{
public:
	static CExtTimer& Access()
	{
		static CExtTimer s_timer;
		return s_timer;
	}

public:
	int GetTick()
	{
#if defined(WIN32) || defined(WIN64)
		LARGE_INTEGER tick;
		QueryPerformanceCounter(&tick);
		return (int) (tick.QuadPart >> m_shiftBits);
#else
		assert(0);
		return 0;
#endif
	}

	int GetFreq() const
	{
		return m_freq;
	}

private:
	static const int c_minFreq = 48 * 1000; // maximum audio freq

private:
	int m_freq;
	int m_shiftBits;

private:
	CExtTimer()
	: m_freq(c_minFreq)
	, m_shiftBits(0)
	{
#if defined(WIN32) || defined(WIN64)
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);

		uint64 f(freq.QuadPart);
		while (f >= 2 * c_minFreq)
		{
			f >>= 1;
			++m_shiftBits;
		}
		m_freq = (int)f;
#else
		assert(0);
#endif
	}
};


static void SfdExternalClockFunction(void* /*pUsrObj*/, Sint32* pTime, Sint32* pTimeUnit)
{
	assert(pTime && pTimeUnit);	
	CExtTimer& timer(CExtTimer::Access());
	*pTime = timer.GetTick();
	*pTimeUnit = timer.GetFreq();
}


static IDirectSound8* GetSoundOutputDevice()
{
	if (gEnv->pSoundSystem) 
	{
		void* outputHandle(0); EOutputHandle outputType(eOUTPUT_NOSOUND);
		gEnv->pSoundSystem->GetOutputHandle(&outputHandle, &outputType);
		if (outputHandle && outputType == eOUTPUT_DSOUND)
			return (IDirectSound8*) outputHandle;
	}

	return 0;
}


static inline unsigned int CalcAvgBitRate(size_t movieFileSize, unsigned int fps, unsigned int numFrames)
{
	uint64 t(movieFileSize);
	t *= 8 * fps;
	t /= numFrames;
	return t;
}


//////////////////////////////////////////////////////////////////////////
// CRI MW framework initialization helper

class CCriMwLibInit
{
public:
	// lifetime management
	static CCriMwLibInit* Create();
	int AddRef();
	int Release();

	// status
	bool IsInitialized() const;
	CShader* GetShader() const;
	bool IsDummyAudio() const;

private:
	CCriMwLibInit();
	~CCriMwLibInit();

private:
	int m_refCount;
	bool m_isInitialized;
	bool m_isDummyAudio;
	MwsfdInitPrm m_sfdInitParams;
};


CCriMwLibInit::CCriMwLibInit()
: m_refCount(1)
, m_isInitialized(false)
, m_isDummyAudio(false)
, m_sfdInitParams()
{
#if 0
	AdxmThreadSprm adxmThreadPrams;
	adxmThreadPrams.priority_vsync = ADXM_PRIO_VSYNC; // ADXM_PRIO_VSYNC = THREAD_PRIORITY_HIGHEST
	adxmThreadPrams.priority_fs = ADXM_PRIO_FS; // ADXM_PRIO_FS = THREAD_PRIORITY_ABOVE_NORMAL
#	if defined(WIN32) || defined(WIN64)
	adxmThreadPrams.priority_mwidle = THREAD_PRIORITY_BELOW_NORMAL;
#	else
	adxmThreadPrams.priority_mwidle = ADXM_PRIO_MWIDLE; // ADXM_PRIO_MWIDLE = THREAD_PRIORITY_IDLE
#	endif
	AdxmThreadSprm* pAdxmThreadPrams(&adxmThreadPrams);
	MwsfdDecSvr decSrvTyp(MWSFD_DEC_SVR_IDLE);
#else
	AdxmThreadSprm* pAdxmThreadPrams(0);
	MwsfdDecSvr decSrvTyp(MWSFD_DEC_SVR_MAIN);
#endif
	static bool s_firstInit(true);
	bool logInit(s_firstInit || gEnv->pSystem->IsDevMode());

	if (logInit)
		gEnv->pLog->Log("Initializing CRI-MW/Sofdec ...");

	ADXPC_SetupFileSystem(0);
	IDirectSound8* pDS(GetSoundOutputDevice());
	m_isDummyAudio = pDS == 0;
	ADXPC_SetupSound(pDS);
	ADXM_SetupFramework(ADXM_FRAMEWORK_DEFAULT, pAdxmThreadPrams); //ADXM_FRAMEWORK_PC_SINGLE_THREAD / ADXM_FRAMEWORK_PC_MULTI_THREAD
	//ADXT_Init();
	ADXM_SetCbErr(SfdLogError, 0);

	memset(&m_sfdInitParams, 0, sizeof(m_sfdInitParams));
	m_sfdInitParams.vhz = MWSFD_VHZ_60_00;
	m_sfdInitParams.dec_svr = decSrvTyp;
	mwPlyInitSfdFx(&m_sfdInitParams);

	m_isInitialized = true;

	if (logInit)
	{
		gEnv->pLog->Log(mwPlyGetVersionStr());
		if (m_isDummyAudio)
			CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "Videos will be played without sound as audio hw acceleration is disabled or not available!");
	}
	s_firstInit = false;
}


CCriMwLibInit::~CCriMwLibInit()
{
	mwPlyFinishSfdFx();
	//ADXT_Finish();
	ADXM_ShutdownFramework();
	ADXPC_ShutdownSound();
	ADXPC_ShutdownFileSystem();
}


CCriMwLibInit* CCriMwLibInit::Create()
{
	return new CCriMwLibInit;
}


int CCriMwLibInit::AddRef()
{
	++m_refCount;
	return m_refCount;
}


int CCriMwLibInit::Release()
{
	--m_refCount;
	int ref(m_refCount);
	if (m_refCount <= 0)
		delete this;
	return ref;
}


bool CCriMwLibInit::IsInitialized() const
{
	return m_isInitialized;
}


bool CCriMwLibInit::IsDummyAudio() const
{
	return m_isDummyAudio;
}

//////////////////////////////////////////////////////////////////////////
// CVideoPlayer implementation

CCriMwLibInit* CVideoPlayer::ms_pCriMwInit(0);
SLinkNode<CVideoPlayer> CVideoPlayer::ms_rootNode;
bool CVideoPlayer::ms_appHasFocus(true);


CVideoPlayer::CVideoPlayer()
: m_playbackStarted(false)
, m_loopPlayback(false)
, m_viewportX0(0)
, m_viewportY0(0)
, m_viewportWidth(0)
, m_viewportHeight(0)
, m_sfdCreationParams()
, m_sfdHandle(0)
, m_pCurFrameY(0)
, m_pCurFrameCb(0)
, m_pCurFrameCr(0)
, m_pCurFrameA(0)
, m_pCriMwShader(0)
, m_pFilePath(0)
, m_audioCodec(MWSFD_AUDIO_CODEC_NONE)
, m_pAudioWB(0)
, m_pVoiceWB(0)
, m_node()
, m_lastDecSvrUpdate(0)
, m_frameReset(0)
, m_perFrameUpdate(false)
, m_pauseRequestLostFocus(false)
, m_curVolume(1.0f)
{
	if (!ms_pCriMwInit)
	{
		ms_pCriMwInit = CCriMwLibInit::Create();
		if (!ms_pCriMwInit || !ms_pCriMwInit->IsInitialized())
			SAFE_RELEASE(ms_pCriMwInit);
#if defined(WIN32) || defined(WIN64)		
		if (!gRenDev->m_bEditor)
		{
			// Need to explicitly check for app focus when running in game mode as it
			// is possible to loose focus without being notified when the app starts up!
			static bool s_appFocusInitialized(false);
			if (!s_appFocusInitialized)
				ms_appHasFocus = gRenDev->GetHWND() == GetForegroundWindow();
		}
#endif
	}
	else
		ms_pCriMwInit->AddRef();

	m_pCriMwShader = gRenDev->m_cEF.mfForName("CriMw", EF_SYSTEM);

	m_frameReset = gRenDev->m_nFrameReset;

	m_fullFilePath[0] = '\0';

	Link();
}


CVideoPlayer::~CVideoPlayer()
{
	Destruct(true);
	
	Unlink();
}


void CVideoPlayer::Destruct(bool callerIsDtor)
{
	if (m_sfdHandle)
	{
		mwPlyStop(m_sfdHandle);

		switch (m_audioCodec)
		{
		case MWSFD_AUDIO_CODEC_AIX:
			{
				mwPlyDetachMultiChannel(m_sfdHandle);
				break;
			}
		case MWSFD_AUDIO_CODEC_AHX:
			{
				mwPlyDetachAhx(m_sfdHandle);
				break;
			}
		default:
			break;
		}

		if (m_pVoiceWB)
			mwPlyDetachVoice(m_sfdHandle);

		mwPlyDestroy(m_sfdHandle);
		m_sfdHandle = 0;
	}

	SAFE_DELETE_ARRAY(m_sfdCreationParams.work);
	
	m_audioCodec = MWSFD_AUDIO_CODEC_NONE;
	SAFE_DELETE_ARRAY(m_pAudioWB);

	SAFE_DELETE_ARRAY(m_pVoiceWB);

	DestroyTextures();

	m_fullFilePath[0] = '\0';
	m_pFilePath = 0;

	EnablePerFrameUpdate(false);
	m_pauseRequestLostFocus = false;

	if (callerIsDtor)
	{
		SAFE_RELEASE(m_pCriMwShader);
		if (ms_pCriMwInit->Release() <= 0)
			ms_pCriMwInit = 0;
	}
}


void CVideoPlayer::Release()
{
	delete this;
}


static inline bool IsAppropriateAudioCh(const MwsfdAudioInfo& ai)
{
	return ai.exist_flag && ((ai.audio_codec == MWSFD_AUDIO_CODEC_AIX && ai.num_channel > 1) || (ai.audio_codec == MWSFD_AUDIO_CODEC_SFA && ai.num_channel > 1));
}


static inline bool IsAppropriateVoiceCh(const MwsfdAudioInfo& ai)
{
	return ai.exist_flag && (ai.audio_codec == MWSFD_AUDIO_CODEC_SFA && ai.num_channel == 1);
}


bool CVideoPlayer::Load(const char* pFilePath, unsigned int options, int audioCh, int voiceCh, bool useSubtitles)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);

	// cleanup previously allocated resources
	Destruct();

	// check if lib is initialized
	assert(ms_pCriMwInit);
	if (!ms_pCriMwInit)
		return false;

	// open file to analyze header
	ICryPak* pPak(gEnv->pCryPak);
	FILE* f(pPak->FOpen(pFilePath, "rb"));
	if (!f)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "Failed to open video file \"%s\"!", pFilePath);
		return false;
	}

	// videos in pak files are not supported as file IO cannot be overwritten
	if (pPak->IsInPak(f))
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "!Playing videos stored in pak files is not supported! [%s]", pFilePath);
		pPak->FClose(f);
		return false;
	}

	// get movie file size
	size_t movieFileSize(pPak->FGetSize(f));

	// read in first 20k of data
	Sint8 fileHeader[20 * 1024];
	if (sizeof(fileHeader) != pPak->FReadRaw(&fileHeader[0], 1, sizeof(fileHeader), f))
	{
		// file not long enough (corrupt?)
		pPak->FClose(f);
		return false;
	}

	// close file
	pPak->FClose(f);

	// get file header information
	MwsfdHdrInf sfdFileHeaderInfo;
	memset(&sfdFileHeaderInfo, 0, sizeof(sfdFileHeaderInfo));
	mwPlyGetHdrInf(&fileHeader[0], sizeof(fileHeader), &sfdFileHeaderInfo);

	if (useSubtitles && sfdFileHeaderInfo.fxtype != MWSFD_COMPO_OPAQ)
		gEnv->pLog->LogWarning("Using subtitles on non-opaque movies is not supported (\"%s\")!", pFilePath);

	// fill Sofdec creation structure
	memset(&m_sfdCreationParams, 0, sizeof(m_sfdCreationParams));
	m_sfdCreationParams.compo_mode = useSubtitles ? MWSFD_COMPO_AUTO : sfdFileHeaderInfo.fxtype;
	m_sfdCreationParams.ftype = sfdFileHeaderInfo.ftype;
	m_sfdCreationParams.max_bps = CalcAvgBitRate(movieFileSize, sfdFileHeaderInfo.fps / 1000, sfdFileHeaderInfo.frmnum);
	m_sfdCreationParams.max_width = sfdFileHeaderInfo.width;
	m_sfdCreationParams.max_height = sfdFileHeaderInfo.height;
	m_sfdCreationParams.nfrm_pool_wk = 2;
	m_sfdCreationParams.max_stm = 1;
	m_sfdCreationParams.buffmt = MWSFD_BUFFMT_DEFAULT;
	m_sfdCreationParams.wksize = mwPlyCalcWorkCprmSfd(&m_sfdCreationParams);
	m_sfdCreationParams.work = new Sint8[m_sfdCreationParams.wksize];

	// create playback handle
	m_sfdHandle = mwPlyCreateSofdec(&m_sfdCreationParams);
	if (!m_sfdHandle)
		return false;

	// audio setup and voice
	if (sfdFileHeaderInfo.num_audio_ch > 0)
	{
		// get audio information from file header
		MwsfdHdrAudioDetail sfdFileHeaderAudioInfo;
		mwPlyGetHdrAudioDetail(&fileHeader[0], sizeof(fileHeader), &sfdFileHeaderAudioInfo);

		// audio requested?
		if (audioCh != -1)
		{
			// make sure requested audio channel is appropriate, otherwise find first available
			if (audioCh < 0 || audioCh > MWSFD_MAX_AUDIO_CH - 1 || !IsAppropriateAudioCh(sfdFileHeaderAudioInfo.audio_info[audioCh]))
			{
				for (audioCh = 0; audioCh < MWSFD_MAX_AUDIO_CH; ++audioCh)
				{
					if (IsAppropriateAudioCh(sfdFileHeaderAudioInfo.audio_info[audioCh]))
						break;
				}			
				if (audioCh == MWSFD_MAX_AUDIO_CH)
					audioCh = -1;
			}

			// setup audio playback and choose channel
			if (audioCh != -1)
			{			
				switch (sfdFileHeaderAudioInfo.audio_info[audioCh].audio_codec)
				{
				case MWSFD_AUDIO_CODEC_AIX:
					{
						size_t workSize(mwPlyGetMultiChannelWorkSize());
						m_pAudioWB = new char[workSize];
						
						m_audioCodec = MWSFD_AUDIO_CODEC_AIX;
						mwPlyAttachMultiChannel(m_sfdHandle, m_pAudioWB, workSize);
						mwPlySetMultiChannelCh(m_sfdHandle, audioCh);
						break;
					}
				case MWSFD_AUDIO_CODEC_SFA:
					{
						m_audioCodec = MWSFD_AUDIO_CODEC_SFA;
						mwPlySetAudioCh(m_sfdHandle, audioCh);
						break;
					}
				default:
					break;
				}
			}
		}

		// voice over requested?
		if (voiceCh != -1)
		{
			// make sure requested voice channel is appropriate, otherwise find first available
			if (audioCh < 0 || audioCh > MWSFD_MAX_AUDIO_CH - 1 || !IsAppropriateVoiceCh(sfdFileHeaderAudioInfo.audio_info[voiceCh]))
			{
				for (voiceCh = 0; voiceCh < MWSFD_MAX_AUDIO_CH; ++voiceCh)
				{
					if (IsAppropriateVoiceCh(sfdFileHeaderAudioInfo.audio_info[voiceCh]))
						break;
				}			
				if (voiceCh == MWSFD_MAX_AUDIO_CH)
					voiceCh = -1;
			}

			// setup voice over and choose channel
			if (voiceCh != -1)
			{
				size_t workSize(mwPlyGetVoiceWorkSize());
				m_pVoiceWB = new char[workSize];
				
				mwPlyAttachVoice(m_sfdHandle, m_pVoiceWB, workSize);
				mwPlySetVoiceCh(m_sfdHandle, voiceCh);
			}
		}
	}

	// set synchronization mode
	mwPlySetFrmSync(m_sfdHandle, MWSFD_FRMSYNC_TIME);

	// save full file path for playback
	m_fullFilePath[0] = '\0';
	m_pFilePath = 0;

	const char* pGameFolder = PathUtil::GetGameFolder();
	if (pGameFolder && *pGameFolder)
	{
		strncat(m_fullFilePath, pGameFolder, sizeof(m_fullFilePath));
		strncat(m_fullFilePath, "/", sizeof(m_fullFilePath) );
	}

	m_pFilePath = &m_fullFilePath[0] + strlen(m_fullFilePath);
	strncat(m_fullFilePath, pFilePath, sizeof(m_fullFilePath));

	// set viewport
	SetViewport(0, 0, sfdFileHeaderInfo.width, sfdFileHeaderInfo.height);

	// set loop flag 
	m_loopPlayback = (options & LOOP_PLAYBACK) != 0;

	// start playback if delay is not requested
	m_playbackStarted = false;
	if (!(options & DELAY_START))
		StartPlayback();

	return true;
}


void CVideoPlayer::StartPlayback()
{
	assert(m_sfdHandle);

	if (ms_pCriMwInit && ms_pCriMwInit->IsDummyAudio())
	{
		mwPlySetAudioSw(m_sfdHandle, OFF);
		mwPlySetCbExtClock(m_sfdHandle, &SfdExternalClockFunction, (Sint32) -1, 0);
		mwPlySetSyncMode(m_sfdHandle, MWSFD_SYNCMODE_EXTCLOCK);
	}

	if (!m_loopPlayback)
		mwPlyStartFname(m_sfdHandle, m_fullFilePath);
	else
		mwPlyStartFnameLp(m_sfdHandle, m_fullFilePath);

	if (!ms_appHasFocus)
	{
		m_pauseRequestLostFocus = mwPlyIsPause(m_sfdHandle) != 0;
		PauseInt(true);
	}

	m_playbackStarted = true;
}


IVideoPlayer::EPlaybackStatus CVideoPlayer::GetStatus() const
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	EPlaybackStatus transStatus(PBS_ERROR);
	if (m_sfdHandle)
	{
		MwsfdStat status(mwPlyGetStat(m_sfdHandle));
		switch(status)
		{
		case MWSFD_STAT_STOP:
			transStatus = PBS_STOPPED;
			break;
		case MWSFD_STAT_PREP:
			transStatus = PBS_PREPARING;
			break;
		case MWSFD_STAT_PLAYING:
			if (mwPlyIsPause(m_sfdHandle))
				transStatus = ms_appHasFocus ? PBS_PAUSED : PBS_PAUSED_NOFOCUS;
			else
				transStatus = PBS_PLAYING;
			break;
		case MWSFD_STAT_PLAYEND:
		case MWSFD_STAT_END:
			transStatus = PBS_FINISHED;
			break;
		case MWSFD_STAT_ERROR:
			transStatus = PBS_ERROR;
			break;
		}
	}

	return transStatus;
}


bool CVideoPlayer::Start()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	if (!m_sfdHandle)
		return false;

	//EPlaybackStatus status(GetStatus());
	//if (status != PBS_STOPPED)
	//	mwPlyStop(m_sfdHandle);

	StartPlayback();
	return true;
}


bool CVideoPlayer::Stop()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	if (!m_sfdHandle)
		return false;

	mwPlyStop(m_sfdHandle);
	return true;
}


bool CVideoPlayer::Pause(bool pause)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	if (ms_appHasFocus)
		return PauseInt(pause);

	//assert(GetStatus() == PBS_PAUSED_NOFOCUS);
	m_pauseRequestLostFocus = pause;
	return true;
}


bool CVideoPlayer::PauseInt(bool pause)
{
	if (!m_sfdHandle)
		return false;

	bool isPaused(mwPlyIsPause(m_sfdHandle) != 0);
	if (isPaused != pause)
		mwPlyPause(m_sfdHandle, pause ? 1 : 0);
	return true;
}


void CVideoPlayer::OnFocusChanged()
{
	if (!m_sfdHandle)
		return;

	if (ms_appHasFocus)
		PauseInt(m_pauseRequestLostFocus);
	else
	{
		m_pauseRequestLostFocus = mwPlyIsPause(m_sfdHandle) != 0;
		PauseInt(true);
	}
}


bool CVideoPlayer::SetViewport(int x0, int y0, int width, int height)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	if (!m_sfdHandle)
		return false;

	m_viewportX0 = x0;
	m_viewportY0 = y0;
	m_viewportWidth = width;
	m_viewportHeight = height;
	return true;
}


static inline int64 CryMwsfdTime2Msec(Sint32 timeValue, Sint32 timeUnit)
{
	return ((int64) 1000 * (int64) timeValue) / (int64) timeUnit;
}


bool CVideoPlayer::DiscardFrame(const MwsfdFrmObj& frm) const
{
	// CryMwsfdTime2Msec impl is based on MWSFD_TIME2MSEC macro; make sure it didn't change!
	assert(CryMwsfdTime2Msec(10000, 6000) == MWSFD_TIME2MSEC(10000, 6000));

	// get current playback time
	Sint32 timeVal(0), timeUnit(0);
	mwPlyGetTime(m_sfdHandle, &timeVal, &timeUnit);
	int64 playbackTime(CryMwsfdTime2Msec(timeVal, timeUnit));

	// get timestamp of video frame
	int64 frameTime(CryMwsfdTime2Msec(frm.time, frm.tunit));

	return (playbackTime - frameTime) >= 64; // 64[msec] = 4 vsync
}


bool CVideoPlayer::IsFullAlphaMovie() const
{
	return m_sfdCreationParams.compo_mode == MWSFD_COMPO_ALPHFULL;
}


bool CVideoPlayer::DynTexDataLost() const
{
	// dynamic texture data lost due to device reset?
	return m_frameReset != gRenDev->m_nFrameReset;
}


void CVideoPlayer::DecoderUpdate(bool checkDeviceLost)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	// perform decoding server processing
	int frameID(gRenDev->GetFrameID(false));
	if (m_lastDecSvrUpdate != frameID)
	{
		ADXM_ExecMain();
		m_lastDecSvrUpdate = frameID;
	}

	// retrieve new video image if available
	MwsfdFrmObj frm;
	mwPlyGetCurFrm(m_sfdHandle, &frm);

	if (frm.bufadr)
	{
		// update texture if allowed and frame is either not too old or dynamic texture data previously got lost
		bool updateAllowed(!checkDeviceLost || !gRenDev->CheckDeviceLost());
		if (updateAllowed && (!DiscardFrame(frm) || DynTexDataLost()))
			UpdateTextures(frm);

		// release video image
		mwPlyRelCurFrm(m_sfdHandle);
	}

	// update volume
  if(gEnv->pSoundSystem)
  {
	  float curVolume(gEnv->pSoundSystem->GetSFXVolume());
	  if (m_curVolume != curVolume)
	  {
		  int vol((int) (log10(clamp_tpl((double)curVolume, 1e-5, 1.0)) * 200.0)); // 0.5 = -6db
		  //int vol((int) (log10(clamp_tpl((double)curVolume, 1e-5, 1.0)) * 332.19281)); // 0.5 = -10db
		  vol = clamp_tpl(vol, -960, 0);
		  mwPlySetOutVol(m_sfdHandle, vol);
		  m_curVolume = curVolume;
	  }
  }
}


bool CVideoPlayer::Render()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	if (!m_sfdHandle || gRenDev->CheckDeviceLost())
		return false;

	// start playback if it hasn't been done already
	if (!m_playbackStarted)
		StartPlayback();

	//ADXM_WaitVsync();

	// decode frames and update video textures
	DecoderUpdate(false);

	// render video to currently bound target
	Display();

	return true;
}


void CVideoPlayer::GetSubtitle(int subtitleCh, char* pStBuf, size_t pStBufLen)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	if (!m_sfdHandle || !pStBuf || !pStBufLen || (subtitleCh < 0 && subtitleCh >= mwPlyGetNumSubtitleCh(m_sfdHandle)))
		return;

	mwPlySetSubtitleCh(m_sfdHandle, subtitleCh);

	MwsfdSbtPrm subtitleInfo;
	if (!mwPlyGetSubtitle(m_sfdHandle, (unsigned char*) pStBuf, pStBufLen, &subtitleInfo))
		pStBuf[0] = '\0';
}


void CVideoPlayer::EnablePerFrameUpdate(bool enable)
{
	m_perFrameUpdate = enable;
}


bool CVideoPlayer::IsPerFrameUpdateEnabled() const
{
	return m_perFrameUpdate;
}


int CVideoPlayer::GetWidth() const
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	if (!m_sfdHandle)
		return -1;

	return m_sfdCreationParams.max_width;
}


int CVideoPlayer::GetHeight() const
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	if (!m_sfdHandle)
		return -1;

	return IsFullAlphaMovie() ? m_sfdCreationParams.max_height / 2 : m_sfdCreationParams.max_height;
}


bool CVideoPlayer::CreateTextures()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);

	bool isFullAlphaMovie(IsFullAlphaMovie());

	assert(!m_pCurFrameY && !m_pCurFrameCb && !m_pCurFrameCr && (isFullAlphaMovie || !m_pCurFrameA));

	const char* pTextureName(m_pFilePath);

	char name[512];
	strncpy(name, pTextureName, 500);
	char* pSuffix(&name[0] + strlen(name));
	
	strcpy(pSuffix, "_Y");
	bool res(CreateTexture(m_pCurFrameY, name, GetWidth(), GetHeight(), 0));
	strcpy(pSuffix, "_Cb");
	res &= CreateTexture(m_pCurFrameCb, name, GetWidth() / 2, GetHeight() / 2, 128);
	strcpy(pSuffix, "_Cr");
	res &= CreateTexture(m_pCurFrameCr, name, GetWidth() / 2, GetHeight() / 2, 128);
	if (isFullAlphaMovie)
	{
		strcpy(pSuffix, "_A");
		res &= CreateTexture(m_pCurFrameA, name, GetWidth(), GetHeight(), 0);
	}
	return res;
}


void CVideoPlayer::DestroyTextures()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);

	SAFE_RELEASE(m_pCurFrameY);
	SAFE_RELEASE(m_pCurFrameCb);
	SAFE_RELEASE(m_pCurFrameCr);
	SAFE_RELEASE(m_pCurFrameA);
}


void CVideoPlayer::UpdateTextures(const MwsfdFrmObj& frm)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);

	bool isFullAlphaMovie(IsFullAlphaMovie());
	if (!m_pCurFrameY || !m_pCurFrameCb || !m_pCurFrameCr || !m_pCurFrameA && isFullAlphaMovie)
		CreateTextures();

	MWS_PLY_YCC_PLANE yccInfo;
	mwPlyCalcYccPlane(frm.bufadr, frm.width, frm.height, &yccInfo);

	//RecreateDeviceTexture(m_pCurFrameY, 0);
	UploadTextureData(m_pCurFrameY, yccInfo.y_ptr, yccInfo.y_width);

	//RecreateDeviceTexture(m_pCurFrameCb, 128);
	UploadTextureData(m_pCurFrameCb, yccInfo.cb_ptr, yccInfo.cb_width);

	//RecreateDeviceTexture(m_pCurFrameCr, 128);
	UploadTextureData(m_pCurFrameCr, yccInfo.cr_ptr, yccInfo.cr_width);

	if (IsFullAlphaMovie())
	{
		//RecreateDeviceTexture(m_pCurFrameA, 0);
		UploadTextureData(m_pCurFrameA, yccInfo.y_ptr + yccInfo.y_width * GetHeight(), yccInfo.y_width);
	}

	m_frameReset = gRenDev->m_nFrameReset;
}


bool CVideoPlayer::CreateTexture(CTexture*& pTexture, const char* pTextureName, int width, int height, unsigned char clearValue)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);

	SAFE_RELEASE(pTexture);
	pTexture = CTexture::Create2DTexture(pTextureName, width, height, 1, FT_STATE_CLAMP|FT_NOMIPS|FT_DONT_ANISO|FT_USAGE_DYNAMIC, 0, eTF_A8, eTF_A8);
	if (pTexture)
	{
		pTexture->SetFilter(FILTER_LINEAR);
		pTexture->SetClamp(true);
		ClearTexture(pTexture, clearValue);
	}

	return pTexture && pTexture->GetDeviceTexture() != 0;
}


void CVideoPlayer::RecreateDeviceTexture(CTexture*& pTexture, unsigned char clearValue)
{
	//FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);

	//if (pTexture && !pTexture->GetDeviceTexture())
	//{
	//	CCryName prevTexName(pTexture->GetCCryName());
	//	CreateTexture(pTexture, prevTexName.c_str(), pTexture->GetWidth(), pTexture->GetHeight(), clearValue);
	//}
}


void CVideoPlayer::Link()
{
	if (!m_node.m_pHandle)
	{
		m_node.m_pHandle = this;
		m_node.Link(&ms_rootNode);
	}
}


void CVideoPlayer::Unlink()
{
	if (m_node.m_pHandle)
	{
		m_node.Unlink();
		m_node.m_pHandle = 0;
	}
}


void CVideoPlayer::ProcessPerFrameUpdates()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	SLinkNode<CVideoPlayer>* pNode(ms_rootNode.m_pNext);
	while (pNode != &ms_rootNode)
	{
		CVideoPlayer* pPlayer(pNode->m_pHandle);
		if (pPlayer->m_sfdHandle && pPlayer->IsPerFrameUpdateEnabled())
			pPlayer->DecoderUpdate(true);
		pNode = pNode->m_pNext;
	}
}


void CVideoPlayer::OnAppFocusChanged(bool appHasFocus)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	if (ms_appHasFocus == appHasFocus)
		return;
	
	ms_appHasFocus = appHasFocus;

	SLinkNode<CVideoPlayer>* pNode(ms_rootNode.m_pNext);
	while (pNode != &ms_rootNode)
	{
		pNode->m_pHandle->OnFocusChanged();
		pNode = pNode->m_pNext;
	}
}


#endif // #ifndef EXCLUDE_CRI_SDK


IVideoPlayer* CRenderer::CreateVideoPlayerInstance() const
{
#ifndef EXCLUDE_CRI_SDK
	return new CVideoPlayer;
#else
	return 0;
#endif
}
