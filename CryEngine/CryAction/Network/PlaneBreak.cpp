/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2007.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  network breakability: Plane breaks
 -------------------------------------------------------------------------
 History:
 - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/

#include "StdAfx.h"
#include "PlaneBreak.h"
#include "BreakReplicator.h"
#include "NetworkCVars.h"

void LogPlaneBreak( const char * from, const SBreakEvent& evt )
{
/*
	if (!pEnt)
		return;

	if (CNetworkCVars::Get().BreakageLog)
	{
		CryLog("[brk] DeformPhysicalEntity on %s @ (%.8f,%.8f,%.8f); n=(%.8f,%.8f,%.8f); energy=%.8f", from, p.x, p.y, p.z, n.x, n.y, n.z, energy);
		CObjectSelector sel(pEnt);
		CryLog("[brk]    selector is %s", sel.GetDescription().c_str());
		switch (pEnt->GetiForeignData())
		{
		case PHYS_FOREIGN_ID_STATIC:
			if (IRenderNode * pRN = (IRenderNode*)pEnt->GetForeignData(PHYS_FOREIGN_ID_STATIC))
			{
				CryLog("[brk]    name is %s", pRN->GetName());
				CryLog("[brk]    entity class name is %s", pRN->GetEntityClassName());
				CryLog("[brk]    debug string is %s", pRN->GetDebugString().c_str());
			}
			break;
		case PHYS_FOREIGN_ID_ENTITY:
			if (IEntity * pEntity = (IEntity*)pEnt->GetForeignData(PHYS_FOREIGN_ID_ENTITY))
			{
				CryLog("[brk]    name is %s", pEntity->GetName());
				Matrix34 m = pEntity->GetWorldTM();
				CryLog("[brk]    world tm:");
				for (int i=0; i<3; i++)
				{
					Vec4 row = m.GetRow4(i);
					CryLog("[brk]       %+12.8f %+12.8f %+12.8f %+12.8f", row[0], row[1], row[2], row[3]);
				}
			}
			break;
		}
	}
*/
}

void SPlaneBreakParams::SerializeWith( TSerialize ser )
{
	ser.Value("breakId", breakId, 'brId');
	bool isEnt = breakEvent.itype == PHYS_FOREIGN_ID_ENTITY;
	ser.Value("isEnt", isEnt, 'bool');
	breakEvent.itype = isEnt? PHYS_FOREIGN_ID_ENTITY : PHYS_FOREIGN_ID_STATIC;
	if (isEnt)
	{
		ser.Value("ent", breakEvent.idEnt, 'eid');
		ser.Value("pos", breakEvent.pos);
		ser.Value("rot", breakEvent.rot);
		ser.Value("scale", breakEvent.scale);
	}
	else
	{
		ser.Value("objpos", breakEvent.objPos);
		ser.Value("objcenter", breakEvent.objCenter);
		ser.Value("objvol", breakEvent.objVolume);
	}
	ser.Value("pt", breakEvent.pt);
	ser.Value("n", breakEvent.n);
	ser.Value("energy", breakEvent.energy);
	ser.Value("penetration", breakEvent.penetration);
	ser.Value("idmat[0]", breakEvent.idmat[0]);
	ser.Value("idmat[1]", breakEvent.idmat[1]);
	ser.Value("partid[0]", breakEvent.partid[0]);
	ser.Value("partid[1]", breakEvent.partid[1]);
	ser.Value("mass[0]", breakEvent.mass[0]);
	ser.Value("mass[1]", breakEvent.mass[1]);
	ser.Value("vloc[0]", breakEvent.vloc[0]);
	ser.Value("vloc[1]", breakEvent.vloc[1]);
	ser.Value("idxChunkId0", breakEvent.idxChunkId0);
	ser.Value("nChunks", breakEvent.nChunks);
	ser.Value("seed", breakEvent.seed);
	ser.Value("radius", breakEvent.radius);
}

void SPlaneBreak::GetAffectedRegion(AABB& rgn)
{
	rgn.min = breakEvents[0].pt - Vec3(20,20,20);
	rgn.max = breakEvents[0].pt + Vec3(20,20,20);
}

void SPlaneBreak::AddSendables( INetSendableSink * pSink, int32 brkId )
{
	if (breakEvents[0].itype == PHYS_FOREIGN_ID_ENTITY)
		pSink->NextRequiresEntityEnabled( breakEvents[0].idEnt );
	for (int i=0; i<breakEvents.size(); ++i)
		CBreakReplicator::SendPlaneBreakWith( SPlaneBreakParams(brkId, breakEvents[i]), pSink );
	AddProceduralSendables(brkId, pSink);
}

CPlaneBreak::CPlaneBreak( const SBreakEvent& be ) : IProceduralBreakType(ePBTF_ChainBreaking, 'd'), m_pPhysEnt(0)
{
	m_absorbIdx = 1;
	m_bes.push_back(be);
}

bool CPlaneBreak::AttemptAbsorb( const IProceduralBreakTypePtr& pBT )
{
	if (pBT->type == type)
	{
		CPlaneBreak * pBrk = (CPlaneBreak*)(IProceduralBreakType*)pBT;
		if (GetObjectSelector(1) == pBrk->GetObjectSelector(1))
		{
			m_bes.push_back(pBrk->m_bes[0]);
			return true;
		}
	}
	return false;
}

