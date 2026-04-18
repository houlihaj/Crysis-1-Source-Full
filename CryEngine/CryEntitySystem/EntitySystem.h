////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   EntitySystem.h
//  Version:     v1.00
//  Created:     24/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __EntitySystem_h__
#define __EntitySystem_h__
#pragma once

#include "IEntitySystem.h"
#include <ISystem.h>
#include "ITimer.h"
#include "SaltBufferArray.h"					// SaltBufferArray<>
#include "EntityTimeoutList.h"
#include <StlUtils.h>
#include "STLPoolAllocator.h"

//////////////////////////////////////////////////////////////////////////
// forward declarations.
//////////////////////////////////////////////////////////////////////////
class  CEntity;
struct ICVar;
struct IPhysicalEntity;
class  CEntityClassRegistry;
class  CScriptBind_Entity;
class  CScriptBind_Boids;
class  CPhysicsEventListener;
class  CAreaManager;
class  CBreakableManager;
class  CEntityArchetypeManager;
class  CPartitionGrid;
class  CProximityTriggerSystem;
//////////////////////////////////////////////////////////////////////////

#ifdef _AMD64_
// workaround for Amd64 compiler
#include <map>
#define hash_map map 
#else //_AMD64_
#if defined(LINUX)
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#endif //_AMD64_

//////////////////////////////////////////////////////////////////////////
#if defined(LINUX)
typedef __gnu_cxx::hash_map<EntityId,CEntity*> EntityMap;
#else
typedef stl::hash_map<EntityId,CEntity*,stl::hash_simple<EntityId> > EntityMap;
#endif



//////////////////////////////////////////////////////////////////////////
struct SEntityTimerEvent
{
	EntityId entityId;
	int nTimerId;
	int nMilliSeconds;
};


typedef CSaltHandle<unsigned short,unsigned short> CEntityHandle;

//////////////////////////////////////////////////////////////////////
class CEntitySystem : public IEntitySystem
{
public:
	CEntitySystem(ISystem *pSystem);
	~CEntitySystem();

	bool Init(ISystem *pSystem);

	// interface IEntitySystem ------------------------------------------------------

	virtual void Release();
	virtual void Update();
	virtual void DeletePendingEntities();
	virtual void PrePhysicsUpdate();
	virtual void Reset();
	virtual IEntityClassRegistry* GetClassRegistry();
	virtual IEntity* SpawnEntity( SEntitySpawnParams &params,bool bAutoInit=true );
	virtual bool InitEntity( IEntity* pEntity,SEntitySpawnParams &params );
	virtual IEntity* GetEntity( EntityId id ) const;
	virtual EntityId GetEntityIdInSlot( EntityId id ) const;
	virtual IEntity* FindEntityByName( const char *sEntityName ) const;
	virtual void ReserveEntityId( const EntityId id );
	virtual void RemoveEntity( EntityId entity,bool bForceRemoveNow=false );
	virtual uint32 GetNumEntities() const;
	virtual IEntityIt* GetEntityIterator();
	virtual void SendEventToAll( SEntityEvent &event );
	virtual int QueryProximity( SEntityProximityQuery &query );
	virtual void ResizeProximityGrid( int nWidth,int nHeight );
	virtual int GetPhysicalEntitiesInBox( const Vec3 &origin, float radius,IPhysicalEntity **&pList, int physFlags ) const;
	virtual IEntity* GetEntityFromPhysics( IPhysicalEntity *pPhysEntity ) const;
	virtual void AddSink( IEntitySystemSink *pSink );
	virtual void RemoveSink( IEntitySystemSink *pSink );
	virtual void PauseTimers(bool bPause,bool bResume=false);
	virtual bool IsIDUsed( EntityId nID ) const;
	virtual void GetMemoryStatistics(ICrySizer *pSizer);
	virtual ISystem* GetSystem() const { return m_pISystem; };
	virtual void SetNextSpawnId(EntityId id);
	virtual void ResetAreas();

