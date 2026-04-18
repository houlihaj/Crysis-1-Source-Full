//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
//	
//	File:CEntitySystem.cpp
//  Description: entity's management
//
//	History:
//	-March 07,2001:Originally created by Marco Corbetta
//  -: modified by everyone
//
//
//////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "EntitySystem.h"
#include "EntityIt.h"

#include "Entity.h"
#include "EntityCVars.h"
#include "EntityClassRegistry.h"
#include "ScriptBind_Entity.h"
#include "PhysicsEventListener.h"
#include "EntityLoader.h"
#include "AreaManager.h"
#include "Boids/ScriptBind_Boids.h"
#include "BreakableManager.h"
#include "EntityArchetype.h"
#include "PartitionGrid.h"
#include "ProximityTriggerSystem.h"
#include "EntityObject.h"
#include "ISerialize.h"

#include <IRenderer.h>
#include <I3DEngine.h>
#include <ILog.h>
#include <ITimer.h>
#include <ISystem.h>
#include <IPhysics.h>
#include <IRenderAuxGeom.h>
#include <Cry_Camera.h>
#include <IAgent.h>

stl::PoolAllocatorNoMT<sizeof(CRenderProxy)> *g_Alloc_RenderProxy = 0;
stl::PoolAllocatorNoMT<sizeof(CEntityObject)> *g_Alloc_EntitySlot = 0;

//////////////////////////////////////////////////////////////////////////
void OnRemoveEntityCVarChange( ICVar *pArgs )
{		
	if (gEnv->pEntitySystem)
	{		
		const char *szEntity=pArgs->GetString();
		IEntity *pEnt=gEnv->pEntitySystem->FindEntityByName(szEntity);
		if (pEnt)
			gEnv->pEntitySystem->RemoveEntity(pEnt->GetId());
	}
}