void CPlaneBreak::AbsorbStep()
{
	if (m_absorbIdx >= m_bes.size())
	{
		GameWarning("CPlaneBreak::AbsorbStep: too many absorbs for structure (or there was an earlier message and we invalidated m_absorbIdx)");
		return;
	}
	if (!m_pPhysEnt)
	{
		GameWarning("CPlaneBreak::AbsorbStep: attempt to deform a null entity");
		m_absorbIdx = m_bes.size();
		return;
	}
	assert(m_absorbIdx < m_bes.size());
	if (m_absorbIdx >= m_bes.size())
		return;
	PreparePlaybackForEvent(m_absorbIdx);
	if (!m_pPhysEnt)
	{
		GameWarning("CPlaneBreak::AbsorbStep: attempt to deform a null entity after preparing index %d", m_absorbIdx);
		m_absorbIdx = m_bes.size();
		return;
	}
	LogPlaneBreak( "CLIENT", m_bes[m_absorbIdx] );
	CActionGame::PerformPlaneBreak( GeneratePhysicsEvent(m_bes[m_absorbIdx]), &m_bes[m_absorbIdx] );

//	if (!gEnv->pPhysicalWorld->DeformPhysicalEntity(m_pPhysEnt, m_bes[m_absorbIdx].pt, -m_bes[m_absorbIdx].n, m_bes[m_absorbIdx].energy))
//		GameWarning("[brk] DeformPhysicalEntity failed");

	++m_absorbIdx;
}

int CPlaneBreak::GetVirtualId( IPhysicalEntity * pEnt )
{
	int iForeignData = pEnt->GetiForeignData();
	void * pForeignData = pEnt->GetForeignData(iForeignData);
	if (iForeignData == PHYS_FOREIGN_ID_ENTITY && m_bes[0].itype == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity * pEnt = (IEntity*) pForeignData;
		if (pEnt->GetId() == m_bes[0].idEnt)
			return 1;
	}
	else if (iForeignData == PHYS_FOREIGN_ID_STATIC)
	{
		IRenderNode *rn = (IRenderNode *) pForeignData;
		if (IsOurStatObj(rn))
			return 1;
	}
	return 0;
}

CObjectSelector CPlaneBreak::GetObjectSelector(int idx)
{
	if (idx == 1)
	{
		if (m_bes[0].itype == PHYS_FOREIGN_ID_ENTITY)
			return CObjectSelector(m_bes[0].idEnt);
		else if (m_bes[0].itype == PHYS_FOREIGN_ID_STATIC)
			return CObjectSelector(m_bes[0].objPos, m_bes[0].objCenter, m_bes[0].objVolume);
	}
	return CObjectSelector();
}

_smart_ptr<SProceduralBreak> CPlaneBreak::CompleteSend()
{
	SPlaneBreak * pPayload = new SPlaneBreak;
	pPayload->breakEvents = m_bes;
	return pPayload;
}

void CPlaneBreak::PreparePlayback()
{
	PreparePlaybackForEvent(0);
}

void CPlaneBreak::PreparePlaybackForEvent( int event )
{
	m_pPhysEnt = 0;
	EntityId srcEnt = 0;
	if (m_bes[event].itype == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity * pEnt = gEnv->pEntitySystem->GetEntity(m_bes[event].idEnt);
		if (!pEnt)
		{
			GameWarning("Unable to find entity to perform Plane break on");
			return;
		}
		pEnt->SetPosRotScale(m_bes[event].pos, m_bes[event].rot, m_bes[event].scale, ENTITY_XFORM_TEMP_MOVE);
		m_pPhysEnt = pEnt->GetPhysics();
	}
	else if (m_bes[event].itype == PHYS_FOREIGN_ID_STATIC)
	{
		m_pPhysEnt = CObjectSelector(m_bes[event].objPos, m_bes[event].objCenter, m_bes[event].objVolume).Find();
		if (!m_pPhysEnt)
			GameWarning("Unable to find static object to perform Plane break on");
	}
}

void CPlaneBreak::BeginPlayback( bool hasJointBreaks )
{
	if (hasJointBreaks && CNetworkCVars::Get().BreakageLog)
		GameWarning("[brk] Plane break has joint breaks attached");
	if (m_pPhysEnt)
	{
		LogPlaneBreak( "CLIENT", m_bes[0] );
		CActionGame::PerformPlaneBreak( GeneratePhysicsEvent(m_bes[0]), &m_bes[0] );
	}
	else
		GameWarning("[brk] No physical entity to deform");
}

bool CPlaneBreak::GotExplosiveObjectState( const SExplosiveObjectState * pState )
{
	return false;
}

bool CPlaneBreak::AllowComplete( const SProceduralBreakRecordingState& state )
{
	return state.numEmptySteps > 3;
}

const EventPhysCollision& CPlaneBreak::GeneratePhysicsEvent( const SBreakEvent& bev )
{
	static EventPhysCollision epc;
	epc = EventPhysCollision();
	epc.pEntity[0] = 0;
	epc.iForeignData[0] = -1;
	epc.pEntity[1] = 0;
	epc.iForeignData[1] = -1;
	epc.pt=bev.pt; epc.n=bev.n;
	epc.vloc[0]=bev.vloc[0]; epc.vloc[1]=bev.vloc[1];
	epc.mass[0]=bev.mass[0]; epc.mass[1]=bev.mass[1];
	epc.idmat[0]=bev.idmat[0]; epc.idmat[1]=bev.idmat[1];
	epc.partid[0]=bev.partid[0]; epc.partid[1]=bev.partid[1];
	epc.penetration = bev.penetration;
	epc.radius = bev.radius;
	if (m_pPhysEnt)
	{
		epc.pEntity[1] = m_pPhysEnt;
		epc.iForeignData[1] = m_pPhysEnt->GetiForeignData();
		epc.pForeignData[1] = m_pPhysEnt->GetForeignData(epc.iForeignData[1]);
	}
	return epc;
}
