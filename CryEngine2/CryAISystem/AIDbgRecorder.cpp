/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   AIDbgRecorder.cpp
	$Id$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 1:7:2005   14:31 : Created by Kirill Bulatsev

*********************************************************************/

#include "StdAfx.h"
#include <ILog.h>
#include <IAgent.h>
#include <IConsole.h>
#include <IEntitySystem.h>
#include "CAISystem.h"
#include "AIDbgRecorder.h"
#include "PipeUser.h"
#include <memory>

// Old AI log
#define	AIRECORDER_FILENAME	"AILog.log"

#define	AIRECORDER_SECONDARYFILENAME	"AISignals.csv"

// New AI recorder
#define AIRECORDER_RCD_FILE "AIRECORD.rcd"
#define AIRECORDER_VERSION 2

// We use some magic numbers to help when debugging the save structure
#define RECORDER_MAGIC	0xAA1234BB
#define UNIT_MAGIC			0xCC9876DD
#define EVENT_MAGIC			0xEE0921FF

// Anything larger than this is assumed to be a corrupt file
#define MAX_NAME_LENGTH 1024 
#define MAX_UNITS				10024
#define MAX_EVENT_STRING_LENGTH 1024


FILE * CAIRecorder::m_pFile = NULL; // Hack!
ICVar * CAIRecorder::m_cvDebugRecord = NULL;  // Hack!

struct RecorderFileHeader
{
	int	version;
	int	unitsNumber;
	int magic;

	RecorderFileHeader() : magic(RECORDER_MAGIC) {}
	bool check(void) { return RECORDER_MAGIC == magic && unitsNumber < MAX_UNITS; }

};

struct UnitHeader
{
	int nameLength;
	int ID;
	int magic;
	// And the variable length name follows this
	UnitHeader() : magic(UNIT_MAGIC) {}
	bool check(void) { return (UNIT_MAGIC == magic && nameLength < MAX_NAME_LENGTH); }
};

struct StreamedEventHeader
{
	int unitID;
	int streamID;
	int magic;

	StreamedEventHeader() : magic(EVENT_MAGIC) {}
	bool check(void) { return EVENT_MAGIC == magic; }

};



char	CAIDbgRecorder::m_BufferString[BUFFER_SIZE];
//
//----------------------------------------------------------------------------------------------
CAIDbgRecorder::CAIDbgRecorder()
{
	if(!GetISystem()->IsDevMode())
	{
		m_pFile = NULL;
		m_pFileSecondary = NULL;
		m_bNeedToFlush = false;
		return;
	}
	string filename = gEnv->pSystem->GetRootFolder();
	filename += AIRECORDER_FILENAME;
	m_pFile = fxopen(filename.c_str(),"wt");
#ifdef AI_LOG_SIGNALS
	string filenamesec = gEnv->pSystem->GetRootFolder();
	filenamesec += AIRECORDER_SECONDARYFILENAME;
	m_pFileSecondary = fxopen(filenamesec.c_str(),"wt");
	LogStringSecondary("Function,Time,Page faults,Entity\n");
#endif
	m_bNeedToFlush = true;
}

//
//----------------------------------------------------------------------------------------------
CAIDbgRecorder::~CAIDbgRecorder()
{
	if(m_pFile)
		fclose(m_pFile);
#ifdef AI_LOG_SIGNALS
	if(m_pFileSecondary)
		fclose(m_pFileSecondary);
#endif
}

//
//----------------------------------------------------------------------------------------------
void CAIDbgRecorder::FlushLog()
{
	if(!m_pFile)
		return;
  if (m_bNeedToFlush)
	{
    fflush(m_pFile);
#ifdef AI_LOG_SIGNALS
		if(m_pFileSecondary)
			fflush(m_pFileSecondary);
#endif
	}
  m_bNeedToFlush = false;
}

//
//----------------------------------------------------------------------------------------------
void	CAIDbgRecorder::LogString(const char* pString) const
{
	if(!m_pFile)
		return;
	bool	mergeWithLog(GetAISystem()->m_cvRecordLog->GetIVal()!=0);
	if(!mergeWithLog)
	{
		fputs(pString, m_pFile);
		m_bNeedToFlush = true;
	}
	else
		GetAISystem()->LogEvent("<AIrec>", pString );
}