	virtual void AddEntityEventListener( EntityId nEntity,EEntityEvent event,IEntityEventListener *pListener );
	virtual void RemoveEntityEventListener( EntityId nEntity,EEntityEvent event,IEntityEventListener *pListener );

	virtual EntityId FindEntityByGuid( const EntityGUID &guid ) const;

	virtual IEntityArchetype* LoadEntityArchetype( const char *sArchetype );
	virtual IEntityArchetype* CreateEntityArchetype( IEntityClass *pClass,const char *sArchetype );

	virtual void Serialize(TSerialize ser);

  virtual void DumpEntities();  

	virtual void LockSpawning(bool lock) {m_bLocked = lock;}
	// ------------------------------------------------------------------------

	bool OnBeforeSpawn(SEntitySpawnParams &params);

	// Sets new entity timer event.
	void AddTimerEvent( SEntityTimerEvent &event, CTimeValue startTime = gEnv->pTimer->GetFrameStartTime());

	//////////////////////////////////////////////////////////////////////////
	// Load entities from XML.
	void LoadEntities( XmlNodeRef &objectsNode );

	//////////////////////////////////////////////////////////////////////////
	// Called from CEntity implementation.
	//////////////////////////////////////////////////////////////////////////
	void RemoveTimerEvent( EntityId id,int nTimerId );
	void ChangeEntityNameRemoveTimerEvent( EntityId id );

	// Puts entity into active list.
	void ActivateEntity( CEntity *pEntity,bool bActivate );
	void ActivatePrePhysicsUpdateForEntity( CEntity *pEntity,bool bActivate );
	bool IsPrePhysicsActive( CEntity * pEntity );
	void OnEntityEvent( CEntity *pEntity,SEntityEvent &event );

	// Access to class that binds script to entity functions.
	// Used by Script proxy.
	CScriptBind_Entity* GetScriptBindEntity() { return m_pEntityScriptBinding; };

	// Access to area manager.
	CAreaManager* GetAreaManager() const { return m_pAreaManager; }
 	virtual bool IsInDiagArea( const Vec3& pos );

	// Access to breakable manager.
	virtual IBreakableManager* GetBreakableManager() const { return m_pBreakableManager; };

	static ILINE CSaltHandle<> IdToHandle( const EntityId id ) { return CSaltHandle<>(id>>16,id&0xffff); }
	static ILINE EntityId HandleToId( const CSaltHandle<> id ) { return (((uint32)id.GetSalt())<<16) | ((uint32)id.GetIndex()); }

	void RegisterEntityGuid( const EntityGUID &guid,EntityId id );
	void UnregisterEntityGuid( const EntityGUID &guid );

	CPartitionGrid* GetPartitionGrid() { return m_pPartitionGrid; }
	CProximityTriggerSystem * GetProximityTriggerSystem() { return m_pProximityTriggerSystem;	}

	static const uint32 scSizeofSIndirectLightingData = 32;//to not include it here, compile time assert makes sure it fits

	void *AllocIndirLightObject() { return m_indirLightAlloc.Allocate();	}
	void DeAllocIndirLightObject(void *pObject) { m_indirLightAlloc.Deallocate(pObject); }
	
	void ChangeEntityName( CEntity *pEntity,const char *sNewName );

	ILINE CEntity* GetEntityFromID( EntityId id ) const { return m_EntityArray[IdToHandle(id).GetIndex()]; }

private: // -----------------------------------------------------------------
	void DoUpdateLoop();

//	void InsertEntity( EntityId id,CEntity *pEntity );
	void DeleteEntity( IEntity *entity );
	void UpdateEntity(CEntity *ce,SEntityUpdateContext &ctx);
	void UpdateDeletedEntities();

	void UpdateNotSeenTimeouts();

	void OnBind( EntityId id,EntityId child );
	void OnUnbind( EntityId id,EntityId child );

	void UpdateTimers();
	void DebugDraw( CEntity *pEntity,float fUpdateTime );

	void ClearEntityArray();

  void DumpEntity(IEntity* pEntity);

	void UpdateTempActiveEntities();

	// slow - to find specific problems
	void CheckInternalConsistency() const;

