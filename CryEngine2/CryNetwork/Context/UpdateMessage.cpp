#include "StdAfx.h"
#include "UpdateMessage.h"
#include "ContextView.h"
#include "History/History.h"
#include "DebugKit/DebugKit.h"
#include "Utils.h"

#if defined(WIN64) && !defined(_CPU_SSE)
#define _CPU_SSE
#endif

CUpdateMessage::CUpdateMessage( const SUpdateMessageConfig& cfg, uint32 syncFlags ) :
	INetSendable(0, cfg.m_pStartUpdateDef->reliability), 
	SUpdateMessageConfig(cfg), 
	m_syncFlags(syncFlags),
	m_wasSent(false)
{
	//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(m_netID);
	SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(m_netID);
	if (ctxObj.main)
	{
		SetPulses( ctxObj.xtra->pPulseState );
	}
	++g_objcnt.updateMessage;
}

CUpdateMessage::~CUpdateMessage()
{
	--g_objcnt.updateMessage;
}

EMessageSendResult CUpdateMessage::Send( INetSender * pSender )
{
	NET_ASSERT(m_handle);

	m_anythingSent = false;
	m_immediateResendAspects = 0;

	if (!m_pView->IsObjectBound(m_netID))
		return eMSR_FailedMessage;

	SContextViewObject * pViewObj = &m_pView->m_objects[m_netID.id];
	if (m_handle != pViewObj->activeHandle)
		return eMSR_FailedMessage;
	SContextViewObjectEx * pViewObjEx = &m_pView->m_objectsEx[m_netID.id];

#if defined _CPU_SSE
	_mm_prefetch((const char *)pViewObj, _MM_HINT_T0);
	if (m_pView->Context()->IsMultiplayer())
	{
		_mm_prefetch((const char *)pViewObjEx, _MM_HINT_T0);
		_mm_prefetch((const char *)&pViewObjEx->partialUpdateReceived[0], _MM_HINT_T0);
		_mm_prefetch((const char *)&pViewObjEx->historyElems[0], _MM_HINT_T0);
	}
#endif

	for (int i=0; i<eH_NUM_HISTORIES; i++)
		m_sentHistories[i] = 0;
	for (int i=0; i<NumAspects; i++)
		m_sentAspectVersions[i] = ~uint32(0);

	SHistorySyncContext hsc;

	// determine which aspects we need to sync for each history
	EMessageSendResult res = InitialChecks(hsc, pSender, pViewObj, pViewObjEx);
	if (res != eMSR_SentOk)
		return res;

	bool containsData = false;
	for (int history = 0; history < eH_NUM_HISTORIES; history++)
		containsData |= m_maybeSendHistories[history] != 0;
	if (!containsData)
		containsData = AnyAttachments(m_pView, m_netID);
	if (!containsData)
		containsData = CheckHook();

	if (containsData)
	{
		DEBUGKIT_SET_OBJECT(m_pView->ContextState()->GetContextObject(m_netID).main->userID);

		TUpdateMessageBrackets brackets = m_pView->GetUpdateMessageBrackets();

		res = WorstMessageSendResult(res, WriteHook( eWH_BeforeBegin, pSender ));
		if (res != eMSR_SentOk)
			return res;
		UpdateMsgHandle();
		if (brackets.first == m_pStartUpdateDef)
			pSender->BeginUpdateMessage( m_netID );
		else
		{
			pSender->BeginMessage( m_pStartUpdateDef );
			pSender->ser.Value("netID", m_netID, 'eid');
		}
		res = WorstMessageSendResult(res, WriteHook( eWH_DuringBegin, pSender ));
		res = WorstMessageSendResult(res, SendMain(hsc, pSender));
		if (brackets.first == m_pStartUpdateDef)
			pSender->EndUpdateMessage();
		else
			pSender->BeginMessage( m_pView->m_config.pEndUpdateMsg );

		DEBUGKIT_SET_OBJECT(0);

		NET_ASSERT(m_anythingSent);
	}
	else
	{
		UpdateMsgHandle();
	}

	return res;
}

void CUpdateMessage::UpdateMsgHandle()
{
	m_pView->m_objects[m_netID.id].msgHandle = SSendableHandle();
	if (m_immediateResendAspects)
		m_pView->ChangedObject( m_netID, eCOF_NeverSubstitute, m_immediateResendAspects );
}