//
//----------------------------------------------------------------------------------------------
#ifdef AI_LOG_SIGNALS
void	CAIDbgRecorder::LogStringSecondary(const char* pString) const
{
	if(!m_pFileSecondary)
		return;
	fputs(pString, m_pFileSecondary);
	m_bNeedToFlush = true;
}
#endif
//
//----------------------------------------------------------------------------------------------
bool CAIDbgRecorder::IsRecording( const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event ) const
{
	if ( !m_pFile )
		return false;

	if ( event == IAIRecordable::E_RESET )
		return true;

	if ( !pTarget )
		return false;

	return !strcmp( GetAISystem()->m_cvStatsTarget->GetString(), "all" )
		|| !strcmp( GetAISystem()->m_cvStatsTarget->GetString(), pTarget->GetName() );
}

//
//----------------------------------------------------------------------------------------------
void CAIDbgRecorder::Record( const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event, const char* pString ) const
{
	if(!m_pFile)
		return;

	if(event == IAIRecordable::E_RESET)
	{
		LogString("\n\n--------------------------------------------------------------------------------------------\n");
		LogString("<RESETTING AI SYSTEM>\n");
		LogString("aiTick:startTime:<entity> ai_event    details\n");
		LogString("--------------------------------------------------------------------------------------------\n");
		return;
	}
#ifdef AI_LOG_SIGNALS
	else	if(event==IAIRecordable::E_SIGNALEXECUTEDWARNING)
	{
		sprintf(m_BufferString,"%s\n",pString);
		LogStringSecondary( m_BufferString );
		return;
	}
	else
#endif

	if(!pTarget)
		return;
	if(	strcmp(GetAISystem()->m_cvStatsTarget->GetString(), "all") &&
			strcmp(GetAISystem()->m_cvStatsTarget->GetString(), pTarget->GetName()))
		return;
	const char* pEventString("");
	switch( event )
	{
	case	IAIRecordable::E_SIGNALRECIEVED:
		pEventString = "signal_recieved      ";
		break;
	case	IAIRecordable::E_SIGNALRECIEVEDAUX:
		pEventString = "auxsignal_recieved   ";
		break;
	case	IAIRecordable::E_SIGNALEXECUTING:
		pEventString = "signal_executing     ";
		break;
	case	IAIRecordable::E_GOALPIPEINSERTED:
		pEventString = "goalpipe_inserted    ";
		break;
	case	IAIRecordable::E_GOALPIPESELECTED:
		pEventString = "goalpipe_selected    ";
		break;
	case	IAIRecordable::E_GOALPIPERESETED:
		pEventString = "goalpipe_reseted     ";
		break;
	case	IAIRecordable::E_BEHAVIORSELECTED:
		pEventString = "behaviour_selected   ";
		break;
	case	IAIRecordable::E_BEHAVIORDESTRUCTOR:
		pEventString = "behaviour_destructor ";
		break;
	case	IAIRecordable::E_BEHAVIORCONSTRUCTOR:
		pEventString = "behaviour_constructor";
		break;
	case	IAIRecordable::E_ATTENTIONTARGET:
		pEventString = "atttarget_change     ";
		break;
	case	IAIRecordable::E_HANDLERNEVENT:
		pEventString = "handler_event        ";
		break;
	case	IAIRecordable::E_ACTIONSTART:
		pEventString = "action_start         ";
		break;
	case	IAIRecordable::E_ACTIONSUSPEND:
		pEventString = "action_suspend       ";
		break;
	case	IAIRecordable::E_ACTIONRESUME:
		pEventString = "action_resume        ";
		break;
	case	IAIRecordable::E_ACTIONEND:
		pEventString = "action_end           ";
		break;
	case	IAIRecordable::E_REFPOINTPOS:
		pEventString = "refpoint_pos         ";
		break;
	case	IAIRecordable::E_EVENT:
		pEventString = "triggering event     ";
		break;
	case	IAIRecordable::E_LUACOMMENT:
		pEventString = "lua comment          ";
		break;
//	case	IAIRecordable::E_SIGNALEXECUTEDWARNING:
		//pEventString = "";
//		break;
	default:
		pEventString = "undefined            ";
		break;
	}
 
	// Fetch the current AI tick and the time that tick started
	int frame = GetAISystem()->GetAITickCount();
	float time = GetAISystem()->GetFrameStartTime().GetSeconds();

	if(!pString)
		sprintf(m_BufferString,"%6d:%8.3f: <%s> %s\t\t\t<null>\n",frame,time,pTarget->GetName(), pEventString);
	else 
		sprintf(m_BufferString,"%6d:%8.3f: <%s> %s\t\t\t%s\n",frame,time,pTarget->GetName(), pEventString, pString);
	LogString( m_BufferString );
}

CAIRecorder *CRecordable::s_pRecorder(NULL);



