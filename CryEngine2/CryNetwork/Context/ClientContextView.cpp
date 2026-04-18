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
#include "StdAfx.h"
#include "ClientContextView.h"
#include "ServerContextView.h"
#include "NetContext.h"
#include "ITimer.h"
#include "Network.h"
#include "BreakagePlayback.h"
#include "History/History.h"
#include "VoiceContext.h"

#if ENABLE_DEBUG_KIT
#include "IEntitySystem.h"
#include "IPhysics.h"
#endif

CClientContextView::CClientContextView( CNetChannel * pNetChannel, CNetContext * pNetContext )
: m_nVoicePacketHandle()
#if ENABLE_DEBUG_KIT
, m_netVis(this)
#endif
{
	SetMMM(pNetChannel->GetChannelMMM());
	SContextViewConfiguration config = {
		NULL, // FlushMsgs
		CServerContextView::ChangeState,
		NULL, // ForceNextState
		CServerContextView::FinishState,
		CServerContextView::BeginUpdateObject,
		CServerContextView::EndUpdateObject,
		NULL, // ReconfigureObject
		NULL, // SetAuthority
		CServerContextView::VoiceData,
		NULL, // RemoveStaticEntity
		CServerContextView::UpdatePhysicsTime,
		// partial update notification messages
		{
			CServerContextView::PartialAspect0,
			CServerContextView::PartialAspect1,
			CServerContextView::PartialAspect2,
			CServerContextView::PartialAspect3,
			CServerContextView::PartialAspect4,
			CServerContextView::PartialAspect5,
			CServerContextView::PartialAspect6,
			CServerContextView::PartialAspect7,
		},
		// set aspect profile messages
		{
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
		},
		// update aspect messages
		{
			CServerContextView::UpdateAspect0,
			CServerContextView::UpdateAspect1,
			CServerContextView::UpdateAspect2,
			CServerContextView::UpdateAspect3,
			CServerContextView::UpdateAspect4,
			CServerContextView::UpdateAspect5,
			CServerContextView::UpdateAspect6,
			CServerContextView::UpdateAspect7,
		},
#if ENABLE_ASPECT_HASHING
		// hash aspect messages
		{
			CServerContextView::HashAspect0,
			CServerContextView::HashAspect1,
			CServerContextView::HashAspect2,
			CServerContextView::HashAspect3,
			CServerContextView::HashAspect4,
			CServerContextView::HashAspect5,
			CServerContextView::HashAspect6,
			CServerContextView::HashAspect7,
		},
#endif
		// rmi messages
		{
			CServerContextView::RMI_ReliableOrdered,
			CServerContextView::RMI_ReliableUnordered,
			CServerContextView::RMI_UnreliableOrdered,
			NULL,
			// must be last
			CServerContextView::RMI_Attachment,
		}
	};

	Init( pNetChannel, pNetContext, &config);
}

void CClientContextView::DefineProtocol( IProtocolBuilder * pBuilder )
{
  DefineExtensionsProtocol(pBuilder);
	pBuilder->AddMessageSink( this,
		CServerContextView::GetProtocolDef(),
		CClientContextView::GetProtocolDef() );
}

void CClientContextView::ChangeContext()
{
	CContextView::ChangeContext();
}

void CClientContextView::CompleteInitialization()
{
	CContextView::CompleteInitialization();
	SetName("Client_" + RESOLVER.ToString(Parent()->GetIP()));
}

void CClientContextView::OnObjectEvent( CNetContextState * pState, SNetObjectEvent * pEvent )
{
	if (pState == ContextState())
	{
		switch (pEvent->event)
		{
		case eNOE_SyncWithGame_End:
#if ENABLE_DEBUG_KIT
			m_netVis.Update();
#endif
			break;
		}
	}
	CContextView::OnObjectEvent(pState, pEvent);
}

void CClientContextView::OnWitnessDeclared()
{
	if (CVoiceContext * pCtx = Context()->GetVoiceContextImpl())
	{
		pCtx->ConfigureCallback( this, eVD_To, GetWitness() );
	}
}

void CClientContextView::SendEstablishedMessage()
{
	// we'd best tell the server
	class CEstablishedContextMessage : public INetMessage
	{
	public:
		CEstablishedContextMessage() : INetMessage(CServerContextView::ClientEstablishedContext)
		{
			++g_objcnt.establishedMsg;
		}
		~CEstablishedContextMessage()
		{
			--g_objcnt.establishedMsg;
		}
		EMessageSendResult WritePayload( TSerialize, uint32, uint32 )
		{
			return eMSR_SentOk;
		}
		void UpdateState( uint32 nFromSeq, ENetSendableStateUpdate update )
		{
		}
		size_t GetSize() { return sizeof(*this); }
	};
	Parent()->AddSendable( new CEstablishedContextMessage, 0, NULL, NULL );
}

