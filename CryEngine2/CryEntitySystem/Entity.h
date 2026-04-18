////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   Entity.h
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __Entity_h__
#define __Entity_h__
#pragma once

#include <IEntity.h>

//////////////////////////////////////////////////////////////////////////
// This is the order in which the proxies are serialized
const static EEntityProxy ProxySerializationOrder[] = { 	
	ENTITY_PROXY_RENDER,
	ENTITY_PROXY_SCRIPT,
	ENTITY_PROXY_SOUND,
	ENTITY_PROXY_AI,
	ENTITY_PROXY_AREA,
	ENTITY_PROXY_BOIDS,
	ENTITY_PROXY_BOID_OBJECT,
	ENTITY_PROXY_CAMERA,
	ENTITY_PROXY_FLOWGRAPH,
	ENTITY_PROXY_SUBSTITUTION,
	ENTITY_PROXY_TRIGGER,
	ENTITY_PROXY_USER,
	ENTITY_PROXY_PHYSICS,
	ENTITY_PROXY_ROPE,
	ENTITY_PROXY_LAST
};

//////////////////////////////////////////////////////////////////////////
// These headers cannot be replaced with forward references.
// They are needed for correct up casting from IEntityProxy to real proxy class.
#include "RenderProxy.h"
#include "PhysicsProxy.h"
#include "ScriptProxy.h"
#include "SubstitutionProxy.h"
//////////////////////////////////////////////////////////////////////////

// forward declarations.
struct IEntitySystem;
struct IEntityArchetype;
class  CEntitySystem;
struct AIObjectParameters;
struct SGridLocation;
struct SProximityElement;



//////////////////////////////////////////////////////////////////////
class CEntity : public IEntity
{
public:
	// Entity constructor.
	// Should only be called from Entity System.
	CEntity( CEntitySystem *pEntitySystem,SEntitySpawnParams &params );
	
	// Entity destructor.
	// Should only be called from Entity System.
	virtual ~CEntity();

	IEntitySystem* GetEntitySystem() const { return g_pIEntitySystem; };

	// Called by entity system to complete initialization of the entity.
	bool Init( SEntitySpawnParams &params );
	// Called by EntitySystem every frame for each pre-physics active entity.
	void PrePhysicsUpdate( float fFrameTime );
	// Called by EntitySystem every frame for each active entity.
	void Update( SEntityUpdateContext &ctx );
	// Called by EntitySystem before entity is destroyed.
	void ShutDown();

	//////////////////////////////////////////////////////////////////////////
	// IEntity interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EntityId GetId() const { return m_nID; };
	virtual EntityGUID GetGuid() const { return m_guid; };

	//////////////////////////////////////////////////////////////////////////
	virtual IEntityClass* GetClass() const { return m_pClass; }
	virtual IEntityArchetype* GetArchetype() { return m_pArchetype; };

	//////////////////////////////////////////////////////////////////////////
	virtual void SetFlags( uint32 flags );
	virtual uint32 GetFlags() const { return m_flags; };
	virtual void AddFlags( uint32 flagsToAdd ) { SetFlags( m_flags|flagsToAdd); };
	virtual void ClearFlags( uint32 flagsToClear ) { SetFlags(m_flags&(~flagsToClear)); };
	virtual bool CheckFlags( uint32 flagsToCheck ) const { return (m_flags&flagsToCheck) == flagsToCheck; };

	virtual void UpdateUseOccluders( bool bUse );

	virtual bool IsGarbage() const { return m_bGarbage; };

	//////////////////////////////////////////////////////////////////////////
	virtual void SetName( const char *sName );
	virtual const char* GetName() const { return m_szName.c_str(); };
	virtual const char* GetEntityTextDescription() const;