	//////////////////////////////////////////////////////////////////////////
	// Variables.
	//////////////////////////////////////////////////////////////////////////

	typedef std::list<IEntitySystemSink*> SinkList;
	typedef std::list<CEntity*>	DeletedEntities;
	typedef std::multimap<CTimeValue,SEntityTimerEvent,std::less<CTimeValue>,stl::STLPoolAllocator<std::pair<CTimeValue,SEntityTimerEvent>,stl::PoolAllocatorSynchronizationSinglethreaded> > EntityTimersMap;
	typedef std::multimap<const char*,EntityId,stl::less_stricmp<const char*> > EntityNamesMap;
	// Hack to make sure 'Dude' (0x7777) is updated last
	struct EntityIdCompare
	{
		bool operator()( EntityId a, EntityId b ) const
		{
			a &= 0xffff;
			b &= 0xffff;
			if (a == 0x7777)
				a = 0x10000;
			if (b == 0x7777)
				b = 0x10000;
			return a < b;
		}
	};
	typedef std::map<EntityId,CEntity*,EntityIdCompare> EntitiesMap;
	typedef std::set<EntityId> EntitiesSet;

	SinkList												m_lstSinks;							// registered sinks get callbacks for creation and removal
	ISystem *												m_pISystem;
	std::vector<CEntity *>					m_EntityArray;					// [id.GetIndex()]=CEntity
	DeletedEntities									m_deletedEntities;
	EntitiesMap                     m_mapActiveEntities;		// Map of currently active entities (All entities that need per frame update).
	bool                            m_tempActiveEntitiesValid; // must be set to false whenever m_mapActiveEntities is changed
	EntitiesSet                     m_mapPrePhysicsEntities; // map of entities requiring pre-physics activation

	EntityNamesMap                  m_mapEntityNames;  // Map entity name to entity ID.

	CSaltBufferArray<>							m_EntitySaltBuffer;			// used to create new entity ids (with uniqueid=salt)
	std::vector<EntityId>           m_tempActiveEntities;   // Temporary array of active entities.

	//////////////////////////////////////////////////////////////////////////

	// Entity timers.
	EntityTimersMap									m_timersMap;
	std::vector<SEntityTimerEvent>	m_currentTriggers;
	bool														m_bTimersPause;
	CTimeValue											m_nStartPause;

	// Binding entity.
	CScriptBind_Entity *						m_pEntityScriptBinding;
	CScriptBind_Boids *							m_pScriptBindsBoids;

	// Entity class registry.
	CEntityClassRegistry *					m_pClassRegistry;
	CPhysicsEventListener *					m_pPhysicsEventListener;

	CAreaManager *									m_pAreaManager;

	// There`s a map of entity id to event listeners for each event.
	typedef std::multimap<EntityId,IEntityEventListener*> EventListenersMap;
	EventListenersMap m_eventListeners[ENTITY_EVENT_LAST];

	typedef std::map<EntityGUID,EntityId> EntityGuidMap;
	EntityGuidMap m_guidMap;

	IBreakableManager* m_pBreakableManager;
	CEntityArchetypeManager *m_pEntityArchetypeManager;

	// Partition grid used by the entity system
	CPartitionGrid *m_pPartitionGrid;
	CProximityTriggerSystem* m_pProximityTriggerSystem;

	EntityId m_idForced;

	//don't spawn any entities without being forced to
	bool		m_bLocked;

	CEntityTimeoutList m_entityTimeoutList;

	friend class CEntityItMap;
	class CCompareEntityIdsByClass;

	// helper to satisfy GCC
	static IEntityClass * GetClassForEntity( CEntity * );

	//////////////////////////////////////////////////////////////////////////
	// Pool Allocators.
	//////////////////////////////////////////////////////////////////////////
public:
	stl::PoolAllocatorNoMT<scSizeofSIndirectLightingData> m_indirLightAlloc;	//pool allocator for indirect lighting
	bool m_bReseting;
	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// Precache resources mode state.
//////////////////////////////////////////////////////////////////////////
extern bool gPrecacheResourcesMode;

#endif // __EntitySystem_h__
