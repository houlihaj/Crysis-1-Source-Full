////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   timedemorecorder.cpp
//  Version:     v1.00
//  Created:     2/8/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TimeDemoRecorder.h"
#include <CryFile.h>
#include <IActorSystem.h>
#include <IAgent.h>
#include <ILevelSystem.h>
#include <ITestSystem.h>
#include <IMovieSystem.h>
#include "IMovementController.h"

#if defined(WIN32)
#include <windows.h>
#endif

#if defined(WIN32) && !defined(WIN64)
//#include "Psapi.h"								// PSAPI is not supported on windows9x
//#pragma comment(lib,"Psapi.lib")	// PSAPI is not supported on windows9x
#endif

//////////////////////////////////////////////////////////////////////////
// Brush Export structures.
//////////////////////////////////////////////////////////////////////////
#define TIMEDEMO_FILE_SIGNATURE "CRY"
#define TIMEDEMO_FILE_TYPE 150
#define TIMEDEMO_FILE_VERSION_1 1
#define TIMEDEMO_FILE_VERSION_2 2
#define TIMEDEMO_FILE_VERSION TIMEDEMO_FILE_VERSION_2

#define  TIMEDEMO_MAX_INPUT_EVENTS 16

#define FIXED_TIME_STEP (30) // Assume runing at 30fps.

enum ETimeDemoFileFlags
{
	eTimeDemoCompressed = 0x0001,
};

#pragma pack(push,1)
struct STimeDemoHeader
{
	char signature[3];	// File signature.
	int filetype;				// File type.
	int	version;				// File version.
	int nDataOffset;    // Offset where frame data starts.

	//////////////////////////////////////////////////////////////////////////
	int numFrames;      // Number of frames.
	int nFrameSize;     // Size of the per frame data in bytes.
	float totalTime;
	char levelname[128];
	// @see ETimeDemoFileFlags
	uint32 nDemoFlags;
	uint32 nCompressedDataSize;
	uint32 nUncompressedDataSze;
	char reserved[116];
};

//////////////////////////////////////////////////////////////////////////
struct STimeDemoFrame_1
{
	Vec3 pos;
	Ang3 angles;
	float frametime;

	unsigned int nActionFlags[2];
	float fLeaning;
	int nPolygonsPerFrame;
	char reserved[28];
};

//////////////////////////////////////////////////////////////////////////
struct STimeDemoFrameEvent_2
{
	uint16 deviceId   : 4;
	uint16 state      : 4;
	uint16 modifiers  : 4;
	uint16 keyId;
	float value;
};
struct STimeDemoFrame_2
{
	Vec3 pos;
	Ang3 camera_angles;
	Quat player_rotation;
	float frametime;

	unsigned int nActionFlags[2];
	float fLeaning;
	int nPolygonsPerFrame;
	uint8 numInputEvents;
	STimeDemoFrameEvent_2 inputEvents[TIMEDEMO_MAX_INPUT_EVENTS];
	char reserved[32];
};
struct STimeDemoFrame_3
{
	int nFrameDataSize; // Size of this frame in bytes.

	Vec3 pos;
	Ang3 camera_angles;
	Quat player_rotation;
	float frametime;

	unsigned int nActionFlags[2];
	float fLeaning;
	int nPolygonsPerFrame;
	int numInputEvents;
	STimeDemoFrameEvent_2 inputEvents[TIMEDEMO_MAX_INPUT_EVENTS];
	char reserved[32];
	//char data[]; // Special frame data.
};
#pragma pack(pop)
//////////////////////////////////////////////////////////////////////////

CTimeDemoRecorder* CTimeDemoRecorder::s_pTimeDemoRecorder = 0;
ICVar* CTimeDemoRecorder::s_timedemo_file = 0;

//////////////////////////////////////////////////////////////////////////
CTimeDemoRecorder::CTimeDemoRecorder( ISystem *pSystem )
{
	s_pTimeDemoRecorder = this;
	m_pSystem = pSystem;
	assert(m_pSystem);

	m_bRecording = false;
	m_bPlaying = false;
	m_bDemoFinished = false;

	m_currFrameIter = m_records.end();

	//m_recordStartTime = 0;
	//m_recordEndTime = 0;
	//m_lastFrameTime = 0;
	//m_totalDemoTime = 0;
	//m_recordedDemoTime = 0;

	m_lastAveFrameRate = 0;
	m_lastPlayedTotalTime = 0;

	m_fixedTimeStep = 0;
	m_maxLoops = 1000;
	m_demo_scroll_pause = 1;
	m_demo_noinfo = 1;
	m_demo_max_frames = 100000;
	m_demo_savestats = 0;
	m_demoProfile = 0;
	m_pTimeDemoInfo = 0;
	m_nCurrentDemoLevel = 0;

	m_bPaused = false;

	IConsole *pConsole = pSystem->GetIConsole();

	// Register demo variables.
	// marked with VF_CHEAT because it offers a MP cheat (impression to run faster on client)
	pConsole->AddCommand( "record",&CTimeDemoRecorder::cmd_StartRecording,0,
		"Starts recording of a time demo.\n"
		"Usage: record demoname\n"
		"File 'demoname.tmd' will be created.");

	pConsole->AddCommand("stoprecording",&CTimeDemoRecorder::cmd_StopRecording,0,
		"Stops recording of a time demo.\n"
		"Usage: stoprecording\n"
		"File 'demoname.?' will be saved.");

	pConsole->AddCommand("demo",&CTimeDemoRecorder::cmd_StartDemo,0,
		"Plays a time demo from file.\n"
		"Usage: demo demoname\n");

	pConsole->AddCommand("stopdemo",&CTimeDemoRecorder::cmd_StopDemo,0,
		"Stop playing a time demo.\n" );

	pConsole->AddCommand("demo_StartDemoChain",&CTimeDemoRecorder::cmd_StartDemoChain,0,"Load`s a file at 1st argument with the list of levels and play time demo on each\n" );

	s_timedemo_file = pSystem->GetIConsole()->RegisterString( "demo_file","timedemo",0,"Time Demo Filename" );
	pConsole->Register( "demo_profile",&m_demoProfile, 0,0,"Enable profiling" );
	pConsole->Register( "demo_num_runs",&m_maxLoops,1000,0,"Number of times to loop timedemo" );
	pConsole->Register( "demo_scroll_pause",&m_demo_scroll_pause,1,0,"ScrollLock pauses demo play/record" );
	pConsole->Register( "demo_noinfo",&m_demo_noinfo,0,0,"Disable info display during demo playback" );
	pConsole->Register( "demo_quit",&m_demo_quit,0,0,"Quit game after demo runs finished" );
	pConsole->Register( "demo_screenshot_frame",&m_demo_screenshot_frame,0,0,"Make screenshot on specified frame during demo playback, If Negative then do screen shoot every N frame" );
	pConsole->Register( "demo_max_frames",&m_demo_max_frames,100000,0,"Max number of frames to save" );
	pConsole->Register( "demo_savestats",&m_demo_savestats,0,0,"Save level stats at the end of the loop" );
	pConsole->Register( "demo_ai",&m_demo_ai,0,0,"Enable/Disable AI during the demo" );
	pConsole->Register( "demo_restart_level",&m_demo_restart_level,0,0,"Restart level after each loop: 0 = Off; 1 = use quicksave on first playback; 2 = load level start" );
	pConsole->Register( "demo_panormaic",&m_demo_panoramic,0,0,"Panormaic view when playing back demo" );
	pConsole->Register( "demo_fixed_timestep",&m_demo_fixed_timestep,FIXED_TIME_STEP,0,"number of updates per second" );
	pConsole->Register( "demo_vtune",&m_demo_vtune,0,0,"Enables VTune profiling when running time demo" );
	pConsole->Register( "demo_time_of_day",&m_demo_time_of_day,-1,0,"Sets the time of day to override in game settings if not negative" );
}