EMessageSendResult CUpdateMessage::InitialChecks(SHistorySyncContext& hsc, INetSender * pSender, SContextViewObject * pViewObj, SContextViewObjectEx * pViewObjEx)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_NETWORK);

	if (!m_pView->IsObjectBound(m_netID))
	{
		NetWarning("UpdateMessage: No such object %s", m_netID.GetText());
		return eMSR_FailedMessage;
	}

	//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(m_netID);
	SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(m_netID);
	if (!ctxObj.main || !ctxObj.main->userID)
	{
		NetWarning("UpdateMessage: No such object %s", m_netID.GetText());
		return eMSR_FailedMessage;
	}
	for (int history = 0; history < eH_NUM_HISTORIES; history ++)
		m_maybeSendHistories[history] = 0xff;
	m_maybeSendHistories[eH_AspectData] = m_pView->GetSentAspects(m_netID, (m_syncFlags & eSCF_AssumeEnabled) != 0, NULL);
	m_maybeSendHistories[eH_Profile] = m_pView->Context()->ServerManagedProfileAspects();
	if (!ctxObj.main->bOwned || ctxObj.main->spawnType == eST_Collected)
		m_maybeSendHistories[eH_Profile] = 0;

	if (m_syncFlags & eSCF_EnsureEnabled)
	{
		if (!m_pView->IsObjectEnabled(m_netID))
		{
			NetWarning("UpdateMessage: Object %s not enabled", m_netID.GetText());
			return eMSR_FailedMessage;
		}
	}

	SSyncContext ctx;
	ctx.basisSeq = pSender->nBasisSeq;
	ctx.currentSeq = pSender->nCurrentSeq;
	ctx.flags = m_syncFlags;
	ctx.objId = m_netID;
	ctx.ctxObj = ctxObj;
	ctx.pViewObj = pViewObj;
	ctx.pViewObjEx = pViewObjEx;
	ctx.pView = m_pView;

	if (pViewObj->locks)
		return eMSR_NotReady;

	uint8 partialUpdateForces = 0;
	bool assumeAuthority = false;
	uint8 sentAspectMask = m_pView->GetSentAspects(m_netID, (m_syncFlags & eSCF_AssumeEnabled) != 0, &assumeAuthority);
	bool isLocal = ctx.pView->IsLocal();
	bool multiplayer = ctx.pView->Context()->IsMultiplayer();
	if (multiplayer && !isLocal)
	{
		CBitIter iter(sentAspectMask);
		int aspect;
		while (iter.Next(aspect))
			partialUpdateForces |= ((g_time - pViewObjEx->partialUpdateReceived[aspect]).GetSeconds() < 1.0f || pViewObjEx->partialUpdatesRemaining[aspect] > 0) << aspect;
		m_maybeSendHistories[eH_AspectData] &= pViewObjEx->dirtyAspects;
	}
	m_maybeSendHistories[eH_AspectData] |= partialUpdateForces;
	m_immediateResendAspects = partialUpdateForces;

	if (multiplayer && !isLocal)
	{
		for (int history = 0; history < eH_NUM_HISTORIES; history++)
		{
			CHistory * pHistory = m_pView->GetHistory((EHistory)history);

			uint8 aspectMask = m_maybeSendHistories[history] & pHistory->indexMask;
			bool isAspectData = (history == eH_AspectData);
			bool sendingAuth = m_maybeSendHistories[eH_Auth] != 0;
			uint8 mustSync = ((isAspectData & sendingAuth) * sentAspectMask);
			mustSync |= isAspectData * partialUpdateForces;
			aspectMask |= mustSync;
			if (aspectMask)
			{
				CBitIter bitIter(aspectMask & ~mustSync);
				int i;
				while (bitIter.Next(i))
				{
					ctx.index = i;
					aspectMask &= ~(int(!hsc.ctx[history][i].NeedToSync(pHistory, ctx)) << i);
				}
				bitIter = CBitIter(aspectMask & mustSync);
				while (bitIter.Next(i))
				{
					ctx.index = i;
					aspectMask &= ~(int(!hsc.ctx[history][i].PrepareToSync(pHistory, ctx)) << i);
				}
			}
			m_maybeSendHistories[history] = aspectMask;
		}
	}
	else
	{
		for (int i=0; i<4; i++)
			m_maybeSendHistories[i] = 0;
	}

	return eMSR_SentOk;
}