	//////////////////////////////////////////////////////////////////////////
	virtual void AttachChild( IEntity *pChildEntity,int nAttachFlags=0 );
	virtual void DetachAll( int nDetachFlags=0  );
	virtual void DetachThis( int nDetachFlags=0,int nWhyFlags=0 );
	virtual int  GetChildCount() const;
	virtual IEntity* GetChild( int nIndex ) const;
	virtual void EnableInheritXForm( bool bEnable );
	virtual IEntity* GetParent() const { return (m_pBinds) ? m_pBinds->pParent : NULL; };
	virtual IEntity* GetAdam()  
	{ 
		IEntity *pParent,*pAdam=this; 
		while(pParent=pAdam->GetParent()) pAdam=pParent; 
		return pAdam;
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void SetWorldTM( const Matrix34 &tm,int nWhyFlags=0 );
	virtual void SetLocalTM( const Matrix34 &tm,int nWhyFlags=0 );

	// Set position rotation and scale at once.
	virtual void SetPosRotScale( const Vec3 &vPos,const Quat &qRotation,const Vec3 &vScale,int nWhyFlags=0 );

	virtual Matrix34 GetLocalTM() const;
	virtual const Matrix34& GetWorldTM() const;

	virtual void GetWorldBounds( AABB &bbox );
	virtual void GetLocalBounds( AABB &bbox );

	//////////////////////////////////////////////////////////////////////////
	virtual void SetPos( const Vec3 &vPos,int nWhyFlags=0 );
	virtual const Vec3& GetPos() const { return m_vPos; }

	virtual void SetRotation( const Quat &qRotation,int nWhyFlags=0 );
	virtual const Quat& GetRotation() const { return m_qRotation; };

	virtual void SetScale( const Vec3 &vScale,int nWhyFlags=0 );
	virtual const Vec3& GetScale() const { return m_vScale; };

	virtual Vec3 GetWorldPos() const { return GetWorldTM().GetTranslation(); };
	virtual Ang3 GetWorldAngles() const;
	virtual Quat GetWorldRotation() const;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	virtual void Activate( bool bActive );
	virtual bool IsActive() const { return m_bActive; };

	virtual void PrePhysicsActivate( bool bActive );
	virtual bool IsPrePhysicsActive();

	//////////////////////////////////////////////////////////////////////////
	virtual void Serialize( TSerialize ser, int nFlags );

	virtual bool SendEvent( SEntityEvent &event );
	//////////////////////////////////////////////////////////////////////////
	
	virtual void SetTimer( int nTimerId,int nMilliSeconds );
	virtual void KillTimer( int nTimerId );

	virtual void Hide( bool bHide );
  virtual bool IsHidden() const { return m_bHidden; }

	virtual void Invisible( bool bInvisible );
  virtual bool IsInvisible() const { return m_bInvisible; }

	//////////////////////////////////////////////////////////////////////////
	virtual CSmartObject* GetSmartObject() const { return m_pSmartObject; }
	virtual void SetSmartObject( CSmartObject* pSmartObject ) { m_pSmartObject = pSmartObject; }
	//////////////////////////////////////////////////////////////////////////
	virtual IAIObject* GetAI() { return m_pAIObject; };
	//////////////////////////////////////////////////////////////////////////
	virtual bool RegisterInAISystem(unsigned short type, const AIObjectParameters &params);
	//////////////////////////////////////////////////////////////////////////
	// reflect changes in position or orientation to the AI object
	virtual void UpdateAIObject( );
	//////////////////////////////////////////////////////////////////////////

	virtual void SetUpdatePolicy( EEntityUpdatePolicy eUpdatePolicy );
	virtual EEntityUpdatePolicy GetUpdatePolicy() const { return (EEntityUpdatePolicy)m_eUpdatePolicy; }

	//////////////////////////////////////////////////////////////////////////
	// Entity Proxies Interfaces access functions.
	//////////////////////////////////////////////////////////////////////////
	virtual IEntityProxy* GetProxy( EEntityProxy proxy ) const;
	virtual void SetProxy(EEntityProxy proxy, IEntityProxy *pProxy);
	virtual IEntityProxy* CreateProxy( EEntityProxy proxy );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Physics.
	//////////////////////////////////////////////////////////////////////////
	virtual void Physicalize( SEntityPhysicalizeParams &params );
	virtual void EnablePhysics( bool enable );
	virtual IPhysicalEntity* GetPhysics() const;

	virtual int PhysicalizeSlot(int slot, SEntityPhysicalizeParams &params);
	virtual void UnphysicalizeSlot(int slot);
	virtual void UpdateSlotPhysics(int slot);

	virtual void SetPhysicsState( XmlNodeRef & physicsState );

	//////////////////////////////////////////////////////////////////////////
	// Custom entity material.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetMaterial( IMaterial *pMaterial );
	virtual IMaterial* GetMaterial();

	//////////////////////////////////////////////////////////////////////////
	// Entity Slots interface.
	//////////////////////////////////////////////////////////////////////////
	virtual bool IsSlotValid( int nSlot ) const;
	virtual void FreeSlot( int nSlot );
	virtual int  GetSlotCount() const;
	virtual bool GetSlotInfo( int nSlot,SEntitySlotInfo &slotInfo ) const;
	virtual const Matrix34& GetSlotWorldTM( int nIndex ) const;
	virtual const Matrix34& GetSlotLocalTM( int nIndex, bool bRelativeToParent) const;
	virtual void SetSlotLocalTM( int nIndex,const Matrix34 &localTM,int nWhyFlags=0  );
	virtual bool SetParentSlot( int nParentSlot,int nChildSlot );
	virtual void SetSlotMaterial( int nSlot,IMaterial *pMaterial );
	virtual void SetSlotFlags( int nSlot,uint32 nFlags );
	virtual uint32 GetSlotFlags( int nSlot ) const;
	virtual bool ShouldUpdateCharacter( int nSlot) const;
	virtual ICharacterInstance* GetCharacter( int nSlot );
	virtual int SetCharacter( ICharacterInstance *pCharacter, int nSlot );
	virtual IStatObj* GetStatObj( int nSlot );
	virtual int SetStatObj( IStatObj *pStatObj, int nSlot, bool bUpdatePhysics, float mass=-1.0f );
	virtual IParticleEmitter* GetParticleEmitter( int nSlot );

	virtual int LoadGeometry( int nSlot,const char *sFilename,const char *sGeomName=NULL,int nLoadFlags=0 );
	virtual int LoadCharacter( int nSlot,const char *sFilename,int nLoadFlags=0 );
	virtual int SetParticleEmitter( int nSlot, IParticleEmitter* pEmitter, bool bSerialize = false );
	virtual int	LoadParticleEmitter( int nSlot, IParticleEffect *pEffect, SpawnParams const* params=NULL, bool bPrime = false, bool bSerialize = false );
	virtual int LoadLight( int nSlot,CDLight *pLight );
	virtual int LoadCloud( int nSlot,const char *sFilename );
	virtual int SetCloudMovementProperties(int nSlot, const SCloudMovementProperties& properties);
	virtual int LoadFogVolume( int nSlot, const SFogVolumeProperties& properties );
	virtual int FadeGlobalDensity( int nSlot, float fadeTime, float newGlobalDensity );
	virtual int LoadVolumeObject(int nSlot, const char* sFilename);
	virtual int SetVolumeObjectMovementProperties(int nSlot, const SVolumeObjectMovementProperties& properties);

	virtual void InvalidateTM( int nWhyFlags=0 );

	// Load/Save entity parameters in XML node.
	virtual void SerializeXML( XmlNodeRef &node,bool bLoading );

	virtual IEntityLink* GetEntityLinks();
	virtual IEntityLink* AddEntityLink( const char *sLinkName,EntityId entityId );
	virtual void RemoveEntityLink( IEntityLink* pLink );
	virtual void RemoveAllEntityLinks();
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	virtual IEntity *UnmapAttachedChild(int &partId);

	virtual void GetMemoryStatistics( ICrySizer *pSizer );

	//////////////////////////////////////////////////////////////////////////
	// Local methods.
	//////////////////////////////////////////////////////////////////////////

	// Returns true if entity was already fully initialized by this point.
	virtual bool IsInitialized() const { return m_bInitialized; }

  virtual bool IsTrainEntity() const {return m_bTrainEntity;}
  virtual void SetTrainEntity() {m_bTrainEntity=true;}

	// Get fast access to the slot, only used internally by entity components.
	class CEntityObject* GetSlot( int nSlot ) const;

	//////////////////////////////////////////////////////////////////////////
	// Return number of proxies in object.
	ILINE int ProxyCount() const
	{
		if (m_bUseProxyArray)
			return m_proxy.array.numProxies;
		else
			return 3;
	}
	// Return proxy object for proxy id.
	IEntityProxy* Proxy( int nProxy ) const
	{
		if (m_bUseProxyArray)
			return m_proxy.array.pProxyArray[nProxy];
		else
		{
			assert(nProxy>=ENTITY_PROXY_RENDER&&nProxy<=ENTITY_PROXY_SCRIPT);
			return m_proxy.simple.pProxyArray[nProxy];
		}
	}
	//////////////////////////////////////////////////////////////////////////
	// Version of Proxy function that can receive any proxy index, and will return 0 if this proxy not exist.
	ILINE IEntityProxy* GetProxyByType( EEntityProxy nProxyType ) const
	{
		switch (nProxyType) {
			case ENTITY_PROXY_RENDER: return GetRenderProxy();
			case ENTITY_PROXY_PHYSICS: return GetPhysicalProxy();
			case ENTITY_PROXY_SCRIPT: return GetScriptProxy();
		}
		for (int i = ENTITY_PROXY_SCRIPT+1; i < ProxyCount(); i++)
		{
			IEntityProxy *pProxy = GetProxyAt(i);
			if (pProxy && pProxy->GetType() == nProxyType)
				return pProxy;
		}
		return 0;
	}
	//////////////////////////////////////////////////////////////////////////
	// Proxy at specified index.
	//////////////////////////////////////////////////////////////////////////
	ILINE IEntityProxy* GetProxyAt( int nIndex ) const
	{
		if (!m_bUseProxyArray)
		{
			if (nIndex <= ENTITY_PROXY_SCRIPT)
				return m_proxy.simple.pProxyArray[nIndex];
			return 0;
		}
		else
		{
			if (nIndex >= m_proxy.array.numProxies)
				return 0;
			return m_proxy.array.pProxyArray[nIndex];
		}
	}
	//////////////////////////////////////////////////////////////////////////
	ILINE void SetProxyAt( int nIndex,IEntityProxy* pProxy )
	{
		if (!m_bUseProxyArray)
		{
			if (nIndex <= ENTITY_PROXY_SCRIPT)
				m_proxy.simple.pProxyArray[nIndex] = pProxy;
		}
		else
		{
			if (nIndex < m_proxy.array.numProxies)
				m_proxy.array.pProxyArray[nIndex] = pProxy;
		}
	}
	// Reallocate proxy array and add one slot.
	// Return new index.
	int AllocNewProxyIndex();
	// Get render proxy object.
	ILINE CRenderProxy* GetRenderProxy() const { return (CRenderProxy*)GetProxyAt(ENTITY_PROXY_RENDER) ; };
	// Get physics proxy object.
	ILINE CPhysicalProxy* GetPhysicalProxy() const { return (CPhysicalProxy*)GetProxyAt(ENTITY_PROXY_PHYSICS); };
	// Get script proxy object.
	ILINE CScriptProxy* GetScriptProxy() const { return (CScriptProxy*)GetProxyAt(ENTITY_PROXY_SCRIPT); };
	//////////////////////////////////////////////////////////////////////////

	// For internal use.
	CEntitySystem* GetCEntitySystem() const { return g_pIEntitySystem; };

	//////////////////////////////////////////////////////////////////////////
	// Activates entity only for specified number of frames.
	// numUpdates must be a small number from 0-15.
	void ActivateForNumUpdates( int numUpdates );
	void SetUpdateStatus();
	// Get status if entity need to be update every frame or not.
	bool GetUpdateStatus() const { return (m_bActive || m_nUpdateCounter) && (!m_bHidden || CheckFlags(ENTITY_FLAG_UPDATE_HIDDEN)); };

	//////////////////////////////////////////////////////////////////////////
	// Description:
	//   Invalidates entity and childs cached world transformation.
	void CalcLocalTM( Matrix34 &tm ) const;

	const Matrix34& GetWorldTM_Fast() const { return m_worldTM; };

	void	Hide(bool bHide, EEntityEvent eEvent1, EEntityEvent eEvent2);

	// for ProximityTriggerSystem
	SProximityElement * GetProximityElement() { return m_pProximityEntity; }

protected:

	//////////////////////////////////////////////////////////////////////////
	// Attachment.
	//////////////////////////////////////////////////////////////////////////
	void AllocBindings();
	void DeallocBindings();
	void OnRellocate( int nWhyFlags );
	void LogEvent( SEntityEvent &event,CTimeValue dt );
	//////////////////////////////////////////////////////////////////////////

private:
	//////////////////////////////////////////////////////////////////////////
	// VARIABLES.
	//////////////////////////////////////////////////////////////////////////
	friend class CEntitySystem;
	friend class CPhysicsEventListener; // For faster access to internals.
	friend class CEntityObject; // For faster access to internals.
	friend class CRenderProxy;  // For faster access to internals.
	friend class CTriggerProxy;
  friend class CPhysicalProxy;

	// Childs structure, only allocated when any child entity is attached.
	struct SBindings
	{
		std::vector<CEntity*> childs;
		CEntity *pParent;
	};

	//////////////////////////////////////////////////////////////////////////
	// Internal entity flags (must be first member var of Centity) (Reduce cache misses on access to entity data).
	//////////////////////////////////////////////////////////////////////////
	unsigned int m_bActive : 1;                 // Active Entities are updated every frame.
	unsigned int m_bInActiveList : 1;           // Added to entity system active list.
	mutable unsigned int m_bBoundsValid : 1;    // Set when the entity bounding box is valid.
	unsigned int m_bInitialized : 1;			      // Set if this entity already Initialized.
	unsigned int m_bHidden : 1;                 // Set if this entity is hidden.
	unsigned int m_bGarbage : 1;                // Set if this entity is garbage and will be removed soon.
	unsigned int m_bUseProxyArray : 1;          // Set if proxy array is allocated (see UProxies).
	unsigned int m_bHaveEventListeners : 1;     // Set if entity have an event listeners associated in entity system.
	unsigned int m_bTrigger : 1;                // Set if entity is proximity trigger itself.
	unsigned int m_bWasRellocated : 1;          // Set if entity was rellocated at least once.
	unsigned int m_nUpdateCounter : 4;          // Update counter is used to activate the entity for the limited number of frames.
	                                            // Usually used for Physical Triggers.
	                                            // When update counter is set, and entity is activated, it will automatically
	                                            // deactivate after this counter reaches zero
	unsigned int m_eUpdatePolicy : 4;           // Update policy defines in which cases to call
	                                            // entity update function every frame.
	unsigned int m_bInvisible: 1;               // Set if this entity is invisible.
	unsigned int m_bNotInheritXform : 1;        // Inherit or not transformation from parent.

	// Unique ID of the entity.
	EntityId m_nID;

	// Optional entity guid.
	EntityGUID m_guid;

	// Entity flags. any combination of EEntityFlags values.
	uint32 m_flags;

	// Name of the entity. 
	string m_szName;

	// Pointer to the class that describe this entity.
	IEntityClass *m_pClass;

	// Pointer to the entity archetype.
	IEntityArchetype* m_pArchetype;

	// Position of the entity in local space.
	Vec3 m_vPos;
	// Rotation of the entity in local space.
	Quat m_qRotation;
	// Scale of the entity in local space.
	Vec3 m_vScale;
	// World transformation matrix of this entity.
	mutable Matrix34 m_worldTM;

	// Pointer to hierarchical binding structure.
	// It contains array of child entities and pointer to the parent entity.
	// Because entity attachments are not used very often most entities do not need it,
	// and space is preserved by keeping it separate structure.
	SBindings *m_pBinds;

	// The representation of this entity in the AI system.
	IAIObject	*m_pAIObject;
	CSmartObject *m_pSmartObject;

	// Custom entity material.
	_smart_ptr<IMaterial> m_pMaterial;

	//////////////////////////////////////////////////////////////////////////
	// m_proxy.simple or m_proxy.array member of this strucre are used 
	// depending on the m_bUseProxyArray flag.
	//////////////////////////////////////////////////////////////////////////
	union UProxies
	{
		// Simple structure is used when only Render and Physics proxies are needed.
		struct SProxySimple {
			IEntityProxy* pProxyArray[3]; // Array of 3 proxies, Only Render/Physics/Script proxy can go here.
		} simple;
		// Proxy array is used when more then just Render and Physics proxies are needed.
		// Proxy array is more expensive as it requires additional allocation of the array of pointers.
		struct SProxyArray {
			IEntityProxy** pProxyArray; // Array of all supported entity proxies.
			int            numProxies;  // Number of proxies in m_pProxyArray
		} array;
	};
	UProxies m_proxy;
	//////////////////////////////////////////////////////////////////////////

	// Entity Links.
	IEntityLink* m_pEntityLinks;

	// For tracking entity in the partition grid.
	SGridLocation *m_pGridLocation;
	// For tracking entity inside proximity trigger system.
	SProximityElement *m_pProximityEntity;

  //  big hack to identify train entity
  bool m_bTrainEntity;
};

#endif // __Entity_h__
