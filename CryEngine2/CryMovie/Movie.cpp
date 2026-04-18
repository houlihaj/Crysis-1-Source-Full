////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   movie.cpp
//  Version:     v1.00
//  Created:     23/4/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Movie.h"
#include "AnimSplineTrack.h"
#include "AnimSequence.h"
#include "SequenceIt.h"
#include "EntityNode.h"
#include "CVarNode.h"
#include "ScriptVarNode.h"
#include "AnimCameraNode.h"
#include "SceneNode.h"
#include "MaterialNode.h"

#include <StlUtils.h>

#include <ISystem.h>
#include <ILog.h>
#include <IConsole.h>
#include <ITimer.h>
#include <IMusicSystem.h>

int CMovieSystem::m_mov_NoCutscenes = 0;

//////////////////////////////////////////////////////////////////////////
CMovieSystem::CMovieSystem( ISystem *system )
{
	m_system = system;
	m_bRecording = false;
	m_pCallback=NULL;
	m_pUser=NULL;
	m_bPaused = false;
	m_bCutscenesPausedInEditor = true;
	m_bLastFrameAnimateOnStop = true;
	m_nProceduralAnimation=1; 
	m_lastGenId = 1;
	m_sequenceStopBehavior = ONSTOP_GOTO_END_TIME;
	m_lastUpdateTime.SetValue(0);

	system->GetIConsole()->Register( "mov_NoCutscenes",&m_mov_NoCutscenes,0,0,"Disable playing of Cut-Scenes" );
}

//////////////////////////////////////////////////////////////////////////
CMovieSystem::~CMovieSystem()
{
}

//////////////////////////////////////////////////////////////////////////
bool CMovieSystem::Load(const char *pszFile, const char *pszMission)
{
	XmlNodeRef rootNode = m_system->LoadXmlFile(pszFile);
	if (!rootNode)
		return false;
	XmlNodeRef Node=NULL;
	for (int i=0;i<rootNode->getChildCount();i++)
	{
		XmlNodeRef missionNode=rootNode->getChild(i);
		XmlString sName;
		if (!(sName = missionNode->getAttr("Name")))
			continue;
		if (stricmp(sName.c_str(), pszMission))
			continue;
		Node=missionNode;
		break;
	}
	if (!Node)
		return false;
	Serialize(Node, true, true, false);
	return true;
}

//////////////////////////////////////////////////////////////////////////
IAnimNode* CMovieSystem::CreateNode( int nodeType,int nodeId )
{
	CAnimNode *node = NULL;
	if (!nodeId)
	{
		// Make uniq id.
		do {
			nodeId = m_lastGenId++;
		} while (GetNode(nodeId) != 0);
	}
	switch (nodeType)
	{
	case ANODE_ENTITY:
		node = new CAnimEntityNode(this);
		break;
	case ANODE_CAMERA:
		node = new CAnimCameraNode(this);
		break;
	case ANODE_CVAR:
		node = new CAnimCVarNode(this);
		break;
	case ANODE_SCRIPTVAR:
		node = new CAnimScriptVarNode(this);
		break;
	case ANODE_SCENE:
		node = new CAnimSceneNode(this);
		nodeId = 0;
		return node;
		break;
	case ANODE_MATERIAL:
		node = new CAnimMaterialNode(this);
		break;
	}
	if (node)
	{
		node->SetId(nodeId);
		m_nodes[nodeId] = node;
	}
	Callback( IMovieCallback::CBR_ADDNODE,node );
	return node;
}

//////////////////////////////////////////////////////////////////////////
IAnimTrack* CMovieSystem::CreateTrack( EAnimTrackType type )
{
	switch (type)
	{
	case ATRACK_TCB_FLOAT:
		return new CTcbFloatTrack;
	case ATRACK_TCB_VECTOR:
		return new CTcbVectorTrack;
	case ATRACK_TCB_QUAT:
		return new CTcbQuatTrack;
	case ATRACK_TCB_VECTOR4:
		return new CTcbVector4Track;
	};
	//ATRACK_TCB_FLOAT,
	//ATRACK_TCB_VECTOR,
	//ATRACK_TCB_QUAT,
	//ATRACK_BOOL,
	// Unknown type of track.
//	CLogFile::WriteLine( "Error: Requesting unknown type of animation track!" );
	assert(0);
	return 0;
}

