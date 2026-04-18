////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   RenderProxy.h
//  Version:     v1.00
//  Created:     19/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __RenderProxy_h__
#define __RenderProxy_h__
#pragma once

#include "EntitySystem.h"
#include <IRenderer.h>
#include <IEntityRenderState.h>
#include "stdafx.h"
#include "VSCompStructures.h"

// forward declarations.
class CEntityObject;
class CEntity;
struct SRendParams;
struct IShaderPublicParams;
class CEntityTimeoutList;
class CRenderProxy;
struct AnimEventInstance;

//////////////////////////////////////////////////////////////////////////
// Description:
//    CRenderProxy provides default implementation of IEntityRenderProxy interface.
//    This is a renderable object that is registered in 3D engine sector.
//    It can contain multiple sub object slots that can have their own relative transformation, and 
//    each slot can represent specific renderable interface (IStatObj,ICharacterInstance,etc..)
//    Slots can also form hierarchical transformation tree, every slot can reference parent slot for transformation.
///////////////////////////////////////////////////////////// /////////////
class CRenderProxy : public IEntityRenderProxy,public IRenderNode
{
public:

	enum EFlags // Must be limited to 16 flags.
	{
		FLAG_BBOX_VALID_LOCAL  = BIT(1),
		FLAG_BBOX_FORCED       = BIT(2),
		FLAG_BBOX_INVALID      = BIT(3),
		FLAG_HIDDEN            = BIT(4), // If render proxy is hidden.
		//FLAG_HAS_ENV_LIGHTING  = 0x0020, // If render proxy have environment lighting.
		FLAG_UPDATE            = BIT(5), // If render proxy needs to be updated.
		FLAG_NOW_VISIBLE       = BIT(6), // If render proxy currently visible.
		FLAG_REGISTERED_IN_3DENGINE = BIT(7), // If render proxy have been registered in 3d engine.
		FLAG_POST_INIT         = BIT(8), // If render proxy have received Post init event.
		FLAG_HAS_LIGHTS        = BIT(9), // If render proxy has some lights.
		FLAG_GEOMETRY_MODIFIED = BIT(10), // Geometry for this render slot was modified at runtime.
		FLAG_PREUPDATE_ANIM    = BIT(11), // call update for animations before physics
		FLAG_HAS_CHILDRENDERNODES = BIT(12), // If render proxy contains child render nodes
		FLAG_HAS_PARTICLES     = BIT(13), // If render proxy contains particle emitters
	};

	CRenderProxy( CEntity *pEntity );
	~CRenderProxy();

	void GetMemoryStatistics( ICrySizer *pSizer );

	// Must be called somewhen after constructor.
	void PostInit();

	//////////////////////////////////////////////////////////////////////////
	// IEntityProxy implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetType() { return ENTITY_PROXY_RENDER; }
	virtual void Release();
	virtual void Done() {};
	// Called when the subsystem initialize.
	virtual void Init( IEntity *pEntity,SEntitySpawnParams &params ) {};  
	virtual	void Update( SEntityUpdateContext &ctx );
	virtual	void ProcessEvent( SEntityEvent &event );
	virtual void SerializeXML( XmlNodeRef &entityNode,bool bLoading );
	virtual void Serialize( TSerialize ser );
	virtual bool NeedSerialize();
	//////////////////////////////////////////////////////////////////////////

  virtual void SetViewDistRatio(int nViewDistRatio);
  virtual void SetMinSpec(int nMinSpec);
  virtual void SetLodRatio(int nLodRatio);

	//////////////////////////////////////////////////////////////////////////
	// IEntityRenderProxy interface.
	//////////////////////////////////////////////////////////////////////////
	// Description:
	//    Force local bounds.
	// Arguments:
	//    bounds - Bounding box in local space.
	//    bDoNotRecalculate - when set to true entity will never try to recalculate local bounding box set by this call.
	virtual void SetLocalBounds( const AABB &bounds,bool bDoNotRecalculate );
	virtual void GetWorldBounds( AABB &bounds );
	virtual void GetLocalBounds( AABB &bounds );
	virtual void InvalidateLocalBounds();

	//////////////////////////////////////////////////////////////////////////
	virtual IMaterial* GetRenderMaterial( int nSlot=-1 );
  // Set shader parameters for this instance
  //virtual void SetShaderParam(const char *pszName, UParamVal &pParam, int nType = 0) ;

