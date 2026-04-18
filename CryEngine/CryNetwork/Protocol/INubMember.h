#ifndef __INUBMEMBER_H__
#define __INUBMEMBER_H__

#pragma once

#include "MementoMemoryManager.h"
#include "FrameTypes.h"

class CNetChannel;

struct INubMember : public CMultiThreadRefCount
{
	// conversion to a channel (may return NULL!)
	virtual CNetChannel * GetNetChannel() = 0;
	// is this item dead?
	virtual bool IsDead() = 0;
	// is this item wanting to die?
	virtual bool IsSuicidal() = 0;
	// should this item die?
	virtual void Die() = 0;
	virtual void RemovedFromNub() = 0;
	// process a packet
	virtual void ProcessPacket( bool inSync, const uint8 * pData, uint32 nLength ) = 0;
	virtual void ProcessSimplePacket( EHeaders hdr ) = 0;
	// house keeping
	virtual void PerformRegularCleanup() = 0;
	virtual void SyncWithGame( ENetworkGameSync type ) = 0;
	virtual void NetDump(ENetDumpType type) = 0;

	virtual void GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false) = 0;
};
typedef _smart_ptr<INubMember> INubMemberPtr;

#endif
