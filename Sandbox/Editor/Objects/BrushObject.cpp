////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   BrushObject.cpp
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: CBrushObject implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BrushObject.h"

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
IMPLEMENT_DYNCREATE(CBrushObject,CBaseObject)

namespace
{
	CBrushPanel* s_brushPanel = NULL;
	int s_brushPanelId = 0;

	CPanelTreeBrowser* s_treePanelPtr = NULL;
	int s_treePanelId = 0;
}


//////////////////////////////////////////////////////////////////////////
CBrushObject::CBrushObject()
{
	m_pGeometry = 0;
	m_pRenderNode = 0;

	m_renderFlags = 0;
	m_bNotSharedGeom = false;
	m_bbox.Reset();

	AddVariable( mv_geometryFile,"Prefab","Geometry",functor(*this,&CBrushObject::OnGeometryChange),IVariable::DT_OBJECT );

	UseMaterialLayersMask(true);

	// Init Variables.
	mv_outdoor = false;
//	mv_castShadows = false;
	//mv_selfShadowing = false;
  mv_castShadowMaps = false;
	mv_castScatterMaps = false;
  mv_registerByBBox = false;
	mv_castLightmap = false;
	mv_noIndirLight = false;
	mv_recvLightmap = false;
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
  static CString sVarName_CastScatterMaps = "CastScatterMaps";
	static CString sVarName_RegisterByBBox = "SupportSecondVisarea";
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
	AddVariable( mv_outdoor,sVarName_OutdoorOnly,functor(*this,&CBrushObject::OnRenderVarChange) );
//	AddVariable( mv_castShadows,sVarName_CastShadows,sVarName_CastShadows2,functor(*this,&CBrushObject::OnRenderVarChange) );
	//AddVariable( mv_selfShadowing,sVarName_SelfShadowing,functor(*this,&CBrushObject::OnRenderVarChange) );
	AddVariable( mv_castShadowMaps,sVarName_CastShadowMaps,functor(*this,&CBrushObject::OnRenderVarChange) );
	AddVariable( mv_castScatterMaps,sVarName_CastScatterMaps,functor(*this,&CBrushObject::OnRenderVarChange) );
	AddVariable( mv_registerByBBox,sVarName_RegisterByBBox,functor(*this,&CBrushObject::OnRenderVarChange) );

	AddVariable( mv_castLightmap,sVarName_CastLightmap,functor(*this,&CBrushObject::OnRenderVarChange) );
	AddVariable( mv_recvLightmap,sVarName_ReceiveLightmap,functor(*this,&CBrushObject::OnRenderVarChange) );
	AddVariable( mv_hideable,sVarName_Hideable,functor(*this,&CBrushObject::OnRenderVarChange) );
//	AddVariable( mv_hideableSecondary,sVarName_HideableSecondary,functor(*this,&CBrushObject::OnRenderVarChange) );
	AddVariable( mv_ratioLOD,sVarName_LodRatio,functor(*this,&CBrushObject::OnRenderVarChange) );
	AddVariable( mv_ratioViewDist,sVarName_ViewDistRatio,functor(*this,&CBrushObject::OnRenderVarChange) );
	AddVariable( mv_excludeFromTriangulation,sVarName_NotTriangulate,functor(*this,&CBrushObject::OnRenderVarChange) );
	AddVariable( mv_aiRadius,sVarName_AIRadius,functor(*this,&CBrushObject::OnAIRadiusVarChange) );
	AddVariable( mv_noDecals,sVarName_NoDecals,functor(*this,&CBrushObject::OnRenderVarChange) );
	AddVariable( mv_lightmapQuality,sVarName_LightmapQuality );
	AddVariable( mv_noIndirLight,sVarName_NoAmbShadowCaster );
  AddVariable( mv_recvWind, sVarName_RecvWind,functor(*this,&CBrushObject::OnRenderVarChange) );

  

	mv_ratioLOD.SetLimits( 0,255 );
	mv_ratioViewDist.SetLimits( 0,255 );

	m_bIgnoreNodeUpdate = false;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::Done()
{
	FreeGameData();

	CBaseObject::Done();
}

bool CBrushObject::IncludeForGI()
{ 
	return true; 
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::FreeGameData()
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
bool CBrushObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	SetColor( RGB(255,255,255) );
	
	if (IsCreateGameObjects())
	{
		if (prev)
		{
			CBrushObject *brushObj = (CBrushObject*)prev;
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
		CBrushObject *brushObj = (CBrushObject*)prev;
		m_bbox = brushObj->m_bbox;
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushObject::CreateGameObject()
{
	if (!m_pRenderNode)
	{
		uint32 ForceID =	GetObjectManager()->ForceID();
		m_pRenderNode = GetIEditor()->Get3DEngine()->CreateRenderNode( eERType_Brush );
		if(ForceID)
		{
			m_pRenderNode->SetEditorObjectId( ForceID++);
			GetObjectManager()->ForceID(ForceID);
		}
		else
			m_pRenderNode->SetEditorObjectId( GetId().Data1 );
		UpdateEngineNode();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::BeginEditParams( IEditor *ie,int flags )
{
	CBaseObject::BeginEditParams( ie,flags );

	if (!s_brushPanel)
	{
		s_brushPanel = new CBrushPanel;
		s_brushPanelId = AddUIPage( _T("Brush Parameters"),s_brushPanel );
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
				s_treePanelPtr->Create( functor(*this,&CBrushObject::OnFileChange),GetClassDesc()->GetFileSpec(),AfxGetMainWnd(),flags );
			}
			if (s_treePanelId == 0)
				s_treePanelId = AddUIPage( _T("Prefab"),s_treePanelPtr,false );
		}

		if (s_treePanelPtr)
		{
			s_treePanelPtr->SetSelectCallback( functor(*this,&CBrushObject::OnFileChange) );
			s_treePanelPtr->SelectFile( prefabName );
		}
	}

	if (s_brushPanel)
		s_brushPanel->SetBrush( this );
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::EndEditParams( IEditor *ie )
{
	CBaseObject::EndEditParams( ie );

	if (s_treePanelId != 0)
	{
		RemoveUIPage( s_treePanelId );
		s_treePanelId = 0;
	}

	if (s_brushPanelId != 0)
	{
		RemoveUIPage( s_brushPanelId );
		s_brushPanel = 0;
		s_brushPanelId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::BeginEditMultiSelParams( bool bAllOfSameType )
{
	CBaseObject::BeginEditMultiSelParams( bAllOfSameType );
	if (bAllOfSameType)
	{
		if (!s_brushPanel)
		{
			s_brushPanel = new CBrushPanel;
			s_brushPanelId = AddUIPage( _T("Brush Parameters"),s_brushPanel );
		}
		if (s_brushPanel)
			s_brushPanel->SetBrush(0);
	}
}
	
//////////////////////////////////////////////////////////////////////////
void CBrushObject::EndEditMultiSelParams()
{
	CBaseObject::EndEditMultiSelParams();
	if (s_brushPanel)
	{
		RemoveUIPage( s_brushPanelId );
		s_brushPanel = 0;
		s_brushPanelId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::OnFileChange( CString filename )
{
	CUndo undo("Brush Prefab Modify");
	StoreUndo( "Brush Prefab Modify" );
	mv_geometryFile = filename;

	// Update variables in UI.
	UpdateUIVars();
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::SetScale( const Vec3 &scale )
{
	// Ignore scale;
	CBaseObject::SetScale( scale );
};

//////////////////////////////////////////////////////////////////////////
void CBrushObject::SetSelected( bool bSelect )
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
void CBrushObject::GetLocalBounds( BBox &box )
{
	box = m_bbox;
}

//////////////////////////////////////////////////////////////////////////
int CBrushObject::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
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
void CBrushObject::Display( DisplayContext &dc )
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

		IStatObj *pStatObj = GetIStatObj();
		if (pStatObj)
		{
			IRenderMesh *pRenderMesh = pStatObj->GetRenderMesh();
			//if (pRenderMesh)
				//pRenderMesh->DebugDraw( dc.GetMatrix()*GetWorldTM(),1 );
		}

		/*
		IStatObj *pObj = m_pGeometry->GetGeometry();
		if (pObj)
		{
			SRendParams rp;
			rp.vPos = Vec3(0,0,0);
			rp.vAngles = Vec3(0,0,0);
			rp.nDLightMask = 0x3;
			rp.AmbientColor = ColorF(1,1,1,1);
			rp.dwFObjFlags |= FOB_TRANS_MASK;
			pObj->Render( rp );
		}
		*/
		
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
XmlNodeRef CBrushObject::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::Serialize( CObjectArchive &ar )
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
bool CBrushObject::HitTest( HitContext &hc )
{
	if (CheckFlags(OBJFLAG_SUBOBJ_EDITING))
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
int CBrushObject::HitTestAxis( HitContext &hc )
{
	//@HACK Temporary hack.
	return 0;
}

//////////////////////////////////////////////////////////////////////////
//! Invalidates cached transformation matrix.
void CBrushObject::InvalidateTM( int nWhyFlags )
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
void CBrushObject::OnEvent( ObjectEvent event )
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
void CBrushObject::WorldToLocalRay( Vec3 &raySrc,Vec3 &rayDir )
{
	raySrc = m_invertTM.TransformPoint( raySrc );
	rayDir = m_invertTM.TransformVector(rayDir).GetNormalized();
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::OnGeometryChange( IVariable *var )
{
	// Load new prefab model.
	CString objName = mv_geometryFile;

	CreateBrushFromPrefab( objName );
	InvalidateTM(0);
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::CreateBrushFromPrefab( const char *meshFilname )
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
		UpdateEngineNode();
	}
	else if (m_pRenderNode)
	{
		// Remove this object from engine node.
		m_pRenderNode->SetEntityStatObj( 0,0,0 );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::ReloadGeometry()
{
	if (m_pGeometry)
	{
		m_pGeometry->ReloadGeometry();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::OnAIRadiusVarChange( IVariable *var )
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
					CBrushObject *obj = (CBrushObject*)pBase;
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
void CBrushObject::OnRenderVarChange( IVariable *var )
{
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
IPhysicalEntity* CBrushObject::GetCollisionEntity() const
{
	// Returns physical object of entity.
	if (m_pRenderNode)
		return m_pRenderNode->GetPhysics();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushObject::ConvertFromObject( CBaseObject *object )
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
		mv_castScatterMaps = entity->IsCastScatter();
		mv_registerByBBox = false;
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
void CBrushObject::UpdateEngineNode( bool bOnlyTransform )
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
	if (mv_castScatterMaps)
		m_renderFlags |= ERF_CASTSCATTERMAPS;
	if (mv_registerByBBox)
		m_renderFlags |= ERF_REGISTER_BY_BBOX;
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

	if (m_pRenderNode->GetRndFlags() & ERF_COLLISION_PROXY)
		m_renderFlags |= ERF_COLLISION_PROXY;

	if (IsSelected() && gSettings.viewports.bHighlightSelectedGeometry)
		m_renderFlags |= ERF_SELECTED;

	int flags = GetRenderFlags();

	int mergeFlags = (ERF_MERGE_RESULT  | ERF_SUBSURFSCATTER) & m_pRenderNode->GetRndFlags();
	m_pRenderNode->SetRndFlags( m_renderFlags | mergeFlags );

	m_pRenderNode->SetMinSpec( GetMinSpec() );
	m_pRenderNode->SetViewDistRatio( mv_ratioViewDist );
	m_pRenderNode->SetLodRatio( mv_ratioLOD );

	m_renderFlags = m_pRenderNode->GetRndFlags();

  m_pRenderNode->SetMaterialLayers( GetMaterialLayersMask() );

	if (m_pGeometry)
	{
		m_pGeometry->GetBounds( m_bbox );

		Matrix34 tm = GetWorldTM();
		m_pRenderNode->SetEntityStatObj( 0,m_pGeometry->GetGeometry(),&tm );
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
IStatObj* CBrushObject::GetIStatObj() const
{
	if (!m_pGeometry)
		return 0;
	return m_pGeometry->GetGeometry();
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::SetMinSpec( uint32 nSpec )
{
	__super::SetMinSpec(nSpec);
	if (m_pRenderNode)
	{
		m_pRenderNode->SetMinSpec( GetMinSpec() );
		m_renderFlags = m_pRenderNode->GetRndFlags();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::UpdateVisibility( bool visible )
{
	if (visible == CheckFlags(OBJFLAG_INVISIBLE))
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
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::SetMaterial( CMaterial *mtl )
{
	CBaseObject::SetMaterial(mtl);
	if (m_pRenderNode)
		UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CBrushObject::GetRenderMaterial() const
{
	if (GetMaterial())
		return GetMaterial();
	if (m_pGeometry)
		return GetIEditor()->GetMaterialManager()->FromIMaterial(m_pGeometry->GetGeometry()->GetMaterial());
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::SetMaterialLayersMask( uint32 nLayersMask )
{
	CBaseObject::SetMaterialLayersMask(nLayersMask);
	UpdateEngineNode(false);
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::Validate( CErrorReport *report )
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
bool CBrushObject::IsSimilarObject( CBaseObject *pObject )
{
	if (pObject->GetClassDesc() == GetClassDesc() && GetRuntimeClass() == pObject->GetRuntimeClass())
	{
		CBrushObject *pBrush = (CBrushObject*)pObject;
		if ((CString)mv_geometryFile == (CString)pBrush->mv_geometryFile)
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushObject::StartSubObjSelection( int elemType )
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
void CBrushObject::EndSubObjectSelection()
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
void CBrushObject::CalculateSubObjectSelectionReferenceFrame( ISubObjectSelectionReferenceFrameCalculator* pCalculator )
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
void CBrushObject::ModifySubObjSelection( SSubObjSelectionModifyContext &modCtx )
{
	if (m_pGeometry)
		m_pGeometry->ModifySelection( modCtx );
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::AcceptSubObjectModify()
{
	if (m_pGeometry)
		m_pGeometry->AcceptModifySelection();
}

//////////////////////////////////////////////////////////////////////////
CEdGeometry* CBrushObject::GetGeometry()
{
	// Return our geometry.
	return m_pGeometry;
}

//////////////////////////////////////////////////////////////////////////
void CBrushObject::SaveToCGF( const CString &filename )
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
