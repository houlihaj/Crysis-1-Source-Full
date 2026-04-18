////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   StatObj.cpp
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: StaticObject implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Entity.h"
#include "EntityPanel.h"
#include "EntityLinksPanel.h"
#include "PanelTreeBrowser.h"
#include "PropertiesPanel.h"
#include "Viewport.h"
#include "Settings.h"
#include "LineGizmo.h"
#include "Settings.h"
#include "Group.h"

#include <I3DEngine.h>
#include <IAgent.h>
#include <IMovieSystem.h>
#include <IEntitySystem.h>
#include <ICryAnimation.h>
#include <IEntityRenderState.h>

#include "EntityPrototype.h"
#include "Material\MaterialManager.h"

#include "HyperGraph\FlowGraphManager.h"
#include "HyperGraph\FlowGraph.h"

#include "BrushObject.h"
#include "GameEngine.h"

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CEntity,CBaseObject)
IMPLEMENT_DYNCREATE(CSimpleEntity,CEntity)
IMPLEMENT_DYNCREATE(CGeomEntity,CEntity)

int CEntity::m_rollupId = 0;
CEntityPanel* CEntity::m_panel = 0;
float CEntity::m_helperScale = 1;

namespace
{
	int s_entityEventsPanelIndex = 0;
	CEntityEventsPanel* s_entityEventsPanel = 0;

	int s_entityLinksPanelIndex = 0;
	CEntityLinksPanel* s_entityLinksPanel = 0;

	int s_propertiesPanelIndex = 0;
	CPropertiesPanel* s_propertiesPanel = 0;

	int s_propertiesPanelIndex2 = 0;
	CPropertiesPanel* s_propertiesPanel2 = 0;

	// Prevent OnPropertyChange to be executed when loading many properties at one time.
	static bool s_ignorePropertiesUpdate = false;

	CPanelTreeBrowser* s_treePanel = NULL;
	int s_treePanelId = 0;

	std::map<EntityId,CEntity*> s_entityIdMap;

	struct AutoReleaseAll
	{
		AutoReleaseAll() {};
		~AutoReleaseAll()
		{
			delete s_propertiesPanel;
			delete s_propertiesPanel2;
			delete s_treePanel;
		}
	} s_autoReleaseAll;
};

