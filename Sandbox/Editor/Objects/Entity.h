////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   Entity.h
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: StaticObject object definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __Entity_h__
#define __Entity_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "BaseObject.h"
#include "EntityScript.h"
#include "TrackGizmo.h"

#include "IMovieSystem.h"
#include "EntityPrototype.h"

class CFlowGraph;
class CEntity;

/*!
 *	CEntityEventTarget is an Entity event target and type.
 */
struct CEntityEventTarget
{
	CBaseObject* target; //! Target object.
	_smart_ptr<CGizmo> pLineGizmo;
	CString event;
	CString sourceEvent;
};

//////////////////////////////////////////////////////////////////////////
// Named link from entity to entity.
//////////////////////////////////////////////////////////////////////////
struct CEntityLink
{
	GUID targetId;   // Target entity id.
	CEntity* target; // Target entity.
	CString name;    // Name of the link.
	_smart_ptr<CGizmo> pLineGizmo;
};

inline const EntityGUID& ToEntityGuid( REFGUID guid )
{
	static EntityGUID eguid;
	eguid = *((EntityGUID*)&guid);
	return eguid;
}

/*!
 *	CEntity is an static object on terrain.
 *
 */
class CRYEDIT_API CEntity : public CBaseObject,public IAnimNodeOwner
{
public:
	DECLARE_DYNCREATE(CEntity)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	//! Return type name of Entity.
	CString GetTypeDescription() const { return GetEntityClass(); };
	
	//////////////////////////////////////////////////////////////////////////
	bool IsSameClass( CBaseObject *obj );

	virtual bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	virtual void InitVariables();
	virtual void Done();
	virtual bool CreateGameObject();
	virtual void Display( DisplayContext &disp );

	virtual int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	void SetName( const CString &name );

	void SetSelected( bool bSelect );

	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams( IEditor *ie );
	void BeginEditMultiSelParams( bool bAllOfSameType );
	void EndEditMultiSelParams();

	virtual void GetLocalBounds( BBox &box );

	virtual bool HitTest( HitContext &hc );
	bool HitTestRect( HitContext &hc );
	void UpdateVisibility( bool visible );
	bool ConvertFromObject( CBaseObject *object );

	virtual void Serialize( CObjectArchive &ar );
	virtual void PostLoad( CObjectArchive &ar );

	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode );

	void SetLookAt( CBaseObject *target );

	//////////////////////////////////////////////////////////////////////////
	void OnEvent( ObjectEvent event );
	
	//////////////////////////////////////////////////////////////////////////
	virtual IAnimNode* GetAnimNode() const { return m_pAnimNode; };
	void SetAnimNode( IAnimNode *pAnimNode );
	virtual void EnableAnimNode( bool bEnable );

	virtual IPhysicalEntity* GetCollisionEntity() const;

	virtual void SetMaterial( CMaterial *mtl );
	virtual CMaterial* GetRenderMaterial() const;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetMinSpec( uint32 nSpec );
	virtual void SetMaterialLayersMask( uint32 nLayersMask );

	//! Attach new child node.
	virtual void AttachChild( CBaseObject* child,bool bKeepPos=true );
	//! Detach all childs of this node.
	virtual void DetachAll( bool bKeepPos=true );
	// Detach this node from parent.
	virtual void DetachThis( bool bKeepPos=true );

	virtual void SetHelperScale( float scale );
	virtual float GetHelperScale() { return m_helperScale; };

	virtual void GatherUsedResources( CUsedResources &resources );
	virtual bool IsSimilarObject( CBaseObject *pObject );

	//////////////////////////////////////////////////////////////////////////
	// END CBaseObject
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// CEntity interface.
	//////////////////////////////////////////////////////////////////////////
	virtual bool SetClass( const CString &entityClass,bool bForceReload=false,bool bGetScriptProperties=true,XmlNodeRef xmlProperties=XmlNodeRef(),XmlNodeRef xmlProperties2=XmlNodeRef() );
	virtual bool SetClass( CEntityScript* pClass,bool bForceReload=false,bool bGetScriptProperties=true,XmlNodeRef xmlProperties=XmlNodeRef(),XmlNodeRef xmlProperties2=XmlNodeRef() );
	virtual void SpawnEntity();
	virtual void DeleteEntity();
	virtual void UnloadScript();
	
	CString GetEntityClass() const { return m_entityClass; };
	int GetEntityId() const { return m_entityId; };

	//! Return entity prototype class if present.
	virtual CEntityPrototype* GetPrototype() const { return m_prototype; };

	//! Get EntityScript object associated with this entity.
	CEntityScript* GetScript() { return m_pClass; }
	//! Reload entity script.
	void Reload( bool bReloadScript=false );

	//////////////////////////////////////////////////////////////////////////
	//! Return number of event targets of Script.
	int		GetEventTargetCount() const { return m_eventTargets.size(); };
	CEntityEventTarget& GetEventTarget( int index ) { return m_eventTargets[index]; };
	//! Add new event target, returns index of created event target.
	//! Event targets are Always entities.
	int AddEventTarget( CBaseObject *target,const CString &event,const CString &sourceEvent,bool bUpdateScript=true );
	//! Remove existing event target by index.
	void RemoveEventTarget( int index,bool bUpdateScript=true );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Entity Links.
	//////////////////////////////////////////////////////////////////////////
	//! Return number of event targets of Script.
	int		GetEntityLinkCount() const { return m_links.size(); };
	CEntityLink& GetEntityLink( int index ) { return m_links[index]; };
	int AddEntityLink( const CString &name,GUID targetEntityId );
	void RenameEntityLink( int index,const CString &newName );
	void RemoveEntityLink( int index );
	void RemoveAllEntityLinks();
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	//! Get Game entity interface.
	IEntity*	GetIEntity() { return m_entity; };