	virtual struct IRenderNode* GetRenderNode() { return this; };
	//virtual IShaderPublicParams* GetShaderPublicParams( bool bCreate=true );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityRenderProxy implementation.
	//////////////////////////////////////////////////////////////////////////
	bool IsSlotValid( int nIndex ) const { return nIndex >=0 && nIndex < (int)m_slots.size() && m_slots[nIndex] != NULL; };
	int  GetSlotCount() const { return m_slots.size(); }
	bool SetParentSlot( int nParentIndex,int nChildIndex );
	bool GetSlotInfo( int nIndex,SEntitySlotInfo &slotInfo ) const;
	void SetSlotMaterial( int nSlot,IMaterial *pMaterial );
	IMaterial* GetSlotMaterial( int nSlot );

	void SetSlotFlags( int nSlot,uint32 nFlags );
	uint32 GetSlotFlags( int nSlot );

	ICharacterInstance* GetCharacter( int nSlot );
	IStatObj* GetStatObj( int nSlot );
	IParticleEmitter* GetParticleEmitter( int nSlot );

	int SetSlotGeometry( int nSlot,IStatObj *pStatObj );
	int SetSlotCharacter( int nSlot, ICharacterInstance* pCharacter );
	int LoadGeometry( int nSlot,const char *sFilename,const char *sGeomName=NULL,int nLoadFlags=0 );
	int LoadCharacter( int nSlot,const char *sFilename,int nLoadFlags=0 );
	int	LoadParticleEmitter( int nSlot, IParticleEffect *pEffect, SpawnParams const* params=NULL, bool bPrime = false, bool bSerialize = false );
	int	SetParticleEmitter( int nSlot, IParticleEmitter* pEmitter, bool bSerialize = false );
	int LoadLight( int nSlot,CDLight *pLight );
	int LoadCloud( int nSlot,const char *sFilename );
	int SetCloudMovementProperties(int nSlot, const SCloudMovementProperties& properties);
	int LoadFogVolume( int nSlot, const SFogVolumeProperties& properties );
	int FadeGlobalDensity( int nSlot, float fadeTime, float newGlobalDensity );
	int LoadVolumeObject(int nSlot, const char* sFilename);
	int SetVolumeObjectMovementProperties(int nSlot, const SVolumeObjectMovementProperties& properties);

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Slots.
	//////////////////////////////////////////////////////////////////////////
	ILINE CEntityObject* GetSlot( int nIndex ) const { return (nIndex >=0 && nIndex < (int)m_slots.size()) ? m_slots[nIndex] : NULL; }
	CEntityObject* GetParentSlot( int nIndex ) const;

	// Allocate a new object slot.
	CEntityObject* AllocSlot( int nIndex );
	// Frees existing object slot, also optimizes size of slot array.
	void FreeSlot( int nIndex );
	void FreeAllSlots();
	void ExpandCompoundSlot0();	// if there's a compound cgf in slot 0, expand it into subobjects (slots)
	
	// Returns world transformation matrix of the object slot.
	const Matrix34& GetSlotWorldTM( int nIndex ) const;
	// Returns local relative to host entity transformation matrix of the object slot.
	const Matrix34& GetSlotLocalTM( int nIndex, bool bRelativeToParent) const;
	// Set local transformation matrix of the object slot.
	void SetSlotLocalTM( int nIndex,const Matrix34 &localTM,int nWhyFlags=0 );

	// Invalidates local or world space bounding box.
	void InvalidateBounds( bool bLocal,bool bWorld );

