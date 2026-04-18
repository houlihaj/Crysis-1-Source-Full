////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   watershapeobject.cpp
//  Version:     v1.00
//  Created:     10/12/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "WaterShapeObject.h"
#include "Material\MaterialManager.h"

#include <I3DEngine.h>

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CWaterShapeObject,CShapeObject)

//////////////////////////////////////////////////////////////////////////
CWaterShapeObject::CWaterShapeObject()
{
	m_pWVRN = 0;

	m_waterVolumeID = -1;
	m_fogPlane = Plane( Vec3( 0, 0, 1 ), 0 );
}

//////////////////////////////////////////////////////////////////////////
bool CWaterShapeObject::Init( IEditor* ie, CBaseObject* prev, const CString& file )
{
	bool res = CShapeObject::Init( ie, prev, file );	
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::InitVariables()
{
	__super::InitVariables();

	mv_waterVolumeDepth = 10.0f;
	mv_waterStreamSpeed = 0.0f;	
	mv_waterFogDensity = 0.1f;
	mv_waterFogColor = Vec3( 0.2f, 0.5f, 0.7f );
	mv_waterFogColorMultiplier = 1.0f;
	mv_waterSurfaceUScale = 1.0f;
	mv_waterSurfaceVScale = 1.0f;

	AddVariable( mv_waterVolumeDepth, "VolumeDepth", "Depth", functor( *this, &CWaterShapeObject::OnWaterPhysicsParamChange ) );
	AddVariable( mv_waterStreamSpeed, "StreamSpeed", "Speed", functor( *this, &CWaterShapeObject::OnWaterPhysicsParamChange ) );
	AddVariable( mv_waterFogDensity, "FogDensity", "FogDensity", functor( *this, &CWaterShapeObject::OnWaterParamChange ) );
	AddVariable( mv_waterFogColor, "FogColor", "FogColor", functor( *this, &CWaterShapeObject::OnWaterParamChange ), IVariable::DT_COLOR );
	AddVariable( mv_waterFogColorMultiplier, "FogColorMultiplier", "FogColorMultiplier", functor( *this, &CWaterShapeObject::OnWaterParamChange ) );
	AddVariable( mv_waterSurfaceUScale, "UScale", "UScale", functor( *this, &CWaterShapeObject::OnParamChange ) );
	AddVariable( mv_waterSurfaceVScale, "VScale", "VScale", functor( *this, &CWaterShapeObject::OnParamChange ) );
}

//////////////////////////////////////////////////////////////////////////
bool CWaterShapeObject::CreateGameObject()
{
	m_pWVRN = static_cast< IWaterVolumeRenderNode* >( GetIEditor()->Get3DEngine()->CreateRenderNode( eERType_WaterVolume ) );
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::SetName( const CString& name )
{
	CShapeObject::SetName( name );
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::Done()
{
	if( m_pWVRN )
		GetIEditor()->Get3DEngine()->DeleteRenderNode( m_pWVRN );
  m_pWVRN = NULL;

	CShapeObject::Done();
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::OnParamChange( IVariable* var )
{
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
Vec3 CWaterShapeObject::GetPremulFogColor() const
{
	return Vec3( mv_waterFogColor ) * max( float( mv_waterFogColorMultiplier ), 0.0f );
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::OnWaterParamChange( IVariable* var )
{
	if( m_pWVRN )
	{
		m_pWVRN->SetFogDensity( mv_waterFogDensity );
		m_pWVRN->SetFogColor( GetPremulFogColor() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::Physicalize()
{
	if( m_pWVRN )
	{
		const Matrix34& wtm = GetWorldTM();

		std::vector<Vec3> points( GetPointCount() );
		for( int i = 0; i < GetPointCount(); i++ )
			points[i] = wtm.TransformPoint( GetPoint( i ) );

		m_pWVRN->Dephysicalize();
		m_pWVRN->SetAreaPhysicalArea( &points[0], points.size(), mv_waterVolumeDepth, mv_waterStreamSpeed, true );
		m_pWVRN->Physicalize();
	}
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::OnWaterPhysicsParamChange( IVariable* var )
{
	Physicalize();
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::SetMaterial( CMaterial* mtl )
{
	CShapeObject::SetMaterial( mtl );
	if( m_pWVRN )
	{
		if( mtl )
			m_pWVRN->SetMaterial( mtl->GetMatInfo() );
		else
			m_pWVRN->SetMaterial( 0 );
	}
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::UpdateGameArea( bool remove )
{
	if( remove )
		return;

	if( m_bIgnoreGameUpdate )
		return;

	if( m_waterVolumeID == -1 )
	{
		m_waterVolumeID = ToEntityGuid( GetId() );
		assert( m_waterVolumeID );
	}

	if( m_pWVRN )
	{
		m_pWVRN->SetMinSpec( GetMinSpec() );
		m_pWVRN->SetMaterialLayers( GetMaterialLayersMask() );

		if( GetPointCount() > 3 )
		{
			const Matrix34& wtm = GetWorldTM();
	
			std::vector<Vec3> points( GetPointCount() );
			for( int i = 0; i < GetPointCount(); i++ )
				points[i] = wtm.TransformPoint( GetPoint( i ) );

			m_fogPlane.SetPlane( wtm.GetColumn2(), points[0] );

			m_pWVRN->SetFogDensity( mv_waterFogDensity );
			m_pWVRN->SetFogColor( GetPremulFogColor() );

			m_pWVRN->CreateArea( m_waterVolumeID, &points[0], points.size(), Vec2( mv_waterSurfaceUScale, mv_waterSurfaceVScale ), m_fogPlane, true );

			Physicalize();

			if( GetMaterial() )
				m_pWVRN->SetMaterial( GetMaterial()->GetMatInfo() );
			else
				m_pWVRN->SetMaterial( 0 );
		}
	}

	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::Serialize( CObjectArchive& ar )
{
	XmlNodeRef xmlNode( ar.node );
	if( ar.bLoading )
	{
		float waterVolumeDepth( 10.0f );
		xmlNode->getAttr( "VolumeDepth", waterVolumeDepth );
		mv_waterVolumeDepth = waterVolumeDepth;

		float waterStreamSpeed( 0.0f );
		xmlNode->getAttr( "StreamSpeed", waterStreamSpeed );
		mv_waterStreamSpeed = waterStreamSpeed;

		float waterFogDensity( 0.1f );
		xmlNode->getAttr( "FogDensity", waterFogDensity );
		mv_waterFogDensity = waterFogDensity;

		Vec3 waterFogColor( 0.2f, 0.5f, 0.7f );
		xmlNode->getAttr( "FogColor", waterFogColor );
		mv_waterFogColor = waterFogColor;

		float waterFogColorMultiplier( 1.0f );
		xmlNode->getAttr( "FogColorMultiplier", waterFogColorMultiplier );
		mv_waterFogColorMultiplier = waterFogColorMultiplier;

		float waterSurfaceUScale( 1.0f );
		xmlNode->getAttr( "UScale", waterSurfaceUScale );
		mv_waterSurfaceUScale = waterSurfaceUScale;

		float waterSurfaceVScale( 1.0f );
		xmlNode->getAttr( "VScale", waterSurfaceVScale );
		mv_waterSurfaceVScale = waterSurfaceVScale;
	}
	else
	{
		xmlNode->setAttr( "VolumeDepth", mv_waterVolumeDepth );
		xmlNode->setAttr( "StreamSpeed", mv_waterStreamSpeed );
		xmlNode->setAttr( "FogDensity", mv_waterFogDensity );
		xmlNode->setAttr( "FogColor", mv_waterFogColor );
		xmlNode->setAttr( "FogColorMultiplier", mv_waterFogColorMultiplier );
		xmlNode->setAttr( "UScale", mv_waterSurfaceUScale );
		xmlNode->setAttr( "VScale", mv_waterSurfaceVScale );
	}

	__super::Serialize( ar );
}

//////////////////////////////////////////////////////////////////////////

XmlNodeRef CWaterShapeObject::Export( const CString& levelPath, XmlNodeRef& xmlNode )
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::SetMinSpec( uint32 nSpec )
{
	__super::SetMinSpec(nSpec);
	if (m_pWVRN)
		m_pWVRN->SetMinSpec(nSpec);
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::SetMaterialLayersMask( uint32 nLayersMask )
{
	__super::SetMinSpec(nLayersMask);
	if (m_pWVRN)
		m_pWVRN->SetMaterialLayers(nLayersMask);
}