//	bool IsCastShadow() const { return mv_castShadows; }
	//bool IsSelfShadowing() const { return mv_selfShadowing; }
  bool IsCastShadow() const { return mv_castShadow; }
	bool IsCastScatter() const { return mv_castScatter; }
  //bool IsRecvShadow() const { return mv_recvShadow; }
	//bool IsCastLightmap() const { return mv_castLightmap; }
	//bool IsRecvLightmap() const { return mv_recvLightmap; }
	bool IsUseOccluders() const { return mv_useOccluders; }

	float GetRatioLod() const { return mv_ratioLOD; };
	float GetRatioViewDist() const { return mv_ratioViewDist; }

	CVarBlock* GetProperties() const { return m_properties; };
	CVarBlock* GetProperties2() const { return m_properties2; };
//	unsigned MemStats();

	void Validate( CErrorReport *report );
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	//! Takes position/orientation from the game entity and apply on editor entity.
	void AcceptPhysicsState();
	void ResetPhysicsState();

	// Open Flow Graph associated with this entity in the view window.
	void SetFlowGraph( CFlowGraph *pGraph );
	void OpenFlowGraph( const CString &groupName );
	void RemoveFlowGraph();
	CFlowGraph* GetFlowGraph() const { return m_pFlowGraph; }

	// Find CEntity from game entityId.
	static CEntity* FindFromEntityId( EntityId id );

	// Retreive smart object class for this entity if exist.
	CString GetSmartObjectClass() const;