//----------------------------------------------------------------------------------------------
void	CRecorderUnit::RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData)
{
	if (!CAIRecorder::IsEnabled()) return;

	float time = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds();

	if (CAIRecorder::m_pFile) //(Hack!)
	{
		bool bSuccess;
		StreamedEventHeader header;
		header.unitID = m_unitID;
		header.streamID = event;
		bSuccess = (fwrite(&header, sizeof(header), 1, CAIRecorder::m_pFile) == 1);
		// We go through the stream to get the right virtual function

		bSuccess &=	m_Streams[event]->WriteValue( pEventData, time);
		if (!bSuccess) {
			GetISystem()->GetILog()->LogError("[AI Recorder] Failed to write stream event");
		}
	}
	else
	{
		m_Streams[event]->AddValue(pEventData, time);
	}
}

//----------------------------------------------------------------------------------------------
bool CRecorderUnit::LoadEvent( IAIRecordable::e_AIDbgEvent stream )
{
	if (CAIRecorder::m_pFile) //(Hack!)
		return m_Streams[stream]->LoadValue( CAIRecorder::m_pFile );
	return true;
}

//
//----------------------------------------------------------------------------------------------
CRecordable::CRecordable():
m_pMyRecord(NULL)
{
}


//
//----------------------------------------------------------------------------------------------
CRecorderUnit::CRecorderUnit( EntityId unitID )
{
	m_unitID = unitID;
	m_Streams[IAIRecordable::E_RESET] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_SIGNALRECIEVED] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_SIGNALRECIEVEDAUX] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_SIGNALEXECUTING] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_GOALPIPESELECTED] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_GOALPIPEINSERTED] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_GOALPIPERESETED] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_BEHAVIORSELECTED] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_BEHAVIORDESTRUCTOR] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_BEHAVIORCONSTRUCTOR] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_ATTENTIONTARGET] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_ATTENTIONTARGETPOS] = new CRecorderUnit::StreamVec3;
	m_Streams[IAIRecordable::E_HANDLERNEVENT] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_ACTIONSUSPEND] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_ACTIONRESUME] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_ACTIONEND] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_EVENT] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_REFPOINTPOS] = new CRecorderUnit::StreamVec3;
	m_Streams[IAIRecordable::E_AGENTPOS] = new CRecorderUnit::StreamVec3;
	m_Streams[IAIRecordable::E_LUACOMMENT] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_HEALTH] = new CRecorderUnit::StreamFloat;
	m_Streams[IAIRecordable::E_HIT_DAMAGE] = new CRecorderUnit::StreamStr;
	m_Streams[IAIRecordable::E_DEATH] = new CRecorderUnit::StreamStr;
}

