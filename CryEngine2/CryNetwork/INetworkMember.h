#ifndef __INETWORKMEMBER_H__
#define __INETWORKMEMBER_H__

#pragma once

#include "TimeValue.h"
#include "INetwork.h"

enum ENetworkMemberUpdateOrder
{
	eNMUO_Context,
	eNMUO_Nub,
	eNMUO_Utility
};

enum ENetDumpType
{
	eNDT_ObjectState,
};

class CNetChannel;

struct INetworkMember : public CMultiThreadRefCount
{
	const ENetworkMemberUpdateOrder UpdateOrder;
	INetworkMember( ENetworkMemberUpdateOrder updateOrder ) : UpdateOrder(updateOrder) {}

	virtual void GetMemoryStatistics( ICrySizer * ) = 0;
	virtual bool IsDead() = 0;
	virtual bool IsSuicidal() = 0; // if we keep calling update, will this member say it's dead eventually?
	virtual void SyncWithGame( ENetworkGameSync type ) = 0;
	virtual void NetDump( ENetDumpType ) = 0;
	virtual void PerformRegularCleanup() = 0; // staggered compacting of memory where appropriate
	virtual CNetChannel * FindFirstClientChannel() {return 0;}
};
typedef _smart_ptr<INetworkMember> INetworkMemberPtr;

#endif