	// Register render proxy in 3d engine.
	void RegisterForRendering( bool bEnable );

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// IRenderNode interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetMaterial( IMaterial *pMat );
	virtual IMaterial* GetMaterial(Vec3 * pHitPos = NULL);
	virtual void SetVertexScattering(const SScatteringObjectData& ScatteringObjectData);
	virtual struct IRenderMesh * GetRenderMesh(int nLod);
	virtual int GetEditorObjectId() const;
	virtual const char * GetEntityClassName() const;
	virtual Vec3 GetPos(bool bWorldOnly = true) const;
	virtual const Ang3& GetAngles(int realA=0) const;
	virtual float GetScale() const;
	virtual const char *GetName() const;
	virtual void	GetRenderBBox( Vec3 &mins,Vec3 &maxs );
	virtual void	GetBBox( Vec3 &mins,Vec3 &maxs );
	virtual bool HasChanged() { return false; }
	virtual bool Render(const struct SRendParams & EntDrawParams);
	virtual struct IStatObj *GetEntityStatObj( unsigned int nPartId, unsigned int nSubPartId, Matrix34 * pMatrix = NULL, bool bReturnOnlyVisible = false);
	virtual void SetEntityStatObj( unsigned int nSlot, IStatObj * pStatObj, const Matrix34 * pMatrix = NULL ) {};
	virtual struct ICharacterInstance* GetEntityCharacter( unsigned int nSlot, Matrix34 * pMatrix = NULL, bool bReturnOnlyVisible = false );
	virtual bool IsRenderProxyVisAreaVisible() const;
	virtual struct IPhysicalEntity* GetPhysics() const { return 0; };
	virtual void SetPhysics( IPhysicalEntity* pPhys ) {};
	virtual void PreloadInstanceResources(Vec3 vPrevPortalPos, float fPrevPortalDistance, float fTime) {};
  
	virtual float GetMaxViewDist();

  virtual void SetMaterialLayersMask( uint8 nMtlLayers );
  virtual uint8 GetMaterialLayersMask() const 
  { 
    return m_nMaterialLayers; 
  }

  virtual void SetMaterialLayersBlend( uint32 nMtlLayersBlend )
  {
    m_nMaterialLayersBlend = nMtlLayersBlend; 
  }

  virtual uint32 GetMaterialLayersBlend( ) const
  {
    return m_nMaterialLayersBlend;
  }

  //! Allows to adjust default motion blur amount setting
  virtual void SetMotionBlurAmount(float fAmount) 
  { 
    m_nMotionBlurAmount =  (uint8) min(255.0f, max(0.0f,  fAmount * 128.0f) ); 
  }

  //! return motion blur amount setting
  virtual float GetMotionBlurAmount() const { return ((float) m_nMotionBlurAmount / 128.0f); }

  //! Allows to adjust vision params:
  // . binocular view uses color and alpha for blending
  virtual void SetVisionParams( float r, float g, float b, float a ) 
  { 
    m_nVisionParams =  (uint32) (int_round( r * 255.0f) << 24)| (int_round( g * 255.0f) << 16)| (int_round( b * 255.0f) << 8) | (int_round( a * 255.0f));
  }

  //! return vision params
  virtual uint32 GetVisionParams() const { return m_nVisionParams; }

  virtual void SetOpacity(float fAmount) 
  {     
    m_nOpacity = int_round(clamp_tpl<float>(fAmount, 0.0f, 1.0f) * 255);
  }

  virtual float GetOpacity() const {  return (float) m_nOpacity / 255.0f; }

  //! Set/Get wind amount
  virtual void SetWindBending( Vec2 &pWindBending ) { m_vWindBending = pWindBending; }
  virtual Vec2 GetWindBending() { return m_vWindBending; }

  virtual const AABB GetBBox() const { return m_WSBBox; }
  virtual void SetBBox( const AABB& WSBBox ) { m_WSBBox = WSBBox; }

	virtual float	GetLastSeenTime() const { return m_fLastSeenTime; }

	virtual void UpdateCharactersBeforePhysics( bool update ) { if (update) AddFlags(FLAG_PREUPDATE_ANIM); else ClearFlags(FLAG_PREUPDATE_ANIM); }
	virtual bool IsCharactersUpdatedBeforePhysics() { return CheckFlags(FLAG_PREUPDATE_ANIM); }
	//////////////////////////////////////////////////////////////////////////
	
	// Internal slot access function.
	ILINE CEntityObject* Slot( int nIndex ) const { assert(nIndex >=0 && nIndex < (int)m_slots.size()); return m_slots[nIndex]; }
	IStatObj *GetCompoundObj() const;