void CMovieSystem::ChangeAnimNodeId( int nodeId,int newNodeId )
{
	if (nodeId == newNodeId)
		return;
	Nodes::iterator it = m_nodes.find(nodeId);
	if (it != m_nodes.end())
	{
		IAnimNode *node = GetNode( nodeId );
		((CAnimNode*)node)->SetId( newNodeId );
		m_nodes[newNodeId] = node;
		m_nodes.erase(it);
	}

}

//////////////////////////////////////////////////////////////////////////
IAnimSequence* CMovieSystem::CreateSequence( const char *sequenceName )
{
	IAnimSequence *seq = new CAnimSequence( this );
	seq->SetName( sequenceName );
	m_sequences.push_back( seq );
	return seq;
}

//////////////////////////////////////////////////////////////////////////
IAnimSequence* CMovieSystem::LoadSequence( const char *pszFilePath )
{
	XmlNodeRef sequenceNode = m_system->LoadXmlFile( pszFilePath );
	if (sequenceNode)
	{
		return LoadSequence( sequenceNode );
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
IAnimSequence* CMovieSystem::LoadSequence( XmlNodeRef &xmlNode, bool bLoadEmpty )
{
	IAnimSequence *seq = new CAnimSequence( this );
	seq->Serialize( xmlNode,true,bLoadEmpty );
	// Delete previous sequence with the same name.
	IAnimSequence *pPrevSeq = FindSequence( seq->GetName() );
	if (pPrevSeq)
		RemoveSequence( pPrevSeq );
	m_sequences.push_back( seq );
	return seq;
}

//////////////////////////////////////////////////////////////////////////
IAnimSequence* CMovieSystem::FindSequence( const char *sequence )
{
	for (Sequences::iterator it = m_sequences.begin(); it != m_sequences.end(); ++it)
	{
		IAnimSequence *seq = *it;
		if (stricmp(seq->GetName(),sequence) == 0)
		{
			return seq;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
ISequenceIt* CMovieSystem::GetSequences(bool bPlayingOnly/* =false */, bool bCutscenesOnly/* =false */)
{
	CSequenceIt *It=new CSequenceIt();
	if (bPlayingOnly == false)
	{
		for (Sequences::iterator it = m_sequences.begin(); it != m_sequences.end(); ++it)
		{
			IAnimSequence* pSeq = *it;
			if (!bCutscenesOnly || (pSeq->GetFlags() & IAnimSequence::CUT_SCENE))
				It->add( pSeq );
		}
	}
	else
	{
		for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); ++it)
		{
			IAnimSequence* pSeq = it->sequence;
			if (!bCutscenesOnly || (pSeq->GetFlags() & IAnimSequence::CUT_SCENE))
				It->add( pSeq );
		}
	}
	return It;
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::RemoveSequence( IAnimSequence *seq )
{
	assert( seq != 0 );
	if (seq)
	{
		IMovieCallback *pCallback=GetCallback();
		SetCallback(NULL);
		StopSequence(seq);

		for (Sequences::iterator it = m_sequences.begin(); it != m_sequences.end(); ++it)
		{
			if (seq == *it)
			{
				m_movieListenerMap.erase(seq);
				m_sequences.erase(it);
				break;
			}
		}
		SetCallback(pCallback);
	}
}

//////////////////////////////////////////////////////////////////////////
IAnimNode* CMovieSystem::GetNode( int nodeId ) const
{
	Nodes::const_iterator it = m_nodes.find(nodeId);
	if (it != m_nodes.end())
		return it->second;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IAnimNode* CMovieSystem::FindNode( const char *nodeName ) const
{
	for (Nodes::const_iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode *node = it->second;
		// Case insesentivy name comparasion.
		if (stricmp(node->GetName(),nodeName) == 0)
		{
			return node;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::RemoveNode( IAnimNode* node )
{
	assert( node != 0 );

	Callback( IMovieCallback::CBR_REMOVENODE,node );

	{
		// Remove this node from all sequences that reference this node.
		for (Sequences::iterator sit = m_sequences.begin(); sit != m_sequences.end(); ++sit)
		{
			(*sit)->RemoveNode( node );
		}
	}

	Nodes::iterator it = m_nodes.find(node->GetId());
	if (it != m_nodes.end())
		m_nodes.erase( it );
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::RemoveAllSequences()
{
	m_bLastFrameAnimateOnStop = false;
	IMovieCallback *pCallback=GetCallback();
	SetCallback(NULL);
	StopAllSequences();
	m_sequences.clear();
	m_movieListenerMap.clear();
	SetCallback(pCallback);
	m_bLastFrameAnimateOnStop = true;
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::RemoveAllNodes()
{
	m_bLastFrameAnimateOnStop = false;
	IMovieCallback *pCallback=GetCallback();
	SetCallback(NULL);
	StopAllSequences();
	m_nodes.clear();
	SetCallback(pCallback);
	m_bLastFrameAnimateOnStop = true;
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::SaveNodes(XmlNodeRef nodesNode)
{
	for (Nodes::iterator It=m_nodes.begin();It!=m_nodes.end();++It)
	{
		XmlNodeRef nodeNode=nodesNode->newChild("Node");
		IAnimNode *pNode=It->second;
		nodeNode->setAttr("Id", pNode->GetId());
		nodeNode->setAttr("Type", pNode->GetType());
		nodeNode->setAttr("Name", pNode->GetName());
		pNode->Serialize( nodeNode,false );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::PlaySequence( const char *sequenceName,IAnimSequence *parentSeq,bool bResetFx,bool bTrackedSequence,float currentTime,float startTime,float endTime )
{
	IAnimSequence *seq = FindSequence(sequenceName);
	if (seq)
	{ 
		PlaySequence(seq,parentSeq,bResetFx,bTrackedSequence,currentTime,startTime,endTime);
	}
	else
		GetSystem ()->GetILog()->Log ("CMovieSystem::PlaySequence: Error: Sequence \"%s\" not found", sequenceName);
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::PlaySequence( IAnimSequence *seq,IAnimSequence *parentSeq,bool bResetFx,bool bTrackedSequence,float currentTime,float startTime,float endTime )
{
	assert( seq != 0 );
	if (!seq || IsPlaying(seq))
		return;

	// disable procedural animations during cutscene as they are supposed
	// to be created by animators, store current status
	ICVar *pVar=GetSystem()->GetIConsole()->GetCVar("ca_eyes_procedural");
	if (pVar)
	{
		m_nProceduralAnimation=pVar->GetIVal();
		pVar->Set((int)(0));
	}

	if ((seq->GetFlags() & IAnimSequence::CUT_SCENE) || (seq->GetFlags() & IAnimSequence::NO_HUD))
	{
		// Dont play cut-scene if this console variable set.
		if (m_mov_NoCutscenes != 0)
			return;
	}

	//GetSystem ()->GetILog()->Log ("TEST: Playing Sequence (%s)", seq->GetName());

	seq->Activate();
	PlayingSequence ps;
	ps.sequence = seq;
	ps.startTime = startTime == -FLT_MAX ? seq->GetTimeRange().start : startTime;
	ps.currentTime = currentTime == -FLT_MAX ? ps.startTime : currentTime;
	ps.endTime = endTime == -FLT_MAX ? seq->GetTimeRange().end : endTime;
	ps.trackedSequence = bTrackedSequence;
	ps.syncWait = false;
	ps.syncMusic = false;
	ps.lastTimeUpdateTime = gEnv->pTimer->GetFrameStartTime();

	// If this sequence is cut scene disable player.
	if (seq->GetFlags() & IAnimSequence::CUT_SCENE)
	{
		seq->SetParentSequence(parentSeq);

		if (!gEnv->pSystem->IsEditorMode() || !m_bCutscenesPausedInEditor)
		{
			if (m_pUser)
			{
				m_pUser->BeginCutScene(seq, seq->GetCutSceneFlags(),bResetFx);
			}

			if (gEnv->pMusicSystem != NULL && seq->GetParentSequence() == NULL && strcmp(seq->GetMusicPatternName(),""))
			{
				gEnv->pMusicSystem->PlayPattern(seq->GetMusicPatternName(),false,false);
				gEnv->pMusicSystem->SeekLastPattern(seq->GetMusicPatternName(),ps.currentTime);
				ps.syncWait = true;
			}
		}
	}

	m_playingSequences.push_back(ps);

	// tell all interested listeners
	NotifyListeners(seq,IMovieListener::MOVIE_EVENT_START);
}

void CMovieSystem::NotifyListeners(IAnimSequence* pSequence, IMovieListener::EMovieEvent event)
{
	TMovieListenerMap::iterator found (m_movieListenerMap.find(pSequence));
	if (found == m_movieListenerMap.end()) return;
	TMovieListenerVec::iterator iter ((*found).second.begin());
	while (iter != (*found).second.end()) {
		(*iter)->OnMovieEvent(event, pSequence);
		++iter;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMovieSystem::StopSequence( const char *sequenceName )
{
	IAnimSequence *seq = FindSequence(sequenceName);
	if (seq)
		return StopSequence(seq);
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CMovieSystem::StopSequence( IAnimSequence *seq)
{
	return InternalStopSequence(seq, false);
}

//////////////////////////////////////////////////////////////////////////
bool CMovieSystem::InternalStopSequence( IAnimSequence *seq, bool bIsAbort /* = false */ )
{
	assert( seq != 0 );
	bool bRet = false;
	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); ++it)
	{
		if (it->sequence == seq)
		{
			m_playingSequences.erase( it );

			if (m_bLastFrameAnimateOnStop)
			{
				if (m_sequenceStopBehavior == ONSTOP_GOTO_END_TIME)
				{
					SAnimContext ac;
					ac.bSingleFrame = true;
					ac.time = seq->GetTimeRange().end;
					seq->Animate(ac);
				}
				else if (m_sequenceStopBehavior == ONSTOP_GOTO_START_TIME)
				{
					SAnimContext ac;
					ac.bSingleFrame = true;
					ac.time = seq->GetTimeRange().start;
					seq->Animate(ac);
				}
				seq->Deactivate();
			}
			
			// If this sequence is cut scene end it.
			if (seq->GetFlags() & IAnimSequence::CUT_SCENE)
			{
				/* AlexL: disabled until clarified with Timur
				// if it's a cutscene and it should not be animated to the end, deactivate it anyway
				if (m_bLastFrameAnimateOnStop == false)
				{
					seq->Deactivate();
				}
				*/

				if (!gEnv->pSystem->IsEditorMode() || !m_bCutscenesPausedInEditor)
				{
					if (m_pUser)
					{
						m_pUser->EndCutScene(seq, seq->GetCutSceneFlags(true));
					}

					if (gEnv->pMusicSystem != NULL && seq->GetParentSequence() == NULL && strcmp(seq->GetMusicPatternName(),""))
					{
						gEnv->pMusicSystem->StopLastPattern(seq->GetMusicPatternName());
					}
				}

				seq->SetParentSequence(NULL);
			}

			// tell all interested listeners
			NotifyListeners(seq, bIsAbort ? IMovieListener::MOVIE_EVENT_ABORTED : IMovieListener::MOVIE_EVENT_STOP);

			bRet = true;
			break;
		}
	}

	// restore procedural animations after cutscene is done	
	ICVar *pVar=GetSystem()->GetIConsole()->GetCVar("ca_eyes_procedural");
	if (pVar)
		pVar->Set((int)(1));

	return bRet;
}

//////////////////////////////////////////////////////////////////////////
bool CMovieSystem::AbortSequence(IAnimSequence *seq, bool bLeaveTime/* =false  */)
{
	assert( seq != 0 );

	// check if it can be aborted
	if (seq->GetCutSceneFlags() & IAnimSequence::NO_ABORT)
		return false;

	m_bLastFrameAnimateOnStop = !bLeaveTime;
	bool bAborted = InternalStopSequence(seq, true);
	m_bLastFrameAnimateOnStop = true;
	return bAborted;
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::StopAllSequences()
{
	while (!m_playingSequences.empty())
	{
		StopSequence( m_playingSequences.begin()->sequence );
	}
	m_playingSequences.clear();
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::StopAllCutScenes()
{
	bool bAnyStoped;
	PlayingSequences::iterator next;
	do {
		bAnyStoped = false;
		for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); it = next)
		{
			next = it; ++next;
			IAnimSequence *seq = it->sequence;
			if (seq->GetFlags() & IAnimSequence::CUT_SCENE)
			{
				bAnyStoped = true;
				StopSequence( seq );
				break;
			}
		}
	} while (bAnyStoped);
}

//////////////////////////////////////////////////////////////////////////
bool CMovieSystem::IsPlaying( IAnimSequence *seq ) const
{
	for (PlayingSequences::const_iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); ++it)
	{
		if (it->sequence == seq)
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::Reset( bool bPlayOnReset,bool bSeekToStart )
{
	m_bLastFrameAnimateOnStop = false;
	StopAllSequences();
	m_bLastFrameAnimateOnStop = true;

	// Reset all sequences.
	for (Sequences::iterator sit = m_sequences.begin(); sit != m_sequences.end(); ++sit)
	{
		IAnimSequence *seq = *sit;
		seq->Reset(bSeekToStart);
	}

	// Reset all nodes.
	for (Nodes::const_iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode *node = it->second;
		node->Reset();
	}

	// Force end Cut-Scene on the reset.
/*	if (m_pUser)	// lennert why is this here ??? if there was a cutscene playing it will be stopped above...
	{
		m_pUser->EndCutScene();
	}*/

	if (bPlayOnReset)
	{
		for (Sequences::iterator sit = m_sequences.begin(); sit != m_sequences.end(); ++sit)
		{
			IAnimSequence *seq = *sit;
			if (seq->GetFlags() & IAnimSequence::PLAY_ONRESET)
				PlaySequence(seq);
		}
	}

	// unpause the moviesystem
	m_bPaused = false;

	// Reset camera.
	SCameraParams CamParams=GetCameraParams();
	CamParams.cameraEntityId = 0;
	CamParams.fFOV=0;
	CamParams.justActivated = true;
	SetCameraParams(CamParams);
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::PlayOnLoadSequences()
{
	for (Sequences::iterator sit = m_sequences.begin(); sit != m_sequences.end(); ++sit)
	{
		IAnimSequence *seq = *sit;
		if (seq->GetFlags() & IAnimSequence::PLAY_ONRESET)
			PlaySequence(seq);
	}

	// Reset camera.
	SCameraParams CamParams=GetCameraParams();
	CamParams.cameraEntityId = 0;
	CamParams.fFOV=0;
	CamParams.justActivated = true;
	SetCameraParams(CamParams);
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::SyncUpdate()
{
	if (!gEnv->bEditor)
		return;

	SAnimContext ac;
	float fps = 60;

	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); ++it)
	{
		PlayingSequence &ps = *it;

		ac.time = ps.currentTime;
		ac.sequence = ps.sequence;
		ac.dt = 0;
		ac.fps = fps;

		ps.sequence->SyncUpdate(eSUT_NonPlaying,ac);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::Update( float dt )
{
	if (m_bPaused)
		return;

	// don't update more than once if dt==0.0
	CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();
	if (dt == 0.0f && curTime == m_lastUpdateTime && !gEnv->bEditor)
		return;
	m_lastUpdateTime = curTime;

	SAnimContext ac;
	float fps = 60;

	std::vector<IAnimSequence*> stopSequences;

	// cap delta time.
	//dt = MAX( 0,MIN(2.0f,dt) );

	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); ++it)
	{
		PlayingSequence &ps = *it;
		bool bJustSynced = false;

		int nSeqFlags = ps.sequence->GetFlags();
		if ((nSeqFlags & IAnimSequence::CUT_SCENE) && m_mov_NoCutscenes != 0)
		{
			// Don't play cut-scene if no cutscenes console variable set.
			stopSequences.push_back(ps.sequence);
			break;
		}

		if (nSeqFlags & IAnimSequence::CUT_SCENE)
		{
			if (ps.sequence->GetParentSequence() == NULL)
			{
				if (gEnv->pMusicSystem != NULL)
				{
					if (!gEnv->pMusicSystem->GetStatus()->bPlaying)
					{
						ps.syncMusic = true;
					}
					else if (ps.syncMusic == true)
					{
						gEnv->pMusicSystem->SeekLastPattern(ps.sequence->GetMusicPatternName(),ps.currentTime);

						ps.syncMusic = false;
						ps.syncWait = true;
					}
				}

				if (ps.syncWait)
				{
					if (gEnv->pMusicSystem != NULL)
					{
						CTimeValue startTime;

						if (gEnv->pMusicSystem->GetLastPatternStartTime(ps.sequence->GetMusicPatternName(),startTime))
						{
							if (startTime.GetValue() != 0 && (gEnv->pTimer->GetAsyncTime()-startTime).GetSeconds() > ps.currentTime)
							{
								ps.currentTime = (gEnv->pTimer->GetAsyncTime()-startTime).GetSeconds();
								ps.syncWait = false;
								ps.lastTimeUpdateTime = curTime;
								bJustSynced = true;
							}
							else
							{
								ps.sequence->SyncUpdate(eSUT_SyncParentWait,ac);
								continue;
							}
						}
						else
						{
							ps.syncWait = false;
						}
					}
					else
					{
						ps.syncWait = false;
					}
				}
			}
			else if (ps.syncWait)
			{
				continue;
			}
		}
		else
		{
			ps.syncWait = false;
		}

		if (ps.lastTimeUpdateTime != curTime)
		{
			ps.currentTime += dt;
			ps.lastTimeUpdateTime = curTime;
		}

		ac.time = ps.currentTime;
		ac.sequence = ps.sequence;
		ac.dt = dt;
		ac.fps = fps;

		// Check time out of range.
		if (ps.currentTime > ps.endTime)
		{
			int seqFlags = ps.sequence->GetFlags();
			if (seqFlags & IAnimSequence::ORT_LOOP)
			{
				// Time wrap's back to the start of the time range.
				ps.currentTime = ps.startTime;
			}
			else if (seqFlags & IAnimSequence::ORT_CONSTANT)
			{
				// Time just continues normally past the end of time range.
			}
			else
			{
				// If no out-of-range type specified sequence stopped when time reaches end of range.
				// Que sequence for stopping.
				if (ps.trackedSequence == false)
				{
				stopSequences.push_back(ps.sequence);
				}
				continue;
			}
		}

		// Animate sequence. (Can invalidate iterator)
		ps.sequence->Animate( ac );

		if (bJustSynced)
		{
			ps.sequence->SyncUpdate(eSUT_SyncParent,ac);
		}
	}

	// Stop queued sequences.
	for (int i = 0; i < (int)stopSequences.size(); i++)
	{
		StopSequence( stopSequences[i] );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::Callback( IMovieCallback::ECallbackReason Reason,IAnimNode *pNode )
{
	if (m_pCallback)
		m_pCallback->OnMovieCallback( Reason,pNode );
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::Serialize( XmlNodeRef &xmlNode,bool bLoading,bool bRemoveOldNodes,bool bLoadEmpty )
{
	if (bLoading)
	{
		//////////////////////////////////////////////////////////////////////////
		// Load sequences from XML.
		//////////////////////////////////////////////////////////////////////////
		XmlNodeRef seqNode=xmlNode->findChild("SequenceData");
		if (seqNode)
		{
			RemoveAllSequences();
			if (bRemoveOldNodes)
			{
				RemoveAllNodes();
			}

			for (int i=0;i<seqNode->getChildCount();i++)
			{
				XmlNodeRef childNode = seqNode->getChild(i);
				if (!LoadSequence(childNode, bLoadEmpty))
					return;
			}
		}
	}else
	{
		XmlNodeRef sequencesNode=xmlNode->newChild("SequenceData");
		ISequenceIt *It=GetSequences();
		IAnimSequence *seq=It->first();;
		while (seq)
		{
			XmlNodeRef sequenceNode=sequencesNode->newChild("Sequence");
			seq->Serialize(sequenceNode, false);
			seq=It->next();
		}
		It->Release();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::SetCameraParams( const SCameraParams &Params )
{
	m_ActiveCameraParams = Params;
	if (m_pUser)
		m_pUser->SetActiveCamera(m_ActiveCameraParams);
	if (m_pCallback)
		m_pCallback->OnSetCamera( m_ActiveCameraParams );
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::SendGlobalEvent( const char *pszEvent )
{
	if (m_pUser)
		m_pUser->SendGlobalEvent(pszEvent);
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::Pause()
{
	if (m_bPaused)
		return;
	m_bPaused = true;

	/*
	PlayingSequences::iterator next;
	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); it = next)
	{
		next = it; ++next;
		PlayingSequence &ps = *it;
		ps.sequence->Pause();
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::Resume()
{
	if (!m_bPaused)
		return;

	m_bPaused = false;

	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); ++it)
	{
		PlayingSequence &ps = *it;

		int nSeqFlags = ps.sequence->GetFlags();

		if ((nSeqFlags & IAnimSequence::CUT_SCENE) && ps.sequence->GetParentSequence() == NULL && gEnv->pMusicSystem != NULL)
		{
			if (strcmp(ps.sequence->GetMusicPatternName(),""))
			{
				ps.syncWait = true;
			}
		}
	}

	/*
	PlayingSequences::iterator next;
	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); it = next)
	{
		next = it; ++next;
		PlayingSequence &ps = *it;
		ps.sequence->Resume();
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::PauseCutScenes()
{
	m_bCutscenesPausedInEditor = true;

	if (m_pUser != NULL)
	{
		for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); ++it)
		{
			if (it->sequence->GetFlags() & IAnimSequence::CUT_SCENE)
			{
				m_pUser->EndCutScene(it->sequence,it->sequence->GetCutSceneFlags(true));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::ResumeCutScenes()
{
	if (m_mov_NoCutscenes != 0)
		return;

	m_bCutscenesPausedInEditor = false;

	if (m_pUser != NULL)
	{
		for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); ++it)
		{
			if (it->sequence->GetFlags() & IAnimSequence::CUT_SCENE)
			{
				m_pUser->BeginCutScene(it->sequence,it->sequence->GetCutSceneFlags(),true);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::OnPlaySound( IAnimSequence* pSeq, ISound *pSound )
{
	if (pSeq->GetFlags() & IAnimSequence::CUT_SCENE)
	{
		if (m_pUser)
			m_pUser->PlaySubtitles( pSeq, pSound );
	}
}

float CMovieSystem::GetPlayingTime(IAnimSequence * pSeq)
{
	if (!pSeq)
		return -1;

	if (!IsPlaying(pSeq))
		return -1;

	PlayingSequences::const_iterator itend = m_playingSequences.end();
	for (PlayingSequences::const_iterator it = m_playingSequences.begin(); it != itend; ++it)
	{
		if (it->sequence == pSeq)
			return it->currentTime;
	}

	return -1;
}

bool CMovieSystem::SetPlayingTime(IAnimSequence * pSeq, float fTime)
{
	if (!pSeq)
		return false;

	if (!IsPlaying(pSeq))
		return false;

	PlayingSequences::iterator itend = m_playingSequences.end();
	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != itend; ++it)
	{
		if (it->sequence == pSeq && !(pSeq->GetFlags() & IAnimSequence::NO_SEEK))
		{
			it->currentTime = fTime;
			it->lastTimeUpdateTime = gEnv->pTimer->GetFrameStartTime();
		}
	}


	return false;
}

bool CMovieSystem::GetStartEndTime(IAnimSequence *pSeq,float &fStartTime,float &fEndTime)
{
	fStartTime = 0.0f;
	fEndTime = 0.0f;

	if (!pSeq)
		return false;

	if (!IsPlaying(pSeq))
		return false;

	PlayingSequences::iterator itend = m_playingSequences.end();
	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != itend; ++it)
	{
		if (it->sequence == pSeq)
		{
			fStartTime = it->startTime;
			fEndTime = it->endTime;
		}
	}

	return false;
}

bool CMovieSystem::SetStartEndTime(IAnimSequence *pSeq,const float fStartTime,const float fEndTime)
{
	if (!pSeq)
		return false;

	if (!IsPlaying(pSeq))
		return false;

	PlayingSequences::iterator itend = m_playingSequences.end();
	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != itend; ++it)
	{
		if (it->sequence == pSeq)
		{
			it->startTime = fStartTime;
			it->endTime = fEndTime;
		}
	}

	return false;
}

bool CMovieSystem::SyncSequence(IAnimSequence *pSeq,ESyncUpdateType syncUpdateType,float fTime)
{
	if (!pSeq)
		return false;

	PlayingSequences::iterator itend = m_playingSequences.end();
	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != itend; ++it)
	{
		if (it->sequence == pSeq)
		{
			switch (syncUpdateType)
			{
				case eSUT_SyncParentWait:
					it->syncWait = true;
					it->lastTimeUpdateTime = gEnv->pTimer->GetFrameStartTime();
					break;
				case eSUT_SyncParent:
					it->syncWait = false;
					it->currentTime = fTime;
					it->lastTimeUpdateTime = gEnv->pTimer->GetFrameStartTime();
					break;
			}

			return true;
		}
	}

	return false;
}

void CMovieSystem::SetSequenceStopBehavior( ESequenceStopBehavior behavior )
{
	m_sequenceStopBehavior = behavior;
}

bool CMovieSystem::AddMovieListener(IAnimSequence* pSequence, IMovieListener* pListener)
{
	assert (pSequence != 0);
	assert (pListener != 0);
	if (std::find(m_sequences.begin(), m_sequences.end(), pSequence) == m_sequences.end())
	{
		GetSystem ()->GetILog()->Log ("CMovieSystem::AddMovieListener: Sequence %p unknown to CMovieSystem", pSequence);
		return false;
	}
	return stl::push_back_unique(m_movieListenerMap[pSequence], pListener);
}

bool CMovieSystem::RemoveMovieListener(IAnimSequence* pSequence, IMovieListener* pListener)
{
	assert (pSequence != 0);
	assert (pListener != 0);
	if (std::find(m_sequences.begin(), m_sequences.end(), pSequence) == m_sequences.end())
	{
		GetSystem ()->GetILog()->Log ("CMovieSystem::AddMovieListener: Sequence %p unknown to CMovieSystem", pSequence);
		return false;
	}
	return stl::find_and_erase(m_movieListenerMap[pSequence], pListener);
}
