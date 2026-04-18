////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   SolidBrushObject.cpp
//  Version:     v1.00
//  Created:     16/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SolidBrushObject.h"

#include "Objects\SubObjSelection.h"

#include "Objects\ObjectManager.h"
#include "Viewport.h"

#include "SolidBrushCreatePanel.h"
#include "SolidBrushPanel.h"
#include "SolidBrushSubObjPanel.h"
#include "Brush.h"

#include "Material\Material.h"
#include "Settings.h"
#include "PropertiesPanel.h"
#include "ISubObjectSelectionReferenceFrameCalculator.h"

#include <I3Dengine.h>
#include <IEntitySystem.h>
#include <IEntityRenderState.h>
#include <IPhysics.h>

#include <GameEngine.h>

#define MIN_BOUNDS_SIZE 0.01f

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CSolidBrushObject,CBaseObject)

//////////////////////////////////////////////////////////////////////////
class CSolidFlagsPanelUI : public CPropertiesPanel
{
public:
	_smart_ptr<CVarBlock> pVarBlock;
	_smart_ptr<CSolidBrushObject> m_pObject;
	
	CSmartVariable<bool> mv_outdoor;
//	CSmartVariable<bool> mv_selfShadowing;
	CSmartVariable<bool> mv_castShadows;
//	CSmartVariable<bool> mv_recvShadows;
	CSmartVariable<bool> mv_castLightmap;
	CSmartVariable<bool> mv_recvLightmap;
//	CSmartVariable<bool> mv_sky;
	CSmartVariable<bool> mv_hideable;
	//CSmartVariable<int> mv_ratioLOD;
	CSmartVariable<int> mv_ratioViewDist;
	CSmartVariable<bool> mv_excludeFromTriangulation;
	CSmartVariable<bool> mv_noStaticDecals;
	CSmartVariable<float> mv_lightmapQuality;

	CSolidFlagsPanelUI()
	{
		pVarBlock = new CVarBlock;

		mv_outdoor = false;
		mv_castShadows = false;
//		mv_selfShadowing = false;
//		mv_recvShadows = false;
		mv_castLightmap = false;
		mv_recvLightmap = false;
		mv_hideable = false;
//		mv_sky = false;
		//mv_ratioLOD = 100;
		mv_ratioViewDist = 100;
		mv_excludeFromTriangulation = false;
		mv_lightmapQuality = 1;
		mv_lightmapQuality->SetLimits( 0,100 );
		//mv_ratioLOD->SetLimits( 0,255 );
		mv_ratioViewDist->SetLimits( 0,255 );

//		pVarBlock->AddVariable( mv_selfShadowing,"Self Shadowing" );
		pVarBlock->AddVariable( mv_castShadows,"Cast Shadows" );
//		pVarBlock->AddVariable( mv_recvShadows,"Recieve Shadows" );
		pVarBlock->AddVariable( mv_castLightmap,"CastRAMmaps" );
		pVarBlock->AddVariable( mv_recvLightmap,"RecieveRAMmaps" );
		pVarBlock->AddVariable( mv_outdoor,"Outdoors Only" );
		//		pVarBlock->AddVariable( mv_sky,"Sky Object" );
		//pVarBlock->AddVariable( mv_ratioLOD,"LOD Ratio" );
		pVarBlock->AddVariable( mv_ratioViewDist,"View Distance Ratio" );
		pVarBlock->AddVariable( mv_lightmapQuality,"RAMmapQuality" );
		pVarBlock->AddVariable( mv_excludeFromTriangulation,"AI Exclude From Triangulation" );
		pVarBlock->AddVariable( mv_hideable,"AI Hideable" );
		pVarBlock->AddVariable( mv_noStaticDecals,"No Static Decals" );
	}
	void AddVariables()
	{
		SetVarBlock( pVarBlock,functor(*this,&CSolidFlagsPanelUI::OnVarChange) );
	}
	void SetObject( CSolidBrushObject *pObject )
	{
		m_pObject = pObject;
		if (pObject)
		{
			DeleteVars();

			mv_lightmapQuality = pObject->GetLMQuality();
			mv_ratioViewDist = pObject->GetViewDistRatio();

			int flags = pObject->GetRenderFlags();
			mv_outdoor = (flags&ERF_OUTDOORONLY) != 0;
//			mv_sky = 	(flags&ERF_SKY_OBJECT) != 0;
//			mv_selfShadowing = (flags&ERF_SELFSHADOW) != 0;
			mv_castShadows = 	(flags&ERF_CASTSHADOWMAPS) != 0;
//			mv_recvShadows = 	(flags&ERF_RECVSHADOWMAPS) != 0;
			mv_castLightmap = (flags&ERF_CASTSHADOWINTORAMMAP) != 0;
			mv_recvLightmap = (flags&ERF_USERAMMAPS) != 0;
			mv_hideable = (flags&ERF_HIDABLE) != 0;
			mv_noStaticDecals = (flags&ERF_NO_DECALNODE_DECALS) != 0;
			mv_excludeFromTriangulation = (flags&ERF_EXCLUDE_FROM_TRIANGULATION) != 0;

			AddVariables();
		}
	}
	void ModifyFlag( int &nFlags,int flag,CSmartVariable<bool> &var,IVariable *pVar )
	{
		if (var.GetVar() == pVar)
			nFlags = (var) ? (nFlags | flag) : (nFlags & (~flag));
	}
	void OnVarChange( IVariable *pVar )
	{
		CSelectionGroup *selection = GetIEditor()->GetSelection();
		for (int i = 0; i < selection->GetCount(); i++)
		{
			CBaseObject *pObj = selection->GetObject(i);
			if (pObj->IsKindOf(RUNTIME_CLASS(CSolidBrushObject)))
			{
				CSolidBrushObject *pSolid = (CSolidBrushObject*)pObj;
				int nFlags = pSolid->GetRenderFlags();
				ModifyFlag( nFlags,ERF_OUTDOORONLY,mv_outdoor,pVar );
				ModifyFlag( nFlags,ERF_CASTSHADOWMAPS,mv_castShadows,pVar );
//				ModifyFlag( nFlags,ERF_RECVSHADOWMAPS,mv_recvShadows,pVar );
				ModifyFlag( nFlags,ERF_CASTSHADOWINTORAMMAP,mv_castLightmap,pVar );
				ModifyFlag( nFlags,ERF_USERAMMAPS,mv_recvLightmap,pVar );
				ModifyFlag( nFlags,ERF_HIDABLE,mv_hideable,pVar );
				ModifyFlag( nFlags,ERF_EXCLUDE_FROM_TRIANGULATION,mv_excludeFromTriangulation,pVar );
				ModifyFlag( nFlags,ERF_NO_DECALNODE_DECALS,mv_noStaticDecals,pVar );
				pSolid->SetRenderFlags( nFlags );

				if (mv_lightmapQuality.GetVar() == pVar)
					pSolid->SetLMQuality( mv_lightmapQuality );
				if (mv_ratioViewDist.GetVar() == pVar)
					pSolid->SetViewDistRatio( mv_ratioViewDist );

				// need to do this to apply ERF_EXCLUDE_FROM_TRIANGULATION flag
				SBrush *pBrush(	pSolid->GetBrush() );
				if (pBrush)
				{
					IStatObj *pGeom = pBrush->GetIStatObj();
					if (pGeom)
					{
						CString sPath = GetIEditor()->GetGameEngine()->GetLevelPath();
						CString guidStr = GuidUtil::ToString(pSolid->GetId());
						CString sRealGeomFileName = Path::AddBackslash(sPath) + "Brush\\" + pSolid->GetName() + "_" + guidStr + "." + CRY_GEOMETRY_FILE_EXT;
						pGeom->SetFilePath(sRealGeomFileName);
						pSolid->GetEngineNode()->SetEntityStatObj( 0,pGeom,&pSolid->GetWorldTM() );
					}
				}
			}
		}
	}
};