void CClientContextView::EstablishedContext()
{
//	if (ContextState())
//		ContextState()->EstablishedContext(this);

	if (!IsLocal())
		SendEstablishedMessage();
}

void CClientContextView::BindObject( SNetObjectID nID )
{
	//const SContextObject * pObj = ContextState()->GetContextObject(nID);
	SContextObjectRef obj = ContextState()->GetContextObject(nID);
	if (obj.main->spawnType == eST_Collected && !obj.main->userID)
		; // we do the bind at break streaming time in this case
	else
	{
		CContextView::BindObject( nID );
		SetSpawnState( nID, eSS_Enabled );
	}
}

void CClientContextView::UnbindObject( SNetObjectID nID )
{
	CContextView::DoUnbindObject( nID, true );
}

const char * CClientContextView::ValidateMessage( const SNetMessageDef * pMsg, bool bNetworkMsg )
{
	if (!bNetworkMsg && pMsg->reliability == eNRT_UnreliableOrdered)
		return "Cannot send unreliable user messages from client until in game";
	return NULL;
}

bool CClientContextView::EnterState( EContextViewState state )
{
	switch (state)
	{
	case eCVS_Initial:
		break;
	case eCVS_Begin:
		if (!IsLocal())
		{
#ifdef __WITH_PB__
			Parent()->NetAddSendable( 
				new CSimpleNetMessage<SInitPunkBusterParams>(SInitPunkBusterParams(isPbClEnabled()!=0), CServerContextView::InitPunkBuster), 
				0, NULL, NULL );
#endif
			Context()->ChangeContext();
			TO_GAME(&CClientContextView::ContinueEnterState, this);
			LockStateChanges("ContinueEnterState");
			return true;
		}
		else
			ClearAllState();
		break;
	case eCVS_EstablishContext:
		break;
	case eCVS_ConfigureContext:
		break;
	case eCVS_SpawnEntities:
		break;
	case eCVS_PostSpawnEntities:
		break;
	case eCVS_InGame:
		break;
	}

	if (!CContextView::EnterState(state))
		return false;

	return true;
}

void CClientContextView::ContinueEnterState()
{
	if (!CContextView::EnterState(eCVS_Begin))
		Parent()->Disconnect(eDC_ProtocolError, "Couldn't enter begin state");
	UnlockStateChanges("ContinueEnterState");
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, ChangeState, eNRT_ReliableUnordered, eMPF_StateChange | eMPF_BlocksStateChange)
{
	return SetRemoteState( param.state );
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, FinishState, eNRT_ReliableUnordered, eMPF_StateChange | eMPF_BlocksStateChange)
{
	if (FinishRemoteState())
	{
		FinishLocalState();
		return true;
	}
	return false;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, ForceNextState, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	PushForcedState( param.state, true );
	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, AuthenticateChallenge, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	CWhirlpoolHash response = param.Hash( Password() );
	CServerContextView::SendAuthenticateResponseWith( response, Parent() );
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, BeginBindObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange | eMPF_DecodeInSync)
{
	return DoBeginBind( ser, 0 );
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, BeginBindStaticObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange | eMPF_DecodeInSync)
{
	return DoBeginBind( ser, eBBF_ReadObjectID | eBBF_FlagStatic );
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, BeginBindPredictedObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange | eMPF_DecodeInSync)
{
	return DoBeginBind( ser, eBBF_ReadObjectID );
}

