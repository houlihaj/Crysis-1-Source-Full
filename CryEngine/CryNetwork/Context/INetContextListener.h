#ifndef __INETCONTEXTLISTENER_H__
#define __INETCONTEXTLISTENER_H__

#pragma once

#include "INetwork.h"
#include "BitFiddling.h"
#include "MementoMemoryManager.h"
#include "Compression/CompressionManager.h"
#include "Utils.h"

class CSimpleOutputStream;
class CVoicePacket;
class CNetContextState;

static const uint8 NoProfile = 255;

enum ENetObjectEvent
{
	eNOE_InvalidEvent         = 0,
	eNOE_BindObject           = 0x00000001,
	eNOE_UnbindObject         = 0x00000002,
	eNOE_Reset                = 0x00000004,
	eNOE_ObjectAspectChange   = 0x00000008,
	eNOE_UnboundObject        = 0x00000010,
	eNOE_ChangeContext        = 0x00000020,
	eNOE_EstablishedContext   = 0x00000040,
	eNOE_ReconfiguredObject   = 0x00000080,
	eNOE_BindAspects          = 0x00000100,
	eNOE_UnbindAspects        = 0x00000200,
	eNOE_ContextDestroyed     = 0x00000400,
	eNOE_SetAuthority         = 0x00000800,
	eNOE_UpdateScheduler      = 0x00001000,
	eNOE_RemoveStaticEntity   = 0x00002000,
	eNOE_InGame               = 0x00004000,
	eNOE_PredictSpawn         = 0x00008000,
	eNOE_SendVoicePackets     = 0x00010000,
	eNOE_RemoveRMIListener    = 0x00020000,
	eNOE_SetAspectProfile     = 0x00040000,
	eNOE_SyncWithGame_Start   = 0x00080000,
	eNOE_SyncWithGame_End     = 0x00100000,
	eNOE_GotBreakage          = 0x00200000,
	eNOE_ValidatePrediction   = 0x00400000,
	eNOE_PartialUpdate        = 0x00800000,

	eNOE_DebugEvent           = 0x80000000
};

enum ENetObjectDebugEvent
{
};

enum ENetChannelEvent
{
	eNCE_InvalidEvent       = 0,
	eNCE_ChannelDestroyed   = 0x00000001,
	eNCE_RevokedAuthority   = 0x00000002,
	eNCE_SetAuthority       = 0x00000004,
	eNCE_UpdatedObject      = 0x00000008,
	eNCE_BoundObject        = 0x00000010,
	eNCE_UnboundObject      = 0x00000020,
	eNCE_SentUpdate         = 0x00000040,
	eNCE_AckedUpdate        = 0x00000080,
	eNCE_NackedUpdate       = 0x00000100,
	eNCE_InGame             = 0x00000200,
	eNCE_DeclareWitness     = 0x00000400,
	eNCE_SendRMI            = 0x00000800,
	eNCE_AckedRMI           = 0x00001000,
	eNCE_NackedRMI          = 0x00002000,
	eNCE_ScheduleRMI				= 0x00004000,
};

// these two functions are implemented in NetContext.cpp
string GetEventName( ENetObjectEvent );
string GetEventName( ENetChannelEvent );

struct SNetObjectAspectChange
{
	ILINE SNetObjectAspectChange() : aspectsChanged(0), forceChange(0) {}
	ILINE SNetObjectAspectChange(uint8 aspectsChanged, uint8 forceChange)
	{
		this->aspectsChanged = aspectsChanged;
		this->forceChange = forceChange;
	}
	ILINE SNetObjectAspectChange& operator|=(const SNetObjectAspectChange& a)
	{
		aspectsChanged |= a.aspectsChanged;
		forceChange |= a.forceChange;
		return *this;
	}
	uint8 aspectsChanged;
	uint8 forceChange;
};

struct SNetIntBreakDescription
{
	const SNetMessageDef * pMessage;
	IBreakDescriptionInfoPtr pMessagePayload;
	DynArray<EntityId> spawnedObjects;
};
struct SNetClientBreakDescription : public DynArray<SNetObjectID>, public CMultiThreadRefCount 
{
	SNetClientBreakDescription()
	{
		++g_objcnt.clientBreakDescription;
	}
	~SNetClientBreakDescription()
	{
		--g_objcnt.clientBreakDescription;
	}
};
typedef _smart_ptr<SNetClientBreakDescription> SNetClientBreakDescriptionPtr;

struct SNetObjectEvent
{
	SNetObjectEvent() :
		event(eNOE_InvalidEvent),
		aspects(0)
	{
	}