//////////////////////////////////////////////////////////////////////////
void OnActivateEntityCVarChange( ICVar *pArgs )
{	
	if (gEnv->pEntitySystem)
	{		
		const char *szEntity=pArgs->GetString();
		IEntity *pEnt=gEnv->pEntitySystem->FindEntityByName(szEntity);
		if (pEnt)
		{
			CEntity *pcEnt=(CEntity *)(pEnt);
			pcEnt->Activate(true);			
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void OnDeactivateEntityCVarChange( ICVar *pArgs )
{
	if (gEnv->pEntitySystem)
	{		
		const char *szEntity=pArgs->GetString();
		IEntity *pEnt=gEnv->pEntitySystem->FindEntityByName(szEntity);
		if (pEnt)
		{
			CEntity *pcEnt=(CEntity *)(pEnt);
			pcEnt->Activate(false);			
		}
	}
}

//////////////////////////////////////////////////////////////////////
CEntitySystem::CEntitySystem(ISystem *pSystem)
:	m_entityTimeoutList(pSystem->GetITimer())
{
	CRenderProxy::SetTimeoutList(&m_entityTimeoutList);

	// Assign allocators.
	g_Alloc_RenderProxy = new stl::PoolAllocatorNoMT<sizeof(CRenderProxy)>;
	g_Alloc_EntitySlot = new stl::PoolAllocatorNoMT<sizeof(CEntityObject)>;

	ClearEntityArray();

	m_pISystem = pSystem;
	m_pClassRegistry = 0;
	m_pEntityScriptBinding = NULL;
	m_pScriptBindsBoids = NULL;

	CVar::Init( pSystem->GetIConsole() );
	
	m_bTimersPause=false;
	m_nStartPause.SetSeconds(-1.0f);

	m_pAreaManager = new CAreaManager(this);
	m_pBreakableManager = new CBreakableManager(this);
	m_pEntityArchetypeManager = new CEntityArchetypeManager;

	m_pPartitionGrid = new CPartitionGrid;
	m_pProximityTriggerSystem = new CProximityTriggerSystem;

	m_idForced = 0;

	m_bReseting = false;

	m_pISystem->GetIConsole()->RegisterString("es_removeEntity","",VF_CHEAT, "Removes an entity",OnRemoveEntityCVarChange );
	m_pISystem->GetIConsole()->RegisterString("es_activateEntity","",VF_CHEAT, "Activates an entity",OnActivateEntityCVarChange );
	m_pISystem->GetIConsole()->RegisterString("es_deactivateEntity","",VF_CHEAT, "Deactivates an entity",OnDeactivateEntityCVarChange );
}

//////////////////////////////////////////////////////////////////////
CEntitySystem::~CEntitySystem()
{
	CRenderProxy::SetTimeoutList(0);

	Reset();

	if (m_pClassRegistry)
		delete m_pClassRegistry;
	m_pClassRegistry = NULL;

	SAFE_DELETE(m_pEntityArchetypeManager);
	SAFE_DELETE(m_pEntityScriptBinding);
	SAFE_DELETE(m_pScriptBindsBoids);
	
	if (m_pPhysicsEventListener)
		delete m_pPhysicsEventListener;
	m_pPhysicsEventListener = NULL;

	delete m_pProximityTriggerSystem;
	delete m_pPartitionGrid;
	delete m_pBreakableManager;

	delete g_Alloc_RenderProxy;
	delete g_Alloc_EntitySlot;
	//ShutDown();
}

//////////////////////////////////////////////////////////////////////
bool CEntitySystem::Init(ISystem *pSystem)
{
	if (!pSystem) return false;
	m_pISystem=pSystem;
	m_lstSinks.clear();
	ClearEntityArray();

	m_pClassRegistry = new CEntityClassRegistry;
	m_pClassRegistry->InitializeDefaultClasses();

	//////////////////////////////////////////////////////////////////////////
	// Initialize entity script bindings.
	m_pEntityScriptBinding = new CScriptBind_Entity( pSystem->GetIScriptSystem(),pSystem, this );
	m_pScriptBindsBoids = new CScriptBind_Boids( pSystem );

	// Initialize physics events handler.
	m_pPhysicsEventListener = new CPhysicsEventListener(this,pSystem->GetIPhysicalWorld());

	//////////////////////////////////////////////////////////////////////////
	// Should reallocate grid if level size change.
	m_pPartitionGrid->AllocateGrid( 4096,4096 );

	m_entityTimeoutList.Clear();
	m_bLocked = false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
IEntityClassRegistry* CEntitySystem::GetClassRegistry()
{
	return m_pClassRegistry;
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::Release()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::Reset()
{
	CheckInternalConsistency();

	m_mapEntityNames.clear();

	m_bReseting = true;
	uint32 dwMaxUsed = (uint32)m_EntitySaltBuffer.GetMaxUsed()+1;

	for(uint32 dwI=0;dwI<dwMaxUsed;++dwI)
	{
		CEntity *ent = m_EntityArray[dwI];
		if(ent)
		{
			if (!ent->m_bGarbage)
			{
				ent->m_flags &= ~ENTITY_FLAG_UNREMOVABLE;
				RemoveEntity( ent->GetId(),false );
			}
			else
			{
				stl::push_back_unique(m_deletedEntities, ent);
			}
		}
	}
	// Delete entity that where added to delete list.
	UpdateDeletedEntities();

	ClearEntityArray();
	m_mapActiveEntities.clear();
	m_tempActiveEntitiesValid = false;
	m_deletedEntities.clear();
	m_guidMap.clear();

	m_EntitySaltBuffer.Reset();

	m_timersMap.clear();

	for (int i = 0; i < ENTITY_EVENT_LAST; i++)
	{
		m_eventListeners[i].clear();
	}
	m_pProximityTriggerSystem->Reset();
	m_pPartitionGrid->Reset();

	m_entityTimeoutList.Clear();
	m_bReseting = false;

	CheckInternalConsistency();
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::AddSink( IEntitySystemSink *pSink )
{
	assert(pSink);

	if(pSink)
		stl::push_back_unique( m_lstSinks,pSink );
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RemoveSink( IEntitySystemSink *pSink )
{
	assert(pSink);

	m_lstSinks.remove(pSink);
}


bool CEntitySystem::OnBeforeSpawn( SEntitySpawnParams &params )
{
	for (SinkList::iterator si=m_lstSinks.begin();si!=m_lstSinks.end();si++)
	{
		if (!(*si)->OnBeforeSpawn(params))
			return false;
	}

	return true;
}

//load an entity from script,set position and add to the entity system
//////////////////////////////////////////////////////////////////////
IEntity* CEntitySystem::SpawnEntity( SEntitySpawnParams &params,bool bAutoInit )
{	
	FUNCTION_PROFILER( m_pISystem,PROFILE_ENTITY );

	// If entity spawns from archetype take class from archetype
	if (params.pArchetype)
		params.pClass = params.pArchetype->GetClass();

	assert( params.pClass != NULL ); // Class must always be specified

	if(m_bLocked)
	{
		if(!params.bIgnoreLock)
		{
			CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, "Spawning entity %s of class %s refused due to spawn lock! Spawning entities is not allowed at this time.", params.sName, params.pClass->GetName());
			return NULL;
		}
	}

	if (!OnBeforeSpawn(params))
		return NULL;

	CEntity *pEntity=0;
  
	if (m_idForced)
	{
		params.id = m_idForced;
		m_idForced = 0;
	}

	if(!params.id)			// entityid wasn't given
	{
		// get entity id and mark it
		if(params.bStaticEntityId)
			params.id = HandleToId(m_EntitySaltBuffer.InsertStatic());
		 else
			params.id = HandleToId(m_EntitySaltBuffer.InsertDynamic());

		if(!params.id)
		{
			EntityWarning( "CEntitySystem::SpawnEntity Failed, Can't spawn entity %s. ID range is full (internal error)",(const char*)params.sName );
			CheckInternalConsistency();
			return NULL;
		}
	}
	else 
	{
		if(m_EntitySaltBuffer.IsUsed(IdToHandle(params.id).GetIndex()))		
		{
			// was reserved or was existing already

			pEntity = m_EntityArray[IdToHandle(params.id).GetIndex()];

			if(pEntity)
				EntityWarning( "Entity with id=%d, %s already spawned on this client...override",pEntity->GetId(),pEntity->GetEntityTextDescription() );
			else 
				m_EntitySaltBuffer.InsertKnownHandle(IdToHandle(params.id));
		}
		else
		{
			m_EntitySaltBuffer.InsertKnownHandle(IdToHandle(params.id));
		}
	}

	if(!pEntity)
	{
		// Makes new entity.
		pEntity = new CEntity(this,params);
		// put it into the entity map
		m_EntityArray[IdToHandle(params.id).GetIndex()]=pEntity;

		if (params.guid)
			RegisterEntityGuid( params.guid,params.id );
	}

	if(bAutoInit)
		if(!InitEntity(pEntity,params))		// calls DeleteEntity() on failure
		{
			return NULL;
		}

	return pEntity;
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySystem::InitEntity( IEntity* pEntity,SEntitySpawnParams &params )
{
	assert( pEntity );

	CEntity *pCEntity = (CEntity*)pEntity;

	// initialize entity
	if(!pCEntity->Init(params))
	{
		DeleteEntity(pEntity);
		return false;
	}

	for (SinkList::iterator si=m_lstSinks.begin();si!=m_lstSinks.end();si++)
		(*si)->OnSpawn(pEntity,params);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DeleteEntity( IEntity *entity )
{
	FUNCTION_PROFILER( m_pISystem,PROFILE_ENTITY );

	if (entity)
	{
		CEntity *ce = ((CEntity *)entity);

		m_mapPrePhysicsEntities.erase( ce->GetId() );

		ce->ShutDown();

		// Make sure this entity is not in active list anymore.
		if (ce->m_bInActiveList)
		{
			m_mapActiveEntities.erase( ce->GetId() );
			m_tempActiveEntitiesValid = false;
			ce->m_bActive = false;
			ce->m_bInActiveList = false;
		}
		else
			assert(m_mapActiveEntities.count(ce->GetId())==0);

		m_EntityArray[IdToHandle(ce->GetId()).GetIndex()]=0;
		m_EntitySaltBuffer.Remove(IdToHandle(ce->GetId()));

		if (ce->m_guid)
			UnregisterEntityGuid( ce->m_guid );

		delete ce;
	}
}


//////////////////////////////////////////////////////////////////////
void CEntitySystem::ClearEntityArray()
{
	CheckInternalConsistency();

	uint32 dwMaxIndex = (uint32)(m_EntitySaltBuffer.GetTSize());

	m_EntityArray.resize(dwMaxIndex);

	for(uint32 dwI=0;dwI<dwMaxIndex;++dwI)
		m_EntityArray[dwI] = 0;

	m_tempActiveEntitiesValid = false;

	CheckInternalConsistency();
}


//////////////////////////////////////////////////////////////////////
void CEntitySystem::RemoveEntity( EntityId entity,bool bForceRemoveNow )
{
	ENTITY_PROFILER
	assert(IdToHandle(entity));

	CEntity *pEntity = m_EntityArray[IdToHandle(entity).GetIndex()];
	if(pEntity)
	{ 

		if(m_bLocked)
			CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, "Removing entity during system lock : %s with id %i", pEntity->GetName(), (int)entity);

		if (pEntity->GetId() != entity)
		{
			EntityWarning("Trying to remove entity with mismatching salts. id1=%d id2=%d", entity, pEntity->GetId());
			CheckInternalConsistency();
			if (ICVar * pVar = gEnv->pConsole->GetCVar("net_assertlogging"))
			{
				if (pVar->GetIVal())
				{
					gEnv->pSystem->debug_LogCallStack();
				}
			}
			return;
		}
		if (!pEntity->m_bGarbage)
		{
			for (SinkList::iterator si=m_lstSinks.begin(),si_end=m_lstSinks.end();si!=si_end;si++)
				if (!(*si)->OnRemove(pEntity))
				{
					// basically unremovable... but hide it anyway to be polite
					pEntity->Hide(true);
					return;
				}

			// Send deactivate event.
			SEntityEvent entevent;
			entevent.event = ENTITY_EVENT_DONE;
			entevent.nParam[0] = pEntity->GetId();
			pEntity->SendEvent(entevent);

			// Make sure this entity is not in active list anymore.
			if (pEntity->m_bInActiveList)
			{
				m_mapActiveEntities.erase( pEntity->GetId() );
				m_tempActiveEntitiesValid = false;
				pEntity->m_bActive = false;
				pEntity->m_bInActiveList = false;
			}
			else
				assert(m_mapActiveEntities.count(pEntity->GetId())==0);


			if (!(pEntity->m_flags&ENTITY_FLAG_UNREMOVABLE))
			{
				pEntity->m_bGarbage = true;
				if (bForceRemoveNow)
					DeleteEntity(pEntity);
				else
				{
					// add entity to deleted list, and actually delete entity on next update.
					m_deletedEntities.push_back(pEntity);
				}
			}
			else
			{
				// Unremovable entities. are hidden and deactivated.
				pEntity->Hide(true);

				pEntity->m_bGarbage = true;
			}
		}
		else if (bForceRemoveNow)
		{
			//DeleteEntity(pEntity);
		}
	}
}

//////////////////////////////////////////////////////////////////////
IEntity* CEntitySystem::GetEntity( EntityId id ) const
{
	if(!IdToHandle(id))
		return 0;						// NIL

	uint32 dwMaxUsed = (uint32)m_EntitySaltBuffer.GetMaxUsed();

	unsigned short hdl = IdToHandle(id).GetIndex();
	if (hdl > dwMaxUsed)
		return 0;

	assert(hdl<=dwMaxUsed);		// bad input id parameter

	CEntity *pEntity = m_EntityArray[hdl];

	if(pEntity)
	{
		if(pEntity->GetId()==id)
			return pEntity;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////
EntityId CEntitySystem::GetEntityIdInSlot( EntityId id ) const
{
	// a little bit unclean... but there you go
	id &= 0xffff;
	if (id < m_EntityArray.size())
	{
		CEntity * pEntity = m_EntityArray[id];
		if (pEntity)
			return pEntity->GetId();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IEntity* CEntitySystem::FindEntityByName( const char *sEntityName ) const
{
	if (!sEntityName || !sEntityName[0])
		return 0; // no entity name specified

	if (CVar::es_DebugFindEntity != 0)
	{
		CryLog( "FindEntityByName: %s",sEntityName );
		if (CVar::es_DebugFindEntity == 2)
			GetISystem()->debug_LogCallStack();
	}

	// Find first object with this name.
	EntityNamesMap::const_iterator it = m_mapEntityNames.lower_bound(sEntityName);
	if (it != m_mapEntityNames.end())
	{
		if (stricmp(it->first,sEntityName) == 0)
			return GetEntity(it->second);
	}
	
	return 0;		// name not found
}


//////////////////////////////////////////////////////////////////////
uint32 CEntitySystem::GetNumEntities() const
{
	uint32 dwRet=0;
	uint32 dwMaxUsed = (uint32)m_EntitySaltBuffer.GetMaxUsed()+1;

	for(uint32 dwI=0;dwI<dwMaxUsed;++dwI)
	{
		CEntity *ce = m_EntityArray[dwI];

		if(ce)
			++dwRet;
	}

	return dwRet;
}


//////////////////////////////////////////////////////////////////////////
IEntity* CEntitySystem::GetEntityFromPhysics( IPhysicalEntity *pPhysEntity ) const
{
	assert(pPhysEntity);
	CEntity *pEntity = NULL;
	if(pPhysEntity)
		pEntity = (CEntity*)pPhysEntity->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
	return pEntity;

	//CPhysicalProxy *pPhysProxy = (CPhysicalProxy*)pPhysEntity->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
	//if (pPhysProxy)
		//return pPhysProxy->GetEntity();
	//return NULL;
}

//////////////////////////////////////////////////////////////////////
int CEntitySystem::GetPhysicalEntitiesInBox(const Vec3 &origin, float radius,IPhysicalEntity **&pList, int physFlags) const
{
	assert(m_pISystem);

	Vec3 bmin = origin - Vec3(radius, radius, radius);
	Vec3 bmax = origin + Vec3(radius, radius, radius);

	return m_pISystem->GetIPhysicalWorld()->GetEntitiesInBox(bmin, bmax, pList, physFlags);
}

//////////////////////////////////////////////////////////////////////////
int CEntitySystem::QueryProximity( SEntityProximityQuery &query )
{
	SPartitionGridQuery q;
	q.aabb = query.box;
	q.nEntityFlags = query.nEntityFlags;
	q.pEntityClass = query.pEntityClass;
	m_pPartitionGrid->GetEntitiesInBox( q );
	query.pEntities = 0;
	query.nCount = (int)q.pEntities->size();
	if (q.pEntities && query.nCount > 0)
	{
		query.pEntities = &((*q.pEntities)[0]);
	}
	return query.nCount;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::UpdateDeletedEntities()
{
	while (!m_deletedEntities.empty())
	{
		CEntity *pEntity = m_deletedEntities.front();
		m_deletedEntities.pop_front();
		DeleteEntity(pEntity);
	}
	m_deletedEntities.clear();
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::UpdateNotSeenTimeouts()
{
	FUNCTION_PROFILER_FAST( m_pISystem,PROFILE_ENTITY,g_bProfilerEnabled );

	float timeoutVal = float(CVar::pNotSeenTimeout->GetIVal());
	if (timeoutVal == 0.0f)
		return;

	while (EntityId timeoutEntityId = m_entityTimeoutList.PopTimeoutEntity(timeoutVal))
	{
		CEntity* pEntity = GetEntityFromID(timeoutEntityId);
		if (pEntity)
		{
			SEntityEvent entityEvent(ENTITY_EVENT_NOT_SEEN_TIMEOUT);
			pEntity->SendEvent(entityEvent);
		}
	}
}

//////////////////////////////////////////////////////////////////////
void CEntitySystem::PrePhysicsUpdate()
{
	if (CVar::pProfileEntities->GetIVal() < 0)
		GetISystem()->VTuneResume();

	g_bProfilerEnabled = m_pISystem->GetIProfileSystem()->IsProfiling();

	FUNCTION_PROFILER_FAST( m_pISystem,PROFILE_ENTITY,g_bProfilerEnabled );

	float fFrameTime = gEnv->pTimer->GetFrameTime();
	for (EntitiesSet::iterator it = m_mapPrePhysicsEntities.begin(); it != m_mapPrePhysicsEntities.end();)
	{
		EntityId eid = *it;
		EntitiesSet::iterator next = it;
		++next;
		CEntity *pEntity = (CEntity*)GetEntity(eid);
		if (pEntity)
		{
			pEntity->PrePhysicsUpdate(fFrameTime);
		}
		it = next;
	}

	if (CVar::pProfileEntities->GetIVal() < 0)
		GetISystem()->VTunePause();
}

//update the entity system
//////////////////////////////////////////////////////////////////////
void CEntitySystem::Update()
{
	if (CVar::pProfileEntities->GetIVal() < 0)
		GetISystem()->VTuneResume();

	FUNCTION_PROFILER_FAST( m_pISystem,PROFILE_ENTITY,g_bProfilerEnabled );

	UpdateTimers();

	DoUpdateLoop();

	// Update info on proximity triggers.
	m_pProximityTriggerSystem->Update();
	
	// Now update area manager to send enter/leave events from areas.
	m_pAreaManager->Update();

	// Delete entities that must be deleted.
	if (!m_deletedEntities.empty())
		UpdateDeletedEntities();

	UpdateNotSeenTimeouts();

	if (CVar::pEntityBBoxes->GetIVal() != 0)
	{
		// Render bboxes of all entities.
		uint32 dwMaxUsed = (uint32)m_EntitySaltBuffer.GetMaxUsed()+1;
		for(uint32 dwI=0;dwI<dwMaxUsed;++dwI)
		{
			CEntity *pEntity = m_EntityArray[dwI];
			if(pEntity)
				DebugDraw(pEntity,-1);
		}

	}

	if (CVar::pDrawAreas->GetIVal() != 0)
		m_pAreaManager->DrawAreas(gEnv->pSystem);
	if (CVar::pDrawAreaGrid->GetIVal() != 0)
		m_pAreaManager->DrawGrid();

	if (CVar::pProfileEntities->GetIVal() < 0)
		GetISystem()->VTunePause();
}

//////////////////////////////////////////////////////////////////////////
ILINE IEntityClass * CEntitySystem::GetClassForEntity(CEntity * p)
{
	return p->m_pClass;
}

class CEntitySystem::CCompareEntityIdsByClass
{
public:
	CCompareEntityIdsByClass( CEntity ** ppEntities ) : m_ppEntities(ppEntities) {}
	
	ILINE bool operator()( EntityId a, EntityId b ) const
	{
		CEntity * pA = m_ppEntities[a&0xffff];
		CEntity * pB = m_ppEntities[b&0xffff];
		IEntityClass * pClsA = pA? GetClassForEntity(pA) : 0;
		IEntityClass * pClsB = pB? GetClassForEntity(pB) : 0;
		return pClsA < pClsB;
	}

private:
	CEntity ** m_ppEntities;
};

void CEntitySystem::UpdateTempActiveEntities()
{
	if (!m_tempActiveEntitiesValid)
	{
		int i = 0;
		int numActive = m_mapActiveEntities.size();
		m_tempActiveEntities.resize( numActive );
		for (EntitiesMap::iterator it = m_mapActiveEntities.begin(); it != m_mapActiveEntities.end(); it++)
		{
			m_tempActiveEntities[i++] = it->first;
		}
		if (CVar::es_SortUpdatesByClass)
			std::sort( m_tempActiveEntities.begin(), m_tempActiveEntities.end(), CCompareEntityIdsByClass(&m_EntityArray[0]) );
		m_tempActiveEntitiesValid = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DoUpdateLoop()
{
	CCamera Cam=m_pISystem->GetViewCamera();
	Vec3 Min, Max;
	int nRendererFrameID = 0;
	if(m_pISystem)
	{
		IRenderer *pRen=m_pISystem->GetIRenderer();
		if(pRen)
			nRendererFrameID=pRen->GetFrameID(false);
		//			nRendererFrameID=pRen->GetFrameUpdateID();
	}

	bool bDebug = false;
	if (CVar::pDebug->GetIVal())
		bDebug = true;

	bool bUpdateEntities=CVar::pUpdateEntities->GetIVal()?true:false;
	bool bProfileEntities = CVar::pProfileEntities->GetIVal() >= 1 || bDebug;
	bool bProfileEntitiesToLog = CVar::pProfileEntities->GetIVal() == 2;
	bool bProfileEntitiesAll = CVar::pProfileEntities->GetIVal() == 3;
	bool bProfileEntitiesDesigner = CVar::pProfileEntities->GetIVal() == 4;

	if (bProfileEntitiesAll)
		bProfileEntitiesToLog=true;

	float fStartTime = 0;

	SEntityUpdateContext ctx;
	ctx.nFrameID = nRendererFrameID;
	ctx.pCamera = &Cam;
	ctx.fCurrTime = m_pISystem->GetITimer()->GetCurrTime();
	ctx.fFrameTime = m_pISystem->GetITimer()->GetFrameTime();
	ctx.bProfileToLog = bProfileEntitiesToLog;
	ctx.numVisibleEntities = 0;
	ctx.numUpdatedEntities = 0;
	ctx.fMaxViewDist = Cam.GetFarPlane();
	ctx.fMaxViewDistSquared = ctx.fMaxViewDist*ctx.fMaxViewDist;
	ctx.vCameraPos = Cam.GetPosition();

	if (!bProfileEntities)
	{	
		// Copy active entity ids into temporary buffer, this is needed because some entity can be added or deleted during Update call.
		UpdateTempActiveEntities();
		int numActive = m_tempActiveEntities.size();
		for (int i = 0; i < numActive; i++)
		{
			CEntity *pEntity = GetEntityFromID( m_tempActiveEntities[i] );
			if (pEntity)
			{
				pEntity->Update( ctx );
			}
		}
	}
	else
	{		
		if (bProfileEntitiesToLog)
			CryLogAlways( "================= Entity Update Times =================" );

		char szProfInfo[256];
		int prevNumUpdated;
		float fProfileStartTime;

		float xpos=10;
		float ypos=70;

		// test consistency before running updates
		CheckInternalConsistency();

		int nCounter=0;

		// Copy active entity ids into temporary buffer, this is needed because some entity can be added or deleted during Update call.
		int i = 0;
		UpdateTempActiveEntities();
		int numActive = m_tempActiveEntities.size();
		for (i = 0; i < numActive; i++)
		{
			CEntity *ce = GetEntityFromID( m_tempActiveEntities[i] );

			fProfileStartTime = m_pISystem->GetITimer()->GetAsyncCurTime();
			prevNumUpdated = ctx.numUpdatedEntities;

			if (!ce || ce->m_bGarbage)
				continue;

			ce->Update( ctx );
			ctx.numUpdatedEntities++;

			//if (prevNumUpdated != ctx.numUpdatedEntities || bProfileEntitiesAll)
			{
				float time = m_pISystem->GetITimer()->GetAsyncCurTime() - fProfileStartTime;
				if (time < 0)
					time = 0;

				float timeMs = time*1000.0f;
				if (bProfileEntitiesToLog || bProfileEntitiesDesigner)
				{
					bool bAIEnabled=false;
					bool bIsAI=false;
					if (ce->GetAI())
					{
						bIsAI=true;
						if (ce->GetAI()->IsEnabled())
							bAIEnabled=true;
						else 
						{
							SAIBodyInfo bodyInfo;
							ce->GetAI()->GetProxy()->QueryBodyInfo( bodyInfo );
							bAIEnabled = bodyInfo.linkedDriverEntity!=NULL;
						}
					}

					CRenderProxy *pProxy=ce->GetRenderProxy();

					float fDiff=-1;
					if (pProxy)
					{
						fDiff=m_pISystem->GetITimer()->GetCurrTime() - pProxy->GetLastSeenTime();
						if (bProfileEntitiesToLog)
							CryLogAlways( "(%d) %.3f ms : %s (was last visible %0.2f seconds ago)-AI =%s (%s)",nCounter,timeMs,ce->GetEntityTextDescription(),fDiff,bIsAI?"":"NOT an AI",bAIEnabled?"true":"false" );
					}
					else
					{
						if (bProfileEntitiesToLog)
							CryLogAlways( "(%d) %.3f ms : %s -AI =%s (%s) ",nCounter,timeMs,ce->GetEntityTextDescription(),bIsAI?"":"NOT an AI",bAIEnabled?"true":"false" );
					}

					if (bProfileEntitiesDesigner && (pProxy && bIsAI))
					{
						if ((fDiff>180&&ce->GetAI()) || !bAIEnabled)
						{
/*
							if (fDiff>30&&ce->GetAI())
							{
								if(ce->GetAI() && ce->GetAI()->GetProxy() && ce->GetAI()->GetProxy()->IsUpdateAlways())
									sprintf(szProfInfo,"Entity: %s is UpdateAlways, but is never visible",ce->GetEntityTextDescription());
								else
									sprintf(szProfInfo,"Entity: %s is forced to being updated, but is never visible",ce->GetEntityTextDescription());
							}
							else
*/
							if (fDiff>180&&ce->GetAI())
								sprintf(szProfInfo,"Entity: %s is updated, but is not visible for more then 3 min",ce->GetEntityTextDescription());
							else
								sprintf(szProfInfo,"Entity: %s is force being updated, but is not required by AI",ce->GetEntityTextDescription());
							float colors[4]={1,1,1,1};
							m_pISystem->GetIRenderer()->Draw2dLabel( xpos,ypos,1.5f,colors,false,szProfInfo );
							ypos+=12;
						}
					}
				}

				DebugDraw( ce,timeMs );
			}
		} //it

		int nNumRenderable = 0;
		int nNumPhysicalize = 0;
		int nNumScriptable = 0;
		if (bDebug || bProfileEntitiesToLog)
		{
			// Draw the rest of not active entities
			uint32 dwMaxUsed = (uint32)m_EntitySaltBuffer.GetMaxUsed()+1;

			for(uint32 dwI=0;dwI<dwMaxUsed;++dwI)
			{
				CEntity *ce = m_EntityArray[dwI];

				if (!ce)
					continue;

				if (ce->GetProxy(ENTITY_PROXY_RENDER))
					nNumRenderable++;
				if (ce->GetProxy(ENTITY_PROXY_PHYSICS))
					nNumPhysicalize++;
				if (ce->GetProxy(ENTITY_PROXY_SCRIPT))
					nNumScriptable++;

				if (ce->m_bGarbage || ce->GetUpdateStatus())
					continue;

				DebugDraw( ce,0 );
			}
		}

		//ctx.numUpdatedEntities = (int)m_mapActiveEntities.size();

		int numEnts = GetNumEntities();
			
		if (bProfileEntitiesToLog)
		{
			CryLogAlways( "================= Entity Update Times =================" );
			CryLogAlways( "%d Entities Updated.",ctx.numUpdatedEntities );
			CryLogAlways( "%d Visible Entities Updated.",ctx.numVisibleEntities );
			CryLogAlways( "%d Active Entity Timers.",(int)m_timersMap.size() );			
			CryLogAlways("Entities: Total=%d, Active=%d, Renderable=%d, Phys=%d, Script=%d",numEnts,ctx.numUpdatedEntities,
				nNumRenderable,nNumPhysicalize,nNumScriptable );

			CVar::pProfileEntities->Set(0);

			if (bProfileEntitiesAll)
			{			
				uint32 dwRet=0;
				uint32 dwMaxUsed = (uint32)m_EntitySaltBuffer.GetMaxUsed()+1;

				for(uint32 dwI=0;dwI<dwMaxUsed;++dwI)
				{
					CEntity *ce = m_EntityArray[dwI];

					if(ce)
					{						
						CryLogAlways( "(%d) : %s ",dwRet,ce->GetEntityTextDescription());
						++dwRet;
					}
				} //dwI
				CVar::pProfileEntities->Set(0);
			}			

		}

		if (bDebug)
			sprintf(szProfInfo,"Entities: Total=%d, Active=%d, Renderable=%d, Phys=%d, Script=%d",numEnts,ctx.numUpdatedEntities,
			nNumRenderable,nNumPhysicalize,nNumScriptable );
		else
			sprintf(szProfInfo,"Entities: Total=%d Active=%d",numEnts,ctx.numUpdatedEntities );
		float colors[4]={1,1,1,1};
		m_pISystem->GetIRenderer()->Draw2dLabel( 10,10,1.5,colors,false,szProfInfo );
	}
}


//////////////////////////////////////////////////////////////////////////
void CEntitySystem::CheckInternalConsistency() const
{
	// slow - code should be kept commented out unless specific problems needs to be tracked
	/*
	std::map<EntityId,CEntity*>::const_iterator it, end=m_mapActiveEntities.end();

	for(it=m_mapActiveEntities.begin(); it!=end; ++it)
	{
		CEntity *ce= it->second;
		EntityId id=it->first;

		CEntity *pSaltBufferEntitiyPtr = GetEntityFromID(id);

		assert(ce==pSaltBufferEntitiyPtr);		// internal consistency is broken
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
IEntityIt *CEntitySystem::GetEntityIterator()
{
	return (IEntityIt*)new CEntityItMap(this);
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySystem::IsIDUsed( EntityId nID ) const
{
	return m_EntitySaltBuffer.IsUsed(nID);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::SendEventToAll( SEntityEvent &event )
{
	uint32 dwMaxUsed = (uint32)m_EntitySaltBuffer.GetMaxUsed()+1;

	for(uint32 dwI=0;dwI<dwMaxUsed;++dwI)
	{
		CEntity *pEntity = m_EntityArray[dwI];

		if(pEntity)
			pEntity->SendEvent( event );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::GetMemoryStatistics(ICrySizer *pSizer)
{
	unsigned nSize = sizeof(*this) ;// + m_EntityIDGenerator.sizeofThis() - sizeof(m_EntityIDGenerator);

	nSize += m_guidMap.size()*sizeof(EntityGuidMap::value_type);

	uint32 dwMaxUsed = (uint32)m_EntitySaltBuffer.GetMaxUsed()+1;

	{
		SIZER_COMPONENT_NAME(pSizer,"Entities");
		for(uint32 dwI=0;dwI<dwMaxUsed;++dwI)
		{
			CEntity *pEntity = m_EntityArray[dwI];

			if(pEntity)
				pEntity->GetMemoryStatistics(pSizer);
		}

	}

	nSize += m_EntityArray.size() * sizeof(CEntity *);	
	nSize += m_lstSinks.size() * sizeof(IEntitySystemSink*);
	
	// Calc partition grid memory stats.
	m_pPartitionGrid->GetMemoryStatistics( pSizer );

	pSizer->AddObject(this, nSize);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::AddTimerEvent( SEntityTimerEvent &event, CTimeValue startTime )
{
	CTimeValue millis;
	millis.SetMilliSeconds(event.nMilliSeconds);
	CTimeValue nTriggerTime = startTime + millis;
	m_timersMap.insert( EntityTimersMap::value_type(nTriggerTime,event) );


	if (CVar::es_DebugTimers)
	{
		CEntity *pEntity = GetEntityFromID(	event.entityId );
		if (pEntity)
			CryLogAlways( "SetTimer (timerID=%d,time=%dms) for Entity %s",event.nTimerId,event.nMilliSeconds,pEntity->GetEntityTextDescription() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RemoveTimerEvent( EntityId id,int nTimerId )
{
	if (nTimerId < 0)
	{
		// Delete all timers of this entity.
		EntityTimersMap::iterator next;
		for (EntityTimersMap::iterator it = m_timersMap.begin(); it != m_timersMap.end(); it = next)
		{
			next = it; next++;
			if (id == it->second.entityId)
			{
				// Remove this item.
				m_timersMap.erase( it ); 
			}
		}
	}
	else
	{
		for (EntityTimersMap::iterator it = m_timersMap.begin(); it != m_timersMap.end(); ++it)
		{
			if (id == it->second.entityId && nTimerId == it->second.nTimerId)
			{
				// Remove this item.
				m_timersMap.erase( it ); 
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::PauseTimers(bool bPause,bool bResume)
{	
	m_bTimersPause=bPause;	
	if (bResume)
	{
		m_nStartPause.SetSeconds(-1.0f);
		return; // just allow timers to be updated next time
	}

	if (bPause)
	{ 
		// record when timers pause was called
		m_nStartPause = m_pISystem->GetITimer()->GetFrameStartTime();
	}
	else if (m_nStartPause > CTimeValue(0.0f))
	{		
		// increase the timers by adding the delay time passed since when
		// it was paused
		CTimeValue nCurrTimeMillis = m_pISystem->GetITimer()->GetFrameStartTime();
		CTimeValue nAdditionalTriggerTime = nCurrTimeMillis-m_nStartPause;
		
		EntityTimersMap::iterator it;
		EntityTimersMap lstTemp;

		for (it = m_timersMap.begin();it!=m_timersMap.end();it++)
		{
			lstTemp.insert( EntityTimersMap::value_type(it->first,it->second) );			
		} //it

		m_timersMap.clear();

		for (it = lstTemp.begin();it!=lstTemp.end();it++)
		{
			CTimeValue nUpdatedTimer = nAdditionalTriggerTime + it->first;
			m_timersMap.insert( EntityTimersMap::value_type(nUpdatedTimer,it->second) );			
		} //it	

		m_nStartPause.SetSeconds(-1.0f);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::UpdateTimers()
{
	if (m_timersMap.empty())
		return;

	FUNCTION_PROFILER_FAST( m_pISystem,PROFILE_ENTITY,g_bProfilerEnabled );

	CTimeValue nCurrTimeMillis = m_pISystem->GetITimer()->GetFrameStartTime();

	// Iterate thru all matching timers.
	EntityTimersMap::iterator first = m_timersMap.begin();
	EntityTimersMap::iterator last = m_timersMap.upper_bound( nCurrTimeMillis );
	if (last != first)
	{
		// Make a separate list, because OnTrigger call can modify original timers map.
		m_currentTriggers.resize(0);
		m_currentTriggers.reserve(10); 
		for (EntityTimersMap::iterator it = first; it != last; ++it)
		{
			m_currentTriggers.push_back(it->second); 
		}

		// Delete these items from map.
		m_timersMap.erase( first,last );

		//////////////////////////////////////////////////////////////////////////
		// Execute OnTrigger events.
		for (int i = 0; i < (int)m_currentTriggers.size(); i++)
		{
			SEntityTimerEvent &event = m_currentTriggers[i];
			// Trigger it.
			CEntity *pEntity = (CEntity*)GetEntity( event.entityId );
			if (pEntity)
			{
				// Send Timer event to the entity.
				SEntityEvent entityEvent;
				entityEvent.event = ENTITY_EVENT_TIMER;
				entityEvent.nParam[0] = event.nTimerId;
				entityEvent.nParam[1] = event.nMilliSeconds;
				pEntity->SendEvent( entityEvent );

				if (CVar::es_DebugTimers)
				{
					CEntity *pEntity = GetEntityFromID(	event.entityId );
					if (pEntity)
						CryLogAlways( "OnTimer Event (timerID=%d,time=%dms) for Entity %s",event.nTimerId,event.nMilliSeconds,pEntity->GetEntityTextDescription() );
				}
			}
		}
	}
} 


//////////////////////////////////////////////////////////////////////////
void CEntitySystem::ReserveEntityId( const EntityId id )
{
	assert(id);

	const CSaltHandle<> hdl = IdToHandle(id);
	if (m_EntitySaltBuffer.IsUsed(hdl.GetIndex())) // Do not reserve if already used.
		return;
	//assert(m_EntitySaltBuffer.IsUsed(hdl.GetIndex()) == false);	// don't reserve twice or overriding in used one

	m_EntitySaltBuffer.InsertKnownHandle(hdl);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::ActivateEntity( CEntity *pEntity,bool bActivate )
{
	assert(pEntity==GetEntityFromID(pEntity->GetId()));

	if (bActivate)
	{
		m_mapActiveEntities[pEntity->GetId()] = pEntity;
		pEntity->m_bInActiveList = true;
	}
	else
	{
		m_mapActiveEntities.erase(pEntity->GetId());
		pEntity->m_bInActiveList = false;
	}
	m_tempActiveEntitiesValid = false;
}

void CEntitySystem::ActivatePrePhysicsUpdateForEntity( CEntity *pEntity,bool bActivate )
{
	if (bActivate)
	{
		m_mapPrePhysicsEntities.insert(pEntity->GetId());
	}
	else
	{
		m_mapPrePhysicsEntities.erase( pEntity->GetId() );
	}
}

bool CEntitySystem::IsPrePhysicsActive( CEntity * pEntity )
{
	return m_mapPrePhysicsEntities.find(pEntity->GetId()) != m_mapPrePhysicsEntities.end();
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::OnEntityEvent( CEntity *pEntity,SEntityEvent &event )
{
	for (SinkList::iterator iter = m_lstSinks.begin(); iter != m_lstSinks.end(); ++iter)
		(*iter)->OnEvent( pEntity, event );

	// Just an optimization, most entities do not have event listeners.
	if (pEntity->m_bHaveEventListeners)
	{
		//////////////////////////////////////////////////////////////////////////
		// To be optimized later.
		//////////////////////////////////////////////////////////////////////////
		EventListenersMap &entitiesMap = m_eventListeners[event.event];
		EventListenersMap::iterator it = entitiesMap.lower_bound(pEntity->m_nID);
		while (it != entitiesMap.end() && it->first == pEntity->m_nID)
		{
			IEntityEventListener *pListener = it->second;
			if(pListener)
			{
				pListener->OnEntityEvent( pEntity,event );
			}
			++it;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::LoadEntities( XmlNodeRef &objectsNode )
{
  LOADING_TIME_PROFILE_SECTION(GetSystem());

	CEntityLoader loader(this);
	loader.LoadEntities(objectsNode);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::AddEntityEventListener( EntityId nEntity,EEntityEvent event,IEntityEventListener *pListener )
{
	CEntity *pCEntity = (CEntity*)GetEntity( nEntity );
	if (pCEntity)
	{
		pCEntity->m_bHaveEventListeners = true; // Just an optimization, most entities dont have it.
		m_eventListeners[event].insert( EventListenersMap::value_type(nEntity,pListener) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RemoveEntityEventListener( EntityId nEntity,EEntityEvent event,IEntityEventListener *pListener )
{
	EventListenersMap &entitiesMap = m_eventListeners[event];
	EventListenersMap::iterator it = entitiesMap.lower_bound(nEntity);
	while (it != entitiesMap.end() && it->first == nEntity)
	{
		if (pListener == it->second)
		{
			entitiesMap.erase(it);
			break;
		}
		it++;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DebugDraw( CEntity *ce,float timeMs )
{
	char szProfInfo[256];

	IRenderer *pRender = m_pISystem->GetIRenderer();
	IRenderAuxGeom *pRenderAux = pRender->GetIRenderAuxGeom();

	Vec3 wp = ce->GetWorldTM().GetTranslation();

	if (wp.IsZero())
		return;

	//float z = 1.0f / max(0.01f,wp.GetDistance(gEnv->pSystem->GetViewCamera().GetPosition()) );
	//float fFontSize = 

	if (ce->GetUpdateStatus() && timeMs >= 0)
	{
		float colors[4]={1,1,1,1};
		float colorsYellow[4]={1,1,0,1};
		float colorsRed[4]={1,0,0,1};

		CRenderProxy *pProxy=ce->GetRenderProxy();
		if (pProxy)
			sprintf(szProfInfo, "%.3f ms : %s (%0.2f ago)",timeMs,ce->GetEntityTextDescription(),m_pISystem->GetITimer()->GetCurrTime() - pProxy->GetLastSeenTime() );
		else
			sprintf(szProfInfo,"%.3f ms : %s",timeMs,ce->GetEntityTextDescription() );
		if (timeMs > 0.5f)
			pRender->DrawLabelEx(wp,1.3f,colorsYellow,true,true,szProfInfo);
		else if (timeMs > 1.0f)
			pRender->DrawLabelEx(wp,1.6f,colorsRed,true,true,szProfInfo);
		else
			pRender->DrawLabelEx(wp,1.1f,colors,true,true,szProfInfo);
	}
	else
	{
		float colors[4]={1,1,1,1};
		pRender->DrawLabelEx( wp,1.2f,colors,true,true,ce->GetEntityTextDescription() );
	}

	// Draw bounds.
	AABB box;
	ce->GetLocalBounds( box );
	if (box.min == box.max)
	{
		box.min = wp - Vec3(0.1f,0.1f,0.1f);
		box.max = wp + Vec3(0.1f,0.1f,0.1f);
	}

	pRenderAux->DrawAABB( box,ce->GetWorldTM(),false,ColorB(255,255,0), eBBD_Extremes_Color_Encoded );
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::RegisterEntityGuid( const EntityGUID &guid,EntityId id )
{
	m_guidMap.insert( EntityGuidMap::value_type(guid,id) );
	CEntity* pCEntity = (CEntity*)GetEntity( id );
	if (pCEntity)
		pCEntity->m_guid = guid;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::UnregisterEntityGuid( const EntityGUID &guid )
{
	m_guidMap.erase(guid);
}

//////////////////////////////////////////////////////////////////////////
EntityId CEntitySystem::FindEntityByGuid( const EntityGUID &guid ) const
{
	if (CVar::es_DebugFindEntity != 0)
	{
#if defined(_MSC_VER)
		CryLog( "FindEntityByGuid: %I64X",guid );
#else
		CryLog( "FindEntityByGuid: %llX",(long long)guid );
#endif
	}
	return stl::find_in_map(m_guidMap,guid,0);
}

//////////////////////////////////////////////////////////////////////////
IEntityArchetype* CEntitySystem::LoadEntityArchetype( const char *sArchetype )
{
	return m_pEntityArchetypeManager->LoadArchetype( sArchetype );
}

//////////////////////////////////////////////////////////////////////////
IEntityArchetype* CEntitySystem::CreateEntityArchetype( IEntityClass *pClass,const char *sArchetype )
{
	return m_pEntityArchetypeManager->CreateArchetype( pClass,sArchetype );
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::Serialize(TSerialize ser)
{
	if(ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("Timers");

		ser.Value("Paused", m_bTimersPause);
		ser.Value("PauseStart", m_nStartPause);

		int count = m_timersMap.size();
		ser.Value("timerCount", count);

		SEntityTimerEvent tempEvent;
		if(ser.IsWriting())
		{
			EntityTimersMap::iterator it;
			for(it = m_timersMap.begin(); it != m_timersMap.end(); ++it)
			{
				tempEvent = it->second;

				ser.BeginGroup("Timer");
				ser.Value("entityID", tempEvent.entityId);
				ser.Value("eventTime", tempEvent.nMilliSeconds);
				ser.Value("timerID", tempEvent.nTimerId);
				CTimeValue start = it->first; 
				ser.Value("startTime", start);
				ser.EndGroup();
			}
		}
		else
		{
			m_timersMap.clear();

			CTimeValue start;
			for(int c = 0; c < count; ++c)
			{
				ser.BeginGroup("Timer");
				ser.Value("entityID", tempEvent.entityId);
				ser.Value("eventTime", tempEvent.nMilliSeconds);
				ser.Value("timerID", tempEvent.nTimerId);
				ser.Value("startTime", start);
				ser.EndGroup();
				start.SetMilliSeconds((int64)(start.GetMilliSeconds()-tempEvent.nMilliSeconds));
				AddTimerEvent(tempEvent, start);

				//assert(GetEntity(tempEvent.entityId));
			}
		}

		ser.EndGroup();

		if (gEnv->pScriptSystem)
		{
			gEnv->pScriptSystem->SerializeTimers( GetImpl(ser) );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::ResizeProximityGrid( int nWidth,int nHeight )
{
	if (m_pPartitionGrid)
		m_pPartitionGrid->AllocateGrid( nWidth,nHeight );
}


//////////////////////////////////////////////////////////////////////////
void CEntitySystem::SetNextSpawnId(EntityId id)
{
	if(id)
		RemoveEntity(id,true);
	m_idForced = id;
}

void CEntitySystem::ResetAreas()
{
	m_pAreaManager->ResetAreas();
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DumpEntities()
{
  IEntityItPtr it	= GetEntityIterator();
  it->MoveFirst();

  int count = 0;
  int acount = 0;

  CryLogAlways("--------------------------------------------------------------------------------");
  while(IEntity *pEntity = it->Next())
  {
    DumpEntity(pEntity);

    if (pEntity->IsActive())
      ++acount;

    ++count;
  }
  CryLogAlways("--------------------------------------------------------------------------------");
  CryLogAlways(" %d entities (%d active)", count, acount);
  CryLogAlways("--------------------------------------------------------------------------------");
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::DumpEntity(IEntity *pEntity)
{
  static struct SFlagData
  {
    unsigned mask;
    const char * name;
  } flagData[] = {
    {ENTITY_FLAG_CLIENT_ONLY, "Client"},
    {ENTITY_FLAG_SERVER_ONLY, "Server"},
    {ENTITY_FLAG_UNREMOVABLE, "NoRemove"},
    {ENTITY_FLAG_NET_PRESENT, "Net"},
    {ENTITY_FLAG_CLIENTSIDE_STATE, "ClientState"},
    {ENTITY_FLAG_MODIFIED_BY_PHYSICS, "PhysMod"},
  };

  string name(pEntity->GetName());
  name += string("[$9") + pEntity->GetClass()->GetName() + string("$o]");
  Vec3 pos(pEntity->GetWorldPos());
  const char *sStatus = pEntity->IsActive() ? "[$3Active$o]" : "[$9Inactive$o]";
  if (pEntity->IsHidden())
    sStatus = "[$9Hidden$o]";

  std::set<string> allFlags;
  for (size_t i=0; i<sizeof(flagData)/sizeof(*flagData); ++i)
  {
    if ((flagData[i].mask & pEntity->GetFlags()) == flagData[i].mask)
      allFlags.insert(flagData[i].name);
  }
  string flags;
  for (std::set<string>::iterator iter = allFlags.begin(); iter != allFlags.end(); ++iter)
  {
    flags += ' ';
    flags += *iter;
  }

	CryLogAlways("%5d: Salt:%d  %-42s  %-14s  pos: (%.2f %.2f %.2f) %d %s",
		pEntity->GetId()&0xffff,pEntity->GetId()>>16,
		name.c_str(), sStatus, pos.x, pos.y, pos.z, pEntity->GetId(), flags.c_str());
}

void CEntitySystem::DeletePendingEntities()
{
	UpdateDeletedEntities();
}

//////////////////////////////////////////////////////////////////////////
void CEntitySystem::ChangeEntityName( CEntity *pEntity,const char *sNewName )
{
	if (!pEntity->m_szName.empty())
	{
		// Remove old name.
		EntityNamesMap::iterator it = m_mapEntityNames.lower_bound(pEntity->m_szName.c_str());
		for (; it != m_mapEntityNames.end(); ++it)
		{
			if (it->second == pEntity->m_nID)
			{
				m_mapEntityNames.erase(it);
				break;
			}
			if (stricmp(it->first,pEntity->m_szName.c_str()) != 0)
				break;
		}
	}

	if (sNewName[0] != 0)
	{
		pEntity->m_szName = sNewName;
		// Insert new name into the map.
		m_mapEntityNames.insert( EntityNamesMap::value_type(pEntity->m_szName.c_str(),pEntity->m_nID) );
	}
	else
	{
		pEntity->m_szName = sNewName;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySystem::IsInDiagArea( const Vec3 &pos )
{
	return m_pAreaManager->IsInDiagArea( pos );
}