void CUpdateMessage::GetPositionInfo( SMessagePositionInfo& pos )
{
	//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(m_netID);
	pos.obj = m_netID;
	SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(m_netID);
	if (!ctxObj.main)
		return;
	pos.havePosition = ctxObj.xtra->hasPosition;
	pos.position = ctxObj.xtra->position;
	pos.haveDrawDistance = ctxObj.xtra->hasDrawDistance;
	pos.drawDistance = ctxObj.xtra->drawDistance;
}

EMessageSendResult CUpdateMessage::SendMain( SHistorySyncContext& hsc, INetSender * pSender )
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_NETWORK);

	SNetChannelEvent event;
	event.event = eNCE_SentUpdate;
	event.id = m_netID;
	event.seq = pSender->nCurrentSeq;
	m_pView->ContextState()->BroadcastChannelEvent( m_pView->Parent(), &event );

	EMessageSendResult res = eMSR_SentOk;

	// validate that the said object still exists
	//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(m_netID);
	if (!m_pView->IsObjectBound(m_netID))
	{
		NET_ASSERT(false);
		return eMSR_FailedMessage;
	}
	SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(m_netID);
	SContextViewObject * pViewObj = &m_pView->m_objects[m_netID.id];
	SContextViewObjectEx * pViewObjEx = &m_pView->m_objectsEx[m_netID.id];
	NET_ASSERT(ctxObj.main && pViewObj && m_pView->IsObjectBound(m_netID));

	// check if we have anything to do before sending the begin message
//	bool hasAttachments = false;

	//NetLog("update object %d", pUpdate->m_netID);

	NET_ASSERT( m_netID );
	// which aspects have been reset that we need to send out notifications for
	res = WorstMessageSendResult(res, SendAttachments( pSender, eRAT_PreAttach ));

	// prepare sync context as well as we can (updated later for per-aspect stuff)
	SSendContext ctx;
	ctx.basisSeq = pSender->nBasisSeq;
	ctx.currentSeq = pSender->nCurrentSeq;
	ctx.flags = m_syncFlags;
	ctx.objId = m_netID;
	ctx.ctxObj = ctxObj;
	ctx.pSender = pSender;
	ctx.pSentAlready = m_sentHistories;
	ctx.pView = m_pView;
	ctx.pViewObj = pViewObj;

	// synchronize histories, one at a time
	int aspect;
	for (int history = 0; res == eMSR_SentOk && history < eH_NUM_HISTORIES; history ++)
	{
		CHistory * pHistory = m_pView->GetHistory((EHistory) history);
		uint8 aspectMask = pHistory->indexMask & m_maybeSendHistories[history];
		if (!aspectMask)
			continue;
		CBitIter itAspects(aspectMask);
		int aspectsSeen = 0;
		while (itAspects.Next(aspect) && res == eMSR_SentOk)
		{
			NET_ASSERT(!(aspectsSeen & (1<<aspect)));
			aspectsSeen |= (1<<aspect);

			ctx.index = aspect;
			switch (hsc.ctx[history][aspect].Send( ctx ))
			{
			case eHSR_Ok:
				m_sentHistories[history] |= 1 << aspect;
				SentData();
				break;
			case eHSR_Failed:
				res = eMSR_FailedMessage;
				break;
			case eHSR_NotNeeded:
				break;
			}
		}
	}

	if (res == eMSR_SentOk)
		res = WorstMessageSendResult(res, SendAttachments( pSender, eRAT_PostAttach ));

	if (res == eMSR_SentOk && m_pView->Context()->IsMultiplayer() && pViewObjEx)
	{
		CBitIter sentAspectDataIter(m_sentHistories[eH_AspectData]);
		while (sentAspectDataIter.Next(aspect))
		{
			int& cnt = pViewObjEx->partialUpdatesRemaining[aspect];
			cnt -= (cnt != 0);

			m_sentAspectVersions[aspect] = ctxObj.xtra->vAspectDataVersion[aspect];
		}

		pViewObjEx->dirtyAspects = 0;
	}

	if (m_wasSent)
	{
		NET_ASSERT(res == eMSR_SentOk);
	}

	return res;
}

