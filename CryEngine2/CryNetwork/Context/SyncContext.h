#ifndef __SYNCCONTEXT_H__
#define __SYNCCONTEXT_H__

#pragma once

#include "INetContextListener.h"
#include "ISerialize.h"

struct INetSender;

static const uint32 LINKEDELEM_TERMINATOR = 0x3fffffff; // top two bits are free to pack history counts

enum ESyncContextFlags
{
	eSCF_AssumeEnabled = 0x0001,
	eSCF_EnsureEnabled = 0x0002,
};

enum ESpawnState
{
	eSS_Unspawned,
	eSS_Spawned,
	eSS_Enabled,
};

enum EHistory
{
	eH_Configuration = 0,
	eH_Auth,
	eH_Profile,
	eH_AspectData,
	eH_NUM_HISTORIES
};

enum EHistorySlots
{
	eHS_Configuration = 1,
	eHS_Auth = 1,
	eHS_Profile = NumAspects,
	eHS_AspectData = 2*NumAspects,
};

enum EHistoryStartSlot
{
	eHSS_Configuration = 0,
	eHSS_Auth = eHSS_Configuration + eHS_Configuration,
	eHSS_Profile = eHSS_Auth + eHS_Auth,
	eHSS_AspectData = eHSS_Profile + eHS_Profile,
	eHSS_NUM_SLOTS = eHSS_AspectData + eHS_AspectData,
};

enum EHistoryCount
{
	eHC_Zero,
	eHC_One,
	eHC_MoreThanOne,
	eHC_Invalid, // should never be seen outside of CRegularlySyncedItem - indicates recalculation is needed
};

struct SHistoryRootPointer
{
	SHistoryRootPointer() : index(LINKEDELEM_TERMINATOR), historyCount(eHC_Zero) 
	{
		++g_objcnt.historyRoot;
	}
	SHistoryRootPointer( const SHistoryRootPointer& rhs )
	{
		index = rhs.index;
		historyCount = rhs.historyCount;
		++g_objcnt.historyRoot;
	}
	~SHistoryRootPointer()
	{
		--g_objcnt.historyRoot;
	}
	SHistoryRootPointer& operator=( const SHistoryRootPointer& rhs )
	{
		this->~SHistoryRootPointer();
		new (this) SHistoryRootPointer(rhs);
		return *this;
	}
	uint32 index : 30;
	uint32 historyCount : 2;
};

struct SContextViewObject
{
	SContextViewObject()
	{
		salt = 0;
		Reset();
	}

	uint8 authority : 1; //min
	EntityId predictionHandle;
	ESpawnState spawnState;
	uint16 salt;
	uint32 locks; //min
	// handle to our most recent ordered no-attach rmi
	SSendableHandle orderedRMIHandle; //min
	// handle to our most recent update message (cleared on send)
	SSendableHandle msgHandle; //xtra (min?)
	SSendableHandle activeHandle;

	void Reset()
	{
		spawnState = eSS_Unspawned;
		authority = false;
		locks = 0;
		orderedRMIHandle = SSendableHandle();
		msgHandle = SSendableHandle();
		activeHandle = SSendableHandle();
		predictionHandle = 0;
#ifndef NDEBUG
		lockers.clear();
#endif
	}

#ifndef NDEBUG
	std::map<string, int> lockers;
#endif
};

struct SContextViewObjectEx
{
	SContextViewObjectEx() 
	{
		for (int i=0; i<eHSS_NUM_SLOTS; i++) // just to avoid an assert below...
			historyElems[i] = SHistoryRootPointer();
		Reset();
	}

	// handles to our hash messages
	SSendableHandle notifyPartialUpdateHandle[NumAspects];

#if ENABLE_ASPECT_HASHING
	uint8 hashedAspects; //xtra
	SSendableHandle hashMsgHandles[NumAspects]; //xtra
	uint32 hashReceived[NumAspects]; //xtra
	uint32 hashSent[NumAspects]; //xtra
#endif

	CTimeValue voiceTransmitTime; //xtra
	CTimeValue voiceReceiptTime; //xtra
	CTimeValue partialUpdateReceived[NumAspects]; //xtra
	int partialUpdatesRemaining[NumAspects];

	SHistoryRootPointer historyElems[eHSS_NUM_SLOTS];

	uint8 dirtyAspects;
	uint32 ackedAspectVersions[NumAspects];

	void Reset()
	{
#if ENABLE_ASPECT_HASHING
		hashedAspects = 0;
#endif
		voiceReceiptTime = 0.0f;
		voiceTransmitTime = 0.0f;
		dirtyAspects = 0;

		for (int i=0; i<NumAspects; i++)
		{
			partialUpdateReceived[i] = 0.0f;
			partialUpdatesRemaining[i] = 0;
			notifyPartialUpdateHandle[i] = SSendableHandle();
			ackedAspectVersions[i] = ~uint32(0);
#if ENABLE_ASPECT_HASHING
			hashMsgHandles[i] = SSendableHandle();
			hashReceived[i] = hashSent[i] = 0;
#endif
		}
		for (int i=0; i<eHSS_NUM_SLOTS; i++)
		{
			NET_ASSERT(historyElems[i].index == LINKEDELEM_TERMINATOR);
			historyElems[i] = SHistoryRootPointer();
		}
	}
};

struct SSyncContext
{
	SSyncContext()
	{
		basisSeq = 0xbadf00d;
		currentSeq = 0xdeadbeef;
		index = 0xde;
		flags = 0;
		pView = 0;
		pViewObj = 0;
		pSentAlready = 0;
		pViewObjEx = 0;
	}
	uint32 basisSeq;
	uint32 currentSeq;
	uint8 index;
	SNetObjectID objId;
	uint32 flags;
	CContextView * pView;
	uint8 * pSentAlready;
	SContextObjectRef ctxObj;
	SContextViewObject * pViewObj;
	SContextViewObjectEx * pViewObjEx;
	//const SContextObject * pCtxObj;
};

struct SSendContext : public SSyncContext
{
	SSendContext() : pSender(0) {}
	INetSender * pSender;
};

struct SReceiveContext : public SSyncContext
{
	SReceiveContext( TSerialize s ) : ser(s) {}
	mutable TSerialize ser;
};

// this is data about an attachment
struct SAttachmentIndex
{
	SNetObjectID id;
	uint32 index;

	bool operator<( const SAttachmentIndex& rhs ) const
	{
		return id < rhs.id || (id == rhs.id && index < rhs.index);
	}
};

#endif
