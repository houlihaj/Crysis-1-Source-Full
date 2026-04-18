////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   PhysicsEventListener.cpp
//  Version:     v1.00
//  Created:     18/8/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PhysicsEventListener.h"
#include "EntityObject.h"
#include "Entity.h"
#include "EntitySystem.h"
#include "EntityCVars.h"
#include "BreakableManager.h"

#include <IPhysics.h>
#include <ParticleParams.h>
#include "IAISystem.h"

//////////////////////////////////////////////////////////////////////////
CPhysicsEventListener::CPhysicsEventListener( CEntitySystem *pEntitySystem,IPhysicalWorld *pPhysics )
{
	assert(pEntitySystem);
	assert(pPhysics);
	m_pEntitySystem = pEntitySystem;

	m_pPhysics = pPhysics;

	m_pPhysics->AddEventClient( EventPhysStateChange::id,OnStateChange,1 );
	m_pPhysics->AddEventClient( EventPhysBBoxOverlap::id,OnBBoxOverlap,1 );
	m_pPhysics->AddEventClient( EventPhysPostStep::id,OnPostStep,1 );
	m_pPhysics->AddEventClient( EventPhysUpdateMesh::id,OnUpdateMesh,1 );
	m_pPhysics->AddEventClient( EventPhysUpdateMesh::id,OnUpdateMesh,0 );
	m_pPhysics->AddEventClient( EventPhysCreateEntityPart::id,OnCreatePhysEntityPart,1 );
	m_pPhysics->AddEventClient( EventPhysCreateEntityPart::id,OnCreatePhysEntityPart,0 );
	m_pPhysics->AddEventClient( EventPhysRemoveEntityParts::id,OnRemovePhysEntityParts,1 );
	m_pPhysics->AddEventClient( EventPhysCollision::id,OnCollision,1,1000.0f );
	m_pPhysics->AddEventClient( EventPhysJointBroken::id,OnJointBreak,1 );
}

//////////////////////////////////////////////////////////////////////////
CPhysicsEventListener::~CPhysicsEventListener()
{
	m_pPhysics->SetPhysicsEventClient(NULL);
}

//////////////////////////////////////////////////////////////////////////
CEntity* CPhysicsEventListener::GetEntity( IPhysicalEntity *pPhysEntity )
{
	assert(pPhysEntity);
	CEntity *pEntity = (CEntity*)pPhysEntity->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
	return pEntity;
}

