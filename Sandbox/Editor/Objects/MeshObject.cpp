////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   MeshObject.cpp
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: CMeshObject implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MeshObject.h"

#include "ObjectManager.h"
#include "..\Viewport.h"

#include "..\Brush\BrushPanel.h"
#include "..\Brush\Brush.h"
#include "PanelTreeBrowser.h"

#include "Entity.h"
#include "Geometry\EdMesh.h"
#include "Material\Material.h"
#include "Material\MaterialManager.h"
#include "Settings.h"
#include "ISubObjectSelectionReferenceFrameCalculator.h"

#include <I3Dengine.h>
#include <IEntitySystem.h>
#include <IEntityRenderState.h>
#include <IPhysics.h>

#define MIN_BOUNDS_SIZE 0.01f

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CMeshObject,CBaseObject)

namespace
{
	CBrushPanel* s_brushPanel = NULL;
	int s_brushPanelId = 0;

	CPanelTreeBrowser* s_treePanelPtr = NULL;
	int s_treePanelId = 0;
}


//////////////////////////////////////////////////////////////////////////
CMeshObject::CMeshObject()
{
	m_pGeometry = 0;
	m_pRenderNode = 0;

	m_renderFlags = 0;
	m_bNotSharedGeom = false;

	UseMaterialLayersMask(true);

	// Init Variables.
	mv_outdoor = false;
//	mv_castShadows = false;
	//mv_selfShadowing = false;
  mv_castShadowMaps = false;
  //mv_recvShadowMaps = true;
	mv_castLightmap = true;
	mv_noIndirLight = false;
	mv_recvLightmap = true;
	mv_hideable = 0;
	mv_ratioLOD = 100;
	mv_ratioViewDist = 100;
	mv_excludeFromTriangulation = false;
  mv_aiRadius = -1.0f;
	mv_noDecals = false;  
	mv_lightmapQuality = 1;
	mv_lightmapQuality.SetLimits( 0,100 );  
  mv_recvWind = false;
  
	static CString sVarName_OutdoorOnly = "OutdoorOnly";
//	static CString sVarName_CastShadows = "CastShadows";
//	static CString sVarName_CastShadows2 = _T("CastShadowVolume");
	//static CString sVarName_SelfShadowing = "SelfShadowing";
	static CString sVarName_CastShadowMaps = "CastShadowMaps";
	static CString sVarName_RecvShadowMaps = "RecvShadowMaps";
	static CString sVarName_CastLightmap = "CastRAMmap";
	static CString sVarName_ReceiveLightmap = "ReceiveRAMmap";
	static CString sVarName_Hideable = "Hideable";
	static CString sVarName_HideableSecondary = "HideableSecondary";
	static CString sVarName_LodRatio = "LodRatio";
	static CString sVarName_ViewDistRatio = "ViewDistRatio";
	static CString sVarName_NotTriangulate = "NotTriangulate";
	static CString sVarName_AIRadius = "AIRadius";
	static CString sVarName_LightmapQuality = "RAMmapQuality";
	static CString sVarName_NoDecals = "NoStaticDecals";
	static CString sVarName_Frozen = "Frozen";
	static CString sVarName_NoAmbShadowCaster = "NoAmnbShadowCaster";
  static CString sVarName_RecvWind = "RecvWind";

	CVarEnumList<int>* pHideModeList = new CVarEnumList<int>;
	pHideModeList->AddItem("None", 0);
	pHideModeList->AddItem("Hideable", 1);
	pHideModeList->AddItem("Secondary", 2);
	mv_hideable.SetEnumList(pHideModeList);

	ReserveNumVariables( 15 );
	AddVariable( mv_outdoor,sVarName_OutdoorOnly,functor(*this,&CMeshObject::OnRenderVarChange) );
//	AddVariable( mv_castShadows,sVarName_CastShadows,sVarName_CastShadows2,functor(*this,&CMeshObject::OnRenderVarChange) );
	//AddVariable( mv_selfShadowing,sVarName_SelfShadowing,functor(*this,&CMeshObject::OnRenderVarChange) );
	AddVariable( mv_castShadowMaps,sVarName_CastShadowMaps,functor(*this,&CMeshObject::OnRenderVarChange) );
	//AddVariable( mv_recvShadowMaps,sVarName_RecvShadowMaps,functor(*this,&CMeshObject::OnRenderVarChange) );

	AddVariable( mv_castLightmap,sVarName_CastLightmap,functor(*this,&CMeshObject::OnRenderVarChange) );
	AddVariable( mv_recvLightmap,sVarName_ReceiveLightmap,functor(*this,&CMeshObject::OnRenderVarChange) );
	AddVariable( mv_hideable,sVarName_Hideable,functor(*this,&CMeshObject::OnRenderVarChange) );
//	AddVariable( mv_hideableSecondary,sVarName_HideableSecondary,functor(*this,&CMeshObject::OnRenderVarChange) );
	AddVariable( mv_ratioLOD,sVarName_LodRatio,functor(*this,&CMeshObject::OnRenderVarChange) );
	AddVariable( mv_ratioViewDist,sVarName_ViewDistRatio,functor(*this,&CMeshObject::OnRenderVarChange) );
	AddVariable( mv_excludeFromTriangulation,sVarName_NotTriangulate,functor(*this,&CMeshObject::OnRenderVarChange) );
	AddVariable( mv_aiRadius,sVarName_AIRadius,functor(*this,&CMeshObject::OnAIRadiusVarChange) );
	AddVariable( mv_noDecals,sVarName_NoDecals,functor(*this,&CMeshObject::OnRenderVarChange) );
	AddVariable( mv_lightmapQuality,sVarName_LightmapQuality );
	AddVariable( mv_noIndirLight,sVarName_NoAmbShadowCaster );
  AddVariable( mv_recvWind, sVarName_RecvWind,functor(*this,&CMeshObject::OnRenderVarChange) );

	mv_ratioLOD.SetLimits( 0,255 );
	mv_ratioViewDist.SetLimits( 0,255 );

	m_bIgnoreNodeUpdate = false;
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::Done()
{
	FreeGameData();

	CBaseObject::Done();
}

bool CMeshObject::IncludeForGI()
{ 
	//return (mv_castShadowMaps || mv_recvShadowMaps); 
	return true; 
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::FreeGameData()
{
	if (m_pRenderNode)
	{
		GetIEditor()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
		m_pRenderNode = 0;
	}
	// Release Mesh.
	if (m_pGeometry)
		m_pGeometry->RemoveUser();
	m_pGeometry = 0;
}

//////////////////////////////////////////////////////////////////////////
bool CMeshObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	SetColor( RGB(255,255,255) );
	
	if (IsCreateGameObjects())
	{
		if (prev)
		{
			CMeshObject *brushObj = (CMeshObject*)prev;
		}
		else if (!file.IsEmpty())
		{
			// Create brush from geometry.
			mv_geometryFile = file;

			CString name = Path::GetFileName( file );
			SetUniqName( name );
		}
		//m_indoor->AddBrush( this );
	}

	// Must be after SetBrush call.
	bool res = CBaseObject::Init( ie,prev,file );
	
	if (prev)
	{
		CMeshObject *brushObj = (CMeshObject*)prev;
		m_bbox = brushObj->m_bbox;
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CMeshObject::CreateGameObject()
{
	if (!m_pRenderNode)
	{
		m_pRenderNode = GetIEditor()->Get3DEngine()->CreateRenderNode( eERType_Brush );
		m_pRenderNode->SetEditorObjectId( GetId().Data1 );
		UpdateEngineNode();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::BeginEditParams( IEditor *ie,int flags )
{
	CBaseObject::BeginEditParams( ie,flags );

	if (!s_brushPanel)
	{
		s_brushPanel = new CBrushPanel;
		s_brushPanelId = ie->AddRollUpPage( ROLLUP_OBJECTS,_T("Brush Parameters"),s_brushPanel );
	}

	if (gSettings.bGeometryBrowserPanel)
	{
		CString prefabName = mv_geometryFile;
		if (!prefabName.IsEmpty())
		{
			if (!s_treePanelPtr)
			{
				s_treePanelPtr = new CPanelTreeBrowser;
				int flags = CPanelTreeBrowser::NO_DRAGDROP|CPanelTreeBrowser::NO_PREVIEW|CPanelTreeBrowser::SELECT_ONCLICK;
				s_treePanelPtr->Create( functor(*this,&CMeshObject::OnFileChange),GetClassDesc()->GetFileSpec(),AfxGetMainWnd(),flags );
			}
			if (s_treePanelId == 0)
				s_treePanelId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,_T("Prefab"),s_treePanelPtr,false );
		}

		if (s_treePanelPtr)
		{
			s_treePanelPtr->SetSelectCallback( functor(*this,&CMeshObject::OnFileChange) );
			s_treePanelPtr->SelectFile( prefabName );
		}
	}

	if (s_brushPanel)
		s_brushPanel->SetBrush( this );
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::EndEditParams( IEditor *ie )
{
	CBaseObject::EndEditParams( ie );

	if (s_treePanelId != 0)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,s_treePanelId );
		s_treePanelId = 0;
	}

	if (s_brushPanel)
	{
		ie->RemoveRollUpPage( ROLLUP_OBJECTS,s_brushPanelId );
		s_brushPanel = 0;
		s_brushPanelId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::BeginEditMultiSelParams( bool bAllOfSameType )
{
	CBaseObject::BeginEditMultiSelParams( bAllOfSameType );
	if (bAllOfSameType)
	{
		if (!s_brushPanel)
		{
			s_brushPanel = new CBrushPanel;
			s_brushPanelId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,_T("Brush Parameters"),s_brushPanel );
		}
		if (s_brushPanel)
			s_brushPanel->SetBrush(0);
	}
}
	
//////////////////////////////////////////////////////////////////////////
void CMeshObject::EndEditMultiSelParams()
{
	CBaseObject::EndEditMultiSelParams();
	if (s_brushPanel)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,s_brushPanelId );
		s_brushPanel = 0;
		s_brushPanelId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::OnFileChange( CString filename )
{
	CUndo undo("Brush Prefab Modify");
	StoreUndo( "Brush Prefab Modify" );
	mv_geometryFile = filename;
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::SetScale( const Vec3 &scale )
{
	// Ignore scale;
	CBaseObject::SetScale( scale );
};

//////////////////////////////////////////////////////////////////////////
void CMeshObject::SetSelected( bool bSelect )
{
	CBaseObject::SetSelected( bSelect );

	if (m_pRenderNode)
	{
		if (bSelect && gSettings.viewports.bHighlightSelectedGeometry)
			m_renderFlags |= ERF_SELECTED;
		else
			m_renderFlags &= ~ERF_SELECTED;
		
		int mergeFlags = (ERF_MERGE_RESULT | ERF_SUBSURFSCATTER) & m_pRenderNode->GetRndFlags();
		m_pRenderNode->SetRndFlags( m_renderFlags | mergeFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::GetLocalBounds( BBox &box )
{
	box = m_bbox;
}

//////////////////////////////////////////////////////////////////////////
int CMeshObject::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown)
	{
		Vec3 pos = view->MapViewToCP( point );
		SetPos( pos );
		if (event == eMouseLDown)
			return MOUSECREATE_OK;
		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback( view,event,point,flags );
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::Display( DisplayContext &dc )
{
	if (!m_pGeometry)
	{
		static int	nagCount = 0;
		if( nagCount < 100 )
		{
			const CString&	geomName = mv_geometryFile;
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Brush '%s' (%s) does not have geometry!", (const char*)GetName(), (const char*)geomName);
			nagCount++;
		}
	}

	if (dc.flags & DISPLAY_2D)
	{
		int flags = 0;

		if (IsSelected())
		{
			dc.SetLineWidth(2);
	
			flags = 1;
			//dc.SetSelectedColor();
			dc.SetColor( RGB(225,0,0) );
		}
		else
		{
			flags = 0;
			dc.SetColor( GetColor() );
		}

		dc.PushMatrix( GetWorldTM() );
		dc.DrawWireBox( m_bbox.min,m_bbox.max );

		dc.PopMatrix();
		//if (m_brush)
			//dc.view->DrawBrush( dc,m_brush,GetWorldTM(),flags );

		if (IsSelected())
			dc.SetLineWidth(0);

		if( m_pGeometry )
			m_pGeometry->Display( dc );

		return;
	}

	if( m_pGeometry )
		m_pGeometry->Display( dc );

	
	/*
	if (IsSelected())
	{
		dc.SetSelectedColor();
		dc.PushMatrix( GetWorldTM() );
		dc.DrawWireBox( m_bbox.min,m_bbox.max );
		//dc.SetColor( RGB(255,255,0),0.1f ); // Yellow selected color.
		//dc.DrawSolidBox( m_bbox.min,m_bbox.max );
		dc.PopMatrix();
	}
	*/
  
  if (IsSelected() && m_pRenderNode)
  {
    Vec3 scaleVec = GetScale();
    float scale = 0.5f * (scaleVec.x + scaleVec.y);
    float aiRadius = mv_aiRadius * scale;
    if (aiRadius > 0.0f)
    {
	    dc.SetColor( 1,0,1.0f,0.7f );
	    dc.DrawTerrainCircle( GetPos(), aiRadius, 0.1f );
    }
  }
	
	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CMeshObject::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::Serialize( CObjectArchive &ar )
{
	XmlNodeRef xmlNode = ar.node;
	m_bIgnoreNodeUpdate = true;
	CBaseObject::Serialize( ar );
	m_bIgnoreNodeUpdate = false;
	if (ar.bLoading)
	{
		ar.node->getAttr( "NotSharedGeometry",m_bNotSharedGeom );
			/*
		if (ar.bUndo)
		{
			OnPrefabChange(0);
		}
		*/
		if (!m_pGeometry)
		{
			CString mesh = mv_geometryFile;
			if (!mesh.IsEmpty())
				CreateBrushFromPrefab( mesh );
		}

		UpdateEngineNode();
	}
	else
	{
		ar.node->setAttr( "RndFlags",m_renderFlags );
		if (m_bNotSharedGeom)
			ar.node->setAttr( "NotSharedGeometry",m_bNotSharedGeom );
		
		if (!ar.bUndo)
		{
			if (m_pGeometry)
			{
				m_pGeometry->Serialize( ar );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMeshObject::HitTest( HitContext &hc )
{
	if (CheckFlags(OBJFLAG_SUBOBJ_EDITING) || hc.nSubObjFlags != 0)
	{
		if (m_pGeometry)
			return m_pGeometry->HitTest( hc );
		return false;
	}

	Vec3 pnt;
	
	Vec3 raySrc = hc.raySrc;
	Vec3 rayDir = hc.rayDir;
	WorldToLocalRay( raySrc,rayDir );

	if (m_bbox.IsIntersectRay( raySrc,rayDir,pnt ))
	{
		if (hc.b2DViewport)
		{
			// World space distance.
			hc.dist = hc.raySrc.GetDistance(GetWorldTM().TransformPoint(pnt));
			return true;
		}

		IPhysicalEntity *physics = 0;

		if (m_pRenderNode)
		{
			physics = m_pRenderNode->GetPhysics();
			if (physics)
			{
				if (physics->GetStatus( &pe_status_nparts() ) == 0)
					physics = 0;
			}
		}

		if (physics)
		{ 
			Vec3r origin = hc.raySrc;
			Vec3r dir = hc.rayDir*10000.0f;
			ray_hit hit;
			int col = GetIEditor()->GetSystem()->GetIPhysicalWorld()->RayTraceEntity( physics,origin,dir,&hit );
			if (col <= 0)
				return false;

			// World space distance.
			hc.dist = hit.dist;
			return true;
		}
		else
		{
			// World space distance.
			hc.dist = hc.raySrc.GetDistance(GetWorldTM().TransformPoint(pnt));
			return true;

			/*
			// No physics collision.
			SBrushFace *face = m_brush->Ray( raySrc,rayDir,&dist );
			//SBrushFace *face = m_brush->Ray( hc.raySrc,hc.rayDir,&dist );
			if (face)
			{
				hc.dist = dist;
				return true;
			}
			*/
		} 
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
//! Invalidates cached transformation matrix.
void CMeshObject::InvalidateTM( int nWhyFlags )
{
	CBaseObject::InvalidateTM(nWhyFlags);

	if (!(nWhyFlags & TM_RESTORE_UNDO)) // Can skip updating game object when restoring undo.
	{
		if (m_pRenderNode)
			UpdateEngineNode(true);
	}
	
	m_invertTM = GetWorldTM();
	m_invertTM.Invert();
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::OnEvent( ObjectEvent event )
{
	switch (event)
	{
	case EVENT_FREE_GAME_DATA:
		FreeGameData();
		return;
	}
	__super::OnEvent(event);
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::WorldToLocalRay( Vec3 &raySrc,Vec3 &rayDir )
{
	raySrc = m_invertTM.TransformPoint( raySrc );
	rayDir = m_invertTM.TransformVector(rayDir).GetNormalized();
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::OnGeometryChange( IVariable *var )
{
	// Load new prefab model.
	CString objName = mv_geometryFile;

	CreateBrushFromPrefab( objName );
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::CreateBrushFromPrefab( const char *meshFilname )
{
	if (m_pGeometry)
	{
		if (m_pGeometry->IsSameObject(meshFilname))
		{
			return;
		}
	}
	if (m_pGeometry)
		m_pGeometry->RemoveUser();

	GetIEditor()->GetErrorReport()->SetCurrentFile( meshFilname );
	m_pGeometry = CEdMesh::LoadMesh( meshFilname );
	if(m_pGeometry)
		m_pGeometry->AddUser();
	GetIEditor()->GetErrorReport()->SetCurrentFile( "" );
	if (m_pGeometry)
	{
		m_pGeometry->GetBounds( m_bbox );
		UpdateEngineNode();
	}
	else if (m_pRenderNode)
	{
		// Remove this object from engine node.
		m_pRenderNode->SetEntityStatObj( 0,0,0 );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::ReloadGeometry()
{
	if (m_pGeometry)
	{
		m_pGeometry->ReloadGeometry();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::OnAIRadiusVarChange( IVariable *var )
{
	if (m_bIgnoreNodeUpdate)
		return;

	if (m_pRenderNode)
	{
		IStatObj *pStatObj = m_pRenderNode->GetEntityStatObj(0);
		if (pStatObj)
		{
			// update all other objects that share pStatObj
			CBaseObjectsArray objects;
			GetIEditor()->GetObjectManager()->GetObjects(objects);
			for (unsigned i = 0 ; i < objects.size() ; ++i)
			{
				CBaseObject* pBase = objects[i];
				if (pBase->GetType() == OBJTYPE_BRUSH)
				{
					CMeshObject *obj = (CMeshObject*)pBase;
					if (obj->m_pRenderNode && obj->m_pRenderNode->GetEntityStatObj(0) == pStatObj)
					{
						obj->mv_aiRadius = mv_aiRadius;
					}
				}
			}
		}
	}
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::OnRenderVarChange( IVariable *var )
{
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
IPhysicalEntity* CMeshObject::GetCollisionEntity() const
{
	// Returns physical object of entity.
	if (m_pRenderNode)
		return m_pRenderNode->GetPhysics();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CMeshObject::ConvertFromObject( CBaseObject *object )
{
	CBaseObject::ConvertFromObject( object );
	if (object->IsKindOf(RUNTIME_CLASS(CEntity)))
	{
		CEntity *entity = (CEntity*)object;
		IEntity *pIEntity = entity->GetIEntity();
		if (!pIEntity)
			return false;

		IStatObj *prefab = pIEntity->GetStatObj(0);
		if (!prefab)
			return false;

		// Copy entity shadow parameters.
		//mv_selfShadowing = entity->IsSelfShadowing();
		mv_castShadowMaps = entity->IsCastShadow();
		//mv_recvShadowMaps = entity->IsRecvShadow();
		//mv_castLightmap = entity->IsCastLightmap();
		//mv_recvLightmap = entity->IsRecvLightmap();
		mv_ratioLOD = entity->GetRatioLod();
		mv_ratioViewDist = entity->GetRatioViewDist();
		mv_noIndirLight = false;
    mv_recvWind = false;

		mv_geometryFile = prefab->GetFilePath();
	}
	/*
	if (object->IsKindOf(RUNTIME_CLASS(CStaticObject)))
	{
		CStaticObject *pStatObject = (CStaticObject*)object;
		mv_hideable = pStatObject->IsHideable();
	}
	*/
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::UpdateEngineNode( bool bOnlyTransform )
{
	if (m_bIgnoreNodeUpdate)
		return;

	if (!m_pRenderNode)
		return;

	//////////////////////////////////////////////////////////////////////////
	// Set brush render flags.
	//////////////////////////////////////////////////////////////////////////
	m_renderFlags = 0;
	if (mv_outdoor)
		m_renderFlags |= ERF_OUTDOORONLY;
	//	if (mv_castShadows)
	//	m_renderFlags |= ERF_CASTSHADOWVOLUME;
	//if (mv_selfShadowing)
		//m_renderFlags |= ERF_SELFSHADOW;
	if (mv_castShadowMaps)
		m_renderFlags |= ERF_CASTSHADOWMAPS;
	//if (mv_recvShadowMaps)
		//m_renderFlags |= ERF_RECVSHADOWMAPS;
	if (mv_castLightmap)
		m_renderFlags |= ERF_CASTSHADOWINTORAMMAP;
	if (mv_recvLightmap)
		m_renderFlags |= ERF_USERAMMAPS;
	if (IsHidden())
		m_renderFlags |= ERF_HIDDEN;
	if (mv_hideable==1)
		m_renderFlags |= ERF_HIDABLE;
	if (mv_hideable==2)
		m_renderFlags |= ERF_HIDABLE_SECONDARY;
//	if (mv_hideableSecondary)
//		m_renderFlags |= ERF_HIDABLE_SECONDARY;
	if (mv_excludeFromTriangulation)
		m_renderFlags |= ERF_EXCLUDE_FROM_TRIANGULATION;
	if (mv_noDecals)
		m_renderFlags |= ERF_NO_DECALNODE_DECALS;  
  if( mv_recvWind )
    m_renderFlags |= ERF_RECVWIND;

	if (IsSelected() && gSettings.viewports.bHighlightSelectedGeometry)
		m_renderFlags |= ERF_SELECTED;

	int flags = GetRenderFlags();

	int mergeFlags = (ERF_MERGE_RESULT  | ERF_SUBSURFSCATTER) & m_pRenderNode->GetRndFlags();
	m_pRenderNode->SetRndFlags( m_renderFlags | mergeFlags );

	m_pRenderNode->SetViewDistRatio( mv_ratioViewDist );
	m_pRenderNode->SetLodRatio( mv_ratioLOD );

  // Set material layer flags..
  uint8 nMaterialLayersFlags = 0;

  m_pRenderNode->SetMaterialLayers( GetMaterialLayersMask() );

	if (m_pGeometry)
	{
		Matrix34 tm = GetWorldTM();
		m_pRenderNode->SetEntityStatObj( 0,m_pGeometry->GetGeometry(),&tm );
		m_pGeometry->SetWorldTM( tm );
	}

  IStatObj *pStatObj = m_pRenderNode->GetEntityStatObj(0);
  if (pStatObj)
  {
    float r = mv_aiRadius;
    pStatObj->SetAIVegetationRadius(r);
  }

	// Fast exit if only transformation needs to be changed.
	if (bOnlyTransform)
		return;


	if (GetMaterial())
	{
		GetMaterial()->AssignToEntity( m_pRenderNode );
	}
	else
	{
		// Reset all material settings for this node.
		m_pRenderNode->SetMaterial(0);
	}

	return;
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CMeshObject::GetIStatObj() const
{
	if (!m_pGeometry)
		return 0;
	return m_pGeometry->GetGeometry();
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::UpdateVisibility( bool visible )
{
	CBaseObject::UpdateVisibility( visible );
	if (m_pRenderNode)
	{
		if (!visible)
			m_renderFlags |= ERF_HIDDEN;
		else
			m_renderFlags &= ~ERF_HIDDEN;

		int mergeFlags = (ERF_MERGE_RESULT | ERF_SUBSURFSCATTER) & m_pRenderNode->GetRndFlags();
		m_pRenderNode->SetRndFlags( m_renderFlags | mergeFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::SetMaterial( CMaterial *mtl )
{
	CBaseObject::SetMaterial(mtl);
	if (m_pRenderNode)
		UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMeshObject::GetRenderMaterial() const
{
	if (GetMaterial())
		return GetMaterial();
	if (m_pGeometry)
		return GetIEditor()->GetMaterialManager()->FromIMaterial(m_pGeometry->GetGeometry()->GetMaterial());
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::SetMaterialLayersMask( uint32 nLayersMask )
{
	CBaseObject::SetMaterialLayersMask(nLayersMask);
	UpdateEngineNode(false);
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::Validate( CErrorReport *report )
{
	CBaseObject::Validate( report );

	if (!m_pGeometry)
	{
		CString file = mv_geometryFile;
		CErrorRecord err;
		err.error.Format( "No Geometry %s for Brush %s",(const char*)file,(const char*)GetName() );
		err.file = file;
		err.pObject = this;
		report->ReportError(err);
	}
	else if (m_pGeometry->IsDefaultObject())
	{
		CString file = mv_geometryFile;
		CErrorRecord err;
		err.error.Format( "Geometry file %s for Brush %s Failed to Load",(const char*)file,(const char*)GetName() );
		err.file = file;
		err.pObject = this;
		report->ReportError(err);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMeshObject::IsSimilarObject( CBaseObject *pObject )
{
	if (pObject->GetClassDesc() == GetClassDesc() && GetRuntimeClass() == pObject->GetRuntimeClass())
	{
		CMeshObject *pBrush = (CMeshObject*)pObject;
		if ((CString)mv_geometryFile == (CString)pBrush->mv_geometryFile)
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CMeshObject::StartSubObjSelection( int elemType )
{
	bool bStarted = false;
	if (m_pGeometry)
	{
		if (m_pGeometry->GetUserCount() > 1)
		{
			CString str;
			str.Format( "Geometry File %s is used by multiple objects.\r\nDo you want to make an unique copy of this geometry?",
										(const char*)m_pGeometry->GetFilename() );
			int res = MessageBox( AfxGetMainWnd()->GetSafeHwnd(),str,"Warning",MB_ICONQUESTION|MB_YESNOCANCEL );
			if (res == IDCANCEL)
				return false;
			if (res == IDYES)
			{
				// Must make a copy of the geometry.
				m_pGeometry = (CEdMesh*)m_pGeometry->Clone();
				CString levelPath = Path::AddBackslash(GetIEditor()->GetLevelFolder());
				CString filename = levelPath+"Objects\\"+GetName()+"."+CRY_GEOMETRY_FILE_EXT;
				filename = Path::MakeGamePath( filename );
				m_pGeometry->SetFilename(filename);
				mv_geometryFile = filename;

				UpdateEngineNode();
				m_bNotSharedGeom = true;
			}
		}
		bStarted = m_pGeometry->StartSubObjSelection( GetWorldTM(),elemType,0 );
	}
	if (bStarted)
		SetFlags(OBJFLAG_SUBOBJ_EDITING);
	return bStarted;
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::EndSubObjectSelection()
{
	ClearFlags(OBJFLAG_SUBOBJ_EDITING);
	if (m_pGeometry)
	{
		m_pGeometry->EndSubObjSelection();
		UpdateEngineNode(true);
		if (m_pRenderNode)
			m_pRenderNode->Physicalize();
		m_pGeometry->GetBounds( m_bbox );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::CalculateSubObjectSelectionReferenceFrame( ISubObjectSelectionReferenceFrameCalculator* pCalculator )
{
	if (m_pGeometry)
	{
		Matrix34 refFrame;
		bool bAnySelected = m_pGeometry->GetSelectionReferenceFrame(refFrame);
		refFrame = this->GetWorldTM() * refFrame;
		pCalculator->SetExplicitFrame(bAnySelected, refFrame);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::ModifySubObjSelection( SSubObjSelectionModifyContext &modCtx )
{
	if (m_pGeometry)
		m_pGeometry->ModifySelection( modCtx );
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::AcceptSubObjectModify()
{
	if (m_pGeometry)
		m_pGeometry->AcceptModifySelection();
}

//////////////////////////////////////////////////////////////////////////
CEdGeometry* CMeshObject::GetGeometry()
{
	// Return our geometry.
	return m_pGeometry;
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::SaveToCGF( const CString &filename )
{
	if (m_pGeometry)
	{
		if (GetMaterial())
			m_pGeometry->SaveToCGF( filename,NULL,GetMaterial()->GetMatInfo() );
		else
			m_pGeometry->SaveToCGF( filename );
	}
	mv_geometryFile = Path::MakeGamePath(filename);
}

//////////////////////////////////////////////////////////////////////////
void CMeshObject::SetMeshGeometry( CEdMesh *pMesh )
{
	m_pGeometry = pMesh;
	m_pGeometry->GetBounds( m_bbox );
	UpdateEngineNode(false);
	InvalidateTM(0);
}

//////////////////////////////////////////////////////////////////////////
CEdMesh* CMeshObject::GetMeshGeometry() const
{
	return m_pGeometry;
}