bool CClientContextView::DoBeginBind( TSerialize ser, uint32 flags )
{
	if (IsBeforeState(eCVS_SpawnEntities))
	{
		NetWarning( "Can't bind before getting to state SpawnEntities" );
		return false;
	}

	// note here we check that the current object id DOESN'T exist (normally we'd check if it DOES exist)
	bool alreadyAllocated = ReadCurrentObjectID(ser, true);

	if (!IsLocal())
	{
		EntityId nUserID = 0;
		if (alreadyAllocated)
		{
			NetWarning( "Attempt to bind an already allocated object" );
			return false;
		}
		if (flags & eBBF_ReadObjectID)
		{
			ser.Value( "userID", nUserID );
			ContextState()->SpawnedObject( nUserID );
		}
		else
		{
			nUserID = ContextState()->GetSpawnedObjectId(false);
		}
		Parent()->SetEntityId(nUserID);
		uint8 nAspectsEnabled;
		ser.Value( "aspects", nAspectsEnabled );

#if LOG_ENTITYID_ERRORS
		if (CNetCVars::Get().LogLevel)
			NetLog("BIND OBJECT: netID=%s entityID=%d aspects=%.2x", CurrentObjectID().GetText(), nUserID, nAspectsEnabled);
#endif

		if (ContextState()->AllocateObject( ContextState()->GetSpawnedObjectId(true), CurrentObjectID(), nAspectsEnabled, false, (flags&eBBF_FlagStatic)? eST_Static : eST_Normal, this ))
			ContextState()->GC_BoundObject( std::make_pair( ContextState()->GetContextObject(CurrentObjectID()).main->userID, nAspectsEnabled ) );
	}
	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, BeginUnbindObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	ContextState()->UnbindObject( param.netID, eUOF_CallGame );
	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, RemoveStaticObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	ContextState()->UnbindStaticObject( param.id );
	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, UnbindPredictedObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	ContextState()->UnbindStaticObject( param.id );
	return true;
}

/*
NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, InvalidatePredictedSpawn, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	ContextState()->ForceObjectRemoval(param.netID);
}
*/

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, BeginUpdateObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange | eMPF_AfterSpawning)
{
#if ENABLE_DEBUG_KIT
	m_startUpdate = GetNetSerializeImplFromSerialize<CNetInputSerializeImpl>(ser)->GetInput().GetBitSize();
#endif
	if (!ReadCurrentObjectID(ser, false))
	{
		Parent()->Disconnect(eDC_ContextCorruption, "BeginUpdateObject");
		return false;
	}
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, EndUpdateObject, eNRT_UnreliableUnordered, 0)
{
#if ENABLE_DEBUG_KIT
	float sz = GetNetSerializeImplFromSerialize<CNetInputSerializeImpl>(ser)->GetInput().GetBitSize() - m_startUpdate;
	if (sz > 0)
		m_netVis.AddSample( CurrentObjectID(), 0, sz );
	m_netVis.AddSample( CurrentObjectID(), 1, 1 );
#endif
	return ClearCurrentObjectID();
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, ReconfigureObject, eNRT_UnreliableUnordered, 0)
{
	bool ok = false;
	SReceiveContext ctx = CreateReceiveContext( ser, 7, nCurSeq, nOldSeq, &ok );
	if (!ok)
	{
		Parent()->Disconnect(eDC_ContextCorruption, "Failed ReconfigureObject 1");
		return false;
	}
	if (!GetHistory(eH_Configuration)->ReadCurrentValue( ctx, !IgnoringCurrentObject() ))
	{
		Parent()->Disconnect(eDC_ContextCorruption, "Failed ReconfigureObject 2");
		return false;
	}
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect0, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
#ifdef SP_DEMO
	return (false);
#else
	return UpdateAspect( 0, ser, nCurSeq, nOldSeq );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect1, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
#ifdef SP_DEMO
	return (false);
#else
	return UpdateAspect( 1, ser, nCurSeq, nOldSeq );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect2, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
#ifdef SP_DEMO
	return (false);
#else
	return UpdateAspect( 2, ser, nCurSeq, nOldSeq );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect3, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
#ifdef SP_DEMO
	return (false);
#else
	return UpdateAspect( 3, ser, nCurSeq, nOldSeq );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect4, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
#ifdef SP_DEMO
	return (false);
#else
	return UpdateAspect( 4, ser, nCurSeq, nOldSeq );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect5, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
#ifdef SP_DEMO
	return (false);
#else
	return UpdateAspect( 5, ser, nCurSeq, nOldSeq );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect6, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
#ifdef SP_DEMO
	return (false);
#else
	return UpdateAspect( 6, ser, nCurSeq, nOldSeq );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect7, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
#ifdef SP_DEMO
	return (false);
#else
	return UpdateAspect( 7, ser, nCurSeq, nOldSeq );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, RMI_UnreliableOrdered, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return HandleRMI( ser, eNRT_UnreliableUnordered, eMPF_BlocksStateChange );
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, RMI_ReliableUnordered, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return HandleRMI( ser, eNRT_ReliableUnordered, eMPF_BlocksStateChange );
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, RMI_ReliableOrdered, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	return HandleRMI( ser, eNRT_ReliableOrdered, eMPF_BlocksStateChange );
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, RMI_Attachment, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return HandleRMI( ser, eNRT_NumReliabilityTypes, true );
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAuthority, eNRT_UnreliableUnordered, eMPF_BlocksStateChange | eMPF_DecodeInSync)
{
	bool ok = false;
	SReceiveContext ctx = CreateReceiveContext( ser, 7, nCurSeq, nOldSeq, &ok );
	if (!ok)
		return false;
	return GetHistory(eH_Auth)->ReadCurrentValue( ctx, !IgnoringCurrentObject() );
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, VoiceData, eNRT_UnreliableUnordered, 0)
{
	SNetObjectID id;
	TVoicePacketPtr pkt=CVoicePacket::Allocate();

	ser.Value("object", id, 'eid');

	pkt->Serialize(ser);

	ReceivedVoice(id);
	if (Context() && Context()->GetVoiceContextImpl())
		Context()->GetVoiceContextImpl()->OnClientVoicePacket(id,pkt);

	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile0, eNRT_UnreliableUnordered, 0)
{
#ifdef SP_DEMO
	return (false);
#else
	return SetAspectProfileMessage( 0, ser );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile1, eNRT_UnreliableUnordered, 0)
{
#ifdef SP_DEMO
	return (false);
#else
	return SetAspectProfileMessage( 1, ser );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile2, eNRT_UnreliableUnordered, 0)
{
#ifdef SP_DEMO
	return (false);
#else
	return SetAspectProfileMessage( 2, ser );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile3, eNRT_UnreliableUnordered, 0)
{
#ifdef SP_DEMO
	return (false);
#else
	return SetAspectProfileMessage( 3, ser );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile4, eNRT_UnreliableUnordered, 0)
{
#ifdef SP_DEMO
	return (false);
#else
	return SetAspectProfileMessage( 4, ser );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile5, eNRT_UnreliableUnordered, 0)
{
#ifdef SP_DEMO
	return (false);
#else
	return SetAspectProfileMessage( 5, ser );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile6, eNRT_UnreliableUnordered, 0)
{
#ifdef SP_DEMO
	return (false);
#else
	return SetAspectProfileMessage( 6, ser );
#endif
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile7, eNRT_UnreliableUnordered, 0)
{
#ifdef SP_DEMO
	return (false);
#else
	return SetAspectProfileMessage( 7, ser );
#endif
}