namespace
{
	CSolidBrushPanel* s_brushPanel = NULL;
	int s_brushPanelId = 0;

	CSolidBrushCreatePanel* s_brushPanelCreate = NULL;
	int s_brushPanelCreateId = 0;

	CSolidFlagsPanelUI* s_brushPanelFlags = NULL;
	int s_brushPanelFlagsId = 0;

	CSolidBrushSubObjPanel* s_brushPanelSubObj = NULL;
	int s_brushPanelSubObjId = 0;
}


//////////////////////////////////////////////////////////////////////////
class CSolidBrushMouseCreateCallback : public IMouseCreateCallback
{
	CSolidBrushObject *m_pObject;
	CPoint m_mouseDownPos;
	Vec3 p0,p1,p2;
	int m_mode;

public:
	//////////////////////////////////////////////////////////////////////////
	CSolidBrushMouseCreateCallback( CSolidBrushObject *pObject )
	{
		m_mode = 0;
		m_pObject = pObject;
	}

	virtual	void Release() { delete this; };
	virtual	bool ContinueCreation() { return true; };

	//////////////////////////////////////////////////////////////////////////
	virtual	MouseCreateResult OnMouseEvent( CViewport *view,EMouseEvent event,CPoint &point,int flags )
	{
		if (event == eMouseRDblClick)
			return MOUSECREATE_ABORT;
		if (event == eMouseMove || event == eMouseLDown || event == eMouseLUp)
		{
			if (m_mode == 0)
			{
				p0 = view->MapViewToCP( point );
				m_pObject->SetPos( p0 );
				if (event == eMouseLDown)
				{
					view->SetConstructionOrigin(p0);
					m_mouseDownPos = point;
					m_mode = 1;
				}
				return MOUSECREATE_CONTINUE;
			}
			if (m_mode == 1)
			{
				p0 = view->MapViewToCP( m_mouseDownPos,AXIS_XY );
				p1 = view->MapViewToCP( point,AXIS_XY );
				p1.z = p0.z + 0.01f;
				if (event == eMouseLUp)
				{
					m_mouseDownPos = point;
					view->SetConstructionOrigin(p1);
					m_mode = 2;
				}
			}
			if (m_mode == 2)
			{
				Vec3 src = view->MapViewToCP( m_mouseDownPos,AXIS_Z );
				Vec3 trg = view->MapViewToCP( point,AXIS_Z );
				Vec3 dir = view->GetCPVector(src,trg,AXIS_Z);
				p1.z = p0.z + (dir.z);
				p1 = view->SnapToGrid( p1 );
			}
			BBox brushBox;
			brushBox.Reset();
			brushBox.Add( p0 );
			brushBox.Add( p1 );

			bool bSolidValid = false;
			// If width or height or depth are zero.
			if (fabs(brushBox.min.x-brushBox.max.x) > 0.0001f &&
					fabs(brushBox.min.y-brushBox.max.y) > 0.0001f &&
					fabs(brushBox.min.z-brushBox.max.z) > 0.0001f)
			{
				Vec3 center = (brushBox.min + brushBox.max)/2.0f;
				brushBox.min -= center;
				brushBox.max -= center;

				m_pObject->SetPos( center );
				bSolidValid = m_pObject->CreateBrush( brushBox,CSolidBrushObject::m_nCreateType,CSolidBrushObject::m_nCreateNumSides );
			}

			if (event == eMouseLDown && m_mode == 2)
			{
				if (bSolidValid)
					return MOUSECREATE_OK;
				else
					return MOUSECREATE_ABORT;
			}
		}
		return MOUSECREATE_CONTINUE;
	}
};