//
//----------------------------------------------------------------------------------------------
CRecorderUnit::~CRecorderUnit()
{
	for(t_StreamMap::iterator strItr(m_Streams.begin()); strItr != m_Streams.end(); ++strItr)
		delete strItr->second;
	m_Streams.clear();
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::StreamBase::Seek(float whereTo)
{

	m_CurIdx=0;
	if(whereTo<0.f)
		return;

	while( m_CurIdx<m_Stream.size() && m_Stream[m_CurIdx]->m_StartTime<=whereTo)
		++m_CurIdx;
//	m_CurIdx = max(0, m_CurIdx-1);
	--m_CurIdx;
}

//
//----------------------------------------------------------------------------------------------
int CRecorderUnit::StreamBase::GetCurrentIdx()
{
//	return m_CurIdx+1 >= (int)(m_Stream.size());
	return m_CurIdx;
}

//
//----------------------------------------------------------------------------------------------
int CRecorderUnit::StreamBase::GetSize()
{
	return (int)(m_Stream.size());
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::StreamBase::Clear()
{
	for(size_t i = 0; i < m_Stream.size(); ++i)
		delete m_Stream[i];
	m_Stream.clear();
}

//
//----------------------------------------------------------------------------------------------
float	CRecorderUnit::StreamBase::GetStartTime()
{
	if(m_Stream.empty())
		return 0.0f;
	return m_Stream.front()->m_StartTime;
}

//
//----------------------------------------------------------------------------------------------
float	CRecorderUnit::StreamBase::GetEndTime()
{
	if(m_Stream.empty())
		return 0.0f;
	return m_Stream.back()->m_StartTime;
}

//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamStr::GetCurrent(float &startingFrom)
{
	if(m_CurIdx >= m_Stream.size())
		return NULL;
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_String.c_str());
}

//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamStr::GetNext(float &startingFrom)
{
	if(++m_CurIdx >= m_Stream.size())
	{
		m_CurIdx = m_Stream.size();
		return NULL;
	}
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_String.c_str());
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::StreamStr::AddValue(const IAIRecordable::RecorderEventData* pEventData, float t)
{
	//GetISystem()->GetILog()->Log("[AI Recorder] At time %f added string %s", GetAISystem()->GetFrameStartTime().GetSeconds(), pEventData->pString);

	m_Stream.push_back( new StreamUnit(t, pEventData->pString));
}


//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::WriteValue( const IAIRecordable::RecorderEventData* pEventData, float t)
{
	return WriteValue( t, pEventData->pString, CAIRecorder::m_pFile );
}


//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::WriteValue( float t, const char * str, FILE * pFile)
{
	int strLen = (str ? strlen(str) : 0);
	if (!fwrite(&t, sizeof(t), 1, pFile)) return false;
	if (!fwrite(&strLen, sizeof(strLen), 1, pFile)) return false;
	if (fwrite(str, sizeof(char), strLen, pFile) != strLen) return false;
	return true;
}

//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::LoadValue( FILE * pFile)
{
	string name;
	float time;
	if (!LoadValue(time, name, pFile)) return false;

	// Check chronological ordering
	if (!m_Stream.empty() && m_Stream.back()->m_StartTime > time) 
	{
		GetISystem()->GetILog()->LogError("[AI Recorder] Aborting - events are not recorded in chronological order");
		return false;
	}

	m_Stream.push_back( new StreamUnit(time, name.c_str()));
	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::SaveStream(FILE	*pFile)
{
	int counter(m_Stream.size());
	if (!fwrite(&counter, sizeof(counter), 1, pFile)) return false;
	for(int idx(0); idx<m_Stream.size(); ++idx)
	{
		StreamUnit *pCurUnit = static_cast<StreamUnit*>(m_Stream[idx]);
		float t = pCurUnit->m_StartTime;
		int strLen = pCurUnit->m_String.size();
		const char *pStr = pCurUnit->m_String.c_str();
		if (!WriteValue( t, pStr, pFile )) return false;		
	}
	return true;
}

bool CRecorderUnit::StreamStr::LoadValue(float &t, string &name, FILE *pFile)
{
	int strLen;
	if (!fread(&t, sizeof(t), 1, pFile)) return false;
	if (!fread(&strLen, sizeof(strLen), 1, pFile)) return false;
	if (strLen < 0 || strLen > MAX_EVENT_STRING_LENGTH) return false;
	name.resize(strLen);
	if (fread(name.begin(), sizeof(char), strLen, pFile) != strLen) return false;
	if (strLen == 0)
		name = "<Empty string>";
	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamStr::LoadStream(FILE	*pFile)
{
	string name;
	int counter;
	fread(&counter, sizeof(counter), 1, pFile);

	for(int idx(0); idx<counter; ++idx)
		if (!LoadValue(pFile)) return false;

	return true;
}



//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamVec3::GetCurrent(float &startingFrom)
{
	if(m_CurIdx >= m_Stream.size())
		return NULL;
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(&static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_Pos);
}

//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamVec3::GetNext(float &startingFrom)
{
	if(++m_CurIdx >= m_Stream.size())
	{
		m_CurIdx = m_Stream.size();
		return NULL;
	}
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(&static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_Pos);
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::StreamVec3::AddValue(const IAIRecordable::RecorderEventData* pEventData, float t)
{
	//GetISystem()->GetILog()->Log("[AI Recorder] At time %f added vector", GetAISystem()->GetFrameStartTime().GetSeconds());

	// Simple point filtering.
	if(!m_Stream.empty())
	{
		StreamUnit* pLastUnit(static_cast<StreamUnit*>(m_Stream.back()));
		Vec3	delta = pLastUnit->m_Pos - pEventData->pos;
		if(delta.len2() > sqr(0.5f))
			m_Stream.push_back( new StreamUnit(t, pEventData->pos));
	}
	else
	{
		m_Stream.push_back( new StreamUnit(t, pEventData->pos));
	}
}


//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::WriteValue( const IAIRecordable::RecorderEventData* pEventData, float t)
{
	return WriteValue( t, pEventData->pos, CAIRecorder::m_pFile);
}


//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::WriteValue( float t, const Vec3 &vec, FILE * pFile)
{
	if (!fwrite(&t, sizeof(t), 1, pFile)) return false;
	if (!fwrite(&vec.x, sizeof(vec.x), 1, pFile)) return false;
	if (!fwrite(&vec.y, sizeof(vec.y), 1, pFile)) return false;
	if (!fwrite(&vec.z, sizeof(vec.z), 1, pFile)) return false;
	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::LoadValue(float &t, Vec3 &vec, FILE *pFile)
{
	if (!fread(&t, sizeof(t), 1, pFile)) return false;
	if (!fread(&vec.x, sizeof(vec.x), 1, pFile)) return false;
	if (!fread(&vec.y, sizeof(vec.y), 1, pFile)) return false;
	if (!fread(&vec.z, sizeof(vec.z), 1, pFile)) return false;
	return true;
}


//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::LoadValue( FILE * pFile)
{
	Vec3 vec;
	float time;
	if (!LoadValue(time, vec, pFile)) return false;

	// Check chronological ordering
	if (!m_Stream.empty() && m_Stream.back()->m_StartTime > time) 
	{
		GetISystem()->GetILog()->LogError("[AI Recorder] Aborting - events are not recorded in chronological order");
		return false;
	}

	m_Stream.push_back( new StreamUnit(time, vec));
	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::LoadStream(FILE	*pFile)
{
	int counter;
	fread(&counter, sizeof(counter), 1, pFile);

	for(int idx(0); idx<counter; ++idx)
		if (!LoadValue(pFile)) return false;

	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamVec3::SaveStream(FILE	*pFile)
{
	int counter(m_Stream.size());
	if (!fwrite(&counter, sizeof(counter), 1, pFile)) return false;
	for(int idx(0); idx<m_Stream.size(); ++idx)
	{
		StreamUnit *pCurUnit(static_cast<StreamUnit*>(m_Stream[idx]));
		float time = pCurUnit->m_StartTime;
		Vec3 &vect = pCurUnit->m_Pos;
		if (!WriteValue(time, vect, pFile)) return false;
	}
	return true;
}


//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamFloat::GetCurrent(float &startingFrom)
{
	if(m_CurIdx >= m_Stream.size())
		return NULL;
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(&static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_Val);
}

//
//----------------------------------------------------------------------------------------------
void* CRecorderUnit::StreamFloat::GetNext(float &startingFrom)
{
	if(++m_CurIdx >= m_Stream.size())
	{
		m_CurIdx = m_Stream.size();
		return NULL;
	}
	startingFrom = m_Stream[m_CurIdx]->m_StartTime;
	return (void*)(&static_cast<StreamUnit*>(m_Stream[m_CurIdx])->m_Val);
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::StreamFloat::AddValue(const IAIRecordable::RecorderEventData* pEventData, float t)
{
	//GetISystem()->GetILog()->Log("[AI Recorder] At time %f added vector", GetAISystem()->GetFrameStartTime().GetSeconds());

	m_Stream.push_back( new StreamUnit(t, pEventData->val));
}


//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::WriteValue( const IAIRecordable::RecorderEventData* pEventData, float t)
{
	return WriteValue( t, pEventData->val, CAIRecorder::m_pFile);
}


//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::WriteValue( float t, float val, FILE * pFile)
{
	if (!fwrite(&t, sizeof(t), 1, pFile)) return false;
	if (!fwrite(&val, sizeof(val), 1, pFile)) return false;
	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::LoadValue(float &t, float& val, FILE *pFile)
{
	if (!fread(&t, sizeof(t), 1, pFile)) return false;
	if (!fread(&val, sizeof(val), 1, pFile)) return false;
	return true;
}


//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::LoadValue( FILE * pFile)
{
	float	val;
	float time;
	if (!LoadValue(time, val, pFile)) return false;

	// Check chronological ordering
	if (!m_Stream.empty() && m_Stream.back()->m_StartTime > time) 
	{
		GetISystem()->GetILog()->LogError("[AI Recorder] Aborting - events are not recorded in chronological order");
		return false;
	}

	m_Stream.push_back( new StreamUnit(time, val));
	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::LoadStream(FILE	*pFile)
{
	int counter;
	fread(&counter, sizeof(counter), 1, pFile);

	for(int idx(0); idx<counter; ++idx)
		if (!LoadValue(pFile)) return false;

	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::StreamFloat::SaveStream(FILE	*pFile)
{
	int counter(m_Stream.size());
	if (!fwrite(&counter, sizeof(counter), 1, pFile)) return false;
	for(int idx(0); idx<m_Stream.size(); ++idx)
	{
		StreamUnit *pCurUnit(static_cast<StreamUnit*>(m_Stream[idx]));
		float time = pCurUnit->m_StartTime;
		float val = pCurUnit->m_Val;
		if (!WriteValue(time, val, pFile)) return false;
	}
	return true;
}



//
//----------------------------------------------------------------------------------------------
IAIDebugStream* CRecorderUnit::GetStream(IAIRecordable::e_AIDbgEvent streamTag)
{
	return m_Streams[streamTag];
}

//
//----------------------------------------------------------------------------------------------
void CRecorderUnit::ResetStreams(CTimeValue startTime)
{
	//GetISystem()->GetILog()->Log("[AI Recorder] At time %f reset streams to time %f", GetAISystem()->GetFrameStartTime().GetSeconds(), startTime.GetSeconds());

	for(t_StreamMap::iterator strItr(m_Streams.begin()); strItr != m_Streams.end(); ++strItr)
		strItr->second->Clear();
	m_startTime = startTime;
}

//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::Save(FILE	*pFile)
{
	// Write the number of streams stored
	int numStreams(m_Streams.size());
	if (!fwrite(&numStreams, sizeof(numStreams), 1, pFile)) return false;

	for(t_StreamMap::iterator strItr(m_Streams.begin()); strItr != m_Streams.end(); ++strItr)
	{
		int	intType((strItr->first)); // The kind of stream
		if (!fwrite(&intType, sizeof(intType),1,pFile)) return false;
		if (!strItr->second->SaveStream(pFile)) return false;
	}
	return true;
}


//
//----------------------------------------------------------------------------------------------
bool CRecorderUnit::Load(FILE	*pFile)
{
	// Read the number of streams stored
	int numStreams;
	if (!fread(&numStreams, sizeof(numStreams), 1, pFile)) return false;

	for(t_StreamMap::iterator strItr(m_Streams.begin()); strItr != m_Streams.end(); ++strItr)
		strItr->second->Clear();

	for(int i(0); i<numStreams; ++i)
	{
		IAIRecordable::e_AIDbgEvent intType; // The kind of stream
		if (!fread(&intType, sizeof(intType),1,pFile)) return false;
		
		if (!m_Streams[intType]->LoadStream(pFile)) return false;
	}
	return true;
}


//
//----------------------------------------------------------------------------------------------
CAIRecorder::CAIRecorder()
{
	m_bStarted = false;
	CPipeUser::s_pRecorder = this;
	m_lowLevelFileBufferSize = 1024;
	m_lowLevelFileBuffer = new char[m_lowLevelFileBufferSize];
	m_cvDebugRecordBuffer = 0;
	m_pLog = NULL;
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::Init(void)
{
  if (!m_cvDebugRecord)
  	m_cvDebugRecord	= GetISystem()->GetIConsole()->GetCVar("ai_Recorder");
  if (!m_cvDebugRecordBuffer)
  	m_cvDebugRecordBuffer = GetISystem()->GetIConsole()->GetCVar("ai_Recorder_Buffer");
  if (!m_pLog)
  	m_pLog = GetISystem()->GetILog();
}

//
//----------------------------------------------------------------------------------------------
bool CAIRecorder::IsEnabled(void)
{
	return (m_cvDebugRecord && m_cvDebugRecord->GetIVal() != 0);
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::Start(void)
{
	if (m_bStarted) return;
	m_bStarted = true;

  Init(); // make sure

	// Clear any late arriving events from last run
	Reset();

	if (m_cvDebugRecord)
	{
		switch (m_cvDebugRecord->GetIVal())
		{	
		case eAIRM_Memory:
			if (!GetISystem()->IsEditor()) {
				// This setting is probably a mistake in launcher - correct it
				m_cvDebugRecord->Set(2);
				m_pLog->LogWarning("[AI Recorder] Mode 1 useless in launcher, correcting to 2");
				// Allow fall through so it works anyway(!)
			} 
			else
			{
				// No action required to start recording
				break;
			}
		case eAIRM_Disk:
			{
				// Redundant check
				if (m_pFile) {
					fclose(m_pFile);
					m_pFile = NULL;
				}

				// File is closed, so we have the chance to adjust buffer size
				int newBufferSize = m_cvDebugRecordBuffer->GetIVal();
				clamp_tpl(newBufferSize, 128, 1024000);
				if (newBufferSize != m_lowLevelFileBufferSize)
				{
					delete[] m_lowLevelFileBuffer;
					m_lowLevelFileBufferSize = newBufferSize;
					m_lowLevelFileBuffer = new char[newBufferSize];
				}

				// Open for streaming, using static file pointer
				m_pFile = fxopen(AIRECORDER_RCD_FILE, "wb");

				if(m_pFile)
				{
					// Note - must use own buffer or memory manager may break!
					setvbuf(m_pFile, m_lowLevelFileBuffer, _IOFBF, m_lowLevelFileBufferSize);

					// Write preamble
					if (!Write(m_pFile))
					{
						fclose(m_pFile);
						m_pFile = NULL;
					}
				}
				// Leave it open
			} 
			break;
		default:
			// In other modes does nothing
			// In mode 0, will quite happily be started and stopped doing nothing
			break;
		}
	}
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::Stop(void)
{
	if (!m_bStarted) return;

	m_bStarted = false;

	// Close the recorder file, should have been streaming
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}

	if (m_cvDebugRecord)
	{
		switch (m_cvDebugRecord->GetIVal())
		{	
		case eAIRM_Disk:
			{
				// Only required to close the recorder file
			}
			break;
		case eAIRM_Memory:
			{
				// Dump the history to disk
				Save();
			}
			break;
		default:
			break;
		}
	}
}

//
//----------------------------------------------------------------------------------------------
CAIRecorder::~CAIRecorder()
{
	if (m_bStarted) 
		Stop();

	for(t_Units::iterator it = m_Units.begin(); it != m_Units.end(); ++it)
		delete it->second;
	m_Units.clear();
	
	if (m_lowLevelFileBuffer) 
		delete[] m_lowLevelFileBuffer;
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::Update()
{
	// This is never called so far
}

//
//----------------------------------------------------------------------------------------------
bool CAIRecorder::Load(const char *filename)
{
	if (m_bStarted) return false; // To avoid undefined behaviour

	// Open the AI recorder dump file
	FILE	*pFile;
	if (filename) 
		pFile = fxopen(filename, "rb");
	else
		pFile = fxopen(AIRECORDER_RCD_FILE,"rb");

	bool success = false;
	if (pFile) {
		// Update static file pointer
		m_pFile = pFile;
	
		success = Read(pFile);

		m_pFile = NULL;
		fclose(pFile);
	}

	return success;
}

bool CAIRecorder::Read(FILE *pFile)
{
	
	// File header
	RecorderFileHeader fileHeader;
	fread(&fileHeader, sizeof(fileHeader),1, pFile );
	if (!fileHeader.check())
		return false;
	if (fileHeader.version > AIRECORDER_VERSION) 
	{
		m_pLog->LogError("[AI Recorder] Saved AI Recorder log is of wrong version number");
		fclose(pFile);
		return false;
	}

	// Make sure all units still exist
	RemoveDeletedUnits();
	
	// Clear all units streams
	Reset();

	// String stores name of each unit
	string name;	

	std::vector <int> tempIDs;  // Ids for units we create just during loading, to skip over data
	std::map <int,int> idMap;   // Mapping the ids recorded in the file into those used in this run

	// For each record
	for (int i = 0; i < fileHeader.unitsNumber; i++)
	{
		UnitHeader header;
		// Read entity name and ID
		if (!fread(&header, sizeof(header), 1, pFile)) return false;
		if (!header.check()) return false;
		name.resize(header.nameLength);
		if (!fread(name.begin(), sizeof(char), header.nameLength, pFile)) return false;
		
		// Fetch the entity and add it to our map
		IEntity *pEntity = GetISystem()->GetIEntitySystem()->FindEntityByName(name.c_str());
		int liveID;
		if (!pEntity)
		{
			// Couldn't find entity by that name
			m_pLog->LogWarning("[AI Recorder] While loading, entity \"%s\" does not exist, attempting to skip", name.c_str());
			// Generate a random ID - unlikely to collide.
			liveID = abs( (int)cry_rand()%100000 ) + 10000;
			tempIDs.push_back(liveID);
		} 
		else
		{
			// Entity did exist, normal case
			liveID = pEntity->GetId();
		}

		// Create map entry from the old, possibly changed ID to the current one
		idMap[header.ID] = liveID;
		CRecorderUnit *unit = AddUnit(liveID, true);

		if (!unit) return false;
		if (!unit->Load(pFile)) return false;
	}

	// After the "announced" data in the file, we continue, looking for more
	// A streamed log will typically have 0 length announced data (providing the IDs/names etc)
	// and all data will follow afterwards

	// For each event that follows
	bool bEndOfFile = false;
	do{
		// Read event metadata
		StreamedEventHeader header;
		bEndOfFile = (!fread(&header, sizeof(header), 1, pFile));
		if (!bEndOfFile)
		{
			if (!header.check())
			{
				m_pLog->LogError("[AI Recorder] corrupt event found reading streamed section");
				return false;
			} else {
				int liveID = idMap[header.unitID];
				if (!liveID)
				{
					// New unannounced ID
					m_pLog->LogWarning("[AI Recorder] Unknown ID %d found in stream", header.unitID);
					// Generate a random ID - unlikely to collide.
					liveID = abs( (int)cry_rand()%100000 ) + 10000;
					tempIDs.push_back(liveID);
					idMap[header.unitID] = liveID;
				}

				CRecorderUnit *unit = AddUnit(liveID, true);
				if (!unit) return false;
				if (!unit->LoadEvent( (IAIRecordable::e_AIDbgEvent) header.streamID)) return false;
			}
		}
	}while ( !bEndOfFile );

	//Remove any temporary IDs
	while (!tempIDs.empty())
	{
		RemoveUnit(tempIDs.back());
		tempIDs.pop_back();
	}

	return true;
}

//
//----------------------------------------------------------------------------------------------
bool CAIRecorder::Save(const char *filename)
{
	if (m_bStarted) return false; // To avoid undefined behaviour

	// This method should not change state at all
	bool bSuccess = false;

	// Open the AI recorder dump file
	FILE	*pFile;
	if (filename) 
		pFile = fxopen(filename, "wb");
	else
		pFile = fxopen(AIRECORDER_RCD_FILE,"wb");

	if(pFile)
	{
		// Update static file pointer
		m_pFile = pFile;

		// Note - must use own buffer or memory manager may break!
		setvbuf(pFile, m_lowLevelFileBuffer, _IOFBF, m_lowLevelFileBufferSize);
		bSuccess = Write(pFile);
		
		m_pFile = NULL;
		fclose(pFile);
	}

	if (!bSuccess)
		m_pLog->LogError("[AI Recorder] Save dump failed");

	return bSuccess;
}


bool CAIRecorder::Write(FILE * pFile)
{
	RemoveDeletedUnits();

	// File header
	RecorderFileHeader fileHeader;
	fileHeader.version = AIRECORDER_VERSION;
	fileHeader.unitsNumber = m_Units.size();
	if (!fwrite(&fileHeader, sizeof(fileHeader),1, pFile)) return false;

	// For each record
	for (t_Units::iterator unitIter = m_Units.begin(); unitIter != m_Units.end(); unitIter++)
	{
		// Fetch entity
		IEntity * pEntity = GetISystem()->GetIEntitySystem()->GetEntity(unitIter->first);
		if (!pEntity)
		{
			m_pLog->LogWarning("[AI Recorder] While saving, entity with ID \"%d\" does not exist - skipping", unitIter->first);
			continue; // We can just skip over this entity
		}

		// Record entity name and ID (ID may be not be preserved)
		const char * name = pEntity->GetName();

		UnitHeader header;
		header.nameLength = strlen(name);
		header.ID = unitIter->first;

		if (!fwrite(&header, sizeof(header), 1, pFile)) return false;
		if (fwrite(name, sizeof(char), header.nameLength, pFile) != header.nameLength) return false;
		
		if (!unitIter->second->Save(pFile)) return false;
	}
	return true;
}



//
//----------------------------------------------------------------------------------------------
CRecorderUnit* CAIRecorder::AddUnit(unsigned ownerId, bool force)
{
	CRecorderUnit *pNewUnit = NULL;
	
	t_Units::iterator curUnit(m_Units.find(ownerId));
	if(curUnit!=m_Units.end())
		return curUnit->second;

	if(IsEnabled() || force)
	{
		// Create a new unit only if activated or required
		pNewUnit = new CRecorderUnit(ownerId);
		m_Units[ownerId]= pNewUnit;
	}
	return pNewUnit;
}

//
//----------------------------------------------------------------------------------------------
void CAIRecorder::RemoveUnit(unsigned ownerId)
{
	t_Units::iterator curUnit(m_Units.find(ownerId));
	if(curUnit!=m_Units.end()) 
	{
		SAFE_DELETE(curUnit->second);
		m_Units.erase(curUnit);
	}
}

//
//----------------------------------------------------------------------------------------------


void CAIRecorder::Reset(void) {
	for (t_Units::iterator unitIter = m_Units.begin(); unitIter != m_Units.end(); unitIter++)
		unitIter->second->ResetStreams(GetAISystem()->GetFrameStartTime());
}


//
//----------------------------------------------------------------------------------------------


void CAIRecorder::RemoveDeletedUnits(void) {
	for (t_Units::iterator unitIter = m_Units.begin(); unitIter != m_Units.end();)
	{
		t_Units::iterator unitIterCpy(unitIter);
		unitIter++;
		
		// Fetch entity
		if (!GetISystem()->GetIEntitySystem()->GetEntity(unitIterCpy->first))
			RemoveUnit(unitIterCpy->first); // Deletes object and removes from map, invalidating iterator
	}	
}



//
//----------------------------------------------------------------------------------------------
/*
void CAIRecorder::ChangeOwnerName(const char* pOldName, const char* pNewName)
{
	t_Units::iterator curUnit(m_Units.find(pOldName));
	if(curUnit==m_Units.end())
		return;

	CRecorderUnit *pTheUnit(curUnit->second);
	m_Units.erase(curUnit);
	m_Units[pNewName]= pTheUnit;
}
*/

//
//----------------------------------------------------------------------------------------------