#if ENABLE_SESSION_IDS
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, InitSessionData, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	CSessionID sessionID;
	sessionID.SerializeWith(ser);
	Context()->SetSessionID(sessionID);
	return true;
}
#endif

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CClientContextView, BeginBreakStream, eNRT_ReliableUnordered, 0)
{
	if (param.brk < 0 || param.brk > MAX_BREAK_STREAMS)
	{
		NetWarning("Illegal break id %d", param.brk);
		return false;
	}
	if (m_pBreakOps[param.brk])
	{
		NetWarning("Trying to use a breakage stream for two breaks... illegal [breakid=%d]", param.brk);
		return false;
	}
	m_pBreakOps[param.brk] = new SNetClientBreakDescription();
	return true;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CClientContextView, DeclareBrokenProduct, eNRT_ReliableUnordered, 0)
{
	if (param.brk < 0 || param.brk > MAX_BREAK_STREAMS)
	{
		NetWarning("Illegal break id %d", param.brk);
		Parent()->Disconnect(eDC_ContextCorruption, "Illegal break id");
		return false;
	}
	if (!m_pBreakOps[param.brk])
	{
		NetWarning("Trying to send break products with no break stream... illegal [breakid=%d, netid=%s]", param.brk, param.id.GetText());
		Parent()->Disconnect(eDC_ContextCorruption, "Trying to send break products with no break stream");
		return false;
	}
	if (!ContextState()->AllocateObject( 0, param.id, 8, false, eST_Collected, NULL ))
	{
		Parent()->Disconnect(eDC_ContextCorruption, "Failed allocating object for broken product");
		return false;
	}
	m_pBreakOps[param.brk]->push_back(param.id);
	return true;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CClientContextView, PerformBreak, eNRT_ReliableUnordered, 0)
{
	if (param.brk < 0 || param.brk > MAX_BREAK_STREAMS)
	{
		NetWarning("Illegal break id %d", param.brk);
		return false;
	}
	if (!m_pBreakOps[param.brk])
	{
		NetWarning("Trying to send break products with no break stream... illegal [breakid=%d]", param.brk);
		return false;
	}
	if (ContextState())
	{
		_smart_ptr<INetBreakagePlayback> pBrk = new CBreakagePlayback(this, m_pBreakOps[param.brk]);
		ContextState()->GetGameContext()->PlaybackBreakage( param.brk, pBrk );
	}
	m_pBreakOps[param.brk] = 0;
	return true;
}

