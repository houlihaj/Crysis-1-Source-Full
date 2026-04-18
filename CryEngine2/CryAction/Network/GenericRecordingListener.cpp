#include "StdAfx.h"
#include "GenericRecordingListener.h"
#include "SimulateCreateEntityPart.h"
#include "SimulateRemoveEntityParts.h"
#include "BreakReplicator.h"

CGenericRecordingListener::CGenericRecordingListener() : m_pDef(0), m_pInfo(0) 
{
}

bool CGenericRecordingListener::AcceptJointBroken(const EventPhysJointBroken * pEvt)
{
	GameWarning("CBreakReplicator::AcceptJointBroken: cannot generically replicate broken joints");
	return false;
}

bool CGenericRecordingListener::AcceptUpdateMesh(const EventPhysUpdateMesh * pEvent )
{
	GameWarning("CBreakReplicator::AcceptUpdateMesh: cannot generically replicate meshes");
	return false;
}

bool CGenericRecordingListener::AcceptCreateEntityPart( const EventPhysCreateEntityPart * pEvent )
{
	GameWarning("CBreakReplicator::AcceptUpdateMesh: cannot generically replicate part creation");
	return false; // temp... testing
/*
	if (pEvent->pMeshNew)
	{
		GameWarning("CBreakReplicator::AcceptCreateEntityPart: cannot generically replicate meshes");
		return false;
	}

	m_pDef = CBreakReplicator::SimulateCreateEntityPart;
	SSimulateCreateEntityPart * pInfo = new SSimulateCreateEntityPart;
	pInfo->bInvalid = pEvent->bInvalid != 0;
	pInfo->breakAngImpulse = pEvent->breakAngImpulse;
	pInfo->breakImpulse = pEvent->breakImpulse;
	for (int i=0; i<2; i++)
	{
		pInfo->cutDirLoc[i] = pEvent->cutDirLoc[i];
		pInfo->cutPtLoc[i] = pEvent->cutPtLoc[i];
	}
	pInfo->cutRadius = pEvent->cutRadius;
	pInfo->entNew = pEvent->pEntNew;
	pInfo->entSrc = pEvent->pEntity;
	pInfo->nTotParts = pEvent->nTotParts;
	pInfo->partidNew = pEvent->partidNew;
	pInfo->partidSrc = pEvent->partidSrc;
	m_pInfo = pInfo;

	return true;
*/
}

bool CGenericRecordingListener::AcceptRemoveEntityParts( const EventPhysRemoveEntityParts * pEvent )
{
	m_pDef = CBreakReplicator::SimulateRemoveEntityParts;
	SSimulateRemoveEntityPartsInfo * pInfo = new SSimulateRemoveEntityPartsInfo;
	pInfo->ent = pEvent->pEntity;
	for (int i=0; i<4; i++)
		pInfo->partIds[i] = pEvent->partIds[i];
	pInfo->massOrg = pEvent->massOrg;
	m_pInfo = pInfo;

	return true;
}

void CGenericRecordingListener::EndEvent( INetContext * pCtx )
{
	if (m_spawned.empty())
		return;

	assert(m_pDef);
	assert(m_pInfo);
	SNetBreakDescription def;
	def.pMessagePayload = m_pInfo;
	def.pEntities = &m_spawned[0];
	def.nEntities = m_spawned.size();
	pCtx->LogBreak( def );

	m_pDef = 0;
	m_pInfo = 0;
	m_spawned.resize(0);
}

void CGenericRecordingListener::OnRemove(IEntity * pEntity)
{
	for (int i=0; i<m_spawned.size(); i++)
		if (m_spawned[i] == pEntity->GetId())
			m_spawned[i] = 0;
}

void CGenericRecordingListener::OnSpawn(IEntity * pEntity, SEntitySpawnParams& params)
{
	m_spawned.push_back(pEntity->GetId());
}
