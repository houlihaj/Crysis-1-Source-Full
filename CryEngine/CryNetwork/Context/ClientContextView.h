/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  Client implementation of a context view
 -------------------------------------------------------------------------
 History:
 - 26/07/2004   : Created by Craig Tiller
*************************************************************************/
#ifndef __CLIENTCONTEXTVIEW_H__
#define __CLIENTCONTEXTVIEW_H__

#pragma once

#include "ContextView.h"
#include "Authentication.h"
#include "DebugKit/NetVis.h"

struct SDeclareBrokenProduct
{
	SDeclareBrokenProduct( SNetObjectID i = SNetObjectID(), int b = -1 ) : id(i), brk(b) {}

	SNetObjectID id;
	int brk;

	void SerializeWith( TSerialize ser )
	{
		ser.Value("id", id, 'eid');
		ser.Value("brk", brk, 'brId');
	}
};

struct SOnlyBreakId
{
	SOnlyBreakId( int b = -1 ) : brk(b) {}

	int brk;

	void SerializeWith( TSerialize ser )
	{
		ser.Value("brk", brk, 'brId');
	}
};

struct SServerPublicAddress
{
	SServerPublicAddress(uint i = 0, ushort p = 0):ip(i),port(p){}
	uint		ip;
	ushort	port;
	void SerializeWith( TSerialize ser )
	{
		ser.Value("ip", ip);
		ser.Value("port", port);
	}
};

// implements CContextView in a way that acts like a client
class CClientContextView : 
	public CNetMessageSinkHelper<CClientContextView, CContextView, 80>
{
public:
	CClientContextView( CNetChannel *, CNetContext * );

#if ENABLE_SESSION_IDS
	NET_DECLARE_IMMEDIATE_MESSAGE(InitSessionData);
#endif
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(AuthenticateChallenge, SAuthenticationSalt);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(ChangeState, SChangeStateMessage);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(ForceNextState, SChangeStateMessage);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(RemoveStaticObject, SRemoveStaticObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(FinishState);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(UpdatePhysicsTime, SPhysicsTime);
	NET_DECLARE_IMMEDIATE_MESSAGE(FlushMsgs);

	NET_DECLARE_IMMEDIATE_MESSAGE(BeginUpdateObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(EndUpdateObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(BeginBindObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(BeginBindPredictedObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(BeginBindStaticObject);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(BeginUnbindObject, SSimpleObjectIDParams);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(UnbindPredictedObject, SRemoveStaticObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(ReconfigureObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect0);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect1);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect2);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect3);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect4);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect5);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect6);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect7);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_UnreliableOrdered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableUnordered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableOrdered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_Attachment);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAuthority);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile0);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile1);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile2);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile3);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile4);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile5);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile6);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile7);
#if ENABLE_ASPECT_HASHING
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect0);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect1);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect2);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect3);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect4);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect5);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect6);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect7);
#endif
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect0);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect1);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect2);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect3);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect4);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect5);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect6);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect7);

	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(BeginBreakStream, SOnlyBreakId);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(DeclareBrokenProduct, SDeclareBrokenProduct);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(PerformBreak, SOnlyBreakId);

	NET_DECLARE_IMMEDIATE_MESSAGE(VoiceData);
	
  //NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(InvalidatePredictedSpawn);

	virtual bool IsClient() const { return true; }
	virtual void CompleteInitialization();

	virtual void ChangeContext();
	virtual void BindObject( SNetObjectID nID );
	virtual void UnbindObject( SNetObjectID nID );
	virtual void EstablishedContext();
	virtual const char * ValidateMessage( 
		const SNetMessageDef *, 
		bool bNetworkMsg );
	virtual bool HasRemoteDef( const SNetMessageDef * pDef );
  virtual void GetMemoryStatistics( ICrySizer * pSizer )
	{
		SIZER_COMPONENT_NAME(pSizer, "CClientContextView");

		pSizer->Add(*this);
		pSizer->AddContainer(m_predictedSpawns);
		CContextView::GetMemoryStatistics(pSizer);

		pSizer->AddContainer(m_tempPackets);
		for (size_t i = 0; i < m_tempPackets.size(); ++i)
			pSizer->AddObject(m_tempPackets[i].get(), sizeof(CVoicePacket));
	}

	// INetMessageSink
	void DefineProtocol( IProtocolBuilder * pBuilder );

#if ENABLE_DEBUG_KIT
	virtual TNetChannelID GetLoggingChannelID( TNetChannelID local, TNetChannelID remote ) { return -(int)remote; }
	void BeginWorldUpdate( SNetObjectID );
	void EndWorldUpdate( SNetObjectID );
#endif

	void BoundCollectedObject( SNetObjectID id, EntityId objId );
	void NC_BoundCollectedObject( SNetObjectID id, EntityId objId );

	void OnObjectEvent( CNetContextState *, SNetObjectEvent * pEvent );

protected:
	virtual bool EnterState( EContextViewState state );
	virtual void UnboundObject( SNetObjectID id );

private:
	enum EBeginBindFlags
	{
		eBBF_ReadObjectID = BIT(0),
		eBBF_FlagStatic = BIT(1),
	};

	virtual bool ShouldInitContext() { return false; }
	virtual const char * DebugString() { return "CLIENT"; }
	void SendEstablishedMessage();
	bool DoBeginBind( TSerialize ser, uint32 flags );
	bool SetAspectProfileMessage( uint8 aspect, TSerialize ser );
	void ContinueEnterState();
	static void OnUpdatePredictedSpawn( void * pUsr1, void * pUsr2, ENetSendableStateUpdate );

	SNetClientBreakDescriptionPtr m_pBreakOps[MAX_BREAK_STREAMS];

	virtual uint32 FilterEventMask( uint32 mask, EContextViewState state )
	{
		return mask | (ENABLE_DEBUG_KIT * eNOE_SyncWithGame_End);
	}

	virtual void OnWitnessDeclared();
	
	std::set<EntityId> m_predictedSpawns;

#if ENABLE_DEBUG_KIT
	CNetVis m_netVis;
	float m_startUpdate;
	Vec3 m_curWorldPos;
#endif

	SSendableHandle                    m_nVoicePacketHandle;
	std::vector<TVoicePacketPtr> m_tempPackets;
};

#endif
