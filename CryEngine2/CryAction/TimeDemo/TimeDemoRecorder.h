////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   timedemorecorder.h
//  Version:     v1.00
//  Created:     2/8/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __timedemorecorder_h__
#define __timedemorecorder_h__
#pragma once

class CTimeDemoRecorder : public IFrameProfilePeakCallback,public IInputEventListener, IEntitySystemSink
{
public:
	CTimeDemoRecorder( ISystem *pSystem );
	~CTimeDemoRecorder();
	
	void PreUpdate();
	void Update();
	void RenderInfo();

	void Record( bool bEnable );
	void Play( bool bEnable );
	bool IsRecording() const { return m_bRecording; };
	bool IsPlaying() const { return m_bPlaying; };

	//! Get number of frames in record.
	int GetNumFrames() const;
	float GetAverageFrameRate() const;

	void Save( const char *filename );
	bool Load(  const char *filename );
	
	void StartChainDemo( const char *levelsListFilename );

	//////////////////////////////////////////////////////////////////////////
	// Implements IFrameProfilePeakCallback interface.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnFrameProfilerPeak( CFrameProfiler *pProfiler,float fPeakTime );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Implements IInputEventListener interface.
	//////////////////////////////////////////////////////////////////////////
	virtual bool OnInputEvent( const SInputEvent &event );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntitySystemSink
	//////////////////////////////////////////////////////////////////////////
	virtual bool OnBeforeSpawn( SEntitySpawnParams &params );
	virtual void OnSpawn( IEntity *pEntity,SEntitySpawnParams &params );
	virtual bool OnRemove( IEntity *pEntity );
	virtual void OnEvent( IEntity *pEntity, SEntityEvent &event );
	//////////////////////////////////////////////////////////////////////////

	void GetMemoryStatistics(ICrySizer * s);

private:
	struct EntityEventRecord
	{
		EntityId   entityId;      // What entity performed event.
		EntityGUID guid;          // What entity performed event.

		uint32 eventType;         // What event.
		uint64 nParam[4];         // event params.
		Vec3 pos;
		Quat q;

		enum Flags
		{
			HIDDEN = BIT(1),
		};

		// Special flags.
		uint32 flags;
	};

	static void cmd_StartRecording( IConsoleCmdArgs *pArgs );
	static void cmd_StopRecording( IConsoleCmdArgs *pArgs );
	static void cmd_StartDemo( IConsoleCmdArgs *pArgs );
	static void cmd_StartDemoChain( IConsoleCmdArgs *pArgs );
	static void cmd_StopDemo( IConsoleCmdArgs *pArgs );

	static const char* GetCurrentLevelPath();

	void RecordFrame();
	void PlayFrame();

	CTimeValue GetTime();
	// Set Value of console variable.
	void SetConsoleVar( const char *sVarName,float value );
	// Get value of console variable.
	float GetConsoleVar( const char *sVarName );

	void StartSession();
	void StopSession();
	void ResetSessionLoop();
	void StartNextChainedLevel();

	void LogRun();
	void LogInfo( const char *format,... ) PRINTF_PARAMS(2, 3);

	void PlayBackEntityEvent( const EntityEventRecord &rec );
	void SaveAllEntitiesState();
	void RestoreAllEntitiesState();

private:
	// Input event list.
	typedef std::vector<SInputEvent> InputEventsList;
	typedef std::vector<EntityEventRecord> EntityEventRecords;

	//! This sructure saved for every frame of time demo.
	struct FrameRecord
	{
		Vec3 playerPos;
		Ang3 cameraAngles;
		Quat playerRotation;
		float frameTime; // Immidiate frame rate, for this frame.
		
		// Snapshot of current processing command.
		unsigned int nActionFlags[2];
		float fLeaning;
		int nPolygons; // Polys rendered in this frame.

		InputEventsList inputEventsList;
		EntityEventRecords entityEvents;
	};
	typedef std::vector<FrameRecord> FrameRecords;
	FrameRecords m_records;

	bool m_bRecording;
	bool m_bPlaying;
	bool m_bPaused;
	bool m_bDemoFinished;

	//! Current play or record frame.
	int m_currentFrame;
	FrameRecords::iterator m_currFrameIter;

	std::vector<SInputEvent> m_currentFrameInputEvents;
	EntityEventRecords m_currentFrameEntityEvents;
	EntityEventRecords m_firstFrameEntityState;

	std::vector<string> m_demoLevels;
	int m_nCurrentDemoLevel;

	//////////////////////////////////////////////////////////////////////////
	// Old values of console vars.
	//////////////////////////////////////////////////////////////////////////
	float m_fixedTimeStep;
	float m_oldPeakTolerance;
	int m_prevGodMode;

	//! Timings.
	CTimeValue m_recordStartTime;
	CTimeValue m_recordEndTime;
	CTimeValue m_recordLastFrameTime;
	CTimeValue m_lastFrameTime;
	CTimeValue m_totalDemoTime;
	CTimeValue m_recordedDemoTime;

	// How many polygons per frame where recorded.
	int m_nTotalPolysRecorded;
	// How many polygons per frame where played.
	int m_nTotalPolysPlayed;
	
	float m_lastPlayedTotalTime;
	float m_lastAveFrameRate;
	float m_minFPS;
	float m_maxFPS;
	float m_currFPS;
	int m_minFPS_Frame;
	int m_maxFPS_Frame;

	int m_nCurrPolys;
	int m_nMaxPolys;
	int m_nMinPolys;
	int m_nPolysPerSec;
	int m_nPolysCounter;
	
	// For calculating current last second fps.
	CTimeValue m_lastFpsTimeRecorded;
	int m_fpsCounter;

	int m_numLoops;
	int m_maxLoops;

	int m_demo_scroll_pause;
	int m_demo_noinfo;
	int m_demo_quit;
	int m_demo_screenshot_frame;
	int m_demo_max_frames;
	int m_demo_savestats;
	int m_demo_ai;
	int m_demo_restart_level;
	int m_demo_panoramic;
	int m_demo_fixed_timestep;
	int m_demo_vtune;
	int m_demoProfile;
	int m_demo_time_of_day;

	bool m_bEnabledProfiling, m_bVisibleProfiling;

	string m_file;

	ISystem *m_pSystem;
	struct STimeDemoInfo *m_pTimeDemoInfo;

	static ICVar *s_timedemo_file;
	static CTimeDemoRecorder *s_pTimeDemoRecorder;
};

#endif // __timedemorecorder_h__
