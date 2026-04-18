/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   AIDbgRecorder.h
$Id$
Description: 

-------------------------------------------------------------------------
History:
- 1:7:2005   14:31 : Created by Kirill Bulatsev

*********************************************************************/


#ifndef __AIDbgRecorder_H__
#define __AIDbgRecorder_H__


#if _MSC_VER > 1000
#pragma once
#endif

#include <IAISystem.h>
#include <vector>
#include <map>
#include <IAIRecorder.h>

class CAIRecorder;

class CRecorderUnit:
	public IAIDebugRecord
{
public:
	CRecorderUnit( EntityId unitID );
	virtual ~CRecorderUnit();

	IAIDebugStream* GetStream(IAIRecordable::e_AIDbgEvent streamTag);
	void	ResetStreams(CTimeValue startTime);

	void RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData);
	bool LoadEvent( IAIRecordable::e_AIDbgEvent stream );

	bool Save(FILE	*pFile);
	bool Load(FILE	*pFile);

protected:

	struct StreamBase:
		public IAIDebugStream
	{
		struct StreamUnitBase{
			float	m_StartTime;
			StreamUnitBase(float time):m_StartTime(time){}
		};
		typedef	std::vector<StreamUnitBase*>	t_Stream;

		virtual ~StreamBase() { Clear(); }
		virtual bool  SaveStream(FILE	*pFile)=0;
		virtual bool  LoadStream(FILE *pFile)=0; 
		virtual void  AddValue(const IAIRecordable::RecorderEventData* pEventData, float t)=0;
		virtual bool  WriteValue(const IAIRecordable::RecorderEventData* pEventData, float t) = 0;
		virtual bool  LoadValue( FILE *pFile ) = 0;
		void	Seek(float whereTo);
		int		GetCurrentIdx();
		int		GetSize();
		float	GetStartTime();
		float	GetEndTime();
		void	Clear();

		t_Stream	m_Stream;
		int			m_CurIdx;

	};

	struct StreamStr:
		public StreamBase
	{
		struct StreamUnit: public StreamBase::StreamUnitBase{
			string	m_String;
			StreamUnit(float time, const char* pStr):StreamUnitBase(time),m_String(pStr){}
		};
		bool  SaveStream(FILE	*pFile);
		bool  LoadStream(FILE *pFile); 
		void  AddValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  WriteValue( float t, const char * str, FILE * pFile);
		bool  WriteValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  LoadValue(float &t, string &name, FILE *pFile);
		bool  LoadValue( FILE *pFile );
		void* GetCurrent(float &startingFrom);
		void* GetNext(float &startingFrom);
		void  Reset();
	};

	struct StreamVec3:
		public StreamBase
	{
		struct StreamUnit: public StreamBase::StreamUnitBase{
			StreamUnit(float time, const Vec3& pos) : StreamUnitBase(time), m_Pos(pos) { }
			Vec3	m_Pos;
		};
		bool  SaveStream(FILE	*pFile);
		bool  LoadStream(FILE *pFile); 
		void  AddValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  WriteValue( float t, const Vec3 &vec, FILE * pFile);
		bool  WriteValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  LoadValue(float &t, Vec3 &vec, FILE *pFile);
		bool  LoadValue( FILE *pFile );
		void* GetCurrent(float &startingFrom);
		void* GetNext(float &startingFrom);
		void  Reset();
	};

	struct StreamFloat:
		public StreamBase
	{
		struct StreamUnit: public StreamBase::StreamUnitBase{
			StreamUnit(float time, float val) : StreamUnitBase(time), m_Val(val) { }
			float		m_Val;
		};
		bool  SaveStream(FILE	*pFile);
		bool  LoadStream(FILE *pFile); 
		void  AddValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  WriteValue( float t, float val, FILE * pFile);
		bool  WriteValue(const IAIRecordable::RecorderEventData* pEventData, float t);
		bool  LoadValue(float &t, float& val, FILE *pFile);
		bool  LoadValue( FILE *pFile );
		void* GetCurrent(float &startingFrom);
		void* GetNext(float &startingFrom);
		void  Reset();
	};

	typedef	std::map<IAIRecordable::e_AIDbgEvent, StreamBase*>	t_StreamMap;

	CTimeValue m_startTime;
	t_StreamMap	m_Streams;
	int m_unitID;
};


class CRecordable
{
public:
	CRecordable();

	//	virtual void	RecordEvent(IAIRecordable::e_AIDbgEvent event, const RecorderEventData* pEventData);
	//	virtual void	RecordSnapshot() {};

	//protected:
	static CAIRecorder *s_pRecorder;
	CRecorderUnit* m_pMyRecord;
};




class CAIRecorder : public IAIRecorder
{
public:

	// Should really be in IAISystem
	enum EAIRecorderMode{ eAIRM_Off = 0, eAIRM_Memory, eAIRM_Disk };

	CAIRecorder();
	~CAIRecorder();

	static bool IsEnabled(void); 

	// Initialise after construction
	void Init(void);

	void	Update();

	// Ignored while m_bStarted
	bool	Save(const char * filename = NULL);

	// Ignored while m_bStarted
	bool	Load(const char * filename = NULL);

	// Prepare to record events
	void  Start(void);

	// Finalise recording, stop recording events
	void  Stop(void);

	// Clear any recording in memory
	void  Reset(void);

	CRecorderUnit* AddUnit(unsigned ownerId, bool force = false);

	void RemoveUnit(unsigned ownerId);

	void RemoveDeletedUnits(void);

	//	void	ChangeOwnerName(const char* pOldName, const char* pNewName);

	static FILE *m_pFile; // Hack!

	
protected:

	bool Read(FILE *pFile);

	bool Write(FILE *pFile);

	bool m_bStarted;

 	typedef std::map<unsigned, CRecorderUnit*> t_Units;

	t_Units	m_Units;

	ICVar *m_cvDebugRecordBuffer;

	ILog* m_pLog;
	
	char * m_lowLevelFileBuffer;
	int m_lowLevelFileBufferSize;

	static ICVar *m_cvDebugRecord;
};


class CAIDbgRecorder
{
public:

	CAIDbgRecorder();
	~CAIDbgRecorder();

	bool IsRecording( const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event ) const;
	void Record( const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event, const char* pString ) const;

  /// should be called periodically (but not too often) by AI system update
  void FlushLog();
#ifdef AI_LOG_SIGNALS
	void	LogStringSecondary(const char* pString) const;
#endif
protected:

	void	LogString(const char* pString) const;

#define	BUFFER_SIZE	256
	static char	m_BufferString[BUFFER_SIZE];

	FILE	*m_pFile;
	
	FILE *m_pFileSecondary;

  mutable bool m_bNeedToFlush;
};

#endif __AIDbgRecorder_H__