protected:
	bool HitTestEntity( HitContext &hc,bool &bHavePhysics );

	//////////////////////////////////////////////////////////////////////////
	//! Must be called after cloning the object on clone of object.
	//! This will make sure object references are cloned correctly.
	virtual void PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx );

	//////////////////////////////////////////////////////////////////////////
	// overrided from IAnimNodeCallback
	virtual void OnNodeAnimated(  IAnimNode *pAnimNode );
	//////////////////////////////////////////////////////////////////////////

	//! Draw default object items.
	virtual void DrawDefault( DisplayContext &dc,COLORREF labelColor=RGB(255,255,255) );

	void OnLoadFailed();

	CVarBlock* CloneProperties( CVarBlock *srcProperties );
	void UpdatePropertyPanel();
	void UpdateTrackGizmo();

	//////////////////////////////////////////////////////////////////////////
	//! Callback called when one of entity properties have been modified.
	void OnPropertyChange( IVariable *var );

	//////////////////////////////////////////////////////////////////////////
	void OnEventTargetEvent( CBaseObject *target,int event );
	void ResolveEventTarget( CBaseObject *object,unsigned int index );
	void ReleaseEventTargets();
	void UpdateMaterialInfo();

	// Called when link info changes.
	virtual void UpdateIEntityLinks( bool bCallOnPropertyChange=true );

	//! Dtor must be protected.
	CEntity();
	void DeleteThis() { delete this; };

	//overrided from CBaseObject.
	void InvalidateTM( int nWhyFlags );

	//! Draw target lines.
	void DrawTargets( DisplayContext &dc );

	//! Recalculate bounding box of entity.
	void CalcBBox();

	//! Force IEntity to the local position/angles/scale.
	void XFormGameEntity();

	//! Sets correct binding for IEntity.
	virtual void BindToParent();
	virtual void BindIEntityChilds();
	virtual void UnbindIEntity();

	void DrawAIInfo( DisplayContext &dc,struct IAIObject *aiObj );

	//////////////////////////////////////////////////////////////////////////
	// Callbacks.
	//////////////////////////////////////////////////////////////////////////
	void OnRenderFlagsChange( IVariable *var );
	void OnEntityFlagsChange( IVariable *var );

	//////////////////////////////////////////////////////////////////////////
	// Radius callbacks.
	//////////////////////////////////////////////////////////////////////////
	void OnRadiusChange( IVariable *var );
	void OnInnerRadiusChange( IVariable *var );
	void OnOuterRadiusChange( IVariable *var );
	//////////////////////////////////////////////////////////////////////////

	void OnUseOccludersChange( IVariable *var );

	void FreeGameData();

	void CheckSpecConfig();

protected:
	unsigned int m_bLoadFailed : 1;
	unsigned int m_bEntityXfromValid : 1;
	unsigned int m_bCalcPhysics : 1;
	unsigned int m_bDisplayBBox : 1;
	unsigned int m_bDisplaySolidBBox : 1;
	unsigned int m_bDisplayAbsoluteRadius : 1;
	unsigned int m_bDisplayArrow : 1;
	unsigned int m_bIconOnTop : 1;
	unsigned int m_bVisible : 1;

	//! Entity class.
	CString m_entityClass;
	//! Id of spawned entity.
	int m_entityId;

//	IEntityClass *m_pEntityClass;
	IEntity* m_entity;
	IStatObj* m_visualObject;
	BBox m_box;

	// Entity class description.
  _smart_ptr<CEntityScript> m_pClass;

	//! True if this is static entity.
	//bool m_staticEntity;

	//////////////////////////////////////////////////////////////////////////
	// Main entity parameters.
	//////////////////////////////////////////////////////////////////////////
	//CVariable<bool> mv_selfShadowing;
	CVariable<bool>	mv_outdoor;
	CVariable<bool> mv_castShadow;
	CVariable<bool> mv_castScatter;
	//CVariable<bool> mv_recvShadow;
  CVariable<float> mv_motionBlurAmount;

	//CVariable<bool> mv_recvShadowMapsASMR;
	//CVariable<bool> mv_castLightmap;
	//CVariable<bool> mv_recvLightmap;
	CVariable<int> mv_ratioLOD;
	CVariable<int> mv_ratioViewDist;
	CVariable<bool> mv_hiddenInGame; // Entity is hidden in game (on start).
  CVariable<bool> mv_recvWind;
	CVariable<bool> mv_useOccluders;
	
	//////////////////////////////////////////////////////////////////////////
	// Temp variables (Not serializable) just to display radiuses from properties.
	//////////////////////////////////////////////////////////////////////////
	// Used for proximity entities.
	float m_proximityRadius;
	float m_innerRadius;
	float m_outerRadius;
	//////////////////////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////////////////////
	// Event Targets.
	//////////////////////////////////////////////////////////////////////////
	//! Array of event targets of this Entity.
	typedef std::vector<CEntityEventTarget> EventTargets;
	EventTargets m_eventTargets;

	//////////////////////////////////////////////////////////////////////////
	// Links
	typedef std::vector<CEntityLink> Links;
	Links m_links;

	//! Animation node, assigned to this object.
	IAnimNode* m_pAnimNode;

	//! Entity prototype. only used by EntityPrototypeObject.
	TSmartPtr<CEntityPrototype> m_prototype;

	//! Per instance properties table.
	CVarBlockPtr m_properties2;

	//! Entity Properties variables.
	CVarBlockPtr m_properties;

	// Can keep reference to one track gizmo.
	CTrackGizmoPtr m_trackGizmo;
	
	// Physics state, as a string.
	//CString m_physicsState;
	XmlNodeRef m_physicsState;

	//////////////////////////////////////////////////////////////////////////
	// Associated FlowGraph.
	CFlowGraph* m_pFlowGraph;

	static int m_rollupId;
	static class CEntityPanel* m_panel;
	static float m_helperScale;
};

