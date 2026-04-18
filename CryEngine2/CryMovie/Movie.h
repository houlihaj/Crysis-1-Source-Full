////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   movie.h
//  Version:     v1.00
//  Created:     23/4/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __movie_h__
#define __movie_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "IMovieSystem.h"

/**	This is descirption of currently playing sequence.
*/
struct PlayingSequence
{
	//! Sequence playnig.
	_smart_ptr<IAnimSequence> sequence;
	//! Start/End/Current playing time for this sequence.
	float startTime;
	float endTime;
	float currentTime;
	//! Sequence from other sequence's sequence track
	bool trackedSequence;
	//! Sync sequence with music or parent
	bool syncWait;
	//! Sync music with sequence
	bool syncMusic;
	//! Last time update time;
	CTimeValue lastTimeUpdateTime;
};

struct string_less : public std::binary_function<string,string,bool> 
{
	bool operator()( const string &s1,const string &s2 ) const
	{
		return stricmp(s1.c_str(),s2.c_str()) < 0;
	}
};


class CMovieSystem : public IMovieSystem
{
public:
	CMovieSystem( ISystem *system );
	~CMovieSystem();

	void Release() { delete this; };

	void SetUser(IMovieUser *pUser) { m_pUser=pUser; }
	IMovieUser* GetUser() { return m_pUser; }

	bool Load(const char *pszFile, const char *pszMission);

	ISystem* GetSystem() { return m_system; }

	IAnimNode* CreateNode( int nodeType,int nodeId=0 );
	IAnimTrack* CreateTrack( EAnimTrackType type );

	void ChangeAnimNodeId( int nodeId,int newNodeId );

	IAnimSequence* CreateSequence( const char *sequence );
	IAnimSequence* LoadSequence( const char *pszFilePath );
	IAnimSequence* LoadSequence( XmlNodeRef &xmlNode, bool bLoadEmpty=true );

	void RemoveSequence( IAnimSequence *seq );
	IAnimSequence* FindSequence( const char *sequence );
	ISequenceIt* GetSequences(bool bPlayingOnly=false, bool bCutscenesOnly=false);

	bool AddMovieListener(IAnimSequence* pSequence, IMovieListener* pListener);
	bool RemoveMovieListener(IAnimSequence* pSequence, IMovieListener* pListener);

	void RemoveAllSequences();
	void RemoveAllNodes();

	void SaveNodes(XmlNodeRef nodesNode);

	//////////////////////////////////////////////////////////////////////////
	// Sequence playback.
	//////////////////////////////////////////////////////////////////////////
	void PlaySequence( const char *sequence,IAnimSequence *parentSeq=NULL,bool bResetFX=true,bool bTrackedSequence=false,float currentTime = -FLT_MAX,float startTime = -FLT_MAX,float endTime = -FLT_MAX );
	void PlaySequence( IAnimSequence *seq,IAnimSequence *parentSeq=NULL,bool bResetFX=true,bool bTrackedSequence=false,float currentTime = -FLT_MAX,float startTime = -FLT_MAX,float endTime = -FLT_MAX );
	void PlayOnLoadSequences();

	bool StopSequence( const char *sequence );
	bool StopSequence( IAnimSequence *seq);
	bool AbortSequence(IAnimSequence *seq, bool bLeaveTime=false);

	void StopAllSequences();
	void StopAllCutScenes();
	void Pause( bool bPause );

	void Reset( bool bPlayOnReset,bool bSeekAllToStart );
	void SyncUpdate();
	void Update( float dt );

	bool IsPlaying( IAnimSequence *seq ) const;

	virtual void Pause();
	virtual void Resume();

	virtual void PauseCutScenes();
	virtual void ResumeCutScenes();

	//////////////////////////////////////////////////////////////////////////
	// Nodes.
	//////////////////////////////////////////////////////////////////////////
	void RemoveNode( IAnimNode* node );
	IAnimNode* GetNode( int nodeId ) const;
	IAnimNode* FindNode( const char *nodeName ) const;

	void SetRecording( bool recording ) { m_bRecording = recording; };
	bool IsRecording() const { return m_bRecording; };

	void SetCallback( IMovieCallback *pCallback ) { m_pCallback=pCallback; }
	IMovieCallback* GetCallback() { return m_pCallback; }
	void Callback( IMovieCallback::ECallbackReason Reason,IAnimNode *pNode );

	void Serialize( XmlNodeRef &xmlNode,bool bLoading, bool bRemoveOldNodes=false, bool bLoadEmpty=true );

	const SCameraParams& GetCameraParams() const { return m_ActiveCameraParams; }
	void SetCameraParams( const SCameraParams &Params );

	void SendGlobalEvent( const char *pszEvent );
	void OnPlaySound( IAnimSequence* pSeq, ISound *pSound );
	void SetSequenceStopBehavior( ESequenceStopBehavior behavior );

	float GetPlayingTime(IAnimSequence * pSeq);
	bool SetPlayingTime(IAnimSequence * pSeq, float fTime);

	bool GetStartEndTime(IAnimSequence *pSeq,float &fStartTime,float &fEndTime);
	bool SetStartEndTime(IAnimSequence *pSeq,const float fStartTime,const float fEndTime);

	bool SyncSequence(IAnimSequence *pSeq,ESyncUpdateType syncUpdateType,float fTime);

protected:
	void NotifyListeners(IAnimSequence* pSequence, IMovieListener::EMovieEvent event);
	bool InternalStopSequence( IAnimSequence *seq, bool bIsAbort=false);

private:
	ISystem*	m_system;

	IMovieUser *m_pUser;
	IMovieCallback *m_pCallback;

	CTimeValue m_lastUpdateTime;
	int m_lastGenId;

	typedef std::vector<_smart_ptr<IAnimSequence> > Sequences;
	Sequences m_sequences;

	typedef std::map<int,_smart_ptr<IAnimNode> > Nodes;
	Nodes m_nodes;

	typedef std::list<PlayingSequence> PlayingSequences;
	PlayingSequences m_playingSequences;

	typedef std::vector<IMovieListener*> TMovieListenerVec;
	typedef std::map<IAnimSequence*, TMovieListenerVec> TMovieListenerMap;

	// a container which maps sequences to all interested listeners
	// listeners is a vector (could be a set in case we have a lot of listeners, stl::push_back_unique!)
	TMovieListenerMap m_movieListenerMap;

	bool	m_bRecording;
	bool	m_bPaused;
	bool  m_bCutscenesPausedInEditor;
	int		m_nProceduralAnimation;

	bool m_bLastFrameAnimateOnStop;

	SCameraParams m_ActiveCameraParams;

	ESequenceStopBehavior m_sequenceStopBehavior;

	static int m_mov_NoCutscenes;
};

#endif // __movie_h__