	//////////////////////////////////////////////////////////////////////////
	// Flags
	//////////////////////////////////////////////////////////////////////////
	ILINE void   SetFlags( uint32 flags ) { m_nFlags = flags; };
	ILINE uint32 GetFlags() { return m_nFlags; };
	ILINE void   AddFlags( uint32 flagsToAdd ) { SetFlags( m_nFlags | flagsToAdd ); };
	ILINE void   ClearFlags( uint32 flagsToClear ) { SetFlags( m_nFlags & ~flagsToClear ); };
	ILINE bool   CheckFlags( uint32 flagsToCheck ) const { return (m_nFlags&flagsToCheck) == flagsToCheck; };
	//////////////////////////////////////////////////////////////////////////

	// Change custom material.
	void SetCustomMaterial( IMaterial *pMaterial );
	// Get assigned custom material.
	IMaterial* GetCustomMaterial() { return m_pMaterial; }

	void	UpdateEntityFlags();
	void  UpdateUseOccluders( bool bUse );
	void	SetLastSeenTime(float fNewTime) { m_fLastSeenTime=fNewTime; }
	
	//////////////////////////////////////////////////////////////////////////
	// Custom new/delete.
	//////////////////////////////////////////////////////////////////////////
	void *operator new( size_t nSize );
	void operator delete( void *ptr );

	static void SetTimeoutList(CEntityTimeoutList* pList)	{s_pTimeoutList = pList;}

private:
	int GetSlotId( CEntityObject *pSlot ) const;
	void CalcLocalBounds();
	void OnEntityXForm( int nWhyFlags );
	void OnHide( bool bHide );
  void OnReset( );

	// Get existing slot or make a new slot if not exist.
	// Is nSlot is negative will allocate a new available slot and return it Index in nSlot parameter.
	CEntityObject* GetOrMakeSlot( int &nSlot );

	I3DEngine* GetI3DEngine() { return gEnv->p3DEngine; }
	bool CheckCharacterForMorphs(ICharacterInstance* pCharacter);
	static int AnimEventCallback(ICharacterInstance *pCharacter, void *userdata);
	void AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event);
  void UpdateMaterialLayersRendParams( SRendParams &pRenderParams );
  bool IsMovableByGame() const;

	void UnRegisterFrom3DEngine();
	void RegisterIn3DEngine();
private:
	friend class CEntityObject;

	static float gsWaterLevel;	//static cached water level, updated each ctor call
	//////////////////////////////////////////////////////////////////////////
	// Variables.
	//////////////////////////////////////////////////////////////////////////
	// Host entity.
	CEntity *m_pEntity;

	// Object Slots.
	typedef std::vector<CEntityObject*> Slots;
	Slots m_slots;

	// Local bounding box of render proxy.
	AABB m_localBBox;

	//! Override material.
	_smart_ptr<IMaterial> m_pMaterial;

	struct IRenderMesh* m_arrScatterVertexBuffer[MAX_VERTEX_SCATTER_LODS_NUM];
	PodArray<struct IRenderMesh*> m_SubObjectScatterVertexBuffer;
	Vec4 m_VertexScatterTransformZ;
	bool m_bHasVertexScatterBuffer;

  // per instance shader public params
	//_smart_ptr<IShaderPublicParams>				m_pShaderPublicParams;
  
	// Wind info 
	Vec2 m_vWindBending;

	// Time passed since this entity was seen last time
	float		m_fLastSeenTime;

	static CEntityTimeoutList* s_pTimeoutList;
	EntityId m_entityId;

	uint32 m_nEntityFlags;

	// Render proxy flags.
	uint32 m_nFlags : 16;

	// Motion blur amount
	uint8 m_nMotionBlurAmount;
  uint8 m_nOpacity;

  // Vision modes stuff
  uint32 m_nVisionParams;
  // Material layers blending amount
  uint32 m_nMaterialLayersBlend;

  AABB m_WSBBox;
};

extern stl::PoolAllocatorNoMT<sizeof(CRenderProxy)> *g_Alloc_RenderProxy;

//////////////////////////////////////////////////////////////////////////
inline void* CRenderProxy::operator new( size_t nSize )
{
	void *ptr = g_Alloc_RenderProxy->Allocate();
	return ptr;
}

//////////////////////////////////////////////////////////////////////////
inline void CRenderProxy::operator delete( void *ptr )
{
	if (ptr)
		g_Alloc_RenderProxy->Deallocate(ptr);
}

#endif // __RenderProxy_h__

