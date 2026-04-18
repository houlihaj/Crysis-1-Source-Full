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
#ifndef __SERVERCONTEXTVIEW_H__
#define __SERVERCONTEXTVIEW_H__

#include "ContextView.h"
#include "Authentication.h"

#ifdef __WITH_PB__
struct SInitPunkBusterParams
{
	SInitPunkBusterParams(bool has = false) : clientHasPunkBuster(has) {}
	bool clientHasPunkBuster;
	void SerializeWith( TSerialize ser )
	{
		ser.Value("hasPunkbuster", clientHasPunkBuster);
	}
};
#endif

// implement CContextView in a way that acts like a server
typedef CNetMessageSinkHelper<CServerContextView, CContextView, 80> TServerContextViewParent;
class CServerContextView : public TServerContextViewParent
{
public:
	CServerContextView( CNetChannel *, CNetContext * );
	~CServerContextView();

	virtual void BindObject( SNetObjectID nID );
	virtual void UnbindObject( SNetObjectID nID );
	virtual void ChangeContext();
	virtual void EstablishedContext();
	virtual const char * ValidateMessage( const SNetMessageDef *, bool bNetworkMsg );
	virtual bool HasRemoteDef( const SNetMessageDef * pDef );
  virtual bool IsClient() const { return false; }
	virtual void ClearAllState();
	virtual void CompleteInitialization();
	virtual void OnObjectEvent( CNetContextState*, SNetObjectEvent * pEvent );

	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(AuthenticateResponse, CWhirlpoolHash);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(ChangeState, SChangeStateMessage);
	NET_DECLARE_IMMEDIATE_MESSAGE(FinishState);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(UpdatePhysicsTime, SPhysicsTime);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(CompletedUnbindObject, SSimpleObjectIDParams);

	NET_DECLARE_IMMEDIATE_MESSAGE(ClientEstablishedContext);

	NET_DECLARE_IMMEDIATE_MESSAGE(BeginUpdateObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(EndUpdateObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect0);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect1);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect2);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect3);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect4);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect5);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect6);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect7);
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
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_UnreliableOrdered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableUnordered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableOrdered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_Attachment);

	NET_DECLARE_IMMEDIATE_MESSAGE(VoiceData);

	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(BoundCollectedObject, SSimpleObjectIDParams);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(SkippedCollectedObject, SSimpleObjectIDParams);

#ifdef __WITH_PB__
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(InitPunkBuster, SInitPunkBusterParams);
#endif

	// INetMessageSink
	void DefineProtocol( IProtocolBuilder * pBuilder );

	virtual void GetMemoryStatistics( ICrySizer * pSizer );
	virtual void SendAuthChecks();

#if ENABLE_DEBUG_KIT
	virtual TNetChannelID GetLoggingChannelID( TNetChannelID local, TNetChannelID remote ) { return local; }
#endif

protected:
	virtual bool EnterState( EContextViewState state );
	virtual void InitChannelEstablishmentTasks( IContextEstablisher * pEst );

private:
	virtual bool ShouldInitContext() { return true; }
	virtual const char * DebugString() { return "SERVER"; }
	virtual void GotBreakage( const SNetIntBreakDescription * pDesc );
	void SendUnbindMessage( SNetObjectID netID, bool bFromBind, CNetObjectBindLock lk );
	void InitSessionIDs();
	void RemoveStaticEntity( EntityId id );

	void GC_BindObject( SNetObjectID, CNetObjectBindLock lk, CChangeStateLock cslk );

	typedef std::auto_ptr<SAuthenticationSalt> TAuthPtr;

	class CBindObjectMessage;
	class CDeclareBrokenProductMessage;
	class CUnbindObjectMessage;
	class CBeginBreakStream;
	class CPerformBreak;

	virtual uint32 FilterEventMask( uint32 mask, EContextViewState state )
	{
		mask |= eNOE_ValidatePrediction;
		if (state >= eCVS_ConfigureContext)
			mask |= eNOE_RemoveStaticEntity;
		return mask;
	}

	virtual void OnWitnessDeclared();

	struct SBreakStreamHandle
	{
		SSendableHandle hdl;
		int id;
	};
	SBreakStreamHandle m_breakStreamHandles[MAX_BREAK_STREAMS];

	typedef Vec3_tpl<int> BreakSegmentID;
	struct CompareBreakSegmentIDs
	{
		bool operator()( const BreakSegmentID& a, const BreakSegmentID& b ) const
		{
			if (a.x < b.x)
				return true;
			else if (a.x > b.x)
				return false;
			else if (a.y < b.y)
				return true;
			else if (a.y > b.y)
				return false;
			else if (a.z < b.z)
				return true;
			else if (a.z > b.z)
				return false;
			return false;
		}
	};
	typedef std::map<BreakSegmentID, SSendableHandle, CompareBreakSegmentIDs, STLMementoAllocator<std::pair<BreakSegmentID, SSendableHandle> > > TBreakSegmentStreams;
	std::auto_ptr<TBreakSegmentStreams> m_pBreakSegmentStreams;

	TAuthPtr m_pAuth;

	// locks for establish context
	CChangeStateLock m_lockRemoteMapLoaded;
	CChangeStateLock m_lockLocalMapLoaded;

	typedef std::map<EntityId, EntityId, std::less<EntityId>, STLMementoAllocator<std::pair<EntityId, EntityId> > > TValidatedPredictionMap;
	std::auto_ptr<TValidatedPredictionMap> m_pValidatedPredictions;

#if USE_SYSTEM_ALLOCATOR
	typedef std::map<SNetObjectID, CNetObjectBindLock, std::less<SNetObjectID> > TPendingUnbinds;
#else
	typedef std::map<SNetObjectID, CNetObjectBindLock, std::less<SNetObjectID>, STLMementoAllocator< std::pair<SNetObjectID, CNetObjectBindLock> > > TPendingUnbinds;
#endif
	std::auto_ptr<TPendingUnbinds> m_pPendingUnbinds;

	std::vector<INetChannel*> m_pVoiceListeners;

	std::vector<std::pair<SNetObjectID,TVoicePacketPtr> > m_tempPackets;
	std::vector<SSendableHandle> m_dependencyStaging;

#ifdef __WITH_PB__
	bool m_clientHasPunkBuster;
#endif
};

#endif