void CUpdateMessage::UpdateState( uint32 nFromSeq, ENetSendableStateUpdate update )
{
	if (m_pView->IsDead())
		return;

	SNetChannelEvent event;
	event.event = update == eNSSU_Ack? eNCE_AckedUpdate : eNCE_NackedUpdate;
	event.id = m_netID;
	event.seq = nFromSeq;
	m_pView->ContextState()->BroadcastChannelEvent( m_pView->Parent(), &event );

	// quick validation useful for debugging...
	switch (update)
	{
	case eNSSU_Ack:
	case eNSSU_Nack:
		break;
	case eNSSU_Requeue:
		m_wasSent = false;
		break;
	case eNSSU_Rejected:
		NET_ASSERT(!m_wasSent);
		break;
	}

	//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(m_netID);
	SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(m_netID);
	bool isBound = m_pView->IsObjectBound(m_netID);
	if (update != eNSSU_Rejected)
	{
		if (ctxObj.main)
		{
			if (isBound)
			{
				SContextViewObject * pViewObj = &m_pView->m_objects[m_netID.id];
				SContextViewObjectEx * pViewObjEx = &m_pView->m_objectsEx[m_netID.id];
				// prepare sync context as well as we can (updated later for per-aspect stuff)
				SSyncContext ctx;
				ctx.currentSeq = nFromSeq;
				ctx.flags = m_syncFlags;
				ctx.objId = m_netID;
				ctx.ctxObj = ctxObj;
				ctx.pSentAlready = m_sentHistories;
				ctx.pView = m_pView;
				ctx.pViewObj = pViewObj;
				ctx.pViewObjEx = pViewObjEx;

				for (int history = 0; history < eH_NUM_HISTORIES; history ++)
				{
					if (!m_sentHistories[history])
						continue;
					CHistory * pHistory = m_pView->GetHistory((EHistory) history);
					for (int aspect = 0; aspect < NumAspects; aspect++)
					{
						if ((m_sentHistories[history] & (1<<aspect)) == 0)
							continue;
						ctx.index = aspect;
						pHistory->Ack( ctx, update == eNSSU_Ack );
					}
				}
			}
		}
		if (m_pView->Context()->IsMultiplayer() && isBound && ctxObj.xtra)
		{
			SContextViewObjectEx * pViewObjEx = &m_pView->m_objectsEx[m_netID.id];
			if (update == eNSSU_Ack)
			{
				int aspect;
				CBitIter iter(m_sentHistories[eH_AspectData]);
				while (iter.Next(aspect))
				{
					uint32 sent = m_sentAspectVersions[aspect];
					uint32& acked = pViewObjEx->ackedAspectVersions[aspect];
					if (sent != ~uint32(0) && (sent > acked || acked == ~uint32(0)))
						acked = sent;
				}
			}
			else
			{
				pViewObjEx->dirtyAspects |= m_sentHistories[eH_AspectData];
			}
		}
	}

	UpdateAttachmentsState( nFromSeq, update );
}

bool CUpdateMessage::AnyAttachments( CContextView * pView, SNetObjectID id )
{
	// TODO: check logic

	if (!pView->IsObjectBound(id))
		return false;

	SAttachmentIndex idxFirst = { id, 0 };
	SAttachmentIndex idxLast = { id, ~uint32(0) };

	for (int i=0; i<2; i++)
	{
		CContextView::TAttachmentMap::const_iterator iterFirst = pView->m_pAttachments[i]->lower_bound( idxFirst );
		CContextView::TAttachmentMap::const_iterator iterLast = pView->m_pAttachments[i]->upper_bound( idxLast );

		if (iterFirst != iterLast)
			return true;
	}

	return false;
}

void CUpdateMessage::UpdateAttachmentsState( uint32 nFromSeq, ENetSendableStateUpdate update )
{
	for (TAttachments::iterator iter = m_attachments.begin(); iter != m_attachments.end(); ++iter)
	{
		if (iter->pMsg->pListener)
			iter->pMsg->pListener->OnAck( m_pView->Parent(), iter->pMsg->userId, nFromSeq, update == eNSSU_Ack );

		switch (iter->pMsg->reliability)
		{
		case eNRT_UnreliableOrdered:
		case eNRT_UnreliableUnordered:
			update = eNSSU_Ack;
			break;
		case eNRT_ReliableUnordered:
			if (update != eNSSU_Ack)
				m_pView->ScheduleAttachment( iter->pMsg );
			break;
		case eNRT_ReliableOrdered:
			if (update != eNSSU_Ack)
				m_pView->ScheduleAttachment( iter->pMsg, &iter->idx );
			break;
		}

		SNetChannelEvent evt;
		evt.event = update == eNSSU_Ack? eNCE_AckedRMI : eNCE_NackedRMI;
		evt.id = m_pView->ContextState()->GetNetID(iter->pMsg->objId);
		evt.seq = nFromSeq;
		evt.pRMIBody = iter->pMsg;
		m_pView->ContextState()->BroadcastChannelEvent( m_pView->Parent(), &evt );
	}

	m_attachments.resize(0);
}