//////////////////////////////////////////////////////////////////////////
CEntity* CPhysicsEventListener::GetEntity( void *pForeignData,int iForeignData )
{
	if (PHYS_FOREIGN_ID_ENTITY == iForeignData)
	{
		return (CEntity*)pForeignData;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
int CPhysicsEventListener::OnPostStep( const EventPhys *pEvent )
{
	EventPhysPostStep *pPostStep = (EventPhysPostStep*)pEvent;
	CEntity *pCEntity = GetEntity(pPostStep->pForeignData,pPostStep->iForeignData);
	IRenderNode *pRndNode = 0;
	if (pCEntity)
	{
		CPhysicalProxy *pPhysProxy = pCEntity->GetPhysicalProxy();
		if (pPhysProxy)// && pPhysProxy->GetPhysicalEntity())
			pPhysProxy->OnPhysicsPostStep(pPostStep);
		pRndNode = pCEntity->GetRenderProxy();
	}
	else if (pPostStep->iForeignData == PHYS_FOREIGN_ID_ROPE)
	{
		IRopeRenderNode *pRenderNode = (IRopeRenderNode*)pPostStep->pForeignData;
		if (pRenderNode)
		{
			pRenderNode->OnPhysicsPostStep();
		}
		pRndNode = pRenderNode;
	}

	if (pRndNode)
	{
		pe_params_flags pf;
		pe_params_foreign_data pfd;
		int bInvisible=0,bInsideVisarea=0,bFaraway=0;
		int bEnableOpt = CVar::es_UsePhysVisibilityChecks && !gEnv->bMultiplayer;
		bInsideVisarea = pRndNode->GetEntityVisArea()!=0;
		float dist = (pRndNode->GetBBox().GetCenter()-GetISystem()->GetViewCamera().GetPosition()).len2();
		bInvisible = bEnableOpt & (isneg(pRndNode->GetDrawFrame()+10-gEnv->pRenderer->GetFrameID()) | isneg(sqr(CVar::es_MaxPhysDist)-dist));
		if (pRndNode->m_nInternalFlags & IRenderNode::WAS_INVISIBLE ^ (-bInvisible & IRenderNode::WAS_INVISIBLE))
		{
			pPostStep->pEntity->GetParams(&pfd);
			pf.flagsAND = ~pef_invisible;
			pf.flagsOR = -bInvisible & ~(-(pfd.iForeignFlags & PFF_ALWAYS_VISIBLE)>>31) & pef_invisible;
			pPostStep->pEntity->SetParams(&pf);
			(pRndNode->m_nInternalFlags &= ~IRenderNode::WAS_INVISIBLE) |= -bInvisible & IRenderNode::WAS_INVISIBLE;
		}
		if (pRndNode->m_nInternalFlags & IRenderNode::WAS_IN_VISAREA ^ (-bInsideVisarea & IRenderNode::WAS_IN_VISAREA))
		{
			pf.flagsAND = ~pef_ignore_ocean;
			pf.flagsOR = -bInsideVisarea & pef_ignore_ocean;
			pPostStep->pEntity->SetParams(&pf);
			(pRndNode->m_nInternalFlags &= ~IRenderNode::WAS_IN_VISAREA) |= -bInsideVisarea & IRenderNode::WAS_IN_VISAREA;
		}
		bFaraway = bEnableOpt & isneg(sqr(CVar::es_MaxPhysDist)+
			bInvisible*(sqr(CVar::es_MaxPhysDistInvisible)-sqr(CVar::es_MaxPhysDist)) - dist);
		if (bFaraway && !(pRndNode->m_nInternalFlags & IRenderNode::WAS_FARAWAY)) 
		{
			pe_params_foreign_data pfd;
			pPostStep->pEntity->GetParams(&pfd);
			bFaraway &= -(pfd.iForeignFlags & PFF_UNIMPORTANT)>>31;
		}
		if ((-bFaraway & IRenderNode::WAS_FARAWAY) != (pRndNode->m_nInternalFlags & IRenderNode::WAS_FARAWAY))
		{
			pe_params_timeout pto;
			pto.timeIdle = 0;
			pto.maxTimeIdle = bFaraway*CVar::es_FarPhysTimeout;
			pPostStep->pEntity->SetParams(&pto);
			(pRndNode->m_nInternalFlags &= ~IRenderNode::WAS_FARAWAY) |= -bFaraway & IRenderNode::WAS_FARAWAY;
		}
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////////
int CPhysicsEventListener::OnBBoxOverlap( const EventPhys *pEvent )
{
	EventPhysBBoxOverlap *pOverlap = (EventPhysBBoxOverlap*)pEvent;

	CEntity *pCEntity = GetEntity(pOverlap->pForeignData[0],pOverlap->iForeignData[0]);
	CEntity *pCEntityTrg = GetEntity(pOverlap->pForeignData[1],pOverlap->iForeignData[1]);
	if (pCEntity && pCEntityTrg)
	{
		CPhysicalProxy *pPhysProxySrc = pCEntity->GetPhysicalProxy();
		if (pPhysProxySrc)
			pPhysProxySrc->OnContactWithEntity( pCEntityTrg );

		CPhysicalProxy *pPhysProxyTrg = pCEntityTrg->GetPhysicalProxy();
		if (pPhysProxyTrg)
			pPhysProxyTrg->OnContactWithEntity( pCEntity );

	}
	return 1;
}

//////////////////////////////////////////////////////////////////////////
int CPhysicsEventListener::OnStateChange( const EventPhys *pEvent )
{
	EventPhysStateChange *pStateChange = (EventPhysStateChange*)pEvent;
	CEntity *pCEntity = GetEntity(pStateChange->pForeignData,pStateChange->iForeignData);
	if (pCEntity)
	{
		EEntityUpdatePolicy policy = (EEntityUpdatePolicy)pCEntity->m_eUpdatePolicy;
		// If its update depends on physics, physics state defines if this entity is to be updated.
		if (policy == ENTITY_UPDATE_PHYSICS || policy == ENTITY_UPDATE_PHYSICS_VISIBLE)
		{
			int nNewSymClass = pStateChange->iSimClass[1];
			int nOldSymClass = pStateChange->iSimClass[0];
			if (nNewSymClass == SC_ACTIVE_RIGID)
			{
				// Should activate entity if physics is awaken.
				pCEntity->Activate(true);
			}
			else if (nNewSymClass == SC_SLEEPING_RIGID)
			{
				// Entity must go to sleep.
				pCEntity->Activate(false);
				//CallStateFunction(ScriptState_OnStopRollSlideContact, "roll");
				//CallStateFunction(ScriptState_OnStopRollSlideContact, "slide");
			}
		}
		int nNewSymClass = pStateChange->iSimClass[0];
		if (nNewSymClass == SC_ACTIVE_RIGID)
		{
			SEntityEvent event(ENTITY_EVENT_PHYSICS_CHANGE_STATE);
			event.nParam[0] = 1;
			pCEntity->SendEvent(event);
		}
		else if (nNewSymClass == SC_SLEEPING_RIGID)
		{
			SEntityEvent event(ENTITY_EVENT_PHYSICS_CHANGE_STATE);
			event.nParam[0] = 0;
			pCEntity->SendEvent(event);
		}
	}
	return 1;
}

//////////////////////////////////////////////////////////////////////////
int CPhysicsEventListener::OnUpdateMesh( const EventPhys *pEvent )
{
	((CBreakableManager*)GetIEntitySystem()->GetBreakableManager())->HandlePhysics_UpdateMeshEvent((EventPhysUpdateMesh*)pEvent);
	return 1;

	/*EventPhysUpdateMesh *pUpdateEvent = (EventPhysUpdateMesh*)pEvent;
	CEntity *pCEntity;
	Matrix34 mtx;
	int iForeignData = pUpdateEvent->pEntity->GetiForeignData();
	void *pForeignData = pUpdateEvent->pEntity->GetForeignData(iForeignData);
	IFoliage *pSrcFoliage=0;
	uint8 nMatLayers = 0;

	bool bNewEntity = false;

	if (iForeignData==PHYS_FOREIGN_ID_ENTITY)
	{
		pCEntity = (CEntity*)pForeignData;
		if (!pCEntity || !pCEntity->GetPhysicalProxy())
			return 1;
		if (pCEntity->GetRenderProxy())
		{
			nMatLayers = pCEntity->GetRenderProxy()->GetMaterialLayers();
			pCEntity->GetRenderProxy()->ExpandCompoundSlot0();
		}
	} else if (iForeignData==PHYS_FOREIGN_ID_STATIC)
	{
		CBreakableManager::SCreateParams createParams;

		CBreakableManager* pBreakMgr = (CBreakableManager*)GetIEntitySystem()->GetBreakableManager();

		IRenderNode *pRenderNode = (IRenderNode*)pUpdateEvent->pForeignData;
		IStatObj *pStatObj = pRenderNode->GetEntityStatObj(0, 0, &mtx);
		
		createParams.fScale = mtx.GetColumn(0).len();
		createParams.pSrcStaticRenderNode = pRenderNode;
		createParams.worldTM = mtx;
		createParams.nMatLayers = pRenderNode->GetMaterialLayers();
		createParams.pCustomMtl = pRenderNode->GetMaterial();

		if (pStatObj)
		{
			pCEntity = pBreakMgr->CreateObjectAsEntity( pStatObj,pUpdateEvent->pEntity,createParams );
			bNewEntity = true;
		}
		
		pSrcFoliage = pRenderNode->GetFoliage();

    // todo Danny - decide if this (or something similar) is useful
		// This passes just the smallest 2D outline into AI so that AI
		// can update its navigation graph
		//IPhysicalEntity* pe = pUpdateEvent->pEntity;
		//pe_status_pos status;
		//pe->GetStatus(&status);
		//{
			// outline (not self-intersecting please!) of the region to disable in AI
		//	std::list<Vec3> outline;
		//	static float extra = 3.0f; // expand the outline a bit - e.g. for trees
		//	outline.push_back(status.pos + Vec3( status.BBox[0].x - extra,  status.BBox[0].y - extra, 0.0f));
		//	outline.push_back(status.pos + Vec3( status.BBox[1].x + extra,  status.BBox[0].y - extra, 0.0f));
		//	outline.push_back(status.pos + Vec3( status.BBox[1].x + extra,  status.BBox[1].y + extra, 0.0f));
		//	outline.push_back(status.pos + Vec3( status.BBox[0].x - extra,  status.BBox[1].y + extra, 0.0f));
		//	gEnv->pAISystem->DisableNavigationInBrokenRegion(outline);
		//}
	}	else
	{
		return 1;
	}

	//if (pUpdateEvent->iReason==EventPhysUpdateMesh::ReasonExplosion)
	//	pCEntity->AddFlags(ENTITY_FLAG_MODIFIED_BY_PHYSICS);

	if (pCEntity)
	{
		pCEntity->GetPhysicalProxy()->DephysicalizeFoliage(0);
		IStatObj *pDeformedStatObj = gEnv->p3DEngine->UpdateDeformableStatObj(pUpdateEvent->pMesh,pUpdateEvent->pLastUpdate,pSrcFoliage);
		pCEntity->GetRenderProxy()->SetSlotGeometry( pUpdateEvent->partid,pDeformedStatObj );

		//pCEntity->GetPhysicalProxy()->CreateRenderGeometry( 0,pUpdateEvent->pMesh, pUpdateEvent->pLastUpdate );
		pCEntity->GetPhysicalProxy()->PhysicalizeFoliage(0);
		
		if (bNewEntity)
		{
			SEntityEvent entityEvent(ENTITY_EVENT_PHYS_BREAK);
			pCEntity->SendEvent( entityEvent );
		}
	}
	
	return 1;*/
}

//////////////////////////////////////////////////////////////////////////
int CPhysicsEventListener::OnCreatePhysEntityPart( const EventPhys *pEvent )
{
	EventPhysCreateEntityPart *pCreateEvent = (EventPhysCreateEntityPart*)pEvent;

	//////////////////////////////////////////////////////////////////////////
	// Let Breakable manager handle creation of the new entity part.
	//////////////////////////////////////////////////////////////////////////
	CBreakableManager* pBreakMgr = (CBreakableManager*)GetIEntitySystem()->GetBreakableManager();
	pBreakMgr->HandlePhysicsCreateEntityPartEvent( pCreateEvent );
	return 1;


	/*if (pCreateEvent->pMeshNew)
		pPhysProxy->CreateRenderGeometry( 0,pCreateEvent->pMeshNew, pCreateEvent->pLastUpdate );
	else 
	{
		int i,nSrcParts,nParts,nSuccParts=0,nSubobj,*slots,slotsbuf[16];
		IStatObj *pSrcStatObj,*pStatObj,*pStatObjDummy;
		pe_params_part pp,pp1;

		nParts = pCreateEvent->pEntNew->GetStatus(&pe_status_nparts());
		nSubobj = pSrcEntity->GetStatObj(0)->GetSubObjectCount();
		slots = nSubobj<=sizeof(slots)/sizeof(slots[0]) ? slotsbuf : new int[nSubobj];
		for(pp.ipart=0; pp.ipart<nParts; pp.ipart++) 
		{
			pCreateEvent->pEntNew->GetParams(&pp);
			slots[pp.ipart] = pp.partid;
			pp1.ipart = pp1.partid = pp.ipart;
			pCreateEvent->pEntNew->SetParams(&pp1);
		}

		pSrcStatObj = pSrcEntity->GetStatObj(0)->SeparateSubobjects(pStatObj, slots,nParts, false);

		if (pCreateEvent->partidSrc) 
		{ // the last removed part in the session, flush removed parts from the source statobj and update phys ids
			nSubobj = pSrcStatObj->GetSubObjectCount();
			for(i=nParts=0;i<nSubobj;i++) if (pSrcStatObj->GetSubObject(i)->nType==STATIC_SUB_OBJECT_MESH)
				slots[i] = nParts++;
			nSrcParts = pCreateEvent->pEntity->GetStatus(&pe_status_nparts());
			for(pp.ipart=i=0; pp.ipart<nSrcParts; pp.ipart++)
			{
				pCreateEvent->pEntity->GetParams(&pp);
				pp1.ipart = pp.ipart; pp1.partid = slots[pp.partid];
				pCreateEvent->pEntity->SetParams(&pp1);
			}
			pSrcStatObj = pSrcStatObj->SeparateSubobjects(pStatObjDummy, 0,0, true);
		}

		pSrcEntity->SetStatObj( pSrcStatObj,0 );
		pEntity->SetStatObj( pStatObj,0 );

		if (slots!=slotsbuf)
			delete slots;
	}*/


	//if (bNewEnt && pEntity->GetPhysicalProxy()->PhysicalizeFoliage(0) && pSrcEntity)
		//pSrcEntity->GetPhysicalProxy()->DephysicalizeFoliage(0);

	return 1;
}

//////////////////////////////////////////////////////////////////////////
int CPhysicsEventListener::OnRemovePhysEntityParts( const EventPhys *pEvent )
{
	EventPhysRemoveEntityParts *pRemoveEvent = (EventPhysRemoveEntityParts*)pEvent;

	CBreakableManager* pBreakMgr = (CBreakableManager*)GetIEntitySystem()->GetBreakableManager();
	pBreakMgr->HandlePhysicsRemoveSubPartsEvent( pRemoveEvent );

	return 1;
}

//////////////////////////////////////////////////////////////////////////
CEntity *MatchPartId(CEntity *pent,int partid)
{
	if (pent->GetPhysicalProxy() && (unsigned int)(partid-pent->GetPhysicalProxy()->GetPartId0())<1000u)
		return pent;
	CEntity *pMatch;
	for(int i=pent->GetChildCount()-1;i>=0;i--)	if (pMatch=MatchPartId((CEntity*)pent->GetChild(i),partid))
		return pMatch;
	return 0;
}

IEntity *CEntity::UnmapAttachedChild(int &partId)
{
	CEntity *pChild;
	if (partId>=PARTID_LINKED && (pChild=MatchPartId(this,partId)))
	{
		if (pChild->GetPhysicalProxy())
			partId -= pChild->GetPhysicalProxy()->GetPartId0();
		return pChild;
	}
	return this;
}

int CPhysicsEventListener::OnCollision( const EventPhys *pEvent )
{
	EventPhysCollision *pCollision = (EventPhysCollision *)pEvent;
	SEntityEvent event;
	CEntity *pEntitySrc = GetEntity(pCollision->pForeignData[0], pCollision->iForeignData[0]), *pChild,
					*pEntityTrg = GetEntity(pCollision->pForeignData[1], pCollision->iForeignData[1]);

 	if (pEntitySrc && pCollision->partid[0]>=PARTID_LINKED && (pChild=MatchPartId(pEntitySrc,pCollision->partid[0])))
	{
		pEntitySrc = pChild;
		pCollision->pForeignData[0] = pEntitySrc;
		pCollision->iForeignData[0] = PHYS_FOREIGN_ID_ENTITY;
		pCollision->partid[0] -= pEntitySrc->GetPhysicalProxy()->GetPartId0();
	}

	if (pEntitySrc)
	{
 		if (pEntityTrg && pCollision->partid[1]>=PARTID_LINKED && (pChild=MatchPartId(pEntityTrg,pCollision->partid[1])))
		{
			pEntityTrg = pChild;
			pCollision->pForeignData[1] = pEntityTrg;
			pCollision->iForeignData[1] = PHYS_FOREIGN_ID_ENTITY;
			pCollision->partid[1] -= pEntityTrg->GetPhysicalProxy()->GetPartId0();
		}

		CPhysicalProxy *pPhysProxySrc = pEntitySrc->GetPhysicalProxy();
		if (pPhysProxySrc)
			pPhysProxySrc->OnCollision(pEntityTrg, pCollision->idmat[1], pCollision->pt, pCollision->n, pCollision->vloc[0], pCollision->vloc[1], pCollision->partid[1], pCollision->mass[1]);

		if (pEntityTrg)
		{
			CPhysicalProxy *pPhysProxyTrg = pEntityTrg->GetPhysicalProxy();
			if (pPhysProxyTrg)
				pPhysProxyTrg->OnCollision(pEntitySrc, pCollision->idmat[0], pCollision->pt, -pCollision->n, pCollision->vloc[1], pCollision->vloc[0], pCollision->partid[0], pCollision->mass[0]);
		}
	}

	event.event = ENTITY_EVENT_COLLISION;
	event.nParam[0] = (INT_PTR)pEvent;
	event.nParam[1] = 0;
	if (pEntitySrc)
		pEntitySrc->SendEvent(event);
	event.nParam[1] = 1;
	if (pEntityTrg)
		pEntityTrg->SendEvent(event);

	return 1;
}

//////////////////////////////////////////////////////////////////////////
int CPhysicsEventListener::OnJointBreak( const EventPhys *pEvent )
{
	EventPhysJointBroken *pBreakEvent = (EventPhysJointBroken*)pEvent;
	CEntity *pCEntity = 0;
	IStatObj *pStatObj = 0;

	bool bShatter = false;
	switch (pBreakEvent->iForeignData[0])
	{
	case PHYS_FOREIGN_ID_ROPE:
		{
			IRopeRenderNode *pRenderNode = (IRopeRenderNode*)pBreakEvent->pForeignData[0];
			if (pRenderNode)
			{
				EntityId id = (EntityId)pRenderNode->GetEntityOwner();
				pCEntity = (CEntity*)g_pIEntitySystem->GetEntityFromID(id);
			}
		}
		break;
	case PHYS_FOREIGN_ID_ENTITY:
		pCEntity = (CEntity*)pBreakEvent->pForeignData[0];
		break;
	case PHYS_FOREIGN_ID_STATIC:
		{
			IRenderNode *pRenderNode = ((IRenderNode*)pBreakEvent->pForeignData[0]);
			pStatObj = pRenderNode->GetEntityStatObj(0);
			bShatter = pRenderNode->GetMaterialLayers() & MTL_LAYER_FROZEN;
		}
	}
	//GetEntity(pBreakEvent->pForeignData[0],pBreakEvent->iForeignData[0]);
	if (pCEntity)
	{
		SEntityEvent event;
		event.event = ENTITY_EVENT_PHYS_BREAK;
		event.nParam[0] = (INT_PTR)pEvent;
		event.nParam[1] = 0;
		pCEntity->SendEvent(event);
		pStatObj = pCEntity->GetStatObj(ENTITY_SLOT_ACTUAL);

		if (pCEntity->GetRenderProxy())
		{
			bShatter = pCEntity->GetRenderProxy()->GetMaterialLayersMask() & MTL_LAYER_FROZEN;
		}
	}

	IStatObj::SSubObject *pSubObj;
	const char *ptr,*peff;
	if (pStatObj)
		while(pStatObj->GetCloneSourceObject())
			pStatObj = pStatObj->GetCloneSourceObject();

	if (pStatObj && pStatObj->GetFlags()&STATIC_OBJECT_COMPOUND)
	{
		Matrix34 tm = Matrix34::CreateTranslationMat(pBreakEvent->pt);
		IStatObj *pObj1=0,*pObj2=0;

		IStatObj::SSubObject *pSubObject1 = pStatObj->GetSubObject(pBreakEvent->partid[0]);
		if (pSubObject1)
			pObj1 = pSubObject1->pStatObj;
		//IStatObj::SSubObject *pSubObject2 = pStatObj->GetSubObject(pBreakEvent->partid[1]);
		//if (pSubObject2)
			//pObj2 = pSubObject2->pStatObj;

		const char *sEffectType = (!bShatter) ? SURFACE_BREAKAGE_TYPE("joint_break") : SURFACE_BREAKAGE_TYPE("joint_shatter");
		CBreakableManager* pBreakMgr = (CBreakableManager*)GetIEntitySystem()->GetBreakableManager();
		if (pObj1)
			pBreakMgr->CreateSurfaceEffect( pObj1,tm,sEffectType );
		//if (pObj2)
			//pBreakMgr->CreateSurfaceEffect( pObj2,tm,sEffectType );
	}

	if (pStatObj && (pSubObj=pStatObj->GetSubObject(pBreakEvent->idJoint)) && 
			pSubObj->nType==STATIC_SUB_OBJECT_DUMMY && !strncmp(pSubObj->name,"$joint",6) && 
			(ptr=strstr(pSubObj->properties,"effect")))
	{
		for(ptr+=6; *ptr && (*ptr==' ' || *ptr=='='); ptr++);
		if (*ptr) 
		{
			char strEff[256];
			for(peff=ptr++; *ptr && *ptr!='\n'; ptr++);	
			strncpy(strEff,peff,min(255,(int)(ptr-peff))); strEff[min(255,(int)(ptr-peff))]=0;
			IParticleEffect *pEffect = gEnv->p3DEngine->FindParticleEffect(strEff);
			pEffect->Spawn(true, IParticleEffect::ParticleLoc(pBreakEvent->pt,pBreakEvent->n));
		}
	}

	return 1;
}
