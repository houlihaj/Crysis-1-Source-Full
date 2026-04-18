#include "StdAfx.h"
#include "BreakagePlayback.h"
#include "ServerContextView.h"
#include "NetCVars.h"

CBreakagePlayback::CBreakagePlayback( CClientContextView * pView, SNetClientBreakDescriptionPtr pBreakOps )
: m_pView(pView)
, m_pBreakOps(pBreakOps)
, m_ofs(0)
{
	++g_objcnt.breakagePlayback;
}

CBreakagePlayback::~CBreakagePlayback()
{
	SCOPED_GLOBAL_LOCK;
	--g_objcnt.breakagePlayback;
	bool anyUncollected = false;
	for (int i=0; i<m_pBreakOps->size(); i++)
	{
		if ((*m_pBreakOps)[i])
		{
			NetWarning("[brk] uncollected item idx=%d netid=%s", i, (*m_pBreakOps)[i].GetText());
			anyUncollected = true;
			if (m_pView->Parent())
				m_pView->Parent()->NetAddSendable( new CSimpleNetMessage<SSimpleObjectIDParams>(SSimpleObjectIDParams((*m_pBreakOps)[i]), CServerContextView::SkippedCollectedObject), 0, 0, 0 );
		}
	}
}

void CBreakagePlayback::SpawnedEntity( int idx, EntityId id )
{
	SCOPED_GLOBAL_LOCK;
	if (idx >= m_pBreakOps->size() || idx < 0 || !(*m_pBreakOps)[idx])
		return;
	if (m_gotBreakOps.size() <= idx)
		m_gotBreakOps.resize(idx+1, 0);
	m_pView->BoundCollectedObject( (*m_pBreakOps)[idx], id );

	m_pView->Context()->RequestRemoteUpdate(id, 0xff);

#if LOG_BREAKABILITY
	NetLog("[brk] collected index %d as %d", idx, id);
#endif
	(*m_pBreakOps)[idx] = SNetObjectID();
	m_gotBreakOps[idx] = id;
}

EntityId CBreakagePlayback::GetEntityIdForIndex( int idx )
{
	if (idx < 0 || idx >= m_gotBreakOps.size())
		return 0;
	return m_gotBreakOps[idx];
}