//////////////////////////////////////////////////////////////////////////
CEntity::CEntity()
{
	m_entity = 0;
	m_bLoadFailed = false;

	m_pClass = 0;

	m_visualObject = 0;

	m_box.min.Set(0,0,0);
	m_box.max.Set(0,0,0);

	m_proximityRadius = 0;
	m_innerRadius = 0;
	m_outerRadius = 0;

	m_pAnimNode = 0;
	m_bDisplayBBox = true;
	m_bDisplaySolidBBox = false;
	m_bDisplayAbsoluteRadius = false;
	m_bIconOnTop = false;
	m_bDisplayArrow = false;
	m_entityId = 0;
	m_bVisible = true;
	m_bCalcPhysics = true;
	//m_staticEntity = false;
	m_pFlowGraph = 0;

	m_bEntityXfromValid = false;

	UseMaterialLayersMask();

	SetColor( RGB(255,255,0) );
	
	// Init Variables.
	//mv_selfShadowing = false;
//	mv_recvShadowMapsASMR = false;
	mv_castShadow = true;
	mv_castScatter = false;
	//mv_recvShadow = true;
	mv_outdoor			=	false;
  mv_recvWind = false;
	mv_useOccluders = false;
  
  mv_motionBlurAmount = 1.0f;  
  mv_motionBlurAmount.SetLimits( 0, 255.0f );

	//mv_castLightmap = false;
	//mv_recvLightmap = false;
	mv_hiddenInGame = false;
	mv_ratioLOD = 100;
	mv_ratioViewDist = 100;
	mv_ratioLOD.SetLimits( 0,255 );
	mv_ratioViewDist.SetLimits( 0,255 );

	m_physicsState = 0;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::InitVariables()
{
	//AddVariable( mv_selfShadowing,"SelfShadowing",functor(*this,&CEntity::OnRenderFlagsChange) );
	AddVariable( mv_outdoor,"OutdoorOnly",_T("Outdoor Only"),functor(*this,&CEntity::OnEntityFlagsChange) );
	AddVariable( mv_castShadow,"CastShadow",_T("Cast Shadow"),functor(*this,&CEntity::OnEntityFlagsChange) );  
	AddVariable( mv_castScatter, "CastScatterMaps", _T("CastScatterMaps"), functor(*this, &CEntity::OnRenderFlagsChange) );
	//AddVariable( mv_recvShadow,"RecvShadow",_T("Recieve Shadow"),functor(*this,&CEntity::OnEntityFlagsChange) );
  AddVariable( mv_motionBlurAmount,"MotionBlurMultiplier",_T("Motion blur multiplier"),functor(*this,&CEntity::OnRenderFlagsChange) );
  
	//AddVariable( mv_recvShadowMapsASMR,"RecvShadowMapsASMR",functor(*this,&CEntity::OnRenderFlagsChange) );
	//AddVariable( mv_castLightmap,"PreCalcShadows",_T("CastLightmap"),functor(*this,&CEntity::OnRenderFlagsChange) );
	//AddVariable( mv_recvLightmap,"ReceiveLightmap",functor(*this,&CEntity::OnRenderFlagsChange) );
	AddVariable( mv_ratioLOD,"LodRatio",functor(*this,&CEntity::OnRenderFlagsChange) );
	AddVariable( mv_ratioViewDist,"ViewDistRatio",functor(*this,&CEntity::OnRenderFlagsChange) );
	AddVariable( mv_hiddenInGame,"HiddenInGame" );
  AddVariable( mv_recvWind,"RecvWind", _T("Receive Wind"),functor(*this,&CEntity::OnEntityFlagsChange) );     
	AddVariable( mv_useOccluders,"UseOccluders", _T("Use Occluders"),functor(*this,&CEntity::OnUseOccludersChange) );     
}

//////////////////////////////////////////////////////////////////////////&
void CEntity::Done()
{
	if (m_pFlowGraph)
	{
		CFlowGraphManager* pFGMGR = GetIEditor()->GetFlowGraphManager();
		if (pFGMGR) 
			pFGMGR->UnregisterAndResetView(m_pFlowGraph);
	}
	SAFE_RELEASE(m_pFlowGraph);

	DeleteEntity();
	UnloadScript();

	if (m_trackGizmo)
	{
		RemoveGizmo( m_trackGizmo );
		m_trackGizmo = 0;
	}
	if (m_pAnimNode)
		EnableAnimNode(false);
	ReleaseEventTargets();
	RemoveAllEntityLinks();

	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::FreeGameData()
{
	DeleteEntity();
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	CBaseObject::Init( ie,prev,file );

	if (prev)
	{
		CEntity *pe = (CEntity*)prev;
	
		// Clone Properties.
		if (pe->m_properties)
		{
			m_properties = CloneProperties(pe->m_properties);
		}
		if (pe->m_properties2)
		{
			m_properties2 = CloneProperties(pe->m_properties2);
		}
		// When cloning entity, do not get properties from script.
		SetClass( pe->GetEntityClass(),false,false );
		SpawnEntity();

		if (pe->m_pFlowGraph)
		{
			SetFlowGraph( (CFlowGraph*)pe->m_pFlowGraph->Clone() );
		}

		UpdatePropertyPanel();
	}
	else if (!file.IsEmpty())
	{
		SetUniqName( file );
		m_entityClass = file;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
CEntity* CEntity::FindFromEntityId( EntityId id )
{
	CEntity *pEntity = stl::find_in_map( s_entityIdMap,id,0 );
	return pEntity;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetAnimNode( IAnimNode *pAnimNode )
{
	if (pAnimNode)
	{
		m_pAnimNode = pAnimNode;
		m_pAnimNode->SetName( GetName() );
		m_pAnimNode->SetNodeOwner(this);
		m_pAnimNode->SetEntityGuid( ToEntityGuid(GetId()) );
		if (IsSelected())
			m_pAnimNode->SetFlags( m_pAnimNode->GetFlags()|ANODE_FLAG_SELECTED );
	}
	else
	{
		m_pAnimNode->SetNodeOwner(0);
		m_pAnimNode = 0;
	}
	UpdateTrackGizmo();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::EnableAnimNode( bool bEnable )
{
	if (bEnable)
	{
		int nodeId = GetId().Data1;
		m_pAnimNode = GetIEditor()->GetMovieSystem()->CreateNode( ANODE_ENTITY,nodeId );
		SetAnimNode( m_pAnimNode );
	}
	else if (m_pAnimNode)
	{
		IAnimNode *pAnimNode = m_pAnimNode;
		SetAnimNode( 0 );
		GetIEditor()->GetMovieSystem()->RemoveNode( pAnimNode );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::IsSameClass( CBaseObject *obj )
{
	if (GetClassDesc() == obj->GetClassDesc())
	{
		CEntity *ent = (CEntity*)obj;
		return GetScript() == ent->GetScript();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::ConvertFromObject( CBaseObject *object )
{
	CBaseObject::ConvertFromObject(object);

	if (object->IsKindOf(RUNTIME_CLASS(CEntity)))
	{
		CEntity *pObject = (CEntity*)object;

		mv_outdoor = pObject->mv_outdoor;
		mv_castShadow = pObject->mv_castShadow;
		mv_castScatter = pObject->mv_castScatter;
		mv_motionBlurAmount = pObject->mv_motionBlurAmount;
		mv_ratioLOD = pObject->mv_ratioLOD;
		mv_ratioViewDist = pObject->mv_ratioViewDist;
		mv_hiddenInGame = pObject->mv_hiddenInGame;
		mv_recvWind = pObject->mv_recvWind;
		mv_useOccluders = pObject->mv_useOccluders;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetLookAt( CBaseObject *target )
{
	CBaseObject::SetLookAt(target);
}

//////////////////////////////////////////////////////////////////////////
IPhysicalEntity* CEntity::GetCollisionEntity() const
{
	// Returns physical object of entity.
	if (m_entity)
		return m_entity->GetPhysics();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GetLocalBounds( BBox &box )
{
	box = m_box;
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::HitTestEntity( HitContext &hc,bool &bHavePhysics )
{
	bHavePhysics = true;
	IPhysicalWorld *pPhysWorld = GetIEditor()->GetSystem()->GetIPhysicalWorld();
	// Test 3D viewport.
	IPhysicalEntity *physic = 0;

	ICharacterInstance *pCharacter = m_entity->GetCharacter(0);
	if (pCharacter)
	{	
		physic = pCharacter->GetISkeletonPose()->GetCharacterPhysics();
		if (physic)
		{
			int type = physic->GetType();
			if (type == PE_NONE || type == PE_PARTICLE || type == PE_ROPE || type == PE_SOFT)
				physic = 0;
			else if (physic->GetStatus( &pe_status_nparts() ) == 0)
				physic = 0;
		}
		if (physic)
		{
			ray_hit hit;
			int col = pPhysWorld->RayTraceEntity( physic,hc.raySrc,hc.rayDir*10000.0f,&hit );
			if (col <= 0)
				return false;
			hc.dist = hit.dist;
			return true;
		}
	}

	physic = m_entity->GetPhysics();
	if (physic)
	{
		int type = physic->GetType();
		if (type == PE_NONE || type == PE_PARTICLE || type == PE_ROPE || type == PE_SOFT)
			physic = 0;
		else if (physic->GetStatus( &pe_status_nparts() ) == 0)
			physic = 0;
	}
	// Now if box intersected try real geometry ray test.
	if (physic)
	{
		ray_hit hit;
		int col = pPhysWorld->RayTraceEntity( physic,hc.raySrc,hc.rayDir*10000.0f,&hit );
		if (col <= 0)
			return false;
		hc.dist = hit.dist;
		return true;
	}
	else
	{
		bHavePhysics = false;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::HitTest( HitContext &hc )
{
	if (!hc.b2DViewport)
	{
		// Test 3D viewport.
		if (m_entity)
		{
			bool bHavePhysics = false;
			if (HitTestEntity( hc,bHavePhysics ))
				return true;
			if (bHavePhysics)
			{
				return false;
			}
		}
		if (m_visualObject && (!gSettings.viewports.bShowIcons || !HaveTextureIcon()))
		{
			Matrix34 tm = GetWorldTM();
			float sz = m_helperScale * gSettings.gizmo.helpersScale;
			tm.ScaleColumn( Vec3(sz,sz,sz) );
			primitives::ray aray; aray.origin = hc.raySrc; aray.dir = hc.rayDir*10000.0f;

			IGeomManager *pGeomMgr = GetIEditor()->GetSystem()->GetIPhysicalWorld()->GetGeomManager();
			IGeometry *pRay = pGeomMgr->CreatePrimitive(primitives::ray::type, &aray);
			geom_world_data gwd;
			gwd.offset = tm.GetTranslation();
			gwd.scale = tm.GetColumn0().GetLength();
			gwd.R = Matrix33(tm);
			geom_contact *pcontacts = 0;
			int col = (m_visualObject->GetPhysGeom() && m_visualObject->GetPhysGeom()->pGeom) ? m_visualObject->GetPhysGeom()->pGeom->Intersect(pRay, &gwd,0, 0, pcontacts) : 0;
			pGeomMgr->DestroyGeometry(pRay);
			if (col > 0)
			{
				if (pcontacts)
					hc.dist = pcontacts[col-1].t;
				return true;
			}
		}
	}
	/*
	else if (m_entity)
	{
	float dist = FLT_MAX;
	bool bHaveHit = false;
	CEntityObject entobj;
	IPhysicalWorld *physWorld = GetIEditor()->GetSystem()->GetIPhysicalWorld();
	int numobj = m_entity->GetNumObjects();
	if (numobj == 0)
	{
	hc.weakHit = true;
	return true;
	}
	vector origin = hc.raySrc;
	vector dir = hc.rayDir*10000.0f;
	Matrix tm;
	GetMatrix( tm );
	tm.Transpose();
	for (int i = 0; i < numobj; i++)
	{
	m_entity->GetEntityObject(i,entobj);
	if (entobj.object && entobj.object->GetPhysGeom())
	{
	ray_hit hit;
	int col = physWorld->RayTraceGeometry( entobj.object->GetPhysGeom(),(float*)tm.m_values,origin,dir,&hit );
	if (col > 0)
	{
	dist = __min(dist,hit.dist);
	bHaveHit = true;
	}
	}
	}
	if (bHaveHit)
	{
	hc.dist = dist;
	return true;
	}
	else
	{
	return false;
	}
	}
	*/


	//////////////////////////////////////////////////////////////////////////
	if ((m_bDisplayBBox && gSettings.viewports.bShowTriggerBounds) || hc.b2DViewport)
	{
		float hitEpsilon = hc.view->GetScreenScaleFactor( GetWorldPos() ) * 0.01f;
		float hitDist;

		float fScale = GetScale().x;
		BBox boxScaled;
		boxScaled.min = m_box.min*fScale;
		boxScaled.max = m_box.max*fScale;

		Matrix34 invertWTM = GetWorldTM();
		invertWTM.Invert();
		//Vec3 xformedRaySrc = invertWTM*hc.raySrc;
		//Vec3 xformedRayDir = GetNormalized( invertWTM*hc.rayDir );


		Vec3 xformedRaySrc = invertWTM.TransformPoint(hc.raySrc);
		Vec3 xformedRayDir = invertWTM.TransformVector(hc.rayDir);
		xformedRayDir.Normalize();

		{
			Vec3 intPnt;
			// Check intersection with bbox edges.
			if (boxScaled.RayEdgeIntersection( xformedRaySrc,xformedRayDir,hitEpsilon,hitDist,intPnt ))
			{
				hc.dist = xformedRaySrc.GetDistance(intPnt);
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::HitTestRect( HitContext &hc )
{
	if (m_visualObject && !gSettings.viewports.bShowIcons)
	{
		AABB box;
		box.SetTransformedAABB( GetWorldTM(),m_visualObject->GetAABB() );
		return HitTestRectBounds( hc,box );
	}
	else
		return __super::HitTestRect( hc );
}

//////////////////////////////////////////////////////////////////////////
int CEntity::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown)
	{
		Vec3 pos;
		// Rise Entity above ground on Bounding box ammount.
		if (GetIEditor()->GetAxisConstrains() != AXIS_TERRAIN)
		{
			pos = view->MapViewToCP(point);
		}
		else
		{
			// Snap to terrain.
			bool hitTerrain;
			pos = view->ViewToWorld( point,&hitTerrain );
			if (hitTerrain)
			{
				pos.z = GetIEditor()->GetTerrainElevation(pos.x,pos.y);
				pos.z = pos.z - m_box.min.z;
			}
			pos = view->SnapToGrid(pos);
		}
		SetPos( pos );

		if (event == eMouseLDown)
			return MOUSECREATE_OK;
		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback( view,event,point,flags );
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::CreateGameObject()
{
	if (!m_pClass)
	{
		if (!m_entityClass.IsEmpty())
			SetClass( m_entityClass,true );
	}
	if (!m_entity)
	{
		if (!m_entityClass.IsEmpty())
			SpawnEntity();
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::SetClass( const CString &entityClass,bool bForceReload,bool bGetScriptProperties,XmlNodeRef xmlProperties,XmlNodeRef xmlProperties2 )
{
	if (entityClass == m_entityClass && m_pClass != 0 && !bForceReload)
		return true;
	if (entityClass.IsEmpty())
		return false;

	m_entityClass = entityClass;
	m_bLoadFailed = false;

	UnloadScript();

	if (!IsCreateGameObjects())
		return false;

	CEntityScript* pClass = CEntityScriptRegistry::Instance()->Find( m_entityClass );
	if (!pClass)
	{
		OnLoadFailed();
		return false;
	}
	SetClass( pClass,bForceReload,bGetScriptProperties,xmlProperties,xmlProperties2 );
	
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::SetClass( CEntityScript* pClass,bool bForceReload,bool bGetScriptProperties,XmlNodeRef xmlProperties,XmlNodeRef xmlProperties2 )
{
	assert(pClass);

	if (pClass == m_pClass && !bForceReload)
		return true;
	m_pClass = pClass;

	m_bLoadFailed = false;
	// Load script if its not loaded yet.
	if (!m_pClass->IsValid())
	{
		if (!m_pClass->Load())
		{
			OnLoadFailed();
			return false;
		}
	}

	int nTexIcon = pClass->GetTextureIcon();
	if (nTexIcon)
	{
		SetTextureIcon( pClass->GetTextureIcon() );
	}

	//////////////////////////////////////////////////////////////////////////
	// Turns of Cast Shadow variable for Light entities.
	if (stricmp(pClass->GetName(),"Light") == 0)
	{
    mv_castShadow = false;
    mv_castScatter = false;
    RemoveVariable( &mv_castShadow );
    RemoveVariable( &mv_castScatter );
    RemoveVariable( &mv_motionBlurAmount );
    RemoveVariable( &mv_ratioLOD );
    RemoveVariable( &mv_recvWind );
		RemoveVariable( &mv_useOccluders );
	}

	//////////////////////////////////////////////////////////////////////////
	// Make visual editor object for this entity.
	//////////////////////////////////////////////////////////////////////////
	if (!m_pClass->GetVisualObject().IsEmpty())
	{
		//m_entity->LoadObject( 0,m_pClass->GetVisualObject(),1 );
		//m_entity->DrawObject( 0,ETY_DRAW_NORMAL );
		m_visualObject = GetIEditor()->Get3DEngine()->LoadStatObj( m_pClass->GetVisualObject() );
		if (m_visualObject)
			m_visualObject->AddRef();
	}

	bool bUpdateUI = false;
	// Create Entity properties from Script properties..
	if (bGetScriptProperties && m_prototype == NULL && m_pClass->GetProperties() != NULL)
	{
		bUpdateUI = true;
		CVarBlockPtr oldProperties = m_properties;
		m_properties = CloneProperties( m_pClass->GetProperties() );

		if (xmlProperties)
		{
			s_ignorePropertiesUpdate = true;
			m_properties->Serialize( xmlProperties,true );
			s_ignorePropertiesUpdate = false;
		}
		else if (oldProperties)
		{
			// If we had propertied before copy thier values to new script.
			s_ignorePropertiesUpdate = true;
			m_properties->CopyValuesByName(oldProperties);
			s_ignorePropertiesUpdate = false;
		}
	}

	// Create Entity properties from Script properties..
	if (bGetScriptProperties && m_pClass->GetProperties2() != NULL)
	{
		bUpdateUI = true;
		CVarBlockPtr oldProperties = m_properties2;
		m_properties2 = CloneProperties( m_pClass->GetProperties2() );

		if (xmlProperties2)
		{
			s_ignorePropertiesUpdate = true;
			m_properties2->Serialize( xmlProperties2,true );
			s_ignorePropertiesUpdate = false;
		}
		else if (oldProperties)
		{
			// If we had propertied before copy thier values to new script.
			s_ignorePropertiesUpdate = true;
			m_properties2->CopyValuesByName(oldProperties);
			s_ignorePropertiesUpdate = false;
		}
	}

	{
		// Populate empty events from the script.
		for (int i = 0,num = m_pClass->GetEmptyLinkCount(); i < num; i++)
		{
			const CString &linkName = m_pClass->GetEmptyLink(i);
			int j = 0;
			int numlinks = (int)m_links.size();
			for (j = 0; j < numlinks; j++)
			{
				if (strcmp(m_links[j].name,linkName) == 0)
					break;
			}
			if (j >= numlinks)
				AddEntityLink(linkName,GUID_NULL);
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SpawnEntity()
{
	if (!m_pClass)
		return;

	// Do not spawn second time.
	if (m_entity)
		return;

	m_bLoadFailed = false;

	IEntitySystem *pEntitySystem = GetIEditor()->GetSystem()->GetIEntitySystem();

	if (m_entityId != 0)
	{
		if (pEntitySystem->IsIDUsed( m_entityId ))
			m_entityId = 0;
	}

	SEntitySpawnParams params;
	params.pClass = m_pClass->GetClass();
	params.nFlags = 0;
	params.sName = (const char*)GetName();
	params.vPosition = GetPos();
	params.qRotation = GetRotation();
	params.id = m_entityId;

	if (m_prototype)
		params.pArchetype = m_prototype->GetIEntityArchetype();

	if (mv_castShadow)
		params.nFlags |= ENTITY_FLAG_CASTSHADOW;
	if (mv_castScatter)
		params.nFlags |= ENTITY_FLAG_CASTSCATTER;
	//if (mv_recvShadow)
		//params.nFlags |= ENTITY_FLAG_RECVSHADOW;
	if (mv_outdoor)
		params.nFlags |= ENTITY_FLAG_OUTDOORONLY;
  if (mv_recvWind)
    params.nFlags |= ENTITY_FLAG_RECVWIND;
	if (mv_useOccluders)
		params.nFlags |= ENTITY_FLAG_GOOD_OCCLUDER;
  
  //if (mv_motionBlurAmount != 1.0f)
  ////  params.nFlags |= ENTITY_FLAG_USEMOTIONBLUR;
	
	if(params.id==0)
		params.bStaticEntityId=true; // Tells to Entity system to generate new static id.

	params.guid = ToEntityGuid(GetId());
	
	// Spawn Entity but not initialize it.
	m_entity = pEntitySystem->SpawnEntity( params,false );
	if (m_entity)
	{
		m_entityId = m_entity->GetId();

		s_entityIdMap[m_entityId] = this;

		// Bind to parent.
		BindToParent();
		BindIEntityChilds();

		//m_pClass->SetCurrentProperties( this );
		m_pClass->SetEventsTable( this );

		UpdateIEntityLinks(false);

		// Mark this entity non destroyable.
		m_entity->AddFlags(ENTITY_FLAG_UNREMOVABLE);
		
		// Force transformation on entity.
		XFormGameEntity();

		if (m_properties != NULL && m_prototype == NULL)
		{
			m_pClass->SetProperties( m_entity,m_properties,false );
		}
		if (m_properties2 != NULL)
		{
			m_pClass->SetProperties2( m_entity,m_properties2,false );
		}

		//////////////////////////////////////////////////////////////////////////
		// Now initialize entity.
		//////////////////////////////////////////////////////////////////////////
		if (!pEntitySystem->InitEntity( m_entity,params ))
		{
			m_entity = 0;
			OnLoadFailed();
			return;
		}

		m_entity->Hide( !m_bVisible );

		//////////////////////////////////////////////////////////////////////////
		// If have material, assign it to the entity.
		if (GetMaterial())
		{
			UpdateMaterialInfo();
		}

		// Update render flags of entity (Must be after InitEntity).
		OnRenderFlagsChange(0);
		OnUseOccludersChange(0);

		if (!m_physicsState)
		{
			m_entity->SetPhysicsState( m_physicsState );
		}

		//////////////////////////////////////////////////////////////////////////
		// Check if needs to display bbox for this entity.
		//////////////////////////////////////////////////////////////////////////
		m_bCalcPhysics = true;
		if (m_entity->GetPhysics() != 0)
		{
			m_bDisplayBBox = false;
			if (m_entity->GetPhysics()->GetType() == PE_SOFT)
			{
				m_bCalcPhysics = false;
				//! Ignore entity being updated from physics.
				m_entity->SetFlags(ENTITY_FLAG_IGNORE_PHYSICS_UPDATE);
			}
		}
		else
		{
			if (m_pClass->GetFlags() & ENTITY_SCRIPT_SHOWBOUNDS)
			{
				m_bDisplayBBox = true;
				m_bDisplaySolidBBox = true;
			}
			else
			{
				m_bDisplayBBox = false;
				m_bDisplaySolidBBox = false;
			}

			m_bDisplayAbsoluteRadius = (m_pClass->GetFlags() & ENTITY_SCRIPT_ABSOLUTERADIUS) ? true : false;
		}

		m_bIconOnTop = (m_pClass->GetFlags() & ENTITY_SCRIPT_ICONONTOP) ? true : false;
		if (m_bIconOnTop)
			SetFlags(OBJFLAG_SHOW_ICONONTOP);
		else
			ClearFlags(OBJFLAG_SHOW_ICONONTOP);

		m_bDisplayArrow = (m_pClass->GetFlags() & ENTITY_SCRIPT_DISPLAY_ARROW) ? true : false;

		//////////////////////////////////////////////////////////////////////////
		// Calculate entity bounding box.
		CalcBBox();
		
		//////////////////////////////////////////////////////////////////////////
		// Assign entity pointer to animation node.
		if (m_pAnimNode)
			m_pAnimNode->SetEntityGuid( ToEntityGuid(GetId()) );

		if (m_pFlowGraph)
		{
			// Re-apply entity for flow graph.
			m_pFlowGraph->SetEntity(this, true);

			IEntityFlowGraphProxy *pFlowGraphProxy = (IEntityFlowGraphProxy*)m_entity->CreateProxy(ENTITY_PROXY_FLOWGRAPH);
			IFlowGraph* pGameFlowGraph = m_pFlowGraph->GetIFlowGraph();
			pFlowGraphProxy->SetFlowGraph( pGameFlowGraph );
			if (pGameFlowGraph)
				pGameFlowGraph->SetActive(true);

		}
		if(m_physicsState)
			m_entity->SetPhysicsState( m_physicsState );
	}
	else
	{
		OnLoadFailed();
	}

	//?
	UpdatePropertyPanel();
	CheckSpecConfig( );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::DeleteEntity()
{
	if (m_entity)
	{
		UnbindIEntity();
		m_entity->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
		IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
		pEntitySystem->RemoveEntity( m_entity->GetId(),true );
		s_entityIdMap.erase( m_entityId );
	}
	m_entity = 0;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UnloadScript()
{
	if (m_entity)
	{
		DeleteEntity();
	}
	if (m_visualObject)
		m_visualObject->Release();
	m_visualObject = 0;
	m_pClass = 0;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::XFormGameEntity()
{
	// Make sure components are correctly calculated.
	const Matrix34 &tm = GetWorldTM();
	if (m_entity)
	{
		int nWhyEntityFlag = ENTITY_XFORM_EDITOR;
		if (GetParent() && !m_entity->GetParent())
		{
			m_entity->SetWorldTM( tm,nWhyEntityFlag );
		}
		else if (GetLookAt())
		{
			m_entity->SetWorldTM( tm,nWhyEntityFlag );
		}
		else
		{
			m_entity->SetPosRotScale( GetPos(),GetRotation(),GetScale(),nWhyEntityFlag );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::CalcBBox()
{
	if (m_entity)
	{
		// Get Local bounding box of entity.
		m_entity->GetLocalBounds( m_box );
		
		if (m_visualObject)
		{
			Vec3 minp = m_visualObject->GetBoxMin()*m_helperScale*gSettings.gizmo.helpersScale;
			Vec3 maxp = m_visualObject->GetBoxMax()*m_helperScale*gSettings.gizmo.helpersScale;
			m_box.Add( minp );
			m_box.Add( maxp );
		}
		float minSize = 0.0001f;
		if (fabs(m_box.max.x-m_box.min.x)+fabs(m_box.max.y-m_box.min.y)+fabs(m_box.max.z-m_box.min.z) < minSize)
		{
			m_box.min = -Vec3( minSize,minSize,minSize );
			m_box.max = Vec3( minSize,minSize,minSize );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetName( const CString &name )
{
	if (name == GetName())
		return;

	CBaseObject::SetName( name );
	if (m_entity)
		m_entity->SetName( (const char*)GetName() );
	if (m_pAnimNode)
	{
		m_pAnimNode->SetName( name );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::BeginEditParams( IEditor *ie,int flags )
{
	CBaseObject::BeginEditParams( ie,flags );

	if (m_properties2 != NULL)
	{
		if (!s_propertiesPanel2)
			s_propertiesPanel2 = new CPropertiesPanel( AfxGetMainWnd() );
		else
			s_propertiesPanel2->DeleteVars();
		s_propertiesPanel2->AddVars( m_properties2 );
		if (!s_propertiesPanelIndex2)
			s_propertiesPanelIndex2 = AddUIPage( CString(GetTypeName()) + " Properties2",s_propertiesPanel2 );
	}

	if (!m_prototype)
	{
		if (m_properties != NULL)
		{
			if (!s_propertiesPanel)
				s_propertiesPanel = new CPropertiesPanel( AfxGetMainWnd() );
			else
				s_propertiesPanel->DeleteVars();
			s_propertiesPanel->AddVars( m_properties );
			if (!s_propertiesPanelIndex)
				s_propertiesPanelIndex = AddUIPage( CString(GetTypeName()) + " Properties",s_propertiesPanel );
		}
	}

	if (!m_panel && m_entity)
	{
		m_panel = new CEntityPanel(AfxGetMainWnd());
		m_panel->Create( CEntityPanel::IDD,AfxGetMainWnd() );
		m_rollupId = AddUIPage( CString("Entity: ")+m_entityClass,m_panel );
	}
	if (m_panel && m_panel->m_hWnd)
		m_panel->SetEntity(this);

	//////////////////////////////////////////////////////////////////////////
	// Links Panel
	if (!s_entityLinksPanel && m_entity)
	{
		s_entityLinksPanel = new CEntityLinksPanel(AfxGetMainWnd());
		s_entityLinksPanel->Create( CEntityLinksPanel::IDD,AfxGetMainWnd() );
		s_entityLinksPanelIndex = AddUIPage( "Entity Links",s_entityLinksPanel,true,-1,false );
	}
	if (s_entityLinksPanel)
		s_entityLinksPanel->SetEntity( this );

	//////////////////////////////////////////////////////////////////////////
	// Events panel
	if (!s_entityEventsPanel && m_entity)
	{
		s_entityEventsPanel = new CEntityEventsPanel(AfxGetMainWnd());
		s_entityEventsPanel->Create( CEntityEventsPanel::IDD,AfxGetMainWnd() );
		s_entityEventsPanelIndex = AddUIPage( "Entity Events",s_entityEventsPanel,true,-1,false );
		GetIEditor()->ExpandRollUpPage( s_entityEventsPanelIndex,FALSE );
	}
	if (s_entityEventsPanel)
		s_entityEventsPanel->SetEntity( this );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::EndEditParams( IEditor *ie )
{
	if (s_entityEventsPanelIndex != 0)
	{
		RemoveUIPage( s_entityEventsPanelIndex );
	}
	s_entityEventsPanelIndex = 0;
	s_entityEventsPanel = 0;

	if (s_entityLinksPanelIndex != 0)
	{
		RemoveUIPage( s_entityLinksPanelIndex );
	}
	s_entityLinksPanelIndex = 0;
	s_entityLinksPanel = 0;

	if (m_rollupId != 0)
		RemoveUIPage( m_rollupId );
	m_rollupId = 0;
	m_panel = 0;

	if (s_propertiesPanelIndex != 0)
		RemoveUIPage( s_propertiesPanelIndex );
	s_propertiesPanelIndex = 0;
	s_propertiesPanel = 0;

	if (s_propertiesPanelIndex2 != 0)
		RemoveUIPage( s_propertiesPanelIndex2 );
	s_propertiesPanelIndex2 = 0;
	s_propertiesPanel2 = 0;

	CBaseObject::EndEditParams( GetIEditor() );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UpdatePropertyPanel()
{
	// If user interface opened reload properties.
	if (s_propertiesPanel && m_properties != 0)
	{
		s_propertiesPanel->DeleteVars();
		s_propertiesPanel->AddVars( m_properties );
	}

	// If user interface opened reload properties.
	if (s_propertiesPanel2 && m_properties2 != 0)
	{
		s_propertiesPanel2->DeleteVars();
		s_propertiesPanel2->AddVars( m_properties2 );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::BeginEditMultiSelParams( bool bAllOfSameType )
{
	CBaseObject::BeginEditMultiSelParams(bAllOfSameType);

	if (!bAllOfSameType)
		return;

	if (m_properties2 != NULL)
	{
		if (!s_propertiesPanel2)
			s_propertiesPanel2 = new CPropertiesPanel( AfxGetMainWnd() );
		else
			s_propertiesPanel2->DeleteVars();

		// Add all selected objects.
		CSelectionGroup *grp = GetIEditor()->GetSelection();
		for (int i = 0; i < grp->GetCount(); i++)
		{
			CEntity *ent = (CEntity*)grp->GetObject(i);
			if (ent->m_properties2)
				s_propertiesPanel2->AddVars( ent->m_properties2 );
		}
		if (!s_propertiesPanelIndex2)
			s_propertiesPanelIndex2 = AddUIPage( CString(GetTypeName()) + " Properties",s_propertiesPanel2 );
	}

	if (m_properties != NULL && m_prototype == NULL)
	{
		if (!s_propertiesPanel)
			s_propertiesPanel = new CPropertiesPanel( AfxGetMainWnd() );
		else
			s_propertiesPanel->DeleteVars();

		// Add all selected objects.
		CSelectionGroup *grp = GetIEditor()->GetSelection();
		for (int i = 0; i < grp->GetCount(); i++)
		{
			CEntity *ent = (CEntity*)grp->GetObject(i);
			CVarBlock *vb = ent->m_properties;
			if (vb)
				s_propertiesPanel->AddVars( vb );
		}
		if (!s_propertiesPanelIndex)
			s_propertiesPanelIndex = AddUIPage( CString(GetTypeName()) + " Properties",s_propertiesPanel );
	}
}
	
//////////////////////////////////////////////////////////////////////////
void CEntity::EndEditMultiSelParams()
{
	if (s_propertiesPanelIndex != 0)
		RemoveUIPage( s_propertiesPanelIndex );
	s_propertiesPanelIndex = 0;
	s_propertiesPanel = 0;

	if (s_propertiesPanelIndex2 != 0)
		RemoveUIPage( s_propertiesPanelIndex2 );
	s_propertiesPanelIndex2 = 0;
	s_propertiesPanel2 = 0;

	CBaseObject::EndEditMultiSelParams();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetSelected( bool bSelect )
{
	CBaseObject::SetSelected( bSelect );

	if (m_entity)
	{
		IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)m_entity->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy)
		{
			IRenderNode *pRenderNode = pRenderProxy->GetRenderNode();
			if (pRenderNode)
			{
				int flags = pRenderNode->GetRndFlags();
				if (bSelect && gSettings.viewports.bHighlightSelectedGeometry)
					flags |= ERF_SELECTED;
				else
					flags &= ~ERF_SELECTED;
				pRenderNode->SetRndFlags( flags );
			}
		}
	}
	if (m_pAnimNode)
		m_pAnimNode->SetFlags( (bSelect) ? (m_pAnimNode->GetFlags()|ANODE_FLAG_SELECTED) : (m_pAnimNode->GetFlags()&(~ANODE_FLAG_SELECTED)) );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnPropertyChange( IVariable *var )
{
	if (s_ignorePropertiesUpdate)
		return;

	if (m_pClass != 0 && m_entity != 0)
	{
		if (m_properties != NULL && m_prototype == NULL)
			m_pClass->SetProperties( m_entity,m_properties,false );
		if (m_properties2)
			m_pClass->SetProperties2( m_entity,m_properties2,false );
		
		m_pClass->CallOnPropertyChange(m_entity);
		
		// After change of properties bounding box of entity may change.
		CalcBBox();
		InvalidateTM(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Display( DisplayContext &dc )
{
	if (!m_entity)
		return;

	//XFormEntity();
	
	Matrix34 wtm = GetWorldTM();

	COLORREF col = GetColor();
	if (IsFrozen())
		col = dc.GetFreezeColor();

	//Vec3 scale = GetScale();

	dc.PushMatrix( wtm );

	if (m_bDisplayArrow)
	{
		// Show direction arrow.
		Vec3 dir = FORWARD_DIRECTION;
		dc.SetColor( 1,1,0 );
		dc.DrawArrow( Vec3(0,0,0),FORWARD_DIRECTION*m_helperScale,m_helperScale );
	}

	bool bDisplaySolidBox = (m_bDisplaySolidBBox && gSettings.viewports.bShowTriggerBounds);
	if (IsSelected())
	{
		dc.SetSelectedColor( 0.5f );
		//dc.renderer->Draw3dBBox( GetPos()-Vec3(m_radius,m_radius,m_radius),GetPos()+Vec3(m_radius,m_radius,m_radius));
		if (!m_visualObject || m_bDisplayBBox || (dc.flags & DISPLAY_2D))
		{
			bDisplaySolidBox = m_bDisplaySolidBBox;
			dc.DrawWireBox( m_box.min,m_box.max );
		}
	}
	else
	{
		if ((m_bDisplayBBox && gSettings.viewports.bShowTriggerBounds) || (dc.flags & DISPLAY_2D))
		{
			dc.SetColor( col,0.3f );
			dc.DrawWireBox( m_box.min,m_box.max );
		}
	}

	// Only display solid BBox if visual object is associated with the entity.
	if (bDisplaySolidBox)
	{
		dc.DepthWriteOff();
		dc.SetColor(col,0.05f);
		dc.DrawSolidBox( m_box.min,m_box.max );
		dc.DepthWriteOn();
	}

	/*
	if (m_entity && m_proximityRadius >= 0)
	{
		float r = m_proximityRadius;
		dc.SetColor( 1,1,0,1 );
		dc.DrawWireBox(  GetPos()-Vec3(r,r,r),GetPos()+Vec3(r,r,r) );
	}
	*/

	// Draw radiuses if present and object selected.
	if (gSettings.viewports.bAlwaysShowRadiuses || IsSelected())
	{
		const Vec3& scale = GetScale();
		float fScale = scale.x; // Ignore matrix scale.
		if (fScale == 0) fScale = 1;
		if (m_innerRadius > 0)
		{
			dc.SetColor( 0,1,0,0.3f );
			dc.DrawWireSphere( Vec3(0,0,0),m_innerRadius/fScale );
		}
		if (m_outerRadius > 0)
		{
			dc.SetColor( 1,1,0,0.8f );
			dc.DrawWireSphere( Vec3(0,0,0),m_outerRadius/fScale );
		}
		if (m_proximityRadius > 0)
		{
			dc.SetColor( 1,1,0,0.8f );
			if( m_bDisplayAbsoluteRadius )
				dc.DrawWireSphere( Vec3(0,0,0),Vec3( m_proximityRadius / scale.x,m_proximityRadius / scale.y,m_proximityRadius / scale.z) );
			else
				dc.DrawWireSphere( Vec3(0,0,0),m_proximityRadius );
		}
	}

	dc.PopMatrix();

	// Entities themself are rendered by 3DEngine.

	if (m_visualObject && (!gSettings.viewports.bShowIcons || !HaveTextureIcon()))
	{
		/*
		float fScale = dc.view->GetScreenScaleFactor(wtm.GetTranslation()) * 0.04f;
		wtm[0][0] *= fScale; wtm[0][1] *= fScale; wtm[0][2] *= fScale;
		wtm[1][0] *= fScale; wtm[1][1] *= fScale; wtm[1][2] *= fScale;
		wtm[2][0] *= fScale; wtm[2][1] *= fScale; wtm[2][2] *= fScale;
		*/
		Matrix34 tm(wtm);
		float sz = m_helperScale*gSettings.gizmo.helpersScale;
		tm.ScaleColumn( Vec3(sz,sz,sz) );

		SRendParams rp;
    Vec3 color;
		if (IsSelected())
			color = Rgb2Vec(dc.GetSelectedColor());
		else
			color = Rgb2Vec(col);
    rp.AmbientColor = ColorF(color[0], color[1], color[2], 1);
    rp.dwFObjFlags |= FOB_TRANS_MASK;
		rp.fAlpha = 1;
		rp.nDLightMask = 1;//GetIEditor()->Get3DEngine()->GetLightMaskFromPosition(wtm.GetTranslation(),1.f) & 0xFFFF;
		rp.pMatrix = &tm;
		rp.pMaterial = GetIEditor()->GetIconManager()->GetHelperMaterial();
		//rp.nShaderTemplate = EFT_HELPER;
		m_visualObject->Render( rp );
	}

	if (IsSelected())
	{
		if (m_entity)
		{
			IAIObject *pAIObj = m_entity->GetAI();
			if (pAIObj)
				DrawAIInfo( dc,pAIObj );
		}
	}

	/*
	if ((dc.flags & DISPLAY_LINKS) && !m_eventTargets.empty())
	{
		DrawTargets(dc);
	}
	*/
	if ((dc.flags & DISPLAY_HIDENAMES) && gSettings.viewports.bDrawEntityLabels)
	{
		// If labels hidden but we draw entity labels enabled, always draw them.
		CGroup *pGroup = GetGroup();
		if (!pGroup || pGroup->IsOpen())
		{
			DrawLabel( dc,GetWorldPos(),col );
		}
	}

	DrawDefault(dc,col);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::DrawAIInfo( DisplayContext &dc,IAIObject *aiObj )
{
	assert( aiObj );

	IAIActor * pAIActor = aiObj->CastToIAIActor();
	if(!pAIActor)
		return;
	const AgentParameters &ap =	pAIActor->GetParameters();

	// Draw ranges.
	bool bTerrainCircle = false;
	Vec3 wp = GetWorldPos();
	float z = GetIEditor()->GetTerrainElevation( wp.x,wp.y );
	if (fabs(wp.z-z) < 5)
		bTerrainCircle = true;
/*
		dc.SetColor( RGB(0,255,0) );
		if (bTerrainCircle)
			dc.DrawTerrainCircle( wp,ap.m_fSoundRange,0.2f );
		else
			dc.DrawCircle( wp,ap.m_fSoundRange );

		dc.SetColor( RGB(255,255,0) );
		if (bTerrainCircle)
			dc.DrawTerrainCircle( wp,ap.m_fCommRange,0.2f );
		else
			dc.DrawCircle( wp,ap.m_fCommRange );

		dc.SetColor( RGB(255,0,0) );
		if (bTerrainCircle)
			dc.DrawTerrainCircle( wp,ap.m_fSightRange,0.2f );
		else
			dc.DrawCircle( wp,ap.m_fSightRange );

*/
	dc.SetColor( RGB(255/2,0,0) );
	if (bTerrainCircle)
		dc.DrawTerrainCircle( wp,ap.m_PerceptionParams.sightRange/2,0.2f );
	else
		dc.DrawCircle( wp,ap.m_PerceptionParams.sightRange/2 );

/*
		dc.SetColor( RGB(0,0,255) );
		if (bTerrainCircle)
			dc.DrawTerrainCircle( wp,ap.m_fAttackRange,0.2f );
		else
			dc.DrawCircle( wp,ap.m_fAttackRange );
*/
	//dc.SetColor( 0,1,0,0.3f );		
	//dc.DrawWireSphere( Vec3(0,0,0),m_innerRadius );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::DrawTargets( DisplayContext &dc )
{
	/*
	BBox box;
	for (int i = 0; i < m_eventTargets.size(); i++)
	{
		CBaseObject *target = m_eventTargets[i].target;
		if (!target)
			return;

		dc.SetColor( 0.8f,0.4f,0.4f,1 );
		GetBoundBox( box );
		Vec3 p1 = 0.5f*Vec3(box.max+box.min);
		target->GetBoundBox( box );
		Vec3 p2 = 0.5f*Vec3(box.max+box.min);

		dc.DrawLine( p1,p2 );

		Vec3 p3 = 0.5f*(p2+p1);

		if (!(dc.flags & DISPLAY_HIDENAMES))
		{
			float col[4] = { 0.8f,0.4f,0.4f,1 };
			dc.renderer->DrawLabelEx( p3+Vec3(0,0,0.3f),1.2f,col,true,true,m_eventTargets[i].event );
		}
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Serialize( CObjectArchive &ar )
{
	CBaseObject::Serialize( ar );
	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		//m_entityId = 0;
		
		//m_physicsState = "";
		// Load
		CString entityClass = m_entityClass;
		m_bLoadFailed = false;

		if (!m_prototype)
			xmlNode->getAttr( "EntityClass",entityClass );
		//xmlNode->getAttr( "EntityId",m_entityId );
		//xmlNode->getAttr( "PhysicsState",m_physicsState );
		m_physicsState = xmlNode->findChild("PhysicsState");

		Vec3 angles;
		// Backward compatability, with FarCry levels.
		if (xmlNode->getAttr( "Angles",angles ))
		{
			angles = DEG2RAD(angles);
			angles.z += gf_PI/2;
			Quat quat;
			quat.SetRotationXYZ(Ang3(angles));
			SetRotation( quat );
		}

		/*
		if (xmlNode->getAttr( "MaterialGUID",m_materialGUID ))
		{
			m_pMaterial = (CMaterial*)GetIEditor()->GetMaterialManager()->FindItem( m_materialGUID );
			if (!m_pMaterial)
			{
				CErrorRecord err;
				err.error.Format( "Material %s for Entity %s not found,",GuidUtil::ToString(m_materialGUID),(const char*)GetName() );
				err.pObject = this;
				err.severity = CErrorRecord::ESEVERITY_WARNING;
				ar.ReportError(err);
				//Warning( "Material %s for Entity %s not found,",GuidUtil::ToString(m_materialGUID),(const char*)GetName() );
			}
			else
			{
				if (m_pMaterial->GetParent())
					SetMaterial( m_pMaterial->GetParent() );
				m_pMaterial->SetUsed();
			}
			UpdateMaterialInfo();
		}
		else
			m_pMaterial = 0;
			*/
		
		// Load Event Targets.
		ReleaseEventTargets();
		XmlNodeRef eventTargets = xmlNode->findChild( "EventTargets" );
		if (eventTargets)
		{
			for (int i = 0; i < eventTargets->getChildCount(); i++)
			{
				XmlNodeRef eventTarget = eventTargets->getChild(i);
				CEntityEventTarget et;
				et.target = 0;
				GUID targetId = GUID_NULL;
				eventTarget->getAttr( "TargetId",targetId );
				eventTarget->getAttr( "Event",et.event );
				eventTarget->getAttr( "SourceEvent",et.sourceEvent );
				m_eventTargets.push_back( et );
				if (targetId != GUID_NULL)
					ar.SetResolveCallback( this,targetId,functor(*this,&CEntity::ResolveEventTarget),i );
			}
		}

		XmlNodeRef propsNode;
		XmlNodeRef props2Node = xmlNode->findChild("Properties2");
		if (!m_prototype)
		{
			propsNode = xmlNode->findChild("Properties");
		}

		bool bLoaded = SetClass( entityClass,!ar.bUndo,true,propsNode,props2Node );
		if (ar.bUndo)
		{
			SpawnEntity();
		}
	}
	else
	{
		// Saving.
		if (!m_entityClass.IsEmpty() && m_prototype == NULL)
			xmlNode->setAttr( "EntityClass",m_entityClass );

		//if (m_entityId != 0)
			//xmlNode->setAttr( "EntityId",m_entityId );

		//if (!m_physicsState.IsEmpty())
			//xmlNode->setAttr( "PhysicsState",m_physicsState );
		if(m_physicsState)
			xmlNode->addChild(m_physicsState);

		if (!m_prototype)
		{
			//! Save properties.
			if (m_properties)
			{
				XmlNodeRef propsNode = xmlNode->newChild("Properties");
				m_properties->Serialize( propsNode,ar.bLoading );
			}
		}

		//! Save properties.
		if (m_properties2)
		{
			XmlNodeRef propsNode = xmlNode->newChild("Properties2");
			m_properties2->Serialize( propsNode,ar.bLoading );
		}

		// Save Event Targets.
		if (!m_eventTargets.empty())
		{
			XmlNodeRef eventTargets = xmlNode->newChild( "EventTargets" );
			for (int i = 0; i < m_eventTargets.size(); i++)
			{
				CEntityEventTarget &et = m_eventTargets[i];
				GUID targetId = GUID_NULL;
				if (et.target != 0)
					targetId = et.target->GetId();

				XmlNodeRef eventTarget = eventTargets->newChild( "EventTarget" );
				eventTarget->setAttr( "TargetId",targetId );
				eventTarget->setAttr( "Event",et.event );
				eventTarget->setAttr( "SourceEvent",et.sourceEvent );
			}
		}

		// Save Entity Links.
		if (!m_links.empty())
		{
			XmlNodeRef linksNode = xmlNode->newChild( "EntityLinks" );
			for (int i = 0,num = m_links.size(); i < num; i++)
			{
				XmlNodeRef linkNode = linksNode->newChild( "Link" );
				linkNode->setAttr( "TargetId",m_links[i].targetId );
				linkNode->setAttr( "Name",m_links[i].name );
			}
		}

		// Save flow graph.
		if (m_pFlowGraph && !m_pFlowGraph->IsEmpty())
		{
			XmlNodeRef graphNode = xmlNode->newChild( "FlowGraph" );
			m_pFlowGraph->Serialize( graphNode, false, &ar );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::PostLoad( CObjectArchive &ar )
{
	if (m_entity)
	{
		// Force entities to register them-self in sectors.
		// force entity to be registered in terrain sectors again.
		XFormGameEntity();
		BindToParent();
		BindIEntityChilds();
		if (m_pClass)
		{
			//m_pClass->SetCurrentProperties( this );
			m_pClass->SetEventsTable( this );
		}
		if (m_physicsState)
			m_entity->SetPhysicsState( m_physicsState );
	}

	//////////////////////////////////////////////////////////////////////////
	// Load Links.
	RemoveAllEntityLinks();
	XmlNodeRef linksNode = ar.node->findChild( "EntityLinks" );
	if (linksNode)
	{
		CString name;
		GUID targetId;
		for (int i = 0; i < linksNode->getChildCount(); i++)
		{
			XmlNodeRef linkNode = linksNode->getChild(i);
			linkNode->getAttr( "Name",name );
			if (linkNode->getAttr( "TargetId",targetId ))
			{
				GUID newTargetId = ar.ResolveID(targetId);
				AddEntityLink( name,newTargetId );
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Load flow graph after loading of everything.
	XmlNodeRef graphNode = ar.node->findChild( "FlowGraph" );
	if (graphNode)
	{
		if (!m_pFlowGraph)
		{
			SetFlowGraph( GetIEditor()->GetFlowGraphManager()->CreateGraphForEntity( this ) );
		}
		if (m_pFlowGraph && m_pFlowGraph->GetIFlowGraph())
			m_pFlowGraph->Serialize( graphNode, true, &ar );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::CheckSpecConfig( )
{
	if( m_entity && m_entity->GetAI() && GetMinSpec()!=0)
	{
		CErrorRecord err;
		err.error.Format( "AI entity %s ->> spec dependent",(const char*)GetName() );
		err.pObject = this;
		err.severity = CErrorRecord::ESEVERITY_WARNING;
		GetIEditor()->GetErrorReport()->ReportError(err);
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CEntity::Export( const CString &levelPath,XmlNodeRef &xmlExportNode )
{
	if (m_bLoadFailed)
		return 0;

	// Do not export entity with bad id.
	if (!m_entityId)
		return CreateXmlNode("Temp");

	CheckSpecConfig( );

	// Export entities to entities.ini
	XmlNodeRef objNode = xmlExportNode->newChild( "Entity" );

	objNode->setAttr( "Name",GetName() );

	if (GetMaterial())
		objNode->setAttr( "Material",GetMaterial()->GetName() );

	if (m_prototype)
		objNode->setAttr( "Archetype",m_prototype->GetFullName() );

	Vec3 pos = GetPos(),scale = GetScale();
	Quat rotate = GetRotation();
	if (GetParent())
	{
		if (GetParent()->IsKindOf( RUNTIME_CLASS(CEntity) ))
		{
			// Store parent entity id.
			CEntity *parentEntity = (CEntity*)GetParent();
			if (parentEntity)
				objNode->setAttr( "ParentId",parentEntity->GetEntityId() );
		}
		else
		{
			// Export world coordinates.
			AffineParts ap;
			ap.SpectralDecompose( GetWorldTM() );
			pos = ap.pos;
			rotate = ap.rot;
			scale = ap.scale;
		}
	}

	if (!IsEquivalent(pos,Vec3(0,0,0),0))
		objNode->setAttr( "Pos",pos );

	if (!rotate.IsIdentity())
		objNode->setAttr( "Rotate",rotate );

	if (!IsEquivalent(scale,Vec3(1,1,1),0))
		objNode->setAttr( "Scale",scale );
	
	objNode->setTag( "Entity" );
	objNode->setAttr( "EntityClass",m_entityClass );
	objNode->setAttr( "EntityId",m_entityId );
	objNode->setAttr( "EntityGuid",ToEntityGuid( GetId() ) );

	if (mv_ratioLOD != 100)
		objNode->setAttr( "LodRatio",(int)mv_ratioLOD );

	if (mv_ratioViewDist != 100)
		objNode->setAttr( "ViewDistRatio",(int)mv_ratioViewDist );

	if (!mv_castShadow)
		objNode->setAttr( "CastShadow",false );

	//if (!mv_recvShadow)
		//objNode->setAttr( "RecieveShadow",false );

  if (mv_recvWind)
    objNode->setAttr( "RecvWind",true );

	if (mv_useOccluders)
		objNode->setAttr( "UseOccluders",true );

	if (mv_outdoor)
		objNode->setAttr( "OutdoorOnly",true );

	if (GetMinSpec() != 0)
		objNode->setAttr( "MinSpec",(uint32)GetMinSpec() );

	if (mv_motionBlurAmount != 1.0f)
    objNode->setAttr( "MotionBlurMultiplier", (float)mv_motionBlurAmount );

	uint32 nMtlLayersMask = GetMaterialLayersMask();
	if (nMtlLayersMask != 0)
		objNode->setAttr( "MatLayersMask",nMtlLayersMask );

	if (mv_hiddenInGame)
		objNode->setAttr( "HiddenInGame",true );

	//if (!m_physicsState.IsEmpty())
		//objNode->setAttr( "PhysicsState",m_physicsState );
	if(m_physicsState)
		objNode->addChild(m_physicsState);

	objNode->setAttr( "Layer",GetLayer()->GetName() );

	// Export Event Targets.
	if (!m_eventTargets.empty())
	{
		XmlNodeRef eventTargets = objNode->newChild( "EventTargets" );
		for (int i = 0; i < m_eventTargets.size(); i++)
		{
			CEntityEventTarget &et = m_eventTargets[i];

			int entityId = 0;
			if (et.target)
			{
				if (et.target->IsKindOf( RUNTIME_CLASS(CEntity) ))
				{
					entityId = ((CEntity*)et.target)->GetEntityId();
				}
			}

			XmlNodeRef eventTarget = eventTargets->newChild( "EventTarget" );
			//eventTarget->setAttr( "Target",obj->GetName() );
			eventTarget->setAttr( "Target",entityId );
			eventTarget->setAttr( "Event",et.event );
			eventTarget->setAttr( "SourceEvent",et.sourceEvent );
		}
	}

	// Save Entity Links.
	if (!m_links.empty())
	{
		XmlNodeRef linksNode = objNode->newChild( "EntityLinks" );
		for (int i = 0,num = m_links.size(); i < num; i++)
		{
			if (m_links[i].target)
			{
				XmlNodeRef linkNode = linksNode->newChild( "Link" );
				linkNode->setAttr( "TargetId",m_links[i].target->GetEntityId() );
				linkNode->setAttr( "Name",m_links[i].name );
			}
		}
	}

	//! Export properties.
	if (m_properties)
	{
		XmlNodeRef propsNode = objNode->newChild("Properties");
		m_properties->Serialize( propsNode,false );
	}
	//! Export properties.
	if (m_properties2)
	{
		XmlNodeRef propsNode = objNode->newChild("Properties2");
		m_properties2->Serialize( propsNode,false );
	}

	if (objNode != NULL && m_entity != NULL)
	{
		// Export internal entity data.
		m_entity->SerializeXML( objNode,false );
	}

	// Save flow graph.
	if (m_pFlowGraph && !m_pFlowGraph->IsEmpty())
	{
		XmlNodeRef graphNode = objNode->newChild( "FlowGraph" );
		m_pFlowGraph->Serialize( graphNode, false, 0 );
	}

	return objNode;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnEvent( ObjectEvent event )
{
	CBaseObject::OnEvent(event);

	switch (event)
	{
		case EVENT_INGAME:
		case EVENT_OUTOFGAME:
			if (m_entity)
			{
				if (event == EVENT_INGAME)
				{
					if (!m_bCalcPhysics)
						m_entity->ClearFlags(ENTITY_FLAG_IGNORE_PHYSICS_UPDATE);
					// Entity must be hidden when going to game.
					if (m_bVisible)
						m_entity->Hide( mv_hiddenInGame );
				}
				else if (event == EVENT_OUTOFGAME)
				{
					// Entity must be returned to editor visibility state.
					m_entity->Hide( !m_bVisible );
					//m_entity->SetGarbageFlag(false); // If marked as garbage, unmark it.
				}
				XFormGameEntity();
				OnRenderFlagsChange(0);
				/*
				if (!m_physicsState.IsEmpty())
				{
					IPhysicalEntity *physic = m_entity->GetPhysics();
					if (physic)
					{
						const char *str = m_physicsState;
						physic->SetStateFromSnapshotTxt( const_cast<char*>(str),m_physicsState.GetLength() );
						physic->PostSetStateFromSnapshot();
					}
				}
				*/

				if (event == EVENT_OUTOFGAME)
				{
					if (!m_bCalcPhysics)
						m_entity->SetFlags(ENTITY_FLAG_IGNORE_PHYSICS_UPDATE);
				}
			}
			break;
		case EVENT_REFRESH:
			if (m_entity)
			{
				// force entity to be registered in terrain sectors again.

				//-- little hack to force reregistration of entities
				//<<FIXME>> when issue with registration in editor is resolved
				Vec3 pos = GetPos();
				pos.z+=1.f;
				m_entity->SetPos( pos );
				//----------------------------------------------------

				XFormGameEntity();
			}
			break;

		case EVENT_UNLOAD_GEOM:
		case EVENT_UNLOAD_ENTITY:
			if (m_pClass)
				m_pClass = 0;
			if (m_entity)
			{
				UnloadScript();
			}
			break;

		case EVENT_RELOAD_ENTITY:
			GetIEditor()->GetErrorReport()->SetCurrentValidatorObject( this );
			if (m_pClass)
				m_pClass->Reload();
			Reload();
			break;

		case EVENT_RELOAD_GEOM:
			GetIEditor()->GetErrorReport()->SetCurrentValidatorObject( this );
			Reload();
			break;

		case EVENT_PHYSICS_GETSTATE:
			AcceptPhysicsState();
			break;
		case EVENT_PHYSICS_RESETSTATE:
			ResetPhysicsState();
			break;
		case EVENT_PHYSICS_APPLYSTATE:
			if (m_physicsState)
				m_entity->SetPhysicsState( m_physicsState );
			break;

		case EVENT_UPDATE_TRACKGIZMO:
			UpdateTrackGizmo();
			break;

		case EVENT_FREE_GAME_DATA:
			FreeGameData();
			break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UpdateTrackGizmo()
{
	bool bNeedTrackGizmo = m_bVisible && m_pAnimNode != 0 && m_pAnimNode->GetAnimBlock();
	if (m_trackGizmo != 0 && !bNeedTrackGizmo)
	{
		RemoveGizmo( m_trackGizmo );
		m_trackGizmo = 0;
	}
	if (m_trackGizmo == 0 && bNeedTrackGizmo)
	{
		m_trackGizmo = new CTrackGizmo;
		AddGizmo( m_trackGizmo );
	}
	if (m_trackGizmo)
	{
		m_trackGizmo->SetAnimNode( m_pAnimNode );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::Reload( bool bReloadScript )
{
	if (!m_pClass || bReloadScript)
		SetClass( m_entityClass,true );
	if (m_entity)
		DeleteEntity();
	SpawnEntity();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UpdateVisibility( bool visible )
{
	if (visible != m_bVisible)
	{
		m_bVisible = visible;
		CBaseObject::UpdateVisibility(visible);
		if (!visible && m_entity)
		{
			m_entity->Hide(true);
			// Unload entity.
			//OnEvent(EVENT_UNLOAD_ENTITY);
		}
		if (visible && m_entity)
		{
			m_entity->Hide(false);
		}
		UpdateTrackGizmo();
	}
};

/*
class StaticInit
{
public:
	StaticInit()
	{
		Vec3 angles(20,11.51,112);
		Vec3 a1,a2;

		Matrix tm;
		tm.Identity();
		tm.RotateMatrix( angles );
		quaternionf qq( angles.z*PI/180.0f,angles.y*PI/180.0f,angles.x*PI/180.0f );

		float mtx[3][3];
		qq.getmatrix_buf( (float*)mtx );

		Quat q(tm);
		Quat q1;
		q1.SetEulerAngles( angles*PI/180.0f );
		Matrix tm1;
		q1.GetMatrix(tm1);
		a1 = q.GetEulerAngles() * 180.0f/PI;
		a2 = q1.GetEulerAngles() * 180.0f/PI;
	}
};
StaticInit ss;
*/

//////////////////////////////////////////////////////////////////////////
void CEntity::DrawDefault( DisplayContext &dc,COLORREF labelColor )
{
	CBaseObject::DrawDefault( dc, labelColor );

	if ( m_entity && gEnv->pAISystem )
		if ( IsSelected() || IsHighlighted() )
			gEnv->pAISystem->DrawSOClassTemplate( m_entity );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnNodeAnimated( IAnimNode *pAnimNode )
{
	if (!m_pAnimNode)
		return;

	Vec3 pos = m_pAnimNode->GetPos();
	Vec3 scale = m_pAnimNode->GetScale();
	Quat rotate = m_pAnimNode->GetRotate();

	SetLocalTM( pos,rotate,scale,TM_ANIMATION );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::InvalidateTM( int nWhyFlags )
{
	CBaseObject::InvalidateTM(nWhyFlags);

	if (nWhyFlags & TM_RESTORE_UNDO) // Can skip updating game object when restoring undo.
		return;

	// If matrix changes.
	if (m_trackGizmo)
	{
		//m_trackGizmo->SetMatrix( GetWorldTM() );
		if (GetParent())
		{
			m_trackGizmo->SetMatrix( GetParent()->GetWorldTM() );
		}
		else
		{
			Matrix34 tm;
			tm.SetIdentity();
			m_trackGizmo->SetMatrix(tm);
		}
	}

	// Make sure components are correctly calculated.
	const Matrix34 &tm = GetWorldTM();
	if (m_entity)
	{
		int nWhyEntityFlag = 0;
		if (nWhyFlags & TM_ANIMATION)
		{
			nWhyEntityFlag = ENTITY_XFORM_TRACKVIEW;
		}
		else
		{
			nWhyEntityFlag = ENTITY_XFORM_EDITOR;
		}

		if (GetParent() && !m_entity->GetParent())
		{
			m_entity->SetWorldTM( tm,nWhyEntityFlag );
		}
		else if (GetLookAt())
		{
			m_entity->SetWorldTM( tm,nWhyEntityFlag );
		}
		else
		{
			if (nWhyFlags & TM_PARENT_CHANGED)
				nWhyEntityFlag |= ENTITY_XFORM_FROM_PARENT;
			m_entity->SetPosRotScale( GetPos(),GetRotation(),GetScale(),nWhyEntityFlag );
		}
	}

	if (m_pAnimNode != NULL)
	{
		float time = GetIEditor()->GetAnimation()->GetTime();
		if (!(nWhyFlags & TM_ANIMATION)) // when Matrix invalidation caused by animation itself do not propogate chanegs to anim node.
		{
			if (nWhyFlags&TM_POS_CHANGED)
				m_pAnimNode->SetPos( time,GetPos() );
			if (nWhyFlags&TM_ROT_CHANGED)
				m_pAnimNode->SetRotate( time,GetRotation() );
			if (nWhyFlags&TM_SCL_CHANGED)
				m_pAnimNode->SetScale( time,GetScale() );
		}
	}

	// Update entity and animation node if something changed.

	/*
	if (m_entity && GetParent())
	{
		if (!m_entity->GetParent())
		{
			m_entity->ForceBindCalculation(true);
			m_entity->SetParentLocale( GetParent()->GetWorldTM() );
			XFormGameEntity();
		}
	}
	*/
	m_bEntityXfromValid = false;
}

//////////////////////////////////////////////////////////////////////////
//! Attach new child node.
void CEntity::AttachChild( CBaseObject* child,bool bKeepPos )
{
	CBaseObject::AttachChild( child,bKeepPos );
	if (child && child->IsKindOf(RUNTIME_CLASS(CEntity)))
	{
		((CEntity*)child)->BindToParent();
	}
 	if (bKeepPos)
	{
		if (m_pAnimNode != NULL)
		{
			float time = GetIEditor()->GetAnimation()->GetTime();
			m_pAnimNode->SetPos( time,GetPos() );
			m_pAnimNode->SetRotate( time,GetRotation() );
			//if (nWhyFlags&TM_SCL_CHANGED)
				//m_pAnimNode->SetScale( time,GetScale() );
		}
	}
}

//! Detach all childs of this node.
void CEntity::DetachAll( bool bKeepPos )
{
	//@FIXME: Unbind all childs.
	CBaseObject::DetachAll(bKeepPos);
}

// Detach this node from parent.
void CEntity::DetachThis( bool bKeepPos )
{
	UnbindIEntity();
	CBaseObject::DetachThis(bKeepPos);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::BindToParent()
{
	if (!m_entity)
		return;

	CBaseObject *parent = GetParent();
	if (parent)
	{
		if (parent->IsKindOf(RUNTIME_CLASS(CEntity)))
		{
			CEntity *parentEntity = (CEntity*)parent;

			IEntity *ientParent = parentEntity->GetIEntity();
			if (ientParent)
			{
				XFormGameEntity();
				ientParent->AttachChild( m_entity,IEntity::ATTACHMENT_KEEP_TRANSFORMATION );
				XFormGameEntity();
			}
		}
		else
		{
			/*
			m_entity->ForceBindCalculation(true);
			m_entity->SetParentLocale( GetParent()->GetWorldTM() );
			XFormGameEntity();
			*/
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::BindIEntityChilds()
{
	if (!m_entity)
		return;

	int numChilds = GetChildCount();
	for (int i = 0; i < numChilds; i++)
	{
		CBaseObject *child = GetChild(i);
		if (child && child->IsKindOf(RUNTIME_CLASS(CEntity)))
		{
			IEntity *ientChild = ((CEntity*)child)->GetIEntity();
			if (ientChild)
			{
				((CEntity*)child)->XFormGameEntity();
				m_entity->AttachChild( ientChild );
				((CEntity*)child)->XFormGameEntity();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UnbindIEntity()
{
	if (!m_entity)
		return;
	
	CBaseObject *parent = GetParent();
	if (parent && parent->IsKindOf(RUNTIME_CLASS(CEntity)))
	{
		CEntity *parentEntity = (CEntity*)parent;
		
		IEntity *ientParent = parentEntity->GetIEntity();
		if (ientParent)
		{
			//m_entity->ForceBindCalculation(false);
			m_entity->DetachThis(IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx )
{
	CBaseObject::PostClone( pFromObject,ctx );
	
	CEntity *pFromEntity = (CEntity*)pFromObject;
	// Clone event targets.
	if (!pFromEntity->m_eventTargets.empty())
	{
		int numTargets = pFromEntity->m_eventTargets.size();
		for (int i = 0; i < numTargets; i++)
		{
			CEntityEventTarget &et = pFromEntity->m_eventTargets[i];
			CBaseObject *pClonedTarget = ctx.FindClone( et.target );
			if (!pClonedTarget)
				pClonedTarget = et.target; // If target not cloned, link to original target.

			// Add cloned event.
			AddEventTarget( pClonedTarget,et.event,et.sourceEvent,true );
		}
	}

	// Clone links.
	if (!pFromEntity->m_links.empty())
	{
		int numTargets = pFromEntity->m_links.size();
		for (int i = 0; i < numTargets; i++)
		{
			CEntityLink &et = pFromEntity->m_links[i];
			CBaseObject *pClonedTarget = ctx.FindClone( et.target );
			if (!pClonedTarget)
				pClonedTarget = et.target; // If target not cloned, link to original target.

			// Add cloned event.
			if (pClonedTarget)
				AddEntityLink( et.name,pClonedTarget->GetId() );
			else
				AddEntityLink( et.name,GUID_NULL );
		}
	}


	if (m_pFlowGraph)
	{
		m_pFlowGraph->PostClone( pFromObject, ctx);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::ResolveEventTarget( CBaseObject *object,unsigned int index )
{
	// Find targetid.
	assert( index >= 0 && index < m_eventTargets.size() );
	if (object)
		object->AddEventListener( functor(*this,&CEntity::OnEventTargetEvent) );
	m_eventTargets[index].target = object;
	if (!m_eventTargets.empty() && m_pClass != 0)
		m_pClass->SetEventsTable( this );

	// Make line gizmo.
	if (!m_eventTargets[index].pLineGizmo && object)
	{
		CLineGizmo *pLineGizmo = new CLineGizmo;
		pLineGizmo->SetObjects( this,object );
		pLineGizmo->SetColor( Vec3(0.8f,0.4f,0.4f),Vec3(0.8f,0.4f,0.4f) );
		pLineGizmo->SetName( m_eventTargets[index].event );
		AddGizmo( pLineGizmo );
		m_eventTargets[index].pLineGizmo = pLineGizmo;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::RemoveAllEntityLinks()
{
	while (!m_links.empty())
		RemoveEntityLink( m_links.size()-1);
	m_links.clear();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::ReleaseEventTargets()
{
	while (!m_eventTargets.empty())
		RemoveEventTarget( m_eventTargets.size()-1,false );
	m_eventTargets.clear();
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnEventTargetEvent( CBaseObject *target,int event )
{
	// When event target is deleted.
	if (event == CBaseObject::ON_DELETE)
	{
		// Find this target in events list and remove.
		int numTargets = m_eventTargets.size();
		for (int i = 0; i < numTargets; i++)
		{
			if (m_eventTargets[i].target == target)
			{
				RemoveEventTarget( i );
				numTargets = m_eventTargets.size();
				i--;
			}
		}
		numTargets = m_links.size();
		for (int i = 0; i < numTargets; i++)
		{
			if (m_links[i].target == target)
			{
				RemoveEntityLink(i);
				numTargets = m_eventTargets.size();
				i--;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CEntity::AddEventTarget( CBaseObject *target,const CString &event,const CString &sourceEvent,bool bUpdateScript )
{
	StoreUndo( "Add EventTarget" );
	CEntityEventTarget et;
	et.target = target;
	et.event = event;
	et.sourceEvent = sourceEvent;

	// Assign event target.
	if (et.target)
		et.target->AddEventListener( functor(*this,&CEntity::OnEventTargetEvent) );

	if (target)
	{
		// Make line gizmo.
		CLineGizmo *pLineGizmo = new CLineGizmo;
		pLineGizmo->SetObjects( this,target );
		pLineGizmo->SetColor( Vec3(0.8f,0.4f,0.4f),Vec3(0.8f,0.4f,0.4f) );
		pLineGizmo->SetName( event );
		AddGizmo( pLineGizmo );
		et.pLineGizmo = pLineGizmo;
	}

	m_eventTargets.push_back( et );

	// Update event table in script.
	if (bUpdateScript && m_pClass != 0)
		m_pClass->SetEventsTable( this );

	return m_eventTargets.size()-1;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::RemoveEventTarget( int index,bool bUpdateScript )
{
	if (index >= 0 && index < m_eventTargets.size())
	{
		StoreUndo( "Remove EventTarget" );

		if (m_eventTargets[index].pLineGizmo)
		{
			RemoveGizmo( m_eventTargets[index].pLineGizmo );
		}

		if (m_eventTargets[index].target)
			m_eventTargets[index].target->RemoveEventListener( functor(*this,&CEntity::OnEventTargetEvent) );
		m_eventTargets.erase( m_eventTargets.begin()+index );

		// Update event table in script.
		if (bUpdateScript && m_pClass != 0 && m_entity != 0)
			m_pClass->SetEventsTable( this );
	}
}

//////////////////////////////////////////////////////////////////////////
int CEntity::AddEntityLink( const CString &name,GUID targetEntityId )
{
	CEntity *target = 0;
	if (targetEntityId != GUID_NULL)
	{
		CBaseObject *pObject = FindObject(targetEntityId);
		if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntity)))
		{
			target = (CEntity*)pObject;
		}
	}

	StoreUndo( "Add EntityLink" );

	CLineGizmo *pLineGizmo = 0;
	
	// Assign event target.
	if (target)
	{
		target->AddEventListener( functor(*this,&CEntity::OnEventTargetEvent) );

		// Make line gizmo.
		pLineGizmo = new CLineGizmo;
		pLineGizmo->SetObjects( this,target );
		pLineGizmo->SetColor( Vec3(0.4f,1.0f,0.0f),Vec3(0.0f,1.0f,0.0f) );
		pLineGizmo->SetName( name );
		AddGizmo( pLineGizmo );
	}

	CEntityLink lnk;
	lnk.targetId = targetEntityId;
	lnk.target = target;
	lnk.name = name;
	lnk.pLineGizmo = pLineGizmo;
	m_links.push_back( lnk );

	if (m_entity != NULL && target != NULL)
	{
		// Add link to entity itself.
		m_entity->AddEntityLink( name,target->GetEntityId() );
	}

	return m_links.size()-1;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::RemoveEntityLink( int index )
{
	if (index >= 0 && index < m_links.size())
	{
		StoreUndo( "Remove EntityLink" );

		if (m_links[index].pLineGizmo)
		{
			RemoveGizmo( m_links[index].pLineGizmo );
		}

		if (m_links[index].target)
			m_links[index].target->RemoveEventListener( functor(*this,&CEntity::OnEventTargetEvent) );
		m_links.erase( m_links.begin()+index );

		UpdateIEntityLinks();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::RenameEntityLink( int index,const CString &newName )
{
	if (index >= 0 && index < m_links.size())
	{
		StoreUndo( "Rename EntityLink" );

		if (m_links[index].pLineGizmo)
		{
			m_links[index].pLineGizmo->SetName(newName);
		}

		m_links[index].name = newName;

		UpdateIEntityLinks();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UpdateIEntityLinks( bool bCallOnPropertyChange )
{
	if (m_entity)
	{
		m_entity->RemoveAllEntityLinks();
		for (int i = 0,num = m_links.size(); i < num; i++)
		{
			if (m_links[i].target)
				m_entity->AddEntityLink(m_links[i].name,m_links[i].target->GetEntityId());
		}

		if (bCallOnPropertyChange)
			m_pClass->CallOnPropertyChange(m_entity);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnEntityFlagsChange( IVariable *var )
{
	if (m_entity)
	{
		int flags = m_entity->GetFlags();

		if (mv_castShadow)
			flags |= ENTITY_FLAG_CASTSHADOW;
		else
			flags &= ~ENTITY_FLAG_CASTSHADOW;

		if (mv_castScatter)
			flags |= ENTITY_FLAG_CASTSCATTER;
		else
			flags &= ~ENTITY_FLAG_CASTSCATTER;

		//if (mv_recvShadow)
			//flags |= ENTITY_FLAG_RECVSHADOW;
		//else
			//flags &= ~ENTITY_FLAG_RECVSHADOW;

		if (mv_outdoor)
			flags |= ENTITY_FLAG_OUTDOORONLY;
		else
			flags &= ~ENTITY_FLAG_OUTDOORONLY;

    if (mv_recvWind)
      flags |= ENTITY_FLAG_RECVWIND;
    else
      flags &= ~ENTITY_FLAG_RECVWIND;

		if (mv_useOccluders)
			flags |= ENTITY_FLAG_GOOD_OCCLUDER;
		else
			flags &= ~ENTITY_FLAG_GOOD_OCCLUDER;

		m_entity->SetFlags( flags );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnRenderFlagsChange( IVariable *var )
{
	if (m_entity)
	{
		IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)m_entity->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy)
		{
			IRenderNode *pRenderNode = pRenderProxy->GetRenderNode();
			if (pRenderNode)
			{
				pRenderNode->SetLodRatio(mv_ratioLOD);
        pRenderProxy->SetMotionBlurAmount(mv_motionBlurAmount);

				// With Custom view dist ratio it is set by code not UI.
				if (!(m_entity->GetFlags() & ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO))
					pRenderNode->SetViewDistRatio(mv_ratioViewDist);
				else
				{
					// Disable UI for view distance ratio.
					mv_ratioViewDist.SetFlags( mv_ratioViewDist.GetFlags()|IVariable::UI_DISABLED );
				}

        int nRndFlags = pRenderNode->GetRndFlags();
        if (mv_recvWind)
          nRndFlags |= ERF_RECVWIND;
        else
          nRndFlags &= ~ERF_RECVWIND;

				if (mv_useOccluders)
					nRndFlags |= ERF_GOOD_OCCLUDER;
				else
					nRndFlags &= ~ERF_GOOD_OCCLUDER;

				if (mv_castScatter)
					nRndFlags |= ERF_CASTSCATTERMAPS;
				else
					nRndFlags &= ~ERF_CASTSCATTERMAPS;

        pRenderNode->SetRndFlags( nRndFlags );

				pRenderNode->SetMinSpec( GetMinSpec() );
			}
			//// Set material layer flags..
			uint8 nMaterialLayersFlags = GetMaterialLayersMask();
			pRenderProxy->SetMaterialLayersMask( nMaterialLayersFlags );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnRadiusChange( IVariable *var )
{
	var->Get(m_proximityRadius);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnInnerRadiusChange( IVariable *var )
{
	var->Get(m_innerRadius);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnOuterRadiusChange( IVariable *var )
{
	var->Get(m_outerRadius);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnUseOccludersChange( IVariable *var )
{
	if (m_entity)
		m_entity->UpdateUseOccluders( mv_useOccluders );
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CEntity::CloneProperties( CVarBlock *srcProperties )
{
	assert( srcProperties );

	CVarBlock *properties = srcProperties->Clone(true);

	//@FIXME Hack to dispay radiuses of properties.
	// wires properties from param block, to this entity internal variables.
	IVariable *var = 0;
	var = properties->FindVariable( "Radius",false );
	if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
	{
		var->Get(m_proximityRadius);
		var->AddOnSetCallback( functor(*this,&CEntity::OnRadiusChange) );
	}
	else 
	{
		var = properties->FindVariable( "radius",false );
		if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
		{
			var->Get(m_proximityRadius);
			var->AddOnSetCallback( functor(*this,&CEntity::OnRadiusChange) );
		}
	}
	
	var = properties->FindVariable( "InnerRadius",false );
	if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
	{
		var->Get(m_innerRadius);
		var->AddOnSetCallback( functor(*this,&CEntity::OnInnerRadiusChange) );
	}
	var = properties->FindVariable( "OuterRadius",false );
	if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
	{
		var->Get(m_outerRadius);
		var->AddOnSetCallback( functor(*this,&CEntity::OnOuterRadiusChange) );
	}

	// Each property must have callback to our OnPropertyChange.
	properties->AddOnSetCallback( functor(*this,&CEntity::OnPropertyChange) );

	return properties;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OnLoadFailed()
{
	m_bLoadFailed = true;

	CErrorRecord err;
	err.error.Format( "Entity %s Failed to Spawn (Script: %s)",(const char*)GetName(),(const char*)m_entityClass );
	err.pObject = this;
	GetIEditor()->GetErrorReport()->ReportError(err);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetMaterial( CMaterial *mtl )
{
	CBaseObject::SetMaterial(mtl);
	UpdateMaterialInfo();
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CEntity::GetRenderMaterial() const
{
	if (GetMaterial())
		return GetMaterial();
	if (m_entity)
	{
		IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)m_entity->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy)
		{
			return GetIEditor()->GetMaterialManager()->FromIMaterial( pRenderProxy->GetRenderMaterial() );
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::UpdateMaterialInfo()
{
	if (m_entity)
	{
		if (GetMaterial())
			m_entity->SetMaterial(GetMaterial()->GetMatInfo());
		else
			m_entity->SetMaterial(0);

		IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)m_entity->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy)
		{
			// Set material layer flags on entity.
			uint8 nMaterialLayersFlags = GetMaterialLayersMask();
			pRenderProxy->SetMaterialLayersMask( nMaterialLayersFlags );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetMaterialLayersMask( uint32 nLayersMask )
{
	__super::SetMaterialLayersMask(nLayersMask);
	UpdateMaterialInfo();
}


//////////////////////////////////////////////////////////////////////////
void CEntity::SetMinSpec( uint32 nMinSpec )
{
	__super::SetMinSpec(nMinSpec);
	OnRenderFlagsChange(0);
	OnUseOccludersChange(0);
}

//////////////////////////////////////////////////////////////////////////
void CEntity::AcceptPhysicsState()
{
	if (m_entity)
	{
		StoreUndo( "Accept Physics State" );

		// [Anton] - StoreUndo sends EVENT_AFTER_LOAD, which forces position and angles to editor's position and
		// angles, which are not updated with the physics value
		SetWorldTM( m_entity->GetWorldTM() );

		IPhysicalEntity *physic = m_entity->GetPhysics();
		if (!physic && m_entity->GetCharacter(0)) // for ropes
			physic = m_entity->GetCharacter(0)->GetISkeletonPose()->GetCharacterPhysics(0);
		if (physic && (physic->GetType()==PE_ARTICULATED || physic->GetType()==PE_ROPE))
		{
			/*
			// Only get state snapshot for articulated characters.
			char str[4096*4];
			int len = physic->GetStateSnapshotTxt( str,sizeof(str) );
			if (len > sizeof(str)-1)
				len = sizeof(str)-1;
			str[len] = 0;
			m_physicsState = str;
			/**/

			IXmlSerializer * pSerializer = GetISystem()->GetXmlUtils()->CreateXmlSerializer();
			if(pSerializer)
			{
				m_physicsState = GetISystem()->CreateXmlNode( "PhysicsState" );
				ISerialize * ser = pSerializer->GetWriter(m_physicsState);
				physic->GetStateSnapshot(ser);
				pSerializer->Release();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::ResetPhysicsState()
{
	if (m_entity)
	{
		//StoreUndo( "Reset Physics State" );
		//m_physicsState = "";
		m_physicsState = 0;
		m_entity->SetPhysicsState( m_physicsState );
		Reload();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetHelperScale( float scale )
{
	bool bChanged = m_helperScale != scale;
	m_helperScale = scale;
	if (bChanged)
	{
		CalcBBox();
	}
}

//////////////////////////////////////////////////////////////////////////
//! Analyze errors for this object.
void CEntity::Validate( CErrorReport *report )
{
	CBaseObject::Validate( report );
	
	if (!m_entity && !m_entityClass.IsEmpty())
	{
		CErrorRecord err;
		err.error.Format( "Entity %s Failed to Spawn (Script: %s)",(const char*)GetName(),(const char*)m_entityClass );
		err.pObject = this;
		report->ReportError(err);
		return;
	}

	if (!m_entity)
		return;

	int slot;
	
	// Check Entity.
	int numObj = m_entity->GetSlotCount();
	for (slot = 0; slot < numObj; slot++)
	{
		IStatObj *pObj = m_entity->GetStatObj(slot);
		if (!pObj)
			continue;

		if (pObj->IsDefaultObject())
		{
			//const char *filename = obj.object->GetFileName();
			// File Not found.
			CErrorRecord err;
			err.error.Format( "Geometry File in Slot %d for Entity %s not found",slot,(const char*)GetName() );
			//err.file = filename;
			err.pObject = this;
			err.flags = CErrorRecord::FLAG_NOFILE;
			report->ReportError(err);
		}
	}
	/*
	IEntityCharacter *pIChar = m_entity->GetCharInterface();
	pIChar->GetCharacter(0);
	for (slot = 0; slot < MAX_ANIMATED_MODELS; slot++)
	{
		ICharacterInstance *pCharacter = pIChar->GetCharacter(slot);
		pCharacter->IsDe
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CEntity::GatherUsedResources( CUsedResources &resources )
{
	CBaseObject::GatherUsedResources( resources );
	if (m_properties)
		m_properties->GatherUsedResources( resources );
	if (m_properties2)
		m_properties2->GatherUsedResources( resources );
	if (m_prototype != NULL && m_prototype->GetProperties())
		m_prototype->GetProperties()->GatherUsedResources( resources );
}

//////////////////////////////////////////////////////////////////////////
bool CEntity::IsSimilarObject( CBaseObject *pObject )
{
	if (pObject->GetClassDesc() == GetClassDesc() && pObject->GetRuntimeClass() == GetRuntimeClass())
	{
		CEntity *pEntity = (CEntity*)pObject;
		if (m_entityClass == pEntity->m_entityClass && 
				m_proximityRadius == pEntity->m_proximityRadius &&
				m_innerRadius == pEntity->m_innerRadius &&
				m_outerRadius == pEntity->m_outerRadius)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntity::SetFlowGraph( CFlowGraph *pGraph )
{
	if (pGraph != m_pFlowGraph)
	{
		if (m_pFlowGraph)
			m_pFlowGraph->Release();
		m_pFlowGraph = pGraph;
		if (m_pFlowGraph)
		{
			m_pFlowGraph->SetEntity(this, true); // true -> re-adjust graph entity nodes
			m_pFlowGraph->AddRef();

			if (m_entity)
			{
				IEntityFlowGraphProxy *pFlowGraphProxy = (IEntityFlowGraphProxy*)m_entity->CreateProxy(ENTITY_PROXY_FLOWGRAPH);
				pFlowGraphProxy->SetFlowGraph( m_pFlowGraph->GetIFlowGraph() );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntity::OpenFlowGraph( const CString &groupName )
{
	if (!m_pFlowGraph)
	{
		StoreUndo("Create Flow Graph");
		//m_pIFlowGraph = GetIEditor()->GetGameEngine()->GetIFlowSystem()->CreateFlowGraph();
		//m_pIFlowGraph->AddRef();
		//m_pIFlowGraph->SetDefaultEntity( GetEntityId() );
		SetFlowGraph( GetIEditor()->GetFlowGraphManager()->CreateGraphForEntity( this, groupName ) );
	}
	GetIEditor()->GetFlowGraphManager()->OpenView( m_pFlowGraph );
}

//////////////////////////////////////////////////////////////////////////
void CEntity::RemoveFlowGraph()
{
	if (m_pFlowGraph)
	{
		StoreUndo("Remove Flow Graph");
		GetIEditor()->GetFlowGraphManager()->UnregisterAndResetView(m_pFlowGraph);
		m_pFlowGraph->Release();
		m_pFlowGraph = 0;

		if (m_entity)
		{
			IEntityFlowGraphProxy *pFlowGraphProxy = (IEntityFlowGraphProxy*)m_entity->GetProxy(ENTITY_PROXY_FLOWGRAPH);
			if (pFlowGraphProxy)
				pFlowGraphProxy->SetFlowGraph( 0 );
		}
	}
	//GetIEditor()->GetFlowGraphManager()->OpenView( m_pFlowGraph );
}

//////////////////////////////////////////////////////////////////////////
CString CEntity::GetSmartObjectClass() const
{
	CString soClass;
	if (m_properties)
	{
		IVariable* pVar = m_properties->FindVariable( "soclasses_SmartObjectClass" );
		if (pVar)
			pVar->Get(soClass);
	}
	return soClass;
}

//////////////////////////////////////////////////////////////////////////
// CSimple Entity.
//////////////////////////////////////////////////////////////////////////
bool CSimpleEntity::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	bool bRes = false;
	if (file.IsEmpty())
	{
		bRes = CEntity::Init( ie,prev,"" );
	}
	else
	{
		bRes = CEntity::Init( ie,prev,"BasicEntity" );
		SetClass( m_entityClass );
		SetGeometryFile( file );
	}

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CSimpleEntity::SetGeometryFile( const CString &filename )
{
	if (m_properties)
	{
		IVariable* pModelVar = m_properties->FindVariable( "object_Model" );
		if (pModelVar)
		{
			pModelVar->Set( filename );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CString CSimpleEntity::GetGeometryFile() const
{
	CString filename;
	if (m_properties)
	{
		IVariable* pModelVar = m_properties->FindVariable( "object_Model" );
		if (pModelVar)
		{
			pModelVar->Get( filename );
		}
	}
	return filename;
}

//////////////////////////////////////////////////////////////////////////
bool CSimpleEntity::ConvertFromObject( CBaseObject *object )
{
	CEntity::ConvertFromObject( object );
	if (object->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		CBrushObject *pBrushObject = (CBrushObject*)object;

		IStatObj *prefab = pBrushObject->GetIStatObj();
		if (!prefab)
			return false;

		// Copy entity shadow parameters.
		int rndFlags = pBrushObject->GetRenderFlags();

//		mv_castShadows = (rndFlags & ERF_CASTSHADOWVOLUME) != 0;
		//mv_selfShadowing = (rndFlags & ERF_SELFSHADOW) != 0;
		mv_castShadow = (rndFlags & ERF_CASTSHADOWMAPS) != 0;
    //mv_recvShadow = (rndFlags & ERF_RECVSHADOWMAPS) != 0;    
		mv_outdoor = (rndFlags & ERF_OUTDOORONLY) != 0;    
		//mv_recvShadowMapsASMR = (rndFlags & ERF_RECVSHADOWMAPS_ACTIVE) != 0;
		//mv_castLightmap = (rndFlags & ERF_CASTSHADOWINTORAMMAP) != 0;
		//mv_recvLightmap = (rndFlags & ERF_USERAMMAPS) != 0;
		mv_ratioLOD = pBrushObject->GetRatioLod();
		mv_ratioViewDist = pBrushObject->GetRatioViewDist();
    mv_recvWind = (rndFlags & ERF_RECVWIND) != 0;    
		mv_useOccluders = (rndFlags & ERF_GOOD_OCCLUDER) != 0;    
    
		SetClass( "BasicEntity" );
		SpawnEntity();
		SetGeometryFile( prefab->GetFilePath() );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSimpleEntity::BeginEditParams( IEditor *ie,int flags )
{
	CEntity::BeginEditParams( ie,flags );

	CString filename = GetGeometryFile();

	if (gSettings.bGeometryBrowserPanel)
	{
		if (!filename.IsEmpty())
		{
			if (!s_treePanel)
			{
				s_treePanel = new CPanelTreeBrowser;
				int flags = CPanelTreeBrowser::NO_DRAGDROP|CPanelTreeBrowser::NO_PREVIEW|CPanelTreeBrowser::SELECT_ONCLICK;
				s_treePanel->Create( functor(*this,&CSimpleEntity::OnFileChange),GetClassDesc()->GetFileSpec(),AfxGetMainWnd(),flags );
			}
			if (s_treePanelId == 0)
				s_treePanelId = AddUIPage( _T("Geometry"),s_treePanel,false );
		}

		if (s_treePanel)
		{
			s_treePanel->SetSelectCallback( functor(*this,&CSimpleEntity::OnFileChange) );
			s_treePanel->SelectFile( filename );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSimpleEntity::EndEditParams( IEditor *ie )
{
	if (s_treePanelId != 0)
	{
		RemoveUIPage( s_treePanelId );
		s_treePanelId = 0;
	}

	CEntity::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
void CSimpleEntity::OnFileChange( CString filename )
{
	CUndo undo("Entity Geom Modify");
	StoreUndo( "Entity Geom Modify" );
	SetGeometryFile( filename );
}

//////////////////////////////////////////////////////////////////////////
//! Analyze errors for this object.
void CSimpleEntity::Validate( CErrorReport *report )
{
	CEntity::Validate( report );
	
	// Checks if object loaded.
}

//////////////////////////////////////////////////////////////////////////
bool CSimpleEntity::IsSimilarObject( CBaseObject *pObject )
{
	if (pObject->GetClassDesc() == GetClassDesc() && pObject->GetRuntimeClass() == GetRuntimeClass())
	{
		CSimpleEntity *pEntity = (CSimpleEntity*)pObject;
		if (GetGeometryFile() == pEntity->GetGeometryFile())
		{
			return true;
		}
	}
	return false;
}



//////////////////////////////////////////////////////////////////////////
// CGeomEntity
//////////////////////////////////////////////////////////////////////////
bool CGeomEntity::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	bool bRes = false;
	if (file.IsEmpty())
	{
		bRes = CEntity::Init( ie,prev,"" );
		SetClass( "GeomEntity" );
	}
	else
	{
		bRes = CEntity::Init( ie,prev,"GeomEntity" );
		SetClass( "GeomEntity" );
		SetGeometryFile( file );
	}

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::InitVariables()
{
	AddVariable( mv_geometry,"Geometry",functor(*this,&CGeomEntity::OnGeometryFileChange),IVariable::DT_OBJECT );
	__super::InitVariables();
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::SetGeometryFile( const CString &filename )
{
	if (filename != mv_geometry)
		mv_geometry = filename;

	OnGeometryFileChange(&mv_geometry);
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::OnGeometryFileChange( IVariable *pVar )
{
	if (m_entity)
	{
		CString filename = (CString)mv_geometry;
		const char *ext = PathUtil::GetExt(filename);
		if (stricmp(ext,CRY_CHARACTER_FILE_EXT) == 0 || stricmp(ext,CRY_CHARACTER_DEFINITION_FILE_EXT) == 0 || stricmp(ext,CRY_ANIM_GEOMETRY_FILE_EXT) == 0)
		{
			m_entity->LoadCharacter( 0,filename,IEntity::EF_AUTO_PHYSICALIZE ); // Physicalize geometry.
		}
		else
		{
			m_entity->LoadGeometry( 0,filename,NULL,IEntity::EF_AUTO_PHYSICALIZE ); // Physicalize geometry.
		}
		CalcBBox();
		InvalidateTM(0);
		OnRenderFlagsChange(0);
		OnUseOccludersChange(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::SpawnEntity()
{
	__super::SpawnEntity();
	SetGeometryFile( mv_geometry );
}

//////////////////////////////////////////////////////////////////////////
CString CGeomEntity::GetGeometryFile() const
{
	return mv_geometry;
}

//////////////////////////////////////////////////////////////////////////
bool CGeomEntity::ConvertFromObject( CBaseObject *object )
{
	CEntity::ConvertFromObject( object );
	if (object->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		CBrushObject *pBrushObject = (CBrushObject*)object;

		IStatObj *prefab = pBrushObject->GetIStatObj();
		if (!prefab)
			return false;

		// Copy entity shadow parameters.
		int rndFlags = pBrushObject->GetRenderFlags();

		//		mv_castShadows = (rndFlags & ERF_CASTSHADOWVOLUME) != 0;
		//mv_selfShadowing = (rndFlags & ERF_SELFSHADOW) != 0;
		mv_castShadow = (rndFlags & ERF_CASTSHADOWMAPS) != 0;
		mv_castScatter = (rndFlags & ERF_CASTSCATTERMAPS) != 0;
		//mv_recvShadow = (rndFlags & ERF_RECVSHADOWMAPS) != 0;    
		mv_outdoor = (rndFlags & ERF_OUTDOORONLY) != 0;    
		//mv_recvShadowMapsASMR = (rndFlags & ERF_RECVSHADOWMAPS_ACTIVE) != 0;
		//mv_castLightmap = (rndFlags & ERF_CASTSHADOWINTORAMMAP) != 0;
		//mv_recvLightmap = (rndFlags & ERF_USERAMMAPS) != 0;
		mv_ratioLOD = pBrushObject->GetRatioLod();
		mv_ratioViewDist = pBrushObject->GetRatioViewDist();
		mv_recvWind = (rndFlags & ERF_RECVWIND) != 0;    
		mv_useOccluders = (rndFlags & ERF_GOOD_OCCLUDER) != 0;    

		SpawnEntity();
		SetGeometryFile( prefab->GetFilePath() );
	}
	else if (object->IsKindOf(RUNTIME_CLASS(CEntity)))
	{
		CEntity *pObject = (CEntity*)object;
		if (!pObject->GetIEntity())
			return false;

		IStatObj *pStatObj = pObject->GetIEntity()->GetStatObj(0);
		if (!pStatObj)
			return false;

		SpawnEntity();
		SetGeometryFile( pStatObj->GetFilePath() );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::BeginEditParams( IEditor *ie,int flags )
{
	CEntity::BeginEditParams( ie,flags );

	CString filename = GetGeometryFile();

	if (gSettings.bGeometryBrowserPanel)
	{
		if (!filename.IsEmpty())
		{
			if (!s_treePanel)
			{
				s_treePanel = new CPanelTreeBrowser;
				int flags = CPanelTreeBrowser::NO_DRAGDROP|CPanelTreeBrowser::NO_PREVIEW|CPanelTreeBrowser::SELECT_ONCLICK;
				s_treePanel->Create( functor(*this,&CGeomEntity::OnFileChange),GetClassDesc()->GetFileSpec(),AfxGetMainWnd(),flags );
			}
			if (s_treePanelId == 0)
				s_treePanelId = AddUIPage( _T("Geometry"),s_treePanel,false );
		}

		if (s_treePanel)
		{
			s_treePanel->SetSelectCallback( functor(*this,&CGeomEntity::OnFileChange) );
			s_treePanel->SelectFile( filename );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::EndEditParams( IEditor *ie )
{
	if (s_treePanelId != 0)
	{
		RemoveUIPage( s_treePanelId );
		s_treePanelId = 0;
	}

	CEntity::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::OnFileChange( CString filename )
{
	CUndo undo("Entity Geom Modify");
	StoreUndo( "Entity Geom Modify" );
	SetGeometryFile( filename );
}

//////////////////////////////////////////////////////////////////////////
//! Analyze errors for this object.
void CGeomEntity::Validate( CErrorReport *report )
{
	CEntity::Validate( report );

	// Checks if object loaded.
}

//////////////////////////////////////////////////////////////////////////
bool CGeomEntity::IsSimilarObject( CBaseObject *pObject )
{
	if (pObject->GetClassDesc() == GetClassDesc() && pObject->GetRuntimeClass() == GetRuntimeClass())
	{
		CGeomEntity *pEntity = (CGeomEntity*)pObject;
		if (GetGeometryFile() == pEntity->GetGeometryFile())
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CGeomEntity::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	XmlNodeRef node = CEntity::Export( levelPath,xmlNode );
	if (node)
	{
		node->setAttr( "Geometry",(CString)mv_geometry );
	}
	return node;
}

//////////////////////////////////////////////////////////////////////////
void CGeomEntity::OnEvent( ObjectEvent event )
{
	__super::OnEvent(event);
	switch (event)
	{
	case EVENT_INGAME:
	case EVENT_OUTOFGAME:
		if (m_entity)
		{
			// Make entity sleep on in/out game events.
			IPhysicalEntity *pe = m_entity->GetPhysics();
			if (pe)
			{
				pe_action_awake aa;
				aa.bAwake = 0;
				pe->Action(&aa);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