EMessageSendResult CUpdateMessage::SendAttachments( INetSender * pSender, ERMIAttachmentType attachments )
{
	CContextView::TAttachmentMap& am = *m_pView->m_pAttachments[attachments];
	SAttachmentIndex idxFirst = { m_netID, 0 };
	CContextView::TAttachmentMap::iterator iterFirst = am.lower_bound( idxFirst );

	EMessageSendResult res = eMSR_SentOk;

	CContextView::TAttachmentMap::iterator iter;
	for (iter = iterFirst; iter != am.end() && iter->first.id == m_netID; ++iter)
	{
		IRMIMessageBodyPtr pBody = iter->second;
		const SNetMessageDef * pDef = pBody->pMessageDef;
		if (!pDef)
			pDef = m_pView->m_config.pRMIMsgs[eNRT_NumReliabilityTypes];

		NET_ASSERT(pDef);

		NET_ASSERT(pDef->reliability == eNRT_UnreliableOrdered || pDef->reliability == eNRT_UnreliableUnordered);

		pSender->BeginMessage( pDef );
		SentData();

		SSentAttachment sa;
		sa.idx = iter->first;
		sa.pMsg = iter->second;

		if (pBody->reliability == eNRT_ReliableOrdered)
			sa.lock = m_pView->LockObject( iter->first.id, "RMI" );

		if (pBody->pListener)
			pBody->pListener->OnSend( m_pView->Parent(), pBody->userId, pSender->nCurrentSeq );

		SNetChannelEvent evt;
		evt.event = eNCE_SendRMI;
		evt.id = m_pView->ContextState()->GetNetID(pBody->objId);
		evt.seq = pSender->nCurrentSeq;
		evt.pRMIBody = pBody;
		m_pView->ContextState()->BroadcastChannelEvent( m_pView->Parent(), &evt );

		if (!pBody->pMessageDef)
		{
			uint8 funcID = pBody->funcId;
			pSender->ser.Value( "funcID", funcID );
		}

		pBody->SerializeWith( pSender->ser );

		m_attachments.push_back( sa );
	}

	am.erase( iterFirst, iter );

	return res;
}

size_t CUpdateMessage::GetSize()
{
	return sizeof(*this) +
		m_attachments.capacity() * sizeof(m_attachments[0]);
}

const char * CUpdateMessage::GetDescription()
{
	if (CNetContextState * pContextState = m_pView->ContextState())
	{
		if (m_description.empty())
		{
			//const SContextObject * pObj = m_pView->ContextState()->GetContextObject(m_netID);
			SContextObjectRef ctxObj = pContextState->GetContextObject(m_netID);
			if (!ctxObj.main)
				m_description.Format( "%s [not-existing %s]", GetBasicDescription(), m_netID.GetText() );
			else
				m_description.Format( "%s %s", GetBasicDescription(), ctxObj.main->GetName() );
		}
	}
	else
	{
		m_description.Format( "%s [no context state, old object was %s]", GetBasicDescription(), m_netID.GetText() );
	}
	NET_ASSERT(!m_description.empty());
	return m_description.c_str();
}

// regular update message - allocation optimization (safe to be single threaded since we always call with the global lock)

void CRegularUpdateMessage::UpdateState(uint32 nFromSeq, ENetSendableStateUpdate state)
{
	CUpdateMessage::UpdateState(nFromSeq, state);
}

CRegularUpdateMessage * CRegularUpdateMessage::Create( const SUpdateMessageConfig& cfg, uint32 flags )
{
	ASSERT_GLOBAL_LOCK;
	TMemHdl hdl = MMM().AllocHdl(sizeof(CRegularUpdateMessage));
	CRegularUpdateMessage * p = new (MMM().PinHdl(hdl)) CRegularUpdateMessage( cfg, flags );
	p->m_memhdl = hdl;
	return p;
}

void CRegularUpdateMessage::DeleteThis()
{
	ASSERT_GLOBAL_LOCK;
	MMM_REGION(&m_pView->GetMMM());
	TMemHdl hdl = m_memhdl;
	this->~CRegularUpdateMessage();
	MMM().FreeHdl(hdl);
}
