#include "StdAfx.h"

#include "ObjectManager.h"
#include "..\Viewport.h"

#include "Entity.h"
#include "Geometry\EdMesh.h"
#include "Material\Material.h"
#include "Material\MaterialManager.h"
#include "Settings.h"
#include "CryEditDoc.h"

#include <I3DEngine.h>
#include <IEntitySystem.h>
#include <IEntityRenderState.h>

#include "DistanceCloudObject.h"


//////////////////////////////////////////////////////////////////////////
// CDistanceCloudObject implementation.
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE( CDistanceCloudObject, CBaseObject )

CDistanceCloudObject::CDistanceCloudObject()
{
	m_pRenderNode = 0;
	m_pPrevRenderNode = 0;
	m_renderFlags = 0;
}

//////////////////////////////////////////////////////////////////////////
void CDistanceCloudObject::BeginEditParams( IEditor* ie, int flags )
{
	CBaseObject::BeginEditParams( ie, flags );
}

//////////////////////////////////////////////////////////////////////////
void CDistanceCloudObject::EndEditParams( IEditor* ie )
{
	CBaseObject::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CDistanceCloudObject::GetDefaultMaterial() const
{
	CMaterial* pDefMat( GetIEditor()->GetMaterialManager()->LoadMaterial( "Materials/Clouds/DistanceClouds" ) );
	pDefMat->AddRef();
	return pDefMat;
}

//////////////////////////////////////////////////////////////////////////
bool CDistanceCloudObject::Init( IEditor* ie, CBaseObject* prev, const CString& file )
{
	CDistanceCloudObject* pSrcObj((CDistanceCloudObject*) prev);

	SetColor(RGB(127, 127, 255));

	if (IsCreateGameObjects())
	{
		if (pSrcObj)
			m_pPrevRenderNode = pSrcObj->m_pRenderNode;
	}

	bool res = CBaseObject::Init(ie, prev, file);
	
	if (pSrcObj)
	{
		// clone custom properties
		m_renderFlags = pSrcObj->m_renderFlags;
	}
	else
	{
		// new distance cloud, set default material
		SetMaterial(GetDefaultMaterial());
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CDistanceCloudObject::CreateGameObject()
{
	if( !m_pRenderNode )
	{
		m_pRenderNode = static_cast< IDistanceCloudRenderNode* >( GetIEditor()->Get3DEngine()->CreateRenderNode( eERType_DistanceCloud ) );
				
		if( m_pRenderNode && m_pPrevRenderNode )
			m_pPrevRenderNode = 0;

		UpdateEngineNode();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CDistanceCloudObject::Done()
{
	if( m_pRenderNode )
	{
		GetIEditor()->Get3DEngine()->DeleteRenderNode( m_pRenderNode );
		m_pRenderNode = 0;
	}

	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
void CDistanceCloudObject::UpdateEngineNode()
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

	// update world transform
	const Matrix34& wtm( GetWorldTM() );
	m_pRenderNode->SetMatrix( wtm );

	// set properties
	SDistanceCloudProperties properties;
	properties.m_pos = wtm.TransformPoint( Vec3( 0, 0, 0 ) );
	properties.m_pMaterialName = GetMaterial()->GetName();
	properties.m_sizeX = wtm.TransformVector( Vec3( 1, 0, 0 ) ).GetLength();
	properties.m_sizeY = wtm.TransformVector( Vec3( 0, 1, 0 ) ).GetLength();
	properties.m_rotationZ = GetWorldAngles().z;
	m_pRenderNode->SetProperties( properties );
}

//////////////////////////////////////////////////////////////////////////
void CDistanceCloudObject::SetHidden( bool bHidden )
{
	CBaseObject::SetHidden( bHidden );
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDistanceCloudObject::UpdateVisibility( bool visible )
{
	CBaseObject::UpdateVisibility( visible );
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void 	CDistanceCloudObject::SetMaterial( CMaterial* mtl )
{
	if( !mtl )
		mtl = GetDefaultMaterial();
	CBaseObject::SetMaterial( mtl );
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDistanceCloudObject::InvalidateTM( int nWhyFlags )
{
	CBaseObject::InvalidateTM( nWhyFlags );
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDistanceCloudObject::GetLocalBounds( BBox& box )
{
	box = BBox( Vec3( -1, -1, -1e-4f ), Vec3( 1, 1, 1e-4f ) );
}

//////////////////////////////////////////////////////////////////////////
void CDistanceCloudObject::Display( DisplayContext& dc )
{
	if (IsSelected())
	{
		const Matrix34& wtm( GetWorldTM() );

		dc.SetSelectedColor();

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

	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
void CDistanceCloudObject::Serialize( CObjectArchive& ar )
{
	CBaseObject::Serialize( ar );

	XmlNodeRef xmlNode( ar.node );
	if( ar.bLoading )
	{
		// load attributes
		xmlNode->getAttr( "RndFlags", m_renderFlags );

		// update render node
		if( !m_pRenderNode )
			CreateGameObject();
		
		UpdateEngineNode();
	}
	else
	{
		// save attributes
		xmlNode->setAttr( "RndFlags", m_renderFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CDistanceCloudObject::Export( const CString& levelPath, XmlNodeRef& xmlNode )
{
	XmlNodeRef distanceCloudNode( CBaseObject::Export( levelPath, xmlNode ) );	
	return distanceCloudNode;
}

//////////////////////////////////////////////////////////////////////////
bool CDistanceCloudObject::HitTest( HitContext& hc )
{
	return false;
}