//////////////////////////////////////////////////////////////////////////
CTimeDemoRecorder::~CTimeDemoRecorder()
{
	if (m_pTimeDemoInfo)
		delete []m_pTimeDemoInfo->pFrames;
	delete m_pTimeDemoInfo;
	s_pTimeDemoRecorder = 0;

	IConsole* pConsole = gEnv->pConsole;

	pConsole->RemoveCommand("record");
	pConsole->RemoveCommand("stoprecording");
	pConsole->RemoveCommand("demo");
	pConsole->RemoveCommand("stopdemo");

	pConsole->UnregisterVariable( "demo_num_runs", true );
	pConsole->UnregisterVariable( "demo_scroll_pause", true );
	pConsole->UnregisterVariable( "demo_noinfo", true );
	pConsole->UnregisterVariable( "demo_quit", true );
	pConsole->UnregisterVariable( "demo_screenshot_frame", true );
	pConsole->UnregisterVariable( "demo_max_frames", true );
	pConsole->UnregisterVariable( "demo_savestats", true );
	pConsole->UnregisterVariable( "demo_ai", true );
	pConsole->UnregisterVariable( "demo_restart_level", true );
	pConsole->UnregisterVariable( "demo_panormaic", true );
	pConsole->UnregisterVariable( "demo_fixed_timestep", true );
	pConsole->UnregisterVariable( "demo_time_of_day", true );
}

