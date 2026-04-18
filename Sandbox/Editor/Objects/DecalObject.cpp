/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 07:11:2005   11:00 : Created by Carsten Wenzel

*************************************************************************/

#include "StdAfx.h"

#include "ObjectManager.h"
#include "..\Viewport.h"
#include "..\DecalObjectPanel.h"

#include "PanelTreeBrowser.h"

#include "Entity.h"
#include "Geometry\EdMesh.h"
#include "Material\Material.h"
#include "Material\MaterialManager.h"
#include "Settings.h"
#include "CryEditDoc.h"

#include <I3DEngine.h>
#include <IEntitySystem.h>
#include <IEntityRenderState.h>

#include "DecalObject.h"


namespace
{
	CDecalObjectPanel* s_pDecalPanel( 0 );
	int s_pDecalPanelId( 0 );
}


//////////////////////////////////////////////////////////////////////////
// CDecalObject implementation.
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE( CDecalObject, CBaseObject )

CDecalObject::CDecalObject()
{
	m_renderFlags = 0;	
	m_projectionType = 0;
	
	AddVariable(m_projectionType, "ProjectionType", functor(*this, &CDecalObject::OnParamChanged));
	
	m_viewDistRatio = 100;
	AddVariable(m_viewDistRatio, "ViewDistRatio", functor(*this, &CDecalObject::OnViewDistRatioChanged));
	m_viewDistRatio.SetLimits(0, 255);
	
	m_sortPrio = 16;
	AddVariable(m_sortPrio, "SortPriority", functor(*this, &CDecalObject::OnParamChanged));
	m_sortPrio.SetLimits(0, 255);

	m_pRenderNode = 0;
	m_pPrevRenderNode = 0;
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::BeginEditParams( IEditor* ie, int flags )
{
	CBaseObject::BeginEditParams( ie, flags );
	if( !s_pDecalPanel )
	{
		s_pDecalPanel = new CDecalObjectPanel;
		s_pDecalPanelId = ie->AddRollUpPage( ROLLUP_OBJECTS, _T( "Decal" ), s_pDecalPanel );
	}

	if( s_pDecalPanel )
		s_pDecalPanel->SetDecalObject( this );
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::EndEditParams( IEditor* ie )
{
	CBaseObject::EndEditParams( ie );
	if( s_pDecalPanel )
	{
		ie->RemoveRollUpPage( ROLLUP_OBJECTS, s_pDecalPanelId );
		s_pDecalPanel = 0;
		s_pDecalPanelId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::OnParamChanged( IVariable* pVar )
{
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::OnViewDistRatioChanged(IVariable* pVar)
{
	if (m_pRenderNode)
	{
		m_pRenderNode->SetViewDistRatio(m_viewDistRatio);

		// set matrix again to for update of view distance ratio in engine
		const Matrix34& wtm( GetWorldTM() );
		m_pRenderNode->SetMatrix( wtm );
	}
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CDecalObject::GetDefaultMaterial() const
{
	CMaterial* pDefMat( GetIEditor()->GetMaterialManager()->LoadMaterial( "Materials/Decals/Default" ) );
	pDefMat->AddRef();
	return pDefMat;
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObject::Init( IEditor* ie, CBaseObject* prev, const CString& file )
{
	CDecalObject* pSrcDecalObj( (CDecalObject*) prev );

	SetColor( RGB( 127, 127, 255 ) );

	if( IsCreateGameObjects() )
	{
		if( pSrcDecalObj )
			m_pPrevRenderNode = pSrcDecalObj->m_pRenderNode;
	}

	// Must be after SetDecal call.
	bool res = CBaseObject::Init( ie, prev, file );
	
	if( pSrcDecalObj )
	{
		// clone custom properties
		m_projectionType = pSrcDecalObj->m_projectionType;
		m_renderFlags = pSrcDecalObj->m_renderFlags;
	}
	else
	{
		// new decal, set default material
		SetMaterial( GetDefaultMaterial() );
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
int CDecalObject::GetProjectionType() const
{
	return m_projectionType;
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObject::CreateGameObject()
{
	if( !m_pRenderNode )
	{
		m_pRenderNode = static_cast< IDecalRenderNode* >( GetIEditor()->Get3DEngine()->CreateRenderNode( eERType_Decal ) );
				
		if( m_pRenderNode && m_pPrevRenderNode )
			m_pPrevRenderNode = 0;

		UpdateEngineNode();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::Done()
{
	if( m_pRenderNode )
	{
		GetIEditor()->Get3DEngine()->DeleteRenderNode( m_pRenderNode );
		m_pRenderNode = 0;
	}

	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::UpdateEngineNode()
{
	if( !m_pRenderNode )
		return;

	// update basic entity render flags
	m_renderFlags = 0;

	if( IsSelected() && gSettings.viewports.bHighlightSelectedGeometry )
		m_renderFlags |= ERF_SELECTED;

	if( IsHidden() )
		m_renderFlags |= ERF_HIDDEN;

	if( IsFrozen() )
		m_renderFlags |= ERF_FROOZEN;

	m_pRenderNode->SetRndFlags( m_renderFlags );

	// set properties
	SDecalProperties decalProperties;
	switch( m_projectionType )
	{
	case SDecalProperties::eProjectOnTerrainAndStaticObjects:
		{
			decalProperties.m_projectionType = SDecalProperties::eProjectOnTerrainAndStaticObjects;
			break;
		}
	case SDecalProperties::eProjectOnStaticObjects:
		{
			decalProperties.m_projectionType = SDecalProperties::eProjectOnStaticObjects;
			break;
		}
	case SDecalProperties::eProjectOnTerrain:
		{
			decalProperties.m_projectionType = SDecalProperties::eProjectOnTerrain;
			break;
		}
	case SDecalProperties::ePlanar:
	default:
		{
			decalProperties.m_projectionType = SDecalProperties::ePlanar;
			break;
		}
	}

  // update world transform
  const Matrix34& wtm( GetWorldTM() );

  // get normalized rotation (remove scaling)
  Matrix33 rotation( wtm ); 
  rotation.SetRow( 0, rotation.GetRow( 0 ).GetNormalized() );
  rotation.SetRow( 1, rotation.GetRow( 1 ).GetNormalized() );
  rotation.SetRow( 2, rotation.GetRow( 2 ).GetNormalized() );

	decalProperties.m_pos = wtm.TransformPoint( Vec3( 0, 0, 0 ) );
	decalProperties.m_normal = wtm.TransformVector( Vec3( 0, 0, 1 ) );
	decalProperties.m_pMaterialName = GetMaterial()->GetName();
	decalProperties.m_radius = decalProperties.m_normal.GetLength();
	decalProperties.m_explicitRightUpFront = rotation;
	decalProperties.m_sortPrio = m_sortPrio;
	m_pRenderNode->SetDecalProperties( decalProperties );

  m_pRenderNode->SetMatrix( wtm );
	m_pRenderNode->SetMinSpec( GetMinSpec() );
	m_pRenderNode->SetMaterialLayers( GetMaterialLayersMask() );
  m_pRenderNode->SetViewDistRatio(m_viewDistRatio);
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::SetHidden( bool bHidden )
{
	CBaseObject::SetHidden( bHidden );
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::UpdateVisibility( bool visible )
{
	CBaseObject::UpdateVisibility( visible );
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::SetMinSpec( uint32 nSpec )
{
	__super::SetMinSpec(nSpec);
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::SetMaterialLayersMask( uint32 nLayersMask )
{
	__super::SetMinSpec(nLayersMask);
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void 	CDecalObject::SetMaterial( CMaterial* mtl )
{
	if( !mtl )
		mtl = GetDefaultMaterial();
	CBaseObject::SetMaterial( mtl );
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::InvalidateTM( int nWhyFlags )
{
	CBaseObject::InvalidateTM( nWhyFlags );
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::GetLocalBounds( BBox& box )
{
	box = BBox( Vec3( -1, -1, -1 ), Vec3( 1, 1, 1 ) );
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::MouseCallbackImpl( CViewport* view, EMouseEvent event, CPoint& point, int flags, bool callerIsMouseCreateCallback )
{
	static bool s_mousePosTracked( false );

	if( ( callerIsMouseCreateCallback && !flags ) || ( callerIsMouseCreateCallback && ( flags & MK_LBUTTON ) ) )
	{			
		Vec3 pos( view->MapViewToCP( point ) );
		SetPos( pos );
		s_mousePosTracked = false;
	}
	else
	{
		static CPoint s_prevMousePos;

		enum EInputMode
		{
			eNone,

			eAlign,
			eScale,
			eRotate
		};

		bool altKeyPressed( 0 != GetAsyncKeyState( VK_MENU ) );

		EInputMode eInputMode( eNone );
		if( ( flags & MK_CONTROL ) && !altKeyPressed )
			eInputMode = eAlign;
		else if( ( flags & MK_CONTROL ) && altKeyPressed )
			eInputMode = eRotate;
		else if( altKeyPressed )
			eInputMode = eScale;

		switch( eInputMode )
		{
		case eAlign:
			{
				Vec3 raySrc, rayDir;
				GetIEditor()->GetActiveView()->ViewToWorldRay( point, raySrc, rayDir ); 
				rayDir = rayDir.GetNormalized() * 1000.0f;

				ray_hit hit;
				if( gEnv->pPhysicalWorld->RayWorldIntersection( raySrc, rayDir, ent_all, rwi_stop_at_pierceable | rwi_ignore_terrain_holes, &hit, 1 ) > 0 )
				{
					const Matrix34& wtm( GetWorldTM() );
					Vec3 x( wtm.GetColumn0().GetNormalized() );
					Vec3 y( wtm.GetColumn1().GetNormalized() );
					Vec3 z( hit.n.GetNormalized() );
					
					y = z.Cross( x );
					if( y.GetLengthSquared() < 1e-4f )
						y = z.GetOrthogonal();
					y.Normalize();
					x = y.Cross( z );

					Matrix33 newOrient;
					newOrient.SetColumn( 0, x );
					newOrient.SetColumn( 1, y );
					newOrient.SetColumn( 2, z );
					Quat q( newOrient );

					SetPos( hit.pt, TM_NOT_INVALIDATE );
					SetRotation( q, TM_NOT_INVALIDATE );
					InvalidateTM( TM_POS_CHANGED | TM_ROT_CHANGED );			
				}
				break;
			}
		case eRotate:
			{
				if( !s_mousePosTracked )
					break;

				float angle( -DEG2RAD( point.x - s_prevMousePos.x ) );
				SetRotation( GetRotation() * Quat( Matrix33::CreateRotationZ( angle ) ) );				
				break;
			}
		case eScale:
			{
				if( !s_mousePosTracked )
					break;

				float scaleDelta( 0.01f * ( s_prevMousePos.y - point.y ) );
				Vec3 newScale( GetScale() + Vec3( scaleDelta, scaleDelta, scaleDelta ) );
				newScale.x = max( newScale.x, 0.1f );
				newScale.y = max( newScale.y, 0.1f );
				newScale.z = max( newScale.z, 0.1f );
				SetScale( newScale );			
				break;
			}
		default:
			{
				break;
			}
		}

		s_prevMousePos = point;
		s_mousePosTracked = true;
	}
}

//////////////////////////////////////////////////////////////////////////
int CDecalObject::MouseCreateCallback( CViewport* view, EMouseEvent event, CPoint& point, int flags )
{
	if( event == eMouseMove || event == eMouseLDown || event == eMouseLUp )
	{
		MouseCallbackImpl( view, event, point, flags, true );

		if( event == eMouseLDown )
			return MOUSECREATE_OK;

		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback( view, event, point, flags );
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::Display( DisplayContext& dc )
{
	if (IsSelected())
	{
		const Matrix34& wtm( GetWorldTM() );

		Vec3 x1( wtm.TransformPoint( Vec3( -1,  0, 0 ) ) );
		Vec3 x2( wtm.TransformPoint( Vec3(  1,  0, 0 ) ) );
		Vec3 y1( wtm.TransformPoint( Vec3(  0, -1, 0 ) ) );
		Vec3 y2( wtm.TransformPoint( Vec3(  0,  1, 0 ) ) );
		Vec3  p( wtm.TransformPoint( Vec3(  0,  0, 0 ) ) );
		Vec3  n( wtm.TransformPoint( Vec3(  0,  0, 1 ) ) );

		if( IsSelected() )
			dc.SetSelectedColor();
		else if( IsFrozen() )
			dc.SetFreezeColor();
		else
			dc.SetColor( GetColor() );

		dc.DrawLine(p, n);
		dc.DrawLine(x1, x2);
		dc.DrawLine(y1, y2);

		if( IsSelected() )
		{
			Vec3 p0 = wtm.TransformPoint( Vec3( -1, -1, 0 ) );
			Vec3 p1 = wtm.TransformPoint( Vec3( -1,  1, 0 ) );
			Vec3 p2 = wtm.TransformPoint( Vec3(  1,  1, 0 ) );
			Vec3 p3 = wtm.TransformPoint( Vec3(  1, -1, 0 ) );
			dc.DrawLine(p0, p1);
			dc.DrawLine(p1, p2);
			dc.DrawLine(p2, p3);
			dc.DrawLine(p3, p0);
			dc.DrawLine(p0, p2);
			dc.DrawLine(p1, p3);
		}
	}

	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::Serialize( CObjectArchive& ar )
{
	CBaseObject::Serialize( ar );

	XmlNodeRef xmlNode( ar.node );

	if( ar.bLoading )
	{
		// load attributes
		int projectionType( SDecalProperties::ePlanar );
		xmlNode->getAttr( "ProjectionType", projectionType );
		m_projectionType = projectionType;

		xmlNode->getAttr( "RndFlags", m_renderFlags );

		// update render node
		if( !m_pRenderNode )
			CreateGameObject();
		
		UpdateEngineNode();
	}
	else
	{
		// save attributes
		xmlNode->setAttr( "ProjectionType", m_projectionType );
		xmlNode->setAttr( "RndFlags", m_renderFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CDecalObject::Export( const CString& levelPath, XmlNodeRef& xmlNode )
{
	XmlNodeRef decalNode( CBaseObject::Export( levelPath, xmlNode ) );	
	decalNode->setAttr( "ProjectionType", m_projectionType );
	return decalNode;
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObject::RayToLineDistance( const Vec3& rayLineP1, const Vec3& rayLineP2, const Vec3& pi, const Vec3& pj, float& distance, Vec3& intPnt )
{
	Vec3 pa,pb;
	float ua,ub;
	if (!LineLineIntersect( pi, pj, rayLineP1, rayLineP2, pa, pb, ua, ub ))
		return false;

	// If before ray origin.
	if( ub < 0 )
		return false;

	float d( 0 );
	if( ua < 0 )
		d = PointToLineDistance( rayLineP1, rayLineP2, pi, intPnt );
	else if (ua > 1)
		d = PointToLineDistance( rayLineP1, rayLineP2, pj, intPnt );
	else
	{
		intPnt = rayLineP1 + ub * ( rayLineP2 - rayLineP1 );
		d = ( pb - pa ).GetLength();
	}
	distance = d;

	return true;
}


//////////////////////////////////////////////////////////////////////////
bool CDecalObject::HitTest( HitContext& hc )
{
	// Selectable by icon.
	/*
	const Matrix34& wtm( GetWorldTM() );

	Vec3 x1( wtm.TransformPoint( Vec3( -1,  0, 0 ) ) );
	Vec3 x2( wtm.TransformPoint( Vec3(  1,  0, 0 ) ) );
	Vec3 y1( wtm.TransformPoint( Vec3(  0, -1, 0 ) ) );
	Vec3 y2( wtm.TransformPoint( Vec3(  0,  1, 0 ) ) );
	Vec3  p( wtm.TransformPoint( Vec3(  0,  0, 0 ) ) );
	Vec3  n( wtm.TransformPoint( Vec3(  0,  0, 1 ) ) );

	float minDist( FLT_MAX );
	Vec3 ip;
	Vec3 intPnt;
	Vec3 rayLineP1( hc.raySrc );
	Vec3 rayLineP2( hc.raySrc + hc.rayDir * 100000.0f );

	bool bWasIntersection( false );

	float d( 0 );
	if( RayToLineDistance( rayLineP1, rayLineP2, p, n, d, ip ) && d < minDist )
		{ minDist = d; intPnt = ip; bWasIntersection = true; }
	if( RayToLineDistance( rayLineP1, rayLineP2, x1, x2, d, ip ) && d < minDist )
		{ minDist = d; intPnt = ip; bWasIntersection = true; }
	if( RayToLineDistance( rayLineP1, rayLineP2, y1, y2, d, ip ) && d < minDist )
		{ minDist = d; intPnt = ip; bWasIntersection = true; }

	float fRoadCloseDistance( 0.8f * hc.view->GetScreenScaleFactor( intPnt ) * 0.01f );
	if( bWasIntersection && minDist < fRoadCloseDistance + hc.distanceTollerance )
	{
		hc.dist = hc.raySrc.GetDistance( intPnt );
		return true;
	}
	*/

	return false;
}