	ENetObjectEvent event;
	SNetObjectID id;
	union
	{
		struct 
		{
			uint8 aspects;
			uint8 profile; // only for eNOE_SetAspectProfile
		};
		// eNOE_SetAuthority
		bool authority;
		// used for eNOE_ObjectAspectChange
		const std::pair<SNetObjectID,SNetObjectAspectChange> * pChanges;
		// used for eNOE_SendVoicePackets
		struct 
		{
			bool	Route;
			const CVoicePacket * pVoicePackets;
			size_t nVoicePackets;
		};
		// eNOE_RemoveStaticEntity, eNOE_UnbindObject
		EntityId userID;
		// eNOE_RemoveRMIListener
		IRMIListener * pRMIListener;
		ENetObjectDebugEvent dbgEvent;
		// eNOE_GotBreakage
		const SNetIntBreakDescription * pBreakage;
		// eNOE_PredictSpawn, eNOE_ValidatePrediction
		struct
		{
			EntityId predictedEntity;
			EntityId predictedReference;
		};
		// eNOE_ChangeContext
		CNetContextState * pNewState;
	};
};

struct SNetChannelEvent
{
	SNetChannelEvent() :
		event(eNCE_InvalidEvent),
		aspects(0),
		seq(~unsigned(0)),
		pRMIBody(0)
	{
	}

	ENetChannelEvent event;
	uint8 aspects;
	SNetObjectID id;
	unsigned seq;
	IRMIMessageBodyPtr pRMIBody;
};

struct SObjectMemUsage
{
	SObjectMemUsage() : required(0), used(0), instances(0) {}
	size_t required;
	size_t used;
	size_t instances;

	SObjectMemUsage& operator+=( const SObjectMemUsage& rhs ) 
	{
		required += rhs.required;
		used += rhs.used;
		instances += rhs.instances;
		return *this;
	}

	friend SObjectMemUsage operator+( const SObjectMemUsage& a, const SObjectMemUsage& b )
	{
		SObjectMemUsage temp = a;
		temp += b;
		return temp;
	}
};

struct INetContextListener : public CMultiThreadRefCount
{
	inline virtual ~INetContextListener() {}
	virtual bool IsDead() = 0;
	virtual void OnObjectEvent( CNetContextState * pState, SNetObjectEvent * pEvent ) = 0;
	virtual void OnChannelEvent( CNetContextState * pState, INetChannel * pFrom, SNetChannelEvent * pEvent ) = 0;
	virtual string GetName() = 0;
	virtual SObjectMemUsage GetObjectMemUsage( SNetObjectID id ) { return SObjectMemUsage(); }
	virtual void PerformRegularCleanup() = 0;
	virtual INetChannel * GetAssociatedChannel() { return NULL; }
	virtual void Die() = 0;
};
typedef _smart_ptr<INetContextListener> INetContextListenerPtr;

template <class T>
class CFowardingNetContextListener : public INetContextListener
{
public:
	CFowardingNetContextListener( T * pListener )
	{
		m_pListener = pListener;
	}

	void Die()
	{
		m_pListener->Die();
		m_pListener = 0;
	}

	bool IsDead()
	{
		return m_pListener? m_pListener->IsDead() : true;
	}

	void OnObjectEvent( SNetObjectEvent * pEvent )
	{
		if (m_pListener)
			m_pListener->OnObjectEvent(pEvent);
	}

	void OnChannelEvent( INetChannel * pFrom, SNetChannelEvent * pEvent )
	{
		if (m_pListener)
			m_pListener->OnChannelEvent(pFrom, pEvent);
	}

	string GetName()
	{
		if (m_pListener)
			return "ForwardingTo_" + m_pListener->GetName();
		else
			return "DeadForwarder";
	}

	SObjectMemUsage GetObjectMemUsage( SNetObjectID id )
	{
		if (m_pListener)
			return m_pListener->GetObjectMemUsage(id);
		else
			return SObjectMemUsage();
	}

	void PerformRegularCleanup()
	{
		if (m_pListener)
			m_pListener->PerformRegularCleanup();
	}

	INetChannel * GetAssociatedChannel()
	{
		if (m_pListener)
			return m_pListener->GetAssociatedChannel();
		else
			return 0;
	}

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CFowardingNetContextListener");

		pSizer->Add(*this);
	}

private:
	T * m_pListener;
};

struct IRMILoggerValue
{
	virtual void WriteStream( CSimpleOutputStream&, const char * ) = 0;
};

struct IRMILogger
{
	virtual void BeginFunction( const char * name ) = 0;
	virtual void OnProperty( const char * name, IRMILoggerValue * pSer ) = 0;
	virtual void EndFunction() = 0;

	virtual void LogCppRMI( EntityId id, IRMICppLogger * pLogger ) = 0;
};

class CContextView;
typedef _smart_ptr<CContextView> CContextViewPtr;

static const int NumAspects = 8;

enum EContextObjectLock
{
	eCOL_Reference = 0,
	eCOL_GameDataSync,
//	eCOL_GameAspectSync,
	eCOL_DisableAspect0,
	eCOL_DisableAspect1,
	eCOL_DisableAspect2,
	eCOL_DisableAspect3,
	eCOL_DisableAspect4,
	eCOL_DisableAspect5,
	eCOL_DisableAspect6,
	eCOL_DisableAspect7,
	eCOL_NUM_LOCKS
};