//////////////////////////////////////////////////////////////////////////
const char* CTimeDemoRecorder::GetCurrentLevelPath()
{
	static char str[256];

	sprintf(str,"%s/levels/%s", PathUtil::GetGameFolder(), gEnv->pGame->GetIGameFramework()->GetLevelName());

	CryLog("DEBUG Levelpath: %s",str);

	return str;
/*
	ILevel *pLevel = gEnv->pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel();
	if (!pLevel)
		return "";
	ILevelInfo *pLevelInfo = pLevel->GetLevelInfo();
	if (!pLevelInfo)
		return "";
	return pLevelInfo->GetPath();
*/
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::cmd_StartRecording( IConsoleCmdArgs *pArgs )
{
	if (pArgs->GetArgCount() > 1)
	{
		s_timedemo_file->Set( pArgs->GetArg(1) );
	}
	if (s_pTimeDemoRecorder)
		s_pTimeDemoRecorder->Record(true);
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::cmd_StopRecording( IConsoleCmdArgs *pArgs )
{
	s_pTimeDemoRecorder->Record(false);
	//string filename = m_currentLevelFolder + "/" + s_timedemo_file->GetString() + ".tmd";
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::cmd_StartDemo( IConsoleCmdArgs *pArgs )
{
	// prevent exploit allowing teleporting around MP maps... only servers can play demos
	//	(and even then, only servers with no remote players connected).
	if(!gEnv->bServer)
	{
		CryLogAlways("Demos can only be played on a server");
		return;
	}
	if(CCryAction::GetCryAction()->GetIActorSystem()->GetActorCount() != 1)
	{
		CryLogAlways("Demos cannot be played when players are connected to the server");
		return;
	}

	if (pArgs->GetArgCount() > 1)
	{
		s_timedemo_file->Set( pArgs->GetArg(1) );
	}
	if (s_pTimeDemoRecorder)
	{
		s_pTimeDemoRecorder->Play(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::cmd_StartDemoChain( IConsoleCmdArgs *pArgs )
{
	if (pArgs->GetArgCount() > 1)
	{
		const char *sLevelsFile = pArgs->GetArg(1);
		s_pTimeDemoRecorder->StartChainDemo( sLevelsFile );
	}
	else
	{
		CryLogAlways( "Expect filename with the list of the levels" );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::cmd_StopDemo( IConsoleCmdArgs *pArgs )
{
	if (s_pTimeDemoRecorder)
		s_pTimeDemoRecorder->Play(false);
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::Record( bool bEnable )
{
	if (bEnable == m_bRecording)
		return;

	if (gEnv->pMovieSystem)
		gEnv->pMovieSystem->StopAllSequences();

	m_bRecording = bEnable;
	m_bPlaying = false;
	if (m_bRecording)
	{
		SaveAllEntitiesState();

		gEnv->pEntitySystem->AddSink(this);

		// Start recording.
		m_records.clear();
		m_records.reserve(1000);

		m_currentFrameInputEvents.clear();
		m_currentFrameEntityEvents.clear();
		// Start listening input events.
		if (gEnv->pInput)
			gEnv->pInput->AddEventListener(this);

		StartSession();

		m_recordStartTime = GetTime();
		m_lastFrameTime = m_recordStartTime;
	}
	else
	{
		// Stop recording.
		m_recordedDemoTime = m_totalDemoTime;
		m_lastFrameTime = GetTime();

		gEnv->pEntitySystem->RemoveSink(this);

		m_currentFrameInputEvents.clear();
		m_currentFrameEntityEvents.clear();
		// Stop listening tho the input events.
		if (gEnv->pInput)
			gEnv->pInput->RemoveEventListener(this);

		StopSession();

		// Save after stopping.
		string filename = PathUtil::Make( GetCurrentLevelPath(),s_timedemo_file->GetString(),"tmd" );
		s_pTimeDemoRecorder->Save( filename.c_str() );
	}
	m_currFrameIter = m_records.end();
	m_currentFrame = 0;
	m_totalDemoTime.SetMilliSeconds(0);
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::Play( bool bEnable )
{
	if (bEnable == m_bPlaying)
		return;

	if (gEnv->pMovieSystem)
		gEnv->pMovieSystem->StopAllCutScenes();

	if (bEnable)
	{
		assert(*GetCurrentLevelPath()!=0);

		// Try to load demo file.
		string filename = PathUtil::Make( GetCurrentLevelPath(),s_timedemo_file->GetString(),"tmd" );
		
		// Put it back later!
		//if (m_records.empty())
			Load( filename );


		if (m_records.empty())
		{
			m_bDemoFinished = true;
			if (m_demo_quit)
			{
				// Save level stats.
				if (m_demo_savestats != 0)
				{
					gEnv->pConsole->ExecuteString("SaveLevelStats");
				}
			}
			return;
		}
	}

	m_bPlaying = bEnable;

	if (m_bPlaying)
	{
		LogInfo( "==============================================================" );
		LogInfo( "TimeDemo Play Started , (Total Frames: %d, Recorded Time: %.2fs)",(int)m_records.size(),m_recordedDemoTime.GetSeconds() );

		m_bDemoFinished = false;

		RestoreAllEntitiesState();

		// Start demo playback.
		m_currFrameIter = m_records.begin();
		m_lastPlayedTotalTime = 0;
		StartSession();

		if (!m_pTimeDemoInfo)
		{
			m_pTimeDemoInfo = new STimeDemoInfo;
			m_pTimeDemoInfo->pFrames = 0;
		}
		if (m_pTimeDemoInfo->nFrameCount != (int)m_records.size())
		{
			delete []m_pTimeDemoInfo->pFrames;
			STimeDemoInfo *pTD = m_pTimeDemoInfo;
			pTD->nFrameCount = (int)m_records.size();
			pTD->pFrames = new STimeDemoFrameInfo[pTD->nFrameCount];
		}
	}
	else
	{
		LogInfo( "TimeDemo Play Ended, (%d Runs Performed)",m_numLoops );
		LogInfo( "==============================================================" );

		// End demo playback.
		m_currFrameIter = m_records.end();
		m_lastPlayedTotalTime = m_totalDemoTime.GetSeconds();
		StopSession();
	}
	m_bRecording = false;
	m_currentFrame = 0;
	m_totalDemoTime.SetMilliSeconds(0);
	m_numLoops = 0;
	m_fpsCounter = 0;
	m_lastFpsTimeRecorded = GetTime();

	m_currFPS = 0;
	m_minFPS = 10000;
	m_maxFPS = -10000;
	m_nMaxPolys = INT_MIN;
	m_nMinPolys = INT_MAX;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::Save( const char *filename )
{
	// Not save empty file.
	if (m_records.empty())
		return;

	CCryFile file;
	if (!file.Open( filename,"wb" ))
	{
		m_pSystem->Warning( VALIDATOR_MODULE_GAME,VALIDATOR_WARNING,0,filename,"Cannot open time demo file %s",filename );
		return;
	}

	m_file = filename;

	// Save Time demo file.
	STimeDemoHeader hdr;
	memset( &hdr,0,sizeof(hdr) );

	strncpy( hdr.levelname,GetCurrentLevelPath(),sizeof(hdr.levelname) );
	hdr.levelname[sizeof(hdr.levelname)-1] = 0;
	hdr.nDataOffset = sizeof(STimeDemoHeader);
	hdr.nFrameSize = sizeof(STimeDemoFrame_2);
	
	memcpy( hdr.signature,TIMEDEMO_FILE_SIGNATURE,3 );
	hdr.filetype = TIMEDEMO_FILE_TYPE;
	hdr.version = TIMEDEMO_FILE_VERSION;

	hdr.nDemoFlags = 0;
	hdr.numFrames = m_records.size();
	hdr.totalTime = m_recordedDemoTime.GetSeconds();
	hdr.nUncompressedDataSze = sizeof(STimeDemoFrame_2)*hdr.numFrames;

	std::vector<STimeDemoFrame_2> file_records;
	file_records.resize( hdr.numFrames );

	for (int i = 0; i < hdr.numFrames; i++)
	{
		FrameRecord &rec = m_records[i];
		STimeDemoFrame_2 &frame = file_records[i];
		frame.player_rotation = rec.playerRotation;
		frame.camera_angles = rec.cameraAngles;
		frame.pos = rec.playerPos;
		frame.frametime = rec.frameTime;
		*frame.nActionFlags = *rec.nActionFlags;
		frame.fLeaning = rec.fLeaning;
		frame.nPolygonsPerFrame = rec.nPolygons;
		frame.numInputEvents = 0;
		for (InputEventsList::const_iterator it = rec.inputEventsList.begin(); 
			it != rec.inputEventsList.end() && frame.numInputEvents < TIMEDEMO_MAX_INPUT_EVENTS; ++it)
		{
			const SInputEvent &inputEvent = *it;
			frame.inputEvents[frame.numInputEvents].deviceId = inputEvent.deviceId;
			frame.inputEvents[frame.numInputEvents].modifiers = inputEvent.modifiers;
			frame.inputEvents[frame.numInputEvents].state = inputEvent.state;
			frame.inputEvents[frame.numInputEvents].keyId = inputEvent.keyId;
			frame.inputEvents[frame.numInputEvents].value = inputEvent.value;
			frame.numInputEvents++;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Save to file.
	size_t nCompressedSize = hdr.nUncompressedDataSze*2+1024;
	char *pCompressedData = new char[nCompressedSize];
	if (GetISystem()->CompressDataBlock( &file_records[0],hdr.nUncompressedDataSze,pCompressedData,nCompressedSize ))
	{
		// Save compressed.
		hdr.nCompressedDataSize = nCompressedSize;
		hdr.nDemoFlags |= eTimeDemoCompressed;

		file.Write( &hdr,sizeof(hdr) );
		file.Write( pCompressedData,hdr.nCompressedDataSize );
	}
	else
	{
		// Save uncompressed.
		file.Write( &hdr,sizeof(hdr) );
		file.Write( &file_records[0],hdr.nUncompressedDataSze );
	}

	delete []pCompressedData;

/*
	XmlNodeRef root = m_pSystem->CreateXmlNode( "TimeDemo" );
	root->setAttr( "TotalTime",m_recordedDemoTime );
	for (FrameRecords::iterator it = m_records.begin(); it != m_records.end(); ++it)
	{
		FrameRecord &rec = *it;
		XmlNodeRef xmlRecord = root->newChild( "Frame" );
		xmlRecord->setAttr( "Pos",rec.playerPos );
		xmlRecord->setAttr( "Ang",rec.playerRotation );
		xmlRecord->setAttr( "Time",rec.frameTime );
	}
	root->saveToFile( filename );
*/
}

//////////////////////////////////////////////////////////////////////////
bool CTimeDemoRecorder::Load(  const char *filename )
{
	m_records.clear();
	m_recordedDemoTime.SetMilliSeconds(0);
	m_totalDemoTime.SetMilliSeconds(0);

	IInput *pIInput = GetISystem()->GetIInput(); // Cache IInput pointer.

	CCryFile file;
	if (!file.Open( filename,"rb" ))
	{
		char str[256];
		CryGetCurrentDirectory(256, str);

		m_pSystem->Warning( VALIDATOR_MODULE_GAME,VALIDATOR_WARNING,0,filename,"Cannot open time demo file %s (%s)",filename,str);
		return false;
	}

	m_file = filename;

	// Load Time demo file.
	STimeDemoHeader hdr;
	file.ReadRaw( &hdr,sizeof(hdr) );
	
	m_recordedDemoTime = hdr.totalTime;
	m_totalDemoTime = m_recordedDemoTime;

	m_records.reserve(hdr.numFrames);

	if (hdr.version == TIMEDEMO_FILE_VERSION_1)
	{
		for (int i = 0; i < hdr.numFrames && !file.IsEof(); i++)
		{
			STimeDemoFrame_1 frame;
			FrameRecord rec;
			file.ReadRaw( &frame,sizeof(frame) );

			Quat rot;
			rot.SetRotationXYZ( Ang3(DEG2RAD(frame.angles)) );
			rot = rot * Quat::CreateRotationZ(gf_PI); // to fix some game to camera rotation issues.
			rec.playerRotation = rot;
			rec.cameraAngles = frame.angles;
			rec.playerPos = frame.pos;
			rec.frameTime = frame.frametime;
			*rec.nActionFlags = *frame.nActionFlags;
			rec.fLeaning = frame.fLeaning;
			rec.nPolygons = frame.nPolygonsPerFrame;
			m_records.push_back( rec );
		}
	}
	else if (hdr.version == TIMEDEMO_FILE_VERSION_2)
	{
		char *pFrameData = new char [hdr.nUncompressedDataSze];
		if (hdr.nDemoFlags & eTimeDemoCompressed)
		{
			char *pCompressedData = new char [hdr.nCompressedDataSize];
			// Read Compressed.
			file.ReadRaw( pCompressedData,hdr.nCompressedDataSize);

			// Uncompress data.
			size_t uncompressedSize = hdr.nUncompressedDataSze;
			GetISystem()->DecompressDataBlock(pCompressedData,hdr.nCompressedDataSize,pFrameData,uncompressedSize );
			assert( uncompressedSize == hdr.nUncompressedDataSze );
			if (uncompressedSize != hdr.nUncompressedDataSze)
			{
				GameWarning( "Corrupted compressed time demo file %s",filename );
				return false;
			}
			delete []pCompressedData;
		}
		else
		{
			// Read Uncompressed.
			if (file.ReadRaw( pFrameData,hdr.nUncompressedDataSze ) != hdr.nUncompressedDataSze)
			{
				GameWarning( "Corrupted time demo file %s",filename );
				return false;
			}
		}
		assert( sizeof(STimeDemoFrame_2)*hdr.numFrames == hdr.nUncompressedDataSze );
		if (sizeof(STimeDemoFrame_2)*hdr.numFrames != hdr.nUncompressedDataSze)
		{
			GameWarning( "Corrupted time demo file %s",filename );
			return false;
		}
		STimeDemoFrame_2 *pFileFrame = (STimeDemoFrame_2*)pFrameData;
		for (int i = 0; i < hdr.numFrames; i++)
		{
			STimeDemoFrame_2 &frame = *pFileFrame++;
			FrameRecord rec;

			rec.playerRotation = frame.player_rotation;
			rec.cameraAngles = frame.camera_angles;
			rec.playerPos = frame.pos;
			rec.frameTime = frame.frametime;
			*rec.nActionFlags = *frame.nActionFlags;
			rec.fLeaning = frame.fLeaning;
			rec.nPolygons = frame.nPolygonsPerFrame;
			if (frame.numInputEvents > 0)
			{
				for (int j = 0; j < frame.numInputEvents; j++)
				{
					SInputEvent inputEvent;
					inputEvent.deviceId = (EDeviceId)frame.inputEvents[j].deviceId;
					inputEvent.modifiers = frame.inputEvents[j].modifiers;
					inputEvent.state = (EInputState)frame.inputEvents[j].state;
					inputEvent.keyId = (EKeyId)frame.inputEvents[j].keyId;
					inputEvent.value = frame.inputEvents[j].value;
					SInputSymbol *pInputSymbol = pIInput? pIInput->LookupSymbol( inputEvent.deviceId,inputEvent.keyId ) : 0;
					if (pInputSymbol)
						inputEvent.keyName = pInputSymbol->name;
					rec.inputEventsList.push_back(inputEvent);
				}
			}
			m_records.push_back( rec );
		}

		delete []pFrameData;
	}
	else
	{
		GameWarning( "Unknown timedemo file version" );
	}

	/*
	XmlNodeRef root = m_pSystem->LoadXmlFile( filename );
	if (!root)
	{
		// No such demo file.
		m_pSystem->Warning( VALIDATOR_MODULE_GAME,VALIDATOR_WARNING,0,filename,"Cannot open time demo file %s",filename );
		return;
	}
	root->getAttr( "TotalTime",m_recordedDemoTime );
	m_totalDemoTime = m_recordedDemoTime;
	for (int i = 0; i < root->getChildCount(); i++)
	{
		FrameRecord rec;
		XmlNodeRef xmlRecord = root->getChild(i);
		xmlRecord->getAttr( "Pos",rec.playerPos );
		xmlRecord->getAttr( "Ang",rec.playerRotation );
		xmlRecord->getAttr( "Time",rec.frameTime );
		m_records.push_back( rec );
	}
	*/

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::PreUpdate()
{
	if (m_bPlaying && !m_bPaused)
	{
		if (!gEnv->pConsole->GetStatus()) // Only when console closed.
		{
			// Reset random number generators seed.
			GetISystem()->GetISystemEventDispatcher()->OnSystemEvent( ESYSTEM_EVENT_RANDOM_SEED,0,0 );
			PlayFrame();
			RenderInfo();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::Update()
{
	if (!m_bPlaying && m_bDemoFinished && m_demo_quit != 0)
	{
		if (!m_demoLevels.empty())
		{
			StartNextChainedLevel();
			return;
		}

		gEnv->pRenderer->RestoreGamma();
		gEnv->pRenderer->ShutDownFast();

		// Quit after demo was finished.
		_exit(0);
	}

	//if(m_pSystem->GetIRenderer()->GetHWND() != ::GetActiveWindow())
		//return;

#ifdef WIN32
	if (!gEnv->pSystem->IsDedicated() && gEnv->pSystem->IsDevMode())		
	{
		// Check if special development keys where pressed.
		bool bAlt = ((CryGetAsyncKeyState(VK_LMENU) & (1<<15)) != 0) || (CryGetAsyncKeyState(VK_RMENU) & (1<<15)) != 0;
		bool bCtrl = (CryGetAsyncKeyState(VK_CONTROL) & (1<<15)) != 0;
		bool bShift = (CryGetAsyncKeyState(VK_SHIFT) & (1<<15)) != 0;

		bool bCancel = CryGetAsyncKeyState(VK_CANCEL) & 1;
		bool bTimeDemoKey = CryGetAsyncKeyState(VK_SNAPSHOT) & 1;

		if (bCancel)
		{
			if (IsRecording())
			{
				// Stop and save
				cmd_StopRecording(0);
				return;
			}
			// Toggle start/stop of demo recording.
			if (IsPlaying())
			{
				// Stop playing.
				Play(false);
				return;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Time demo on/off
		//////////////////////////////////////////////////////////////////////////
		if ((bCtrl||bAlt) && bTimeDemoKey)
		{
			if (!IsRecording())
			{
				// Start record.
				Record(true);
			}
		}
		if (bShift && bTimeDemoKey)
		{
			if (!IsPlaying())
			{
				// Load and start playing.
				Play(true);
			}
		}
	}
#endif

	if (m_bRecording)
	{
		if (!gEnv->pConsole->GetStatus()) // Only when console closed.
		// Reset random number generators seed.
		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent( ESYSTEM_EVENT_RANDOM_SEED,0,0 );
		RecordFrame();
		RenderInfo();
	}

	//Ang3 ang = RAD2DEG(Ang3( GetISystem()->GetViewCamera().GetMatrix() ));
	//Vec3 pos = GetISystem()->GetViewCamera().GetMatrix().GetTranslation();
	//CryLog( "CameraPos=[%.3f,%.3f,%.3f],  CameraAngles=[%.3f,%.3f,%.3f]",pos.x,pos.y,pos.z,ang.x,ang.y,ang.z );
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::RecordFrame()
{
	CTimeValue time = GetTime();
	float frameTime = (time - m_lastFrameTime).GetSeconds();

	if (m_bPaused)
	{
		m_lastFrameTime = time;
		return;
	}

	FrameRecord rec;
	memset( &rec,0,sizeof(rec) );
	rec.frameTime = frameTime;

	IEntity *pPlayer = NULL;

	IActor *pClActor = m_pSystem->GetIGame()->GetIGameFramework()->GetClientActor();
	if (pClActor)
	{
		pPlayer = pClActor->GetEntity();
			/*
		pClActor->GetEntity()->SetPos(pos);
		SOBJECTSTATE	cont;
		//		cont.m_desiredActions = 0;
		cont.fire = false;
		cont.fMovementUrgency = 1.0f;	// max speed.
		cont.vMoveDir = Vec3( 0,0,0 );
		cont.vLookDir = viewDir;
		pClActor->SetActorMovement( cont );
		*/
	}

	if (pPlayer)
	{
		rec.playerPos = pPlayer->GetPos();
		rec.playerRotation = pClActor->GetViewRotation();
	}
	Ang3 cameraAngles = Ang3( GetISystem()->GetViewCamera().GetMatrix() );
	rec.cameraAngles = RAD2DEG(cameraAngles);

	//CryLog( "[TimeDemoRec] [%04d] Angle: %.2f, %.2f, %.2f",m_currentFrame,rec.playerAngles.x,rec.playerAngles.y,rec.playerAngles.z );
	// Record current processing command.
	/*
	CXEntityProcessingCmd &cmd = ((CXGame*)m_pGame)->m_pClient->m_PlayerProcessingCmd;
	*rec.nActionFlags = *cmd.m_nActionFlags;
	rec.fLeaning = cmd.GetLeaning();
	*/

	//////////////////////////////////////////////////////////////////////////
	// Record input events.
	//////////////////////////////////////////////////////////////////////////
	rec.inputEventsList = m_currentFrameInputEvents;
	rec.entityEvents = m_currentFrameEntityEvents;
	m_currentFrameInputEvents.clear();
	m_currentFrameEntityEvents.clear();
	//////////////////////////////////////////////////////////////////////////

	m_totalDemoTime += rec.frameTime;

	int nPolygons,nShadowVolPolys;
	m_pSystem->GetIRenderer()->GetPolyCount(nPolygons,nShadowVolPolys);
	rec.nPolygons = nPolygons;

	m_records.push_back( rec );

	m_currentFrame++;
	m_lastFrameTime = GetTime();

	if (m_currentFrame >= m_demo_max_frames)
	{
		Record(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::PlayFrame()
{
	if (m_currFrameIter == m_records.end()) // can't playback empty records.
		return;

	CTimeValue time = GetTime();
	CTimeValue deltaFrameTime = (time - m_lastFrameTime);
	float frameTime = deltaFrameTime.GetSeconds();

	if (m_bPaused)
	{
		m_lastFrameTime = time;
		return;
	}

	//override game time of day
	if( 0 <= m_demo_time_of_day )
	{
		//force update if time is significantly different from current time
		float fTime = gEnv->p3DEngine->GetTimeOfDay()->GetTime();
		const float cfSmallDeltaTime = 0.1f;

		if( ( fTime >  m_demo_time_of_day + cfSmallDeltaTime ) ||
			( fTime <  m_demo_time_of_day - cfSmallDeltaTime ) )
		{
			gEnv->p3DEngine->GetTimeOfDay()->SetTime( m_demo_time_of_day, true );
			SetConsoleVar( "e_time_of_day_speed", 0 );
		}
	}

	FrameRecord &rec = *m_currFrameIter;

	//////////////////////////////////////////////////////////////////////////
	// Play input events.
	//////////////////////////////////////////////////////////////////////////
	if (!rec.inputEventsList.empty())
	{
		//string str;
		FrameRecord &rec = *m_currFrameIter;
		for (InputEventsList::const_iterator it = rec.inputEventsList.begin(), end = rec.inputEventsList.end(); it != end; ++it)
		{
			const SInputEvent &inputEvent = *it;
			if (gEnv->pInput)
				gEnv->pInput->PostInputEvent( inputEvent );
			//string str1;
			//str1.Format( "%s: %.2f",inputEvent.keyName,inputEvent.value );
			//str += str1;
		}
		//CryLog( str );
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Play back entity events.
	//////////////////////////////////////////////////////////////////////////
	{
		for (int i = 0; i < rec.entityEvents.size(); i++)
		{
			PlayBackEntityEvent( rec.entityEvents[i] );
		}
	}
	//////////////////////////////////////////////////////////////////////////

	IEntity *pPlayer = NULL;
	IActor *pClActor = m_pSystem->GetIGame()->GetIGameFramework()->GetClientActor();
	if (pClActor)
	{
		pPlayer = pClActor->GetEntity();
	}

	Quat qPlayRot;
	if (pPlayer)
	{
		//qPlayRot = Quat::CreateRotationXYZ( DEG2RAD(rec.cameraAngles) );

		if (pPlayer->GetParent() == 0)
		{
			// Only if player is not linked to anything.
			pPlayer->SetPos( rec.playerPos,ENTITY_XFORM_TIMEDEMO );
			pClActor->SetViewRotation( rec.playerRotation );
		}
	}

	//Ang3 cameraAngles = RAD2DEG(Ang3( GetISystem()->GetViewCamera().GetMatrix() ));
//	CryLog( "[TimeDemoPlay] [%04d] Angle: %.2f, %.2f, %.2f",m_currentFrame,cameraAngles.x,cameraAngles.y,cameraAngles.z );

	m_totalDemoTime += deltaFrameTime;

	int nPolygons,nShadowVolPolys;
	m_pSystem->GetIRenderer()->GetPolyCount(nPolygons,nShadowVolPolys);
	m_nPolysCounter += nPolygons;
	m_nCurrPolys = nPolygons;
	if (nPolygons > m_nMaxPolys)
		m_nMaxPolys = nPolygons;
	if (nPolygons < m_nMinPolys)
		m_nMinPolys = nPolygons;

	m_nTotalPolysRecorded += rec.nPolygons;
	m_nTotalPolysPlayed += nPolygons;

	//////////////////////////////////////////////////////////////////////////
	// Calculate Frame Rates.
	//////////////////////////////////////////////////////////////////////////
	// Skip some frames before calculating frame rates.
	if ((time - m_lastFpsTimeRecorded).GetSeconds() > 1)
	{
		// Skip some frames before recording frame rates.
		if (m_currentFrame > 60)
		{
			m_currFPS = (float)m_fpsCounter / (time - m_lastFpsTimeRecorded).GetSeconds();
			if (m_currFPS > m_maxFPS)
			{
				m_maxFPS_Frame = m_currentFrame;
				m_maxFPS = m_currFPS;
			}
			if (m_currFPS < m_minFPS)
			{
				m_minFPS_Frame = m_currentFrame;
				m_minFPS = m_currFPS;
			}
		}
		
		m_nPolysPerSec = (int)(m_nPolysCounter  / (time - m_lastFpsTimeRecorded).GetSeconds());
		m_nPolysCounter = 0;

		m_fpsCounter = 0;
		m_lastFpsTimeRecorded = time;
	}
	else
	{
		m_fpsCounter++;
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Fill Time Demo Info structure.
	//////////////////////////////////////////////////////////////////////////
	m_pTimeDemoInfo->pFrames[m_currentFrame].fFrameRate = (float)(1.0 / deltaFrameTime.GetSeconds());
	m_pTimeDemoInfo->pFrames[m_currentFrame].nPolysRendered = nPolygons;
	m_pTimeDemoInfo->pFrames[m_currentFrame].nDrawCalls = gEnv->pRenderer->GetCurrentNumberOfDrawCalls();
	//////////////////////////////////////////////////////////////////////////

	if ((m_numLoops == 0) &&
			((m_demo_screenshot_frame && m_currentFrame == m_demo_screenshot_frame) ||
			(m_demo_screenshot_frame < 0) && (m_currentFrame % abs(m_demo_screenshot_frame) == 0) ))
	{
/*		ICVar *	p_demo_file = pSystem->GetIConsole()->GetCVar("demo_file");
		char szFileName[256]="";
		sprinyf("demo_%s_%d.jpg",p_demo_file->GetString(),m_currentFrame);*/
		m_pSystem->GetIRenderer()->ScreenShot();
	}

	m_currFrameIter++;
	m_currentFrame++;

	// Play looped.
	if (m_currFrameIter == m_records.end() || m_currentFrame >= m_demo_max_frames)
	{
		m_lastPlayedTotalTime = m_totalDemoTime.GetSeconds();
		m_lastAveFrameRate = GetAverageFrameRate();

		// Log info to file.
		LogRun();

		m_totalDemoTime.SetMilliSeconds(0);
		m_currFrameIter = m_records.begin();
		m_currentFrame = 0;
		m_numLoops++;
		m_nTotalPolysPlayed = 0;
		m_nTotalPolysRecorded = 0;

		// Stop playing if max runs reached.
		if (m_numLoops >= m_maxLoops)
		{
			Play(false);
			m_bDemoFinished = true;
		}

		ResetSessionLoop();
	}

	m_lastFrameTime = GetTime();
}

//////////////////////////////////////////////////////////////////////////
CTimeValue CTimeDemoRecorder::GetTime()
{
	// Must be asynchronius time, used for profiling.
	return m_pSystem->GetITimer()->GetAsyncTime();
}

//////////////////////////////////////////////////////////////////////////
int CTimeDemoRecorder::GetNumFrames() const
{
	return m_records.size();
}

//////////////////////////////////////////////////////////////////////////
float CTimeDemoRecorder::GetAverageFrameRate() const
{
	if(m_currentFrame)
	{
		float aveFrameTime = m_totalDemoTime.GetSeconds() / m_currentFrame;
		float aveFrameRate = 1.0f / aveFrameTime;
		return aveFrameRate;
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::RenderInfo()
{
	if (m_demo_noinfo != 0)
		return;

	const char *sInfo = "";
	m_bPaused = false;
	if (m_bRecording || m_bPlaying)
	{
		if (m_demo_scroll_pause != 0 && GetISystem()->GetIInput())
		{
			static TKeyName scrollKey("scrolllock");
			bool bPaused = GetISystem()->GetIInput()->InputState(scrollKey, eIS_Down);
			if (bPaused)
			{
				m_bPaused = true;
				sInfo = " (Paused)";
			}
		}
	}

	IRenderer *pRenderer = m_pSystem->GetIRenderer();
	if (m_bRecording)
	{
		float fColor[4] = {1,0,0,1};
		pRenderer->Draw2dLabel( 1,1, 1.3f, fColor,false,"Recording TimeDemo%s, Frames: %d",sInfo,m_currentFrame );
	}
	else if (m_bPlaying)
	{
		float aveFrameRate = GetAverageFrameRate();
		float polyRatio = m_nTotalPolysPlayed ? (float)m_nTotalPolysRecorded/m_nTotalPolysPlayed : 0.0f;
		int numFrames = GetNumFrames();
		float fColor[4] = {0,1,0,1};
		pRenderer->Draw2dLabel( 1,1, 1.3f, fColor,false,"Playing TimeDemo%s, Frame %d of %d (Looped: %d)",sInfo,m_currentFrame,numFrames,m_numLoops );
		pRenderer->Draw2dLabel( 1,1+15, 1.3f, fColor,false,"Last Played Length: %.2fs, FPS: %.2f",m_lastPlayedTotalTime,m_lastAveFrameRate );
		pRenderer->Draw2dLabel( 1,1+30, 1.3f, fColor,false,"Average FPS: %.2f, FPS: %.2f, Polys/Frame: %d",aveFrameRate,m_currFPS,m_nCurrPolys );
		pRenderer->Draw2dLabel( 1,1+45, 1.3f, fColor,false,"Polys Rec/Play Ratio: %.2f",polyRatio );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::SetConsoleVar( const char *sVarName,float value )
{
	ICVar *pVar = m_pSystem->GetIConsole()->GetCVar( sVarName );
	if (pVar)
		pVar->Set( value );
}

//////////////////////////////////////////////////////////////////////////
float CTimeDemoRecorder::GetConsoleVar( const char *sVarName )
{
	ICVar *pVar = m_pSystem->GetIConsole()->GetCVar( sVarName );
	if (pVar)
		return pVar->GetFVal();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::StartSession()
{
\
#ifdef SP_DEMO
	//allow devmode in SP_DEMO only during demo playback
	CCryAction::GetCryAction()->CreateDevMode();
#endif

	m_nTotalPolysRecorded = 0;
	m_nTotalPolysPlayed = 0;
	m_lastAveFrameRate = 0;
	m_lastPlayedTotalTime = 0;

	//////////////////////////////////////////////////////////////////////////
	if (m_bRecording && m_demo_ai)
	{
		SaveAllEntitiesState();
	}
	//////////////////////////////////////////////////////////////////////////

	// Register to frame profiler.

	m_bEnabledProfiling = m_pSystem->GetIProfileSystem()->IsEnabled();
	m_bVisibleProfiling = m_pSystem->GetIProfileSystem()->IsVisible();

	if (m_demoProfile)
	{
		m_pSystem->GetIProfileSystem()->Enable(true, false);
	}

	m_pSystem->GetIProfileSystem()->AddPeaksListener( this );
	// Remember old time step, and set constant one.
	m_fixedTimeStep = GetConsoleVar( "fixed_time_step" );
	SetConsoleVar( "fixed_time_step",1.0f/(float)m_demo_fixed_timestep );
	if (m_demo_panoramic != 0)
		SetConsoleVar( "e_panorama",1 );

	if (m_demo_ai == 0)
	{
#ifdef SP_DEMO
		// force these for sp demo during demo recording because there is no devmode available
		ICVar *pVar = m_pSystem->GetIConsole()->GetCVar( "ai_systemupdate" );
		if (pVar) pVar->ForceSet( "0" );
		pVar = m_pSystem->GetIConsole()->GetCVar( "ai_ignoreplayer" );
		if (pVar) pVar->ForceSet( "1" );
		pVar = m_pSystem->GetIConsole()->GetCVar( "ai_soundperception" );
		if (pVar) pVar->ForceSet( "0" );
#else
		SetConsoleVar( "ai_systemupdate",0 );
		SetConsoleVar( "ai_ignoreplayer",1 );
		SetConsoleVar( "ai_soundperception",0 );
#endif
	}
	else
	{
		SetConsoleVar( "ai_UseCalculationStopperCounter",1 ); // To make AI async time independent.
	}
	// Not cut-scenes during timedemo.
	SetConsoleVar( "mov_NoCutscenes",1 );

	// No wait for key-press on level load.
	SetConsoleVar( "hud_startPaused",0 );

	m_prevGodMode = (int)GetConsoleVar("g_godMode");
	SetConsoleVar("g_godMode",1);

	// Profile
	m_oldPeakTolerance = GetConsoleVar( "profile_peak" );
	SetConsoleVar( "profile_peak",50 );

	ResetSessionLoop();

	if (m_bPlaying)
	{
		IActor *pClientActor = m_pSystem->GetIGame()->GetIGameFramework()->GetClientActor();
		if (pClientActor)
		{
			pClientActor->EnableTimeDemo(true);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Force player out of the vehicle on start.
	//////////////////////////////////////////////////////////////////////////
	gEnv->pConsole->ExecuteString( "v_exit_player" );

	if (m_demo_vtune)
		GetISystem()->VTuneResume();

	m_lastFrameTime = GetTime();
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::StopSession()
{
#ifdef SP_DEMO
	CCryAction::GetCryAction()->RemoveDevMode();
#endif
	if (m_demo_vtune)
		GetISystem()->VTunePause();

	// Set old time step.
	SetConsoleVar( "fixed_time_step",m_fixedTimeStep );
	if (m_demo_panoramic != 0)
		SetConsoleVar( "e_panorama",0 );
	
	if (m_demo_ai == 0)
	{
		SetConsoleVar( "ai_systemupdate",1 );
		SetConsoleVar( "ai_ignoreplayer",0 );
		SetConsoleVar( "ai_soundperception",1 );
	}
	else
	{
		SetConsoleVar( "ai_UseCalculationStopperCounter",0 ); // To make AI async time independent.
	}
	SetConsoleVar( "mov_NoCutscenes",0 );
	SetConsoleVar("g_godMode",m_prevGodMode);

	// Profile.
	SetConsoleVar( "profile_peak",m_oldPeakTolerance );
	m_pSystem->GetIProfileSystem()->RemovePeaksListener( this );

	if (m_demoProfile)
	{
		m_pSystem->GetIProfileSystem()->Enable(m_bEnabledProfiling, m_bVisibleProfiling);
	}


	IActor *pClientActor = m_pSystem->GetIGame()->GetIGameFramework()->GetClientActor();
	if (pClientActor)
	{
		pClientActor->EnableTimeDemo(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::ResetSessionLoop()
{
	if (m_demo_restart_level != 0 && m_bPlaying)
	{
		switch( m_demo_restart_level )
		{
		case 1:
			// Quick load at the begining of the playback, so we restore initial state as good as possible.
			CCryAction::GetCryAction()->LoadGame( PathUtil::Make( "",s_timedemo_file->GetString(),".qsv"),true );
			break;

		case 2:
		default:	
			//load save made at start of level
			if( gEnv->pGame->GetIGameFramework() )
			{
				string levelstart( gEnv->pGame->GetIGameFramework()->GetLevelName() );
				if(const char* visibleName = gEnv->pGame->GetMappedLevelName(levelstart))
				{
					levelstart = visibleName;
				}
				levelstart.append("_crysis.crysisjmsf");	//TODO: refactor to remove crysis
				gEnv->pGame->GetIGameFramework()->LoadGame(levelstart.c_str(), true, true);
			}
		}


/*
		if (!GetISystem()->IsEditor())
			gEnv->pGame->InitMapReloading();
		gEnv->pAISystem->Reset( IAISystem::RESET_ENTER_GAME );
*/
	}
	// Reset random number generators seed.
	GetISystem()->GetISystemEventDispatcher()->OnSystemEvent( ESYSTEM_EVENT_RANDOM_SEED,0,0 );
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::LogInfo( const char *format,... )
{
	if (m_demo_noinfo != 0)
		return;

	va_list	ArgList;
	char szBuffer[1024];

	va_start(ArgList, format);
	vsprintf_s(szBuffer, sizeof(szBuffer), format, ArgList);
	va_end(ArgList);

	char path_buffer[_MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	m_pSystem->GetILog()->Log( szBuffer  );

	_splitpath( m_file.c_str(), drive, dir, fname, ext );
	_makepath( path_buffer, drive, dir,fname,"log" );
	FILE *hFile = fopen( path_buffer,"at" );
	if (hFile)
	{
		// Write the string to the file and close it
		fprintf(hFile, "%s\n",szBuffer );
		fclose(hFile);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::OnFrameProfilerPeak( CFrameProfiler *pProfiler,float fPeakTime )
{
	if (m_bPlaying && !m_bPaused)
	{
		LogInfo( "    -Peak at Frame %d, %.2fms : %s (count: %d)",m_currentFrame,fPeakTime,pProfiler->m_name,pProfiler->m_count );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTimeDemoRecorder::OnInputEvent( const SInputEvent &event )
{
	if ((event.deviceId == eDI_Keyboard) && event.keyId == eKI_Tilde)
	{
		return false;
	}
	if (event.deviceId != eDI_Unknown)
	{
		m_currentFrameInputEvents.push_back(event);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::LogRun()
{
	STimeDemoInfo *pTD = m_pTimeDemoInfo;
	pTD->lastPlayedTotalTime = m_lastPlayedTotalTime;
	pTD->lastAveFrameRate = m_lastAveFrameRate;
	pTD->minFPS = m_minFPS;
	pTD->maxFPS = m_maxFPS;
	pTD->minFPS_Frame = m_minFPS_Frame;
	pTD->maxFPS_Frame = m_maxFPS_Frame;
	pTD->nTotalPolysRecorded = m_nTotalPolysRecorded;
	pTD->nTotalPolysPlayed = m_nTotalPolysPlayed;
	GetISystem()->GetITestSystem()->SetTimeDemoInfo( m_pTimeDemoInfo );

	int numFrames = m_records.size();
	LogInfo( "!TimeDemo Run %d Finished.",m_numLoops );
	LogInfo( "    Play Time: %.2fs, Average FPS: %.2f",m_lastPlayedTotalTime,m_lastAveFrameRate );
	LogInfo( "    Min FPS: %.2f at frame %d, Max FPS: %.2f at frame %d",m_minFPS,m_minFPS_Frame,m_maxFPS,m_maxFPS_Frame );
	LogInfo( "    Average Tri/Sec: %d, Tri/Frame: %d",(int)(m_nTotalPolysPlayed/m_lastPlayedTotalTime),m_nTotalPolysPlayed/numFrames );
	LogInfo( "    Recorded/Played Tris ratio: %.2f",(float)m_nTotalPolysRecorded/m_nTotalPolysPlayed );

	// Save level stats, after each run.
	if (m_demo_savestats != 0)
	{
		// Save stats after last run only.
		if (m_demo_quit && m_numLoops >= m_maxLoops-1)
		{
			gEnv->pConsole->ExecuteString("SaveLevelStats");
		}
	}

#if defined(WIN32) && !defined(WIN64)

	// PSAPI is not supported on window9x
	// so, don't use it

	//PROCESS_MEMORY_COUNTERS pc;
	//HANDLE hProcess = GetCurrentProcess();
	//pc.cb = sizeof(pc);
	//GetProcessMemoryInfo( hProcess,&pc,sizeof(pc) );
	//int MB = 1024*1024;
	//LogInfo( "    Memory Usage: WorkingSet=%dMb, PageFile=%dMb, PageFaults=%d",pc.WorkingSetSize/MB,pc.PagefileUsage/MB,pc.PageFaultCount );

#endif
}

void CTimeDemoRecorder::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "TimeDemoRecorder");
	s->Add(*this);
	s->AddContainer(m_records);
	s->AddContainer(m_currentFrameInputEvents);
	s->AddContainer(m_currentFrameEntityEvents);
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::StartChainDemo( const char *levelsListFilename )
{
	m_demoLevels.clear();
	if (levelsListFilename && *levelsListFilename)
	{
		// Open file with list of levels for timedemo.
		// This is used only for development so doesn`t need to use CryPak!
		FILE *f = fopen( levelsListFilename,"rt" );
		if (f)
		{
			while (!feof(f))
			{
				char str[512];
				fgets( str,sizeof(str),f );
				if (*str)
				{
					string level = str;
					level.Trim();
					m_demoLevels.push_back(level);
				}
			}
			fclose(f);
		}
	}

	m_nCurrentDemoLevel = 0;
	StartNextChainedLevel();
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::StartNextChainedLevel()
{
	if (!m_demoLevels.empty())
	{
		// This support loading level/playnig time demo, then loading next level, etc...
		if (m_nCurrentDemoLevel < m_demoLevels.size())
		{
			string mapCmd = string("map ") + m_demoLevels[m_nCurrentDemoLevel];
			gEnv->pConsole->ExecuteString( mapCmd );
			Play(true);
			m_nCurrentDemoLevel++;
			return;
		}
	}
	if (m_demo_quit)
	{
		gEnv->pRenderer->RestoreGamma();
		gEnv->pRenderer->ShutDownFast();

		// If No more chained levels. quit.
		exit(0);
	}
}

bool CTimeDemoRecorder::OnBeforeSpawn( SEntitySpawnParams &params )
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::OnSpawn( IEntity *pEntity,SEntitySpawnParams &params )
{
}

//////////////////////////////////////////////////////////////////////////
bool CTimeDemoRecorder::OnRemove( IEntity *pEntity )
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::OnEvent( IEntity *pEntity, SEntityEvent &event )
{
	if (m_bRecording)
	{
		// Record entity event for this frame.
		EntityGUID guid = pEntity->GetGuid();
		if (!guid)
			return;

		// Record entity event for this frame.
		switch (event.event)
		{
			// Events to save.
		case ENTITY_EVENT_XFORM:
		case ENTITY_EVENT_HIDE:
		case ENTITY_EVENT_UNHIDE:
		case ENTITY_EVENT_ATTACH:
		case ENTITY_EVENT_DETACH:
		case ENTITY_EVENT_DETACH_THIS:
		case ENTITY_EVENT_ENABLE_PHYSICS:
		case ENTITY_EVENT_ENTER_SCRIPT_STATE:
			{
				EntityEventRecord rec;
				memset( &rec,0,sizeof(rec) );
				rec.entityId = pEntity->GetId();
				rec.guid = guid;
				rec.eventType = event.event;
				rec.nParam[0] = event.nParam[0];
				rec.nParam[1] = event.nParam[1];
				rec.nParam[2] = event.nParam[2];
				rec.nParam[3] = event.nParam[3];
				rec.pos = pEntity->GetPos();
				rec.q = pEntity->GetRotation();
				m_currentFrameEntityEvents.push_back(rec);
			}
			break;

			// Skip all other events.
		default:
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::PlayBackEntityEvent( const EntityEventRecord &rec )
{
	EntityId entityId = gEnv->pEntitySystem->FindEntityByGuid(rec.guid);
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	if (!pEntity)
		return;

	switch (rec.eventType)
	{
		// Events to save.
	case ENTITY_EVENT_XFORM:
		pEntity->SetPosRotScale( rec.pos,rec.q,pEntity->GetScale(),ENTITY_XFORM_TIMEDEMO );
		break;
	case ENTITY_EVENT_HIDE:
		pEntity->Hide( true );
		break;
	case ENTITY_EVENT_UNHIDE:
		pEntity->Hide( false );
		break;
	case ENTITY_EVENT_ATTACH:
		{
			IEntity *pChild = gEnv->pEntitySystem->GetEntity(rec.nParam[0]); // Get Child entity.
			if (pChild)
				pEntity->AttachChild( pChild );
		}
		break;
	case ENTITY_EVENT_DETACH:
		break;
	case ENTITY_EVENT_DETACH_THIS:
		pEntity->DetachThis(0,ENTITY_XFORM_TIMEDEMO);
		break;
	case ENTITY_EVENT_ENABLE_PHYSICS:
		if (rec.nParam[0] == 0)
			pEntity->EnablePhysics(false);
		else
			pEntity->EnablePhysics(true);
		break;
	case ENTITY_EVENT_ENTER_SCRIPT_STATE:
		{
			IEntityScriptProxy *pScriptProxy = (IEntityScriptProxy*)pEntity->GetProxy(ENTITY_PROXY_SCRIPT);
			if (pScriptProxy)
			{
				pScriptProxy->GotoStateId( rec.nParam[0] );
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::SaveAllEntitiesState()
{
	m_firstFrameEntityState.clear();
	// Record all objects positions.
	IEntity *pEntity;
	IEntityItPtr pEntityIter = gEnv->pEntitySystem->GetEntityIterator();
	while (pEntity = pEntityIter->Next())
	{
		EntityGUID guid = pEntity->GetGuid();
		if (guid)
		{
			EntityEventRecord rec;
			memset( &rec,0,sizeof(rec) );
			rec.entityId = pEntity->GetId();
			rec.guid = guid;
			rec.eventType = ENTITY_EVENT_RESET;
			rec.pos = pEntity->GetPos();
			rec.q = pEntity->GetRotation();
			rec.flags |= (pEntity->IsHidden()) ? EntityEventRecord::HIDDEN : 0;
			m_firstFrameEntityState.push_back(rec);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::RestoreAllEntitiesState()
{
	for (int i = 0; i < m_firstFrameEntityState.size(); i++)
	{
		EntityEventRecord &rec = m_firstFrameEntityState[i];
		if (rec.eventType == ENTITY_EVENT_RESET)
		{
			EntityId entityId = gEnv->pEntitySystem->FindEntityByGuid(rec.guid);
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityId);
			if (!pEntity)
				continue;
			
			pEntity->Hide( (rec.flags & EntityEventRecord::HIDDEN) != 0 );
			pEntity->SetPosRotScale( rec.pos,rec.q,pEntity->GetScale(),ENTITY_XFORM_TIMEDEMO );
		}
	}

	if (1 == m_demo_restart_level)
	{
		//QS does not work when playing a timedemo, so pretend we're not...
		bool bCurrentlyPlaying = m_bPlaying;
		m_bPlaying = false;

		// Quick save at the begining of the playback, so we restore initial state as good as possible.
		CCryAction::GetCryAction()->SaveGame( PathUtil::Make( "",s_timedemo_file->GetString(),".qsv"), true,true,eSGR_QuickSave );

		m_bPlaying = bCurrentlyPlaying;
	}
}
