/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  the part of a defence wall that every connection needs
 -------------------------------------------------------------------------
 History:
 - 10/08/2004   : Created by Craig Tiller, based on CDefenceWall by Timur
*************************************************************************/

#ifndef __DEFENCECONTEXT_H__
#define __DEFENCECONTEXT_H__

#pragma once

#include "Config.h"
#include "Network.h"

#if USE_DEFENCE

#include "TimeValue.h"
#include "IDataProbe.h"
#include "DefenceData.h"
#include "NetHelpers.h"
#include "NetTimer.h"
#include "STLMementoAllocator.h"

class CNetChannel;

struct SCheckContext
{
	string filename;
};

struct IDefenceContext : public INetMessageSink
{
	inline virtual ~IDefenceContext() {}
	virtual void AddProtectedFile( const CDefenceData::SProtectedFile& ) = 0;
	virtual void ClearProtectedFiles() = 0;
	virtual bool HasRemoteDef( const SNetMessageDef * pDef ) = 0;

	// true once there's no outstanding validation requests
	virtual bool CanRemove() = 0;
};

class CProbeRequest
{
public:
	CProbeRequest( const SDataProbeContext& c, bool sv, volatile int * pRefCount ) : ctx(c), ncodes(0), isServer(sv), m_pRefCount(pRefCount) 
	{
		SCOPED_GLOBAL_LOCK;
		CryInterlockedIncrement(m_pRefCount);
	}
	SDataProbeContext ctx;
	// maximum probes done by the probe thread
	static const int NUM_CODES = 2;
	uint64 codes[NUM_CODES];
	int ncodes;
	bool isServer;

	virtual void Complete() = 0;

protected:
	// must be instantiated at the start of Complete()
	class CReleaseRefCount
	{
	public:
		CReleaseRefCount( CProbeRequest * pReq )
		{
			m_pRefCount = pReq->m_pRefCount;
		}

		~CReleaseRefCount()
		{
			CryInterlockedDecrement(m_pRefCount);
		}

	private:
		volatile int * m_pRefCount;
	};

private:
	// pointer to an owner refcount - for implementing IDefenceContext::CanRemove
	volatile int * m_pRefCount;
};

class CProbeThread : public CrySimpleThread<>
{
public:
	CProbeThread() {}

	void Run();
	void Cancel() { Push(NULL); }

	void Push(CProbeRequest*);

private:
	typedef CryCondLock<CRYLOCK_RECURSIVE> TLock;
	typedef CryCond<TLock> TCond;
	TLock m_mtx;
	TCond m_cond;
	std::queue<CProbeRequest*> m_incoming;
};

class CClientDefence : public CNetMessageSinkHelper<CClientDefence, IDefenceContext>
{
public:
	CClientDefence( CNetChannel * );
	void DefineProtocol( IProtocolBuilder * pBuilder );
	virtual void AddProtectedFile( const CDefenceData::SProtectedFile& );
	virtual void ClearProtectedFiles();
	virtual bool HasRemoteDef( const SNetMessageDef * pDef );
	virtual bool CanRemove() { return m_pendingRequests == 0; }

	NET_DECLARE_IMMEDIATE_MESSAGE(CheckMessage);

private:
	CNetChannel * m_pParent;
	CMementoMemoryManagerPtr m_pMMM;
	volatile int m_pendingRequests;

	class CClientReplyMessage;
	class CClientQuery;
};

class CServerDefence : public CNetMessageSinkHelper<CServerDefence, IDefenceContext>
{
public:
	CServerDefence( CNetChannel * );
	~CServerDefence();
	void DefineProtocol( IProtocolBuilder * pBuilder );
	virtual void AddProtectedFile( const CDefenceData::SProtectedFile& );
	virtual void ClearProtectedFiles();
	virtual bool HasRemoteDef( const SNetMessageDef * pDef );
	virtual bool CanRemove() { return m_pendingRequests == 0; }

	NET_DECLARE_IMMEDIATE_MESSAGE(CheckMessage);

private:
	void SendNextRequest();
	void SentRequest( uint32 id, const SDataProbeContext& ctx );
	bool CheckLevel( const string& level );

	NetTimerId m_timer;
	NetTimerId m_nextSend;
	static void TimerCallback(NetTimerId, void* p, CTimeValue now);
	static void SendNextRequestCallback(NetTimerId, void* p, CTimeValue now);

	CNetChannel * m_pParent;
	CMementoMemoryManagerPtr m_pMMM;

	typedef std::multimap<uint32, CDefenceData::SProtectedFile, std::less<uint32>, STLMementoAllocator< std::pair<uint32, string> > > TPFMap;
	std::auto_ptr<TPFMap> m_protectedFiles;
	struct SPending
	{
		SPending( const SDataProbeContext& c ) : ctx(c), when(g_time) {}
		SDataProbeContext ctx;
		CTimeValue when;
	};
	typedef std::map<uint32, SPending, std::less<uint32>, STLMementoAllocator< std::pair<uint32, SPending> > > TPMap;
	std::auto_ptr<TPMap> m_pending;
	uint32 m_nextID;

	class CServerCheckMessage;
	class CServerValidate;

	// refcount for number of defence contexts - so we can tear up/down our fake lowspec directory
	static int m_serverDefenceCount;
	// were low spec paks enabled
	static int m_oldLowSpecPak;

	bool m_incDefenceCount;
	volatile int m_pendingRequests;
};

#endif //USE_DEFENCE

#endif