enum ESpawnType
{
	eST_Normal = 0,
	eST_Static,
	eST_Collected,
	eST_NUM_SPAWN_TYPES
};

struct SContextObject
{
	SContextObject() : 
		bAllocated(false), 
		bOwned(false), 
		pController(NULL),
		salt(1)
	{
		for (size_t i = 0; i < NumAspects; ++i)
		{
			vAspectProfiles[i] = 255; // TODO: find out correct default value
			vAspectDefaultProfiles[i] = 255;
		}
	}
	bool bAllocated : 1; //min
	// is this context the owner of the object (can set authority)
	bool bOwned : 1; //min?
	bool bControlled : 1; //min?
	ESpawnType spawnType : CompileTimeIntegerLog2_RoundUp<eST_NUM_SPAWN_TYPES>::result; //min
	// all of the different kinds of global locks an object can have
	uint16 vLocks[eCOL_NUM_LOCKS]; //min
#if CHECKING_BIND_REFCOUNTS
	std::map<string, int> debug_whosLocking;
#endif
	// our parent object
	SNetObjectID parent; //min?
	// the user id for this object
	EntityId userID; //min?
	// the original user id for this object (it's never cleared to zero)
	EntityId refUserID; //min?
	// currently controlling channel (or NULL)
	INetContextListenerPtr pController; //min
	// which profile is each aspect in
	uint8 vAspectProfiles[NumAspects]; //xtra
	// what is the default profile for each aspect
	uint8 vAspectDefaultProfiles[NumAspects]; //xtra
	uint16 salt;

	const char * GetName() const;

	const char * GetLockString() const
	{
		static char s[eCOL_NUM_LOCKS+1];
		for (int i=0; i<eCOL_NUM_LOCKS; i++)
		{
			if (vLocks[i] < 10)
				s[i] = '0' + vLocks[i];
			else
				s[i] = '#';
		}
		s[eCOL_NUM_LOCKS] = 0;
		return s;
	}
};

struct SContextObjectEx
{
	SContextObjectEx()
		: position(ZERO), direction(ZERO), drawDistance(0.0f), fov(0.0f)
	{
		hasPosition = false;
		hasDirection = false;
		hasDrawDistance = false;
		hasFov = false;

		nAspectsEnabled = 0;

		for (size_t i = 0; i < NumAspects; ++i)
		{
			for (size_t j = 0; j < MaxProfilesPerAspect; ++j)
				vAspectProfileChunks[i][j] = 0;
#if ENABLE_ASPECT_HASHING
			hash[i] = 0;
#endif
			vAspectData[i] = CMementoMemoryManager::InvalidHdl;
			vRemoteAspectData[i] = CMementoMemoryManager::InvalidHdl;
		}
	}

	bool hasPosition : 1; //xtra
	bool hasDirection : 1; //xtra
	bool hasDrawDistance : 1; //xtra
	bool hasFov : 1; //xtra

	// which aspects are enabled on this object?
	uint8 nAspectsEnabled; //min
	// mapping of profile --> chunk for this object
	ChunkID vAspectProfileChunks[NumAspects][MaxProfilesPerAspect]; //xtra
#if ENABLE_ASPECT_HASHING
	uint32 hash[NumAspects]; //xtra
#endif

	// scheduling profiles
	uint32 scheduler_normal; //xtra
	uint32 scheduler_owned; //xtra
	CPriorityPulseStatePtr pPulseState; //xtra
	// optional - hasPosition
	Vec3 position; //xtra
	// optional - hasDirection
	Vec3 direction; //xtra
	// optional - hasDrawDistance
	float drawDistance; //xtra
	// optional - hasFov
	float fov; //xtra

	// current state of each aspect
	TMemHdl vAspectData[NumAspects]; //xtra
	TMemHdl vRemoteAspectData[NumAspects];
	uint32 vAspectDataVersion[NumAspects]; //xtra
};

struct SContextObjectRef
{
	SContextObjectRef() : main(NULL), xtra(NULL) {}
	SContextObjectRef( const SContextObject* m, const SContextObjectEx* x ) : main(m), xtra(x) {}
	const SContextObject * main;
	const SContextObjectEx * xtra;
};

inline ChunkID GetChunkIDFromObject( const SContextObjectRef& ref, uint8 nAspect )
{
	if (ref.main->vAspectProfiles[nAspect] > MaxProfilesPerAspect)
		return ref.xtra->vAspectProfileChunks[nAspect][ref.main->vAspectDefaultProfiles[nAspect]];
	return ref.xtra->vAspectProfileChunks[nAspect][ref.main->vAspectProfiles[nAspect]];
}

#endif