//////////////////////////////////////////////////////////////////////////
// CSolidBrushObject implementation.
//////////////////////////////////////////////////////////////////////////
int CSolidBrushObject::m_nCreateNumSides = 4;
ESolidBrushCreateType CSolidBrushObject::m_nCreateType = BRUSH_CREATE_TYPE_BOX;

CSolidBrushObject::CSolidBrushObject()
{
	m_pRenderNode = 0;
	m_renderFlags = 0;

	m_viewDistRatio = 100;
	m_lightmapQuality = 1;

	SetColor( RGB(0,255,255) );
	m_bbox.min.Set(0,0,0);
	m_bbox.max.Set(0,0,0);

	m_bIgnoreNodeUpdate = false;

	UseMaterialLayersMask(true);
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::Done()
{
	if (m_pRenderNode)
	{
		if(m_pRenderNode->GetRenderNodeType()>=0)
			GetIEditor()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
		m_pRenderNode = 0;
	}

	//free brush.
	m_brush = 0;

	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CSolidBrushObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	if (prev)
	{
		CSolidBrushObject *brushObj = (CSolidBrushObject*)prev;
		if (brushObj->GetBrush())
		{
			SetBrush( brushObj->GetBrush()->Clone(true) );
		}
	}

	// Must be after SetBrush call.
	bool res = CBaseObject::Init( ie,prev,file );	
	if (prev)
	{
		CSolidBrushObject *brushObj = (CSolidBrushObject*)prev;
		m_bbox = brushObj->m_bbox;
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CSolidBrushObject::CreateGameObject()
{
	if (!m_pRenderNode)
	{
		m_pRenderNode = GetIEditor()->Get3DEngine()->CreateRenderNode( eERType_Brush );
		m_pRenderNode->SetEditorObjectId( GetId().Data1 );
		//OnRenderVarChange(0);
		UpdateEngineNode();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::BeginEditParams( IEditor *ie,int flags )
{
	CBaseObject::BeginEditParams( ie,flags );

	if (flags & OBJECT_CREATE)
	{
		if (!s_brushPanelCreate)
		{
			s_brushPanelCreate = new CSolidBrushCreatePanel();
			s_brushPanelCreateId = ie->AddRollUpPage( ROLLUP_OBJECTS,_T("Create Brush Parameters"),s_brushPanelCreate );
		}
	}

	if (!s_brushPanelFlags)
	{
		s_brushPanelFlags = new CSolidFlagsPanelUI();
		s_brushPanelFlagsId = ie->AddRollUpPage( ROLLUP_OBJECTS,_T("Geometry Flags"),s_brushPanelFlags );
	}
	s_brushPanelFlags->SetMultiSelect(false);
	s_brushPanelFlags->SetObject(this);

	if (!s_brushPanel)
	{
		s_brushPanel = new CSolidBrushPanel();
		s_brushPanelId = ie->AddRollUpPage( ROLLUP_OBJECTS,_T("Solid Parameters"),s_brushPanel );
	}
	if (s_brushPanel)
	{
		s_brushPanel->SetBrush( this );
	}

}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::EndEditParams( IEditor *ie )
{
	CBaseObject::EndEditParams( ie );

	if (s_brushPanelCreate)
	{
		ie->RemoveRollUpPage( ROLLUP_OBJECTS,s_brushPanelCreateId );
		s_brushPanelCreate = 0;
		s_brushPanelCreateId = 0;
	}

	if (s_brushPanel)
	{
		ie->RemoveRollUpPage( ROLLUP_OBJECTS,s_brushPanelId );
		s_brushPanel = 0;
		s_brushPanelId = 0;
	}

	if (s_brushPanelFlags)
	{
		ie->RemoveRollUpPage( ROLLUP_OBJECTS,s_brushPanelFlagsId );
		s_brushPanelFlags = 0;
		s_brushPanelFlagsId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::BeginEditMultiSelParams( bool bAllOfSameType )
{
	CBaseObject::BeginEditMultiSelParams( bAllOfSameType );
	if (bAllOfSameType)
	{
		if (!s_brushPanelFlags)
		{
			s_brushPanelFlags = new CSolidFlagsPanelUI();
			s_brushPanelFlags->AddVariables();
			s_brushPanelFlagsId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,_T("Geometry Flags"),s_brushPanelFlags );
		}
		s_brushPanelFlags->SetMultiSelect(true);

		if (!s_brushPanel)
		{
			s_brushPanel = new CSolidBrushPanel;
			s_brushPanelId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,_T("Solid Parameters"),s_brushPanel );
		}
		if (s_brushPanel)
		{
			s_brushPanel->SetBrush( 0 );
		}
	}
}
	
//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::EndEditMultiSelParams()
{
	CBaseObject::EndEditMultiSelParams();

	if (s_brushPanelFlags)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,s_brushPanelFlagsId );
		s_brushPanelFlags = 0;
		s_brushPanelFlagsId = 0;
	}
	if (s_brushPanel)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,s_brushPanelId );
		s_brushPanel = 0;
		s_brushPanelId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::SetSelected( bool bSelect )
{
	CBaseObject::SetSelected( bSelect );

	if (m_pRenderNode)
	{
		if (bSelect && gSettings.viewports.bHighlightSelectedGeometry)
			m_renderFlags |= ERF_SELECTED;
		else
			m_renderFlags &= ~ERF_SELECTED;
		m_pRenderNode->SetRndFlags( m_renderFlags );
	}
	if (m_brush)
		m_brush->SetSelected( bSelect );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::GetBoundBox( BBox &box )
{
	box.SetTransformedAABB( GetWorldTM(),m_bbox );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::GetLocalBounds( BBox &box )
{
	box = m_bbox;
}

//////////////////////////////////////////////////////////////////////////
IMouseCreateCallback* CSolidBrushObject::GetMouseCreateCallback()
{
	return new CSolidBrushMouseCreateCallback(this);
};

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::Display( DisplayContext &dc )
{
	if (dc.flags & DISPLAY_2D)
	{
		if (IsSelected())
			//dc.SetSelectedColor();
			dc.SetColor( RGB(225,0,0) );
		else
			dc.SetColor( GetColor() );
		if (m_brush)
			m_brush->Display( dc );
		return;
	}
	else
	{
		//dc.SetColor( GetColor() );
		//dc.PushMatrix( GetWorldTM() );
		//dc.DrawWireBox( m_bbox.min,m_bbox.max );
		//dc.PopMatrix();
	}

	dc.SetColor( GetColor() );
	if (m_brush)
		m_brush->Display( dc );
 
	/*
	if (m_brush != 0 && m_indoor != 0)
	{
		if (!(m_brush->m_flags&BRF_RE_VALID))
		{
			m_indoor->UpdateObject( m_brush->GetIndoorGeom() );
			m_indoor->RecalcBounds();
		}
		Vec3 invCamSrc = dc.camera->GetPos();
		invCamSrc = m_invertTM.TransformPoint(invCamSrc);

		m_brush->Render( dc.renderer,invCamSrc )
	}
	*/
	
	if (IsSelected() && !m_bbox.IsEmpty())
	{
		dc.SetSelectedColor();
		dc.PushMatrix( GetWorldTM() );
		dc.DrawWireBox( m_bbox.min,m_bbox.max );
		//dc.SetColor( RGB(255,255,0),0.1f ); // Yellow selected color.
		//dc.DrawSolidBox( m_bbox.min,m_bbox.max );
		
		float fScreenScale = dc.view->GetScreenScaleFactor(GetWorldTM().GetTranslation());
		
		// Display bounding box dimensions.
		float ofs = 0.02f * fScreenScale;
		Vec3 b0(m_bbox.min.x,m_bbox.max.y,m_bbox.min.z);
		Vec3 b1(m_bbox.max.x,m_bbox.max.y,m_bbox.min.z);
		Vec3 b2(m_bbox.max.x,m_bbox.min.y,m_bbox.min.z);
		Vec3 b3(m_bbox.max.x,m_bbox.max.y,m_bbox.max.z);
		Vec3 p0 = b0 + Vec3(0,ofs,0);
		Vec3 p1 = b1 + Vec3(ofs,ofs,0);
		Vec3 p2 = b2 + Vec3(ofs,0,0);
		Vec3 p3 = b3 + Vec3(ofs,ofs,0);

		float fArrowScale = 0.02f * fScreenScale;

		dc.SetColor( RGB(255,255,255),0.3f );
		dc.DrawArrow(p0,p1,fArrowScale,true);
		dc.DrawArrow(p1,p2,fArrowScale,true);
		dc.DrawArrow(p1,p3,fArrowScale,true);

		BBox box;
		GetBoundBox( box );

		dc.SetColor( RGB(200,200,200) );
		char str[32];
		sprintf( str,"%.2f",box.max.x-box.min.x );
		dc.DrawTextLabel( (p0+p1)*0.5f,1.1f,str );
		sprintf( str,"%.2f",box.max.y-box.min.y );
		dc.DrawTextLabel( (p1+p2)*0.5f,1.1f,str );
		sprintf( str,"%.2f",box.max.z-box.min.z );
		dc.DrawTextLabel( (p1+p3)*0.5f,1.1f,str );
		
		dc.PopMatrix();
	}
	
	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CSolidBrushObject::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::Serialize( CObjectArchive &ar )
{
	XmlNodeRef xmlNode = ar.node;
	m_bIgnoreNodeUpdate = true;
	CBaseObject::Serialize( ar );
	m_bIgnoreNodeUpdate = false;
	if (ar.bLoading)
	{
		ar.node->getAttr( "RndFlags",m_renderFlags );
		ar.node->getAttr( "ViewDistRatio",m_viewDistRatio );
		ar.node->getAttr( "LMQuality",m_lightmapQuality );
		XmlNodeRef brushNode = xmlNode->findChild( "Brush" );
		if (brushNode)
		{
			SBrush *brush = m_brush;
			if (!brush)
				brush = new SBrush;
			brush->Serialize( brushNode,ar.bLoading );

			if (!m_brush)
				SetBrush( brush );
		}
		UpdateEngineNode();

		if (ar.bUndo)
			InvalidateBrush(false);
	}
	else
	{
		ar.node->setAttr( "RndFlags",m_renderFlags );
		ar.node->setAttr( "ViewDistRatio",m_viewDistRatio );
		ar.node->setAttr( "LMQuality",m_lightmapQuality );
		if (m_brush)
		{
			XmlNodeRef brushNode = xmlNode->newChild( "Brush" );
			m_brush->Serialize( brushNode,ar.bLoading );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSolidBrushObject::HitTest( HitContext &hc )
{
	if (!m_brush)
		return false;

	m_brush->HitTest( hc );
	Vec3 pnt;
	
	Vec3 localRaySrc = hc.raySrc;
	Vec3 localRayDir = hc.rayDir;
	WorldToLocalRay( localRaySrc,localRayDir );

	bool bHit = false;

	if (m_bbox.IsIntersectRay( localRaySrc,localRayDir,pnt ))
	{
		Vec3 prevSrc = hc.raySrc;
		Vec3 prevDir = hc.rayDir;
		hc.raySrc = localRaySrc;
		hc.rayDir = localRayDir;

		if (m_brush)
			bHit = m_brush->HitTest(hc);
		
		hc.raySrc = prevSrc;
		hc.rayDir = prevDir;
	}
	return bHit;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::SetBrush( SBrush *brush )
{
	if (m_brush == brush)
		return;

	m_brush = brush;
	if (m_brush)
	{
		m_brush->SetMatrix( GetWorldTM() );
		UpdateEngineNode();
		m_bbox = m_brush->m_bounds;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::InvalidateBrush( bool bBuildSolid )
{
	if (m_brush)
	{
		m_brush->SetMatrix( GetWorldTM() );
		if (bBuildSolid)
			m_brush->BuildSolid();
		m_brush->GetBounds( m_bbox );
		UpdateEngineNode();
		if (m_pRenderNode)
			m_pRenderNode->Physicalize();
	}
}
	
//! Retrieve brush assigned to object.
SBrush* CSolidBrushObject::GetBrush()
{
	return m_brush;
}

//////////////////////////////////////////////////////////////////////////
//! Invalidates cached transformation matrix.
void CSolidBrushObject::InvalidateTM( int nWhyFlags )
{
	CBaseObject::InvalidateTM(nWhyFlags);

	if (m_brush)
	{
		m_brush->SetMatrix( GetWorldTM() );
	}
	if (m_pRenderNode)
		UpdateEngineNode(true);
	
	m_invertTM = GetWorldTM();
	m_invertTM.Invert();
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::WorldToLocalRay( Vec3 &raySrc,Vec3 &rayDir )
{
	raySrc = m_invertTM.TransformPoint( raySrc );
	rayDir = m_invertTM.TransformVector(rayDir).GetNormalized();
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::SelectBrushSide( const Vec3 &raySrc,const Vec3 &rayDir,bool shear )
{
	if (!m_brush)
		return;

	m_subSelection.Clear();

	Vec3 rSrc = raySrc;
	Vec3 rDir = rayDir;
	Vec3 rTrg;
	WorldToLocalRay( rSrc,rDir );
	rTrg = rSrc + rDir*32768.0f;

	m_brush->SelectSide( rSrc,rDir,shear,m_subSelection );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::MoveSelectedPoints( const Vec3 &worldOffset )
{
	if (!m_brush)
		return;

	// Store undo.
	StoreUndo( "Stretch Brush" );

	BBox prevBounds = m_brush->m_bounds;

	std::vector<Vec3> prevPoints;
	prevPoints.resize( m_subSelection.points.size() );

	const Matrix34 &tm = GetWorldTM();

	//CHANGED_BY_IVO
	Vec3 ofs = m_invertTM.TransformVector( worldOffset );

	for (int i = 0; i < m_subSelection.points.size(); i++)
	{
		Vec3 pnt = *m_subSelection.points[i];
		prevPoints[i] = pnt; // Remember previous point.
		pnt = m_invertTM.TransformPoint(pnt) + ofs;
		*m_subSelection.points[i] = tm.TransformPoint(pnt);
	}
	
	if (m_brush->BuildSolid(false))
	{
		// Now optimize brush if it correctly created.
		// Now move brush. to make position center of the brush again.
		//Vec3 halfSize = (m_brush->m_bounds.max - m_brush->m_bounds.min)/2;
		Vec3 prevMid = (prevBounds.max + prevBounds.min)/2;
		Vec3 mid = (m_brush->m_bounds.max + m_brush->m_bounds.min)/2;
		Vec3 ofs = mid - prevMid;
		m_brush->Move( -ofs );

		SetPos( GetPos() + GetWorldTM().TransformVector(ofs) );

		m_bbox = m_brush->m_bounds;
	}
	else
	{
		// Restore previous brush.
		for (int i = 0; i < m_subSelection.points.size(); i++)
		{
			*m_subSelection.points[i] = prevPoints[i];
		}
		m_brush->BuildSolid(false);
		CLogFile::WriteLine( "Invalid brush, operation aborted." );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSolidBrushObject::CreateBrush( const AABB &bbox,ESolidBrushCreateType createType,int numSides )
{
	bool bSolidValid = false;
	if (!IsEquivalent(bbox.min,bbox.max))
	{
		_smart_ptr<SBrush> pBrush = new SBrush;
		pBrush->Create( bbox.min,bbox.max,numSides );
		bSolidValid = pBrush->BuildSolid();
		SetBrush( (bSolidValid) ? pBrush : NULL );
		if (bSolidValid)
			m_bbox = pBrush->m_bounds;
	}
	return bSolidValid;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::OnRenderVarChange( IVariable *var )
{
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
IPhysicalEntity* CSolidBrushObject::GetCollisionEntity() const
{
	// Returns physical object of entity.
	if (m_pRenderNode)
		return m_pRenderNode->GetPhysics();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CSolidBrushObject::ConvertFromObject( CBaseObject *object )
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::SetRenderFlags( int nRndFlags )
{
	m_renderFlags = nRndFlags;
	if (m_pRenderNode)
		m_pRenderNode->SetRndFlags( m_renderFlags );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::SetViewDistRatio( int nRatio )
{
	m_viewDistRatio = nRatio;
	if (m_pRenderNode)
		m_pRenderNode->SetViewDistRatio( m_viewDistRatio );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::UpdateEngineNode( bool bOnlyTransform )
{
	if (m_bIgnoreNodeUpdate)
		return;

	if (!m_pRenderNode)
		return;

	if (IsSelected() && gSettings.viewports.bHighlightSelectedGeometry)
		m_renderFlags |= ERF_SELECTED;
	else
		m_renderFlags &= ~ERF_SELECTED;

	m_pRenderNode->SetRndFlags( m_renderFlags );
	m_pRenderNode->SetViewDistRatio( m_viewDistRatio );
	m_pRenderNode->SetMinSpec( GetMinSpec() );
	m_pRenderNode->SetMaterialLayers( GetMaterialLayersMask() );
	m_renderFlags = m_pRenderNode->GetRndFlags();

	/*
	if (m_prefabGeom)
	{
		Matrix34 tm = GetBrushMatrix();
		m_pRenderNode->SetEntityStatObj( 0,m_prefabGeom->GetGeometry(),&tm );
	}
*/

	if (m_brush)
	{
		IStatObj *pGeom = m_brush->GetIStatObj();
		if (pGeom)
		{
			CString sPath = GetIEditor()->GetGameEngine()->GetLevelPath();
			CString guidStr = GuidUtil::ToString(GetId());
			CString sRealGeomFileName = Path::AddBackslash(sPath) + "Brush\\" + GetName() + "_" + guidStr + "." + CRY_GEOMETRY_FILE_EXT;
			pGeom->SetFilePath(sRealGeomFileName);
			m_pRenderNode->SetEntityStatObj( 0,pGeom,&GetWorldTM() );
		}
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
	// Set material can change render node flags.
	m_renderFlags = m_pRenderNode->GetRndFlags();
}

//////////////////////////////////////////////////////////////////////////
Matrix34 CSolidBrushObject::GetBrushMatrix() const
{
	return GetWorldTM();
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::UpdateVisibility( bool visible )
{
	CBaseObject::UpdateVisibility( visible );
	if (m_pRenderNode)
	{
		if (!visible)
			m_renderFlags |= ERF_HIDDEN;
		else
			m_renderFlags &= ~ERF_HIDDEN;
		m_pRenderNode->SetRndFlags( m_renderFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::SetMaterial( CMaterial *mtl )
{
	CBaseObject::SetMaterial(mtl);
	if (m_pRenderNode)
		UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::SetMaterialLayersMask( uint32 nLayersMask )
{
	CBaseObject::SetMaterialLayersMask(nLayersMask);
	if (m_pRenderNode)
		UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::SetMinSpec( uint32 nSpec )
{
	__super::SetMinSpec(nSpec);
	if (m_pRenderNode)
	{
		m_pRenderNode->SetMinSpec( GetMinSpec() );
		m_renderFlags = m_pRenderNode->GetRndFlags();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::Validate( CErrorReport *report )
{
	CBaseObject::Validate( report );

	if (!m_brush)
	{
		CErrorRecord err;
		err.error.Format( "Empty Solid Brush %s",(const char*)GetName() );
		err.pObject = this;
		report->ReportError(err);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSolidBrushObject::IsSimilarObject( CBaseObject *pObject )
{
	if (pObject->GetClassDesc() == GetClassDesc() && GetRuntimeClass() == pObject->GetRuntimeClass())
	{
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::OnEvent( ObjectEvent event )
{
	CBaseObject::OnEvent( event );
	switch (event)
	{
	case EVENT_OUTOFGAME:
		if (m_brush)
		{
			IStatObj *pStatObj = m_brush->GetIStatObj();
			if (pStatObj && (pStatObj->GetFlags() & STATIC_OBJECT_GENERATED))
			{
				// Stat object was modified in game, rebuild it.
				m_brush->UpdateMesh();
				InvalidateBrush(false);
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSolidBrushObject::StartSubObjSelection( int elemType )
{
	bool bStarted = false;
	if (m_brush)
		bStarted = m_brush->StartSubObjSelection( GetWorldTM(),elemType,0 );
	if (bStarted)
		SetFlags(OBJFLAG_SUBOBJ_EDITING);

	if (m_pRenderNode != 0 && IsSelected())
	{
		m_renderFlags &= ~ERF_SELECTED;
		m_pRenderNode->SetRndFlags( m_renderFlags );
	}

	if (bStarted)
	{
		if (!s_brushPanelSubObjId)
		{
			s_brushPanelSubObj = new CSolidBrushSubObjPanel();
			s_brushPanelSubObj->SetObject(this);
			s_brushPanelSubObjId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,_T("Sub Object Edit"),s_brushPanelSubObj );
		}
	}

	return bStarted;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::EndSubObjectSelection()
{
	if (m_pRenderNode != 0 && IsSelected() && gSettings.viewports.bHighlightSelectedGeometry)
	{
		m_renderFlags |= ERF_SELECTED;
		m_pRenderNode->SetRndFlags( m_renderFlags );
	}
	ClearFlags(OBJFLAG_SUBOBJ_EDITING);
	if (m_brush)
	{
		PivotToCenter();
		m_brush->EndSubObjSelection();
		InvalidateBrush();
	}
	if (s_brushPanelSubObjId)
	{
		GetIEditor()->RemoveRollUpPage(ROLLUP_OBJECTS,s_brushPanelSubObjId);
		s_brushPanelSubObjId = 0;
		s_brushPanelSubObj = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::CalculateSubObjectSelectionReferenceFrame( ISubObjectSelectionReferenceFrameCalculator* pCalculator )
{
	if (m_brush)
		pCalculator->AddBrush(this->GetWorldTM(), m_brush);
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::ModifySubObjSelection( SSubObjSelectionModifyContext &modCtx )
{
	if (m_brush)
	{
		m_brush->ModifySelection( modCtx );
		m_brush->GetBounds( m_bbox );
		UpdateEngineNode();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::AcceptSubObjectModify()
{
	if (m_brush)
	{
		m_brush->AcceptModifySelection();
		m_brush->GetBounds( m_bbox );
		UpdateEngineNode();
	}
}

//////////////////////////////////////////////////////////////////////////
CEdGeometry* CSolidBrushObject::GetGeometry()
{
	// Return our geometry.
	return m_brush;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::ResetTransform()
{
	if (!m_brush)
		return;

	StoreUndo( "Reset Transform" );

	// Reset brush transformation.
	Matrix34 brushTM = GetWorldTM();
	brushTM.SetTranslation( Vec3(0,0,0) );
	m_brush->Transform( brushTM );
	m_bbox = m_brush->m_bounds;

	Matrix34 tm;
	tm.SetIdentity();
	tm.SetTranslation( GetWorldTM().GetTranslation() );
	SetWorldTM( tm );

	InvalidateBrush(false);
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::SnapPointsToGrid()
{
	StoreUndo( "Snap to Grid" );

	if (m_brush)
		m_brush->SnapToGrid();
	InvalidateBrush(false);
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::SaveToCgf( const CString filename )
{
	if (m_brush != 0 && m_brush->GetIStatObj())
	{
		m_brush->GetIStatObj()->SaveToCGF( filename );
	}
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::RegisterCommands( CRegistrationContext &rc )
{
	rc.pCommandManager->RegisterCommand( "Brush.ResetTransform",functor(&CSolidBrushObject::Command_ResetTransform) );
	rc.pCommandManager->RegisterCommand( "Brush.SnapPointsToGrid",functor(&CSolidBrushObject::Command_SnapPointsToGrid) );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::Command_ResetTransform()
{
	CUndo undo("Brush Reset Transform");
	CSelectionGroup *pSel = ::GetIEditor()->GetSelection();
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		if (pSel->GetObject(i)->IsKindOf(RUNTIME_CLASS(CSolidBrushObject)))
		{
			CSolidBrushObject *pSolid = (CSolidBrushObject*)pSel->GetObject(i);
			pSolid->ResetTransform();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::Command_SnapPointsToGrid()
{
	CUndo undo("Brush Snap Points To Grid");
	CSelectionGroup *pSel = ::GetIEditor()->GetSelection();
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		if (pSel->GetObject(i)->IsKindOf(RUNTIME_CLASS(CSolidBrushObject)))
		{
			CSolidBrushObject *pSolid = (CSolidBrushObject*)pSel->GetObject(i);
			pSolid->SnapPointsToGrid();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::PivotToVertices()
{
	StoreUndo( "Align" );

	std::vector<Vec3*> points;

	for(int f=0; f<m_brush->m_Faces.size(); f++)
	{
		SBrushPoly * pPoly = m_brush->m_Faces[f]->m_Poly;
		if(pPoly)
			for(int p = 0; p<pPoly->m_Pts.size(); p++)
				if(pPoly->m_Pts[p].bSelected)
					points.push_back(& pPoly->m_Pts[p].xyz);
	}

	if(points.size()==0)
		return;

	BBox bb(*points[0], *points[0]);
	for(int i=0; i<points.size(); i++)
		bb.Add(*points[i]);

	Vec3 store = GetPos();
	Vec3 v = GetWorldTM().TransformPoint(bb.GetCenter());

	v = GetIEditor()->GetActiveView()->SnapToGrid( v );

	Matrix34 m = GetWorldTM();
	m.SetTranslation(v);
	SetWorldTM(m);
	store = store - GetPos();

	m = GetWorldTM();
	m.SetTranslation(Vec3(0, 0, 0));
	Matrix34 invtm = m.GetInverted();
	store = invtm * store;


	for(int f=0; f<m_brush->m_Faces.size(); f++)
	{
		m_brush->m_Faces[f]->m_PlanePts[0] += store;
		m_brush->m_Faces[f]->m_PlanePts[1] += store;
		m_brush->m_Faces[f]->m_PlanePts[2] += store;
	}

	InvalidateBrush(true);
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushObject::PivotToCenter()
{
	StoreUndo( "Align" );

	std::vector<Vec3*> points;

	for(int f=0; f<m_brush->m_Faces.size(); f++)
	{
		SBrushPoly * pPoly = m_brush->m_Faces[f]->m_Poly;
		if(pPoly)
			for(int p = 0; p<pPoly->m_Pts.size(); p++)
				points.push_back(& pPoly->m_Pts[p].xyz);
	}
	if(points.size()==0)
		return;

	BBox bb(*points[0], *points[0]);
	for(int i=0; i<points.size(); i++)
		bb.Add(*points[i]);

	Vec3 store = GetPos();
	Vec3 v = GetWorldTM().TransformPoint(bb.GetCenter());

	v = GetIEditor()->GetActiveView()->SnapToGrid( v );

	Matrix34 m = GetWorldTM();
	m.SetTranslation(v);
	SetWorldTM(m);
	store = store - GetPos();

	m = GetWorldTM();
	m.SetTranslation(Vec3(0, 0, 0));
	Matrix34 invtm = m.GetInverted();
	store = invtm * store;

	for(int f=0; f<m_brush->m_Faces.size(); f++)
	{
		m_brush->m_Faces[f]->m_PlanePts[0] += store;
		m_brush->m_Faces[f]->m_PlanePts[1] += store;
		m_brush->m_Faces[f]->m_PlanePts[2] += store;
	}

	InvalidateBrush(true);
}

