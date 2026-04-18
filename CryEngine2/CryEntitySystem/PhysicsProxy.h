////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   PhysicsProxy.h
//  Version:     v1.00
//  Created:     25/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __PhysicsProxy_h__
#define __PhysicsProxy_h__
#pragma once

// forward declarations.
struct SEntityEvent;
struct IPhysicalEntity;
struct IPhysicalWorld;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Implements base physical proxy class for entity.
//////////////////////////////////////////////////////////////////////////
class CPhysicalProxy : public IEntityPhysicalProxy
{
public:
	enum EFlags
	{
		CHARACTER_SLOT_MASK     = 0x000F, // Slot Id, of physicalized character.
		// When set Physical proxy will ignore incoming xform events from the entity.
		// Needed to prevent cycle change, when physical entity change entity xform and recieve back an event about entity xform.
		FLAG_IGNORE_XFORM_EVENT = 0x0010,
		FLAG_IGNORE_BUOYANCY    = 0x0020,
		FLAG_PHYSICS_DISABLED   = 0x0040,
		FLAG_SYNC_CHARACTER     = 0x0080,
		FLAG_WAS_HIDDEN					= 0x0100,
		FLAG_PHYS_CHARACTER     = 0x0200,
		FLAG_PHYS_RESTRICTED_RAGDOLL = 0x0400
	};

	CPhysicalProxy( CEntity *pEntity );
	~CPhysicalProxy() {};
	CEntity* GetEntity() const { return m_pEntity; };

	//////////////////////////////////////////////////////////////////////////
	// IEntityProxy interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetType() { return ENTITY_PROXY_PHYSICS; }
	virtual void Release();
	virtual void Done() {};
	virtual	void Update( SEntityUpdateContext &ctx );
	virtual	void ProcessEvent( SEntityEvent &event );
	virtual void Init( IEntity *pEntity,SEntitySpawnParams &params ) {};
	virtual void SerializeXML( XmlNodeRef &entityNode,bool bLoading );
	virtual void Serialize( TSerialize ser );
	virtual bool NeedSerialize();
	virtual void SerializeTyped( TSerialize ser, int type, int flags );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityPhysicalProxy interface.
	//////////////////////////////////////////////////////////////////////////
	virtual void GetLocalBounds( AABB &bbox );
	virtual void GetWorldBounds( AABB &bbox );
		
	virtual void Physicalize( SEntityPhysicalizeParams &params );
	virtual IPhysicalEntity* GetPhysicalEntity() const { return m_pPhysicalEntity; }
	virtual void EnablePhysics( bool bEnable );
	virtual void AddImpulse( int ipart, const Vec3 &pos,const Vec3 &impulse,bool bPos,float fAuxScale );

	virtual void SetTriggerBounds( const AABB &bbox );
	virtual void GetTriggerBounds( AABB &bbox );

  virtual int GetPartId0() { return m_partId0; }

	virtual void	EnableRestrictedRagdoll( bool enable ) { m_nFlags = enable? ( m_nFlags|FLAG_PHYS_RESTRICTED_RAGDOLL ):( m_nFlags&~FLAG_PHYS_RESTRICTED_RAGDOLL ); } ;
	virtual bool	IsRestrictedRagdoll() const { return ( m_nFlags & FLAG_PHYS_RESTRICTED_RAGDOLL ); };
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Called from physics events.
	//////////////////////////////////////////////////////////////////////////
	// Called back by physics PostStep event for the owned physical entity.
	void OnPhysicsPostStep(EventPhysPostStep *pEvent=0);
	void AttachToPhysicalEntity( IPhysicalEntity *pPhysEntity );
	void CreateRenderGeometry( int nSlot,IGeometry *pFromGeom, bop_meshupdate *pLastUpdate=0 );
	void UpdateGeometry( IGeometry *pFromGeom,IStatObj *pStatObj,IStatObj *pOriginalObj );
	void OnContactWithEntity( CEntity *pEntity );
	void OnCollision(CEntity *pTarget, int matId, const Vec3 &pt, const Vec3 &n, const Vec3 &vel, const Vec3 &targetVel, int partId, float mass);
	//////////////////////////////////////////////////////////////////////////

	void SetFlags( int nFlags ) { m_nFlags = nFlags; };
	uint32 GetFlags() const { return m_nFlags; };
	bool CheckFlags( uint32 nFlags ) { return (m_nFlags&nFlags) == nFlags; }
	void UpdateSlotGeometry(int nSlot,IStatObj *pStatObjNew=0,float mass=-1.0f);
	void AssignPhysicalEntity( IPhysicalEntity *pPhysEntity, int nSlot=-1 );

	virtual bool PhysicalizeFoliage( int iSlot );
	virtual void DephysicalizeFoliage( int iSlot );

	int AddSlotGeometry( int nSlot,SEntityPhysicalizeParams &params );
	void RemoveSlotGeometry( int nSlot );

	void SetPartId0(int partId0) { m_partId0 = partId0; }
	
protected:
	IPhysicalWorld* PhysicalWorld() const { return gEnv->pPhysicalWorld; }
	void OnEntityXForm( SEntityEvent &event );
	void OnChangedPhysics( bool bEnabled );
	void DestroyPhysicalEntity( bool bDestroyCharacters=true, int iMode=0 );
	
	void PhysicalizeSimple( SEntityPhysicalizeParams &params );
	void PhysicalizeLiving( SEntityPhysicalizeParams &params );
	void PhysicalizeParticle( SEntityPhysicalizeParams &params );
	void PhysicalizeSoft( SEntityPhysicalizeParams &params );
	void PhysicalizeArea( SEntityPhysicalizeParams &params );
	bool PhysicalizeCharacter( SEntityPhysicalizeParams &params );
	bool ConvertCharacterToRagdoll( SEntityPhysicalizeParams &params, const Vec3 &velInitial );

	void CreatePhysicalEntity( SEntityPhysicalizeParams &params );
	phys_geometry* GetSlotGeometry( int nSlot );
	void SyncCharacterWithPhysics();

	//////////////////////////////////////////////////////////////////////////
	// Handle colliders.
	//////////////////////////////////////////////////////////////////////////
	void AddCollider( EntityId id );
	void RemoveCollider( EntityId id );
	void CheckColliders();
	//////////////////////////////////////////////////////////////////////////

	IPhysicalEntity* QueryPhyscalEntity( IEntity *pEntity ) const;
	CEntity* GetCEntity( IPhysicalEntity* pPhysEntity );

protected:
	uint32 m_nFlags;

	CEntity *m_pEntity;

	 // Pointer to physical object.
	IPhysicalEntity *m_pPhysicalEntity;

	//////////////////////////////////////////////////////////////////////////
	//! List of colliding entities, used by all triggers.
	//! When entity is first added to this list it is considered as entering
	//! to proximity, when it erased from it it is leaving proximity.
	typedef std::set<EntityId> ColliderSet;
	struct Colliders {
		IPhysicalEntity *m_pPhysicalBBox;  // Pointer to physical bounding box (optional).
		ColliderSet colliders;
	};
	Colliders *m_pColliders;
	int m_partId0;	// partid in physics = slot# + m_partsidOffs
	float m_timeLastSync;
};

#endif // __PhysicsProxy_h__