#if ENABLE_ASPECT_HASHING
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect0, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(0, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect1, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(1, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect2, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(2, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect3, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(3, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect4, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(4, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect5, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(5, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect6, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(6, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect7, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(7, ser);
	return true;
}
#endif

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect0, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(0, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect1, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(1, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect2, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(2, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect3, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(3, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect4, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(4, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect5, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(5, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect6, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(6, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect7, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(7, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, FlushMsgs, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	StartFlushUpdates();
	return true;
}

void CClientContextView::BoundCollectedObject( SNetObjectID id, EntityId objId )
{
	FROM_GAME(&CClientContextView::NC_BoundCollectedObject, this, id, objId );
}

void CClientContextView::NC_BoundCollectedObject( SNetObjectID id, EntityId objId )
{
	ASSERT_GLOBAL_LOCK;
	SSimpleObjectIDParams msg(id);
	CServerContextView::SendBoundCollectedObjectWith( msg, Parent() );
	ContextState()->RebindObject( id, objId );
}

bool CClientContextView::SetAspectProfileMessage( uint8 aspect, TSerialize ser )
{
	ASSERT_GLOBAL_LOCK;
	uint8 profile;
	ser.Value( "profile", profile );
	//const SContextObject * pObj = ContextState()->GetContextObject( CurrentObjectID() );
	SContextObjectRef obj = ContextState()->GetContextObject( CurrentObjectID() );
	if (!obj.main)
	{
#if LOG_ENTITYID_ERRORS
		NetLog("Object %s not found for SetAspectProfile", CurrentObjectID().GetText());
#endif
		Parent()->Disconnect(eDC_ContextCorruption, "SetAspectProfile - object not found");
		return false;
	}
	if (!ContextState()->DoSetAspectProfile( obj.main->userID, 1<<aspect, profile, false, false, false ))
	{
#if LOG_ENTITYID_ERRORS
		NetLog("DoSetAspectProfile for object %s failed for SetAspectProfile", CurrentObjectID().GetText());
#endif
		Parent()->Disconnect(eDC_ContextCorruption, "SetAspectProfile - context failed");
		return false;
	}
	bool ok = true;
//	m_objects[CurrentObjectID()].vAspectProfiles[aspect] = profile;
//	ClearAspects( CurrentObjectID(), 1<<aspect );
	return ok;
}

bool CClientContextView::HasRemoteDef( const SNetMessageDef * pDef )
{
	return CServerContextView::ClassHasDef( pDef );
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, UpdatePhysicsTime, eNRT_UnreliableUnordered, 0)
{
	return SetPhysicsTime(param.tm);
}

void CClientContextView::UnboundObject( SNetObjectID id )
{
	CServerContextView::SendCompletedUnbindObjectWith( id, Parent() );
	CContextView::UnboundObject(id);
}

#if ENABLE_DEBUG_KIT
void CClientContextView::BeginWorldUpdate( SNetObjectID obj )
{
	ASSERT_GLOBAL_LOCK;
	IEntity * pEnt = gEnv->pEntitySystem->GetEntity( ContextState()->GetContextObject(obj).main->userID );
	if (!pEnt)
		return;
	IPhysicalEntity * pPhys = pEnt->GetPhysics();
	if (!pPhys)
		return;
	pe_status_pos pos;
	pPhys->GetStatus( &pos );
	m_curWorldPos = pos.pos;
}

void CClientContextView::EndWorldUpdate( SNetObjectID obj )
{
	ASSERT_GLOBAL_LOCK;
	IEntity * pEnt = gEnv->pEntitySystem->GetEntity( ContextState()->GetContextObject(obj).main->userID );
	if (!pEnt)
		return;
	IPhysicalEntity * pPhys = pEnt->GetPhysics();
	if (!pPhys)
		return;
	pe_status_pos pos;
	pPhys->GetStatus( &pos );
	m_netVis.AddSample(obj, 2, pos.pos.GetDistance(m_curWorldPos));

	Vec3 witnessPos, witnessDir;
	if ( GetWitnessDirection(witnessDir) && GetWitnessPosition(witnessPos) )
	{
		DEBUGKIT_LOG_SNAPPING(witnessPos, witnessDir, m_curWorldPos, pos.pos, pEnt->GetClass()->GetName());
	}
}
#endif