/*!
 * Class Description of Entity
 */
class CEntityClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {C80F8AEA-90EF-471f-82C7-D14FA80B9203}
		static const GUID guid = { 0xc80f8aea, 0x90ef, 0x471f, { 0x82, 0xc7, 0xd1, 0x4f, 0xa8, 0xb, 0x92, 0x3 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_ENTITY; };
	const char* ClassName() { return "StdEntity"; };
	const char* Category() { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CEntity); };
	const char* GetFileSpec() { return "*EntityClass"; };
	int GameCreationOrder() { return 200; };
};

//////////////////////////////////////////////////////////////////////////
// Simple entity.
//////////////////////////////////////////////////////////////////////////
class CSimpleEntity : public CEntity
{
public:
	DECLARE_DYNCREATE(CSimpleEntity)

	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	bool ConvertFromObject( CBaseObject *object );
	
	void BeginEditParams( IEditor *ie,int flags );
	void EndEditParams( IEditor *ie );
	
	void Validate( CErrorReport *report );
	bool IsSimilarObject( CBaseObject *pObject );

	CString GetGeometryFile() const;

private:
	void SetGeometryFile( const CString &filename );
	void OnFileChange( CString filename );
};

/*!
 * Class Description of Entity
 */
class CSimpleEntityClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {F7820713-E4EE-44b9-867C-C0F9543B4871}
		static const GUID guid = { 0xf7820713, 0xe4ee, 0x44b9, { 0x86, 0x7c, 0xc0, 0xf9, 0x54, 0x3b, 0x48, 0x71 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_ENTITY; };
	const char* ClassName() { return "SimpleEntity"; };
	const char* Category() { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CSimpleEntity); };
	const char* GetFileSpec() { return "Objects\\*.cgf;*.chr;*.cga;*.cdf"; };
	int GameCreationOrder() { return 202; };
};

//////////////////////////////////////////////////////////////////////////
// Geometry entity.
//////////////////////////////////////////////////////////////////////////
class CGeomEntity : public CEntity
{
public:
	DECLARE_DYNCREATE(CGeomEntity)

	//////////////////////////////////////////////////////////////////////////
	// CEntity
	//////////////////////////////////////////////////////////////////////////
	virtual bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	virtual void InitVariables();
	virtual void SpawnEntity();
	virtual bool ConvertFromObject( CBaseObject *object );

	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams( IEditor *ie );
	virtual XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode );

	virtual void Validate( CErrorReport *report );
	virtual bool IsSimilarObject( CBaseObject *pObject );
	virtual void OnEvent( ObjectEvent event );
	//////////////////////////////////////////////////////////////////////////

	CString GetGeometryFile() const;

protected:
	void SetGeometryFile( const CString &filename );
	void OnFileChange( CString filename );
	void OnGeometryFileChange( IVariable *pVar );

	CVariable<CString> mv_geometry;
};

/*!
* Class Description of Entity
*/
class CGeomEntityClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {A23DD1AD-7D93-4850-8EE8-BC37CCE0FAC7}
		static const GUID guid = { 0xa23dd1ad, 0x7d93, 0x4850, { 0x8e, 0xe8, 0xbc, 0x37, 0xcc, 0xe0, 0xfa, 0xc7 } };
		return guid;
	}
	virtual ObjectType GetObjectType() { return OBJTYPE_ENTITY; };
	virtual const char* ClassName() { return "GeomEntity"; };
	virtual const char* Category() { return "Geom Entity"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CGeomEntity); };
	virtual const char* GetFileSpec() { return "Objects\\*.cgf;*.chr;*.cga;*.cdf"; };
	virtual int GameCreationOrder() { return 201; };
};



#endif // __CEntity_h__