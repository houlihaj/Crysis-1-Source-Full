////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   RiverObject.cpp
//  Version:     v1.00
//  Created:     25/07/2005 by Timur Davidenko.
//  Compilers:   Visual C++ 6.0
//  Description: CRiverObject implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "RiverObject.h"
#include "Material/Material.h"
#include <I3DEngine.h>

//////////////////////////////////////////////////////////////////////////
// CRiverObject implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CRiverObject,CRoadObject)

//////////////////////////////////////////////////////////////////////////
CRiverObject::CRiverObject()
{
	//m_entityClass = "AreaRiver";
	m_pszEditParams = "River Parameters";
	SetColor( Vec2Rgb( Vec3( 0, 0, 1 ) ) );
	
	m_waterVolumeID = -1;
	m_fogPlane = Plane( Vec3( 0, 0, 1 ), 0 );
	
	UseMaterialLayersMask( false );
}

//////////////////////////////////////////////////////////////////////////
bool CRiverObject::Init( IEditor* ie, CBaseObject* prev, const CString& file )
{
	bool res = __super::Init( ie, prev, file );

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CRiverObject::InitVariables()
{
	//__super::InitVariables();
	CRoadObject::InitBaseVariables();

	mv_waterVolumeDepth = 10.0f;
	mv_waterStreamSpeed = 0.0f;	
	mv_waterFogDensity = 0.1f;
	mv_waterFogColor = Vec3( 0.2f, 0.5f, 0.7f );
	mv_waterFogColorMultiplier = 1.0f;
	mv_waterSurfaceUScale = 1.0f;
	mv_waterSurfaceVScale = 1.0f;

	AddVariable( mv_waterVolumeDepth, "VolumeDepth", "Depth", functor( *this, &CRiverObject::OnRiverPhysicsParamChange ) );
	AddVariable( mv_waterStreamSpeed, "StreamSpeed", "Speed", functor( *this, &CRiverObject::OnRiverPhysicsParamChange ) );
	AddVariable( mv_waterFogDensity, "FogDensity", "FogDensity", functor( *this, &CRiverObject::OnRiverParamChange ) );
	AddVariable( mv_waterFogColor, "FogColor", "FogColor", functor( *this, &CRiverObject::OnRiverParamChange ), IVariable::DT_COLOR );
	AddVariable( mv_waterFogColorMultiplier, "FogColorMultiplier", "FogColorMultiplier", functor( *this, &CRiverObject::OnRiverParamChange ) );
	AddVariable( mv_waterSurfaceUScale, "UScale", "UScale", functor( *this, &CRiverObject::OnParamChange ) );
	AddVariable( mv_waterSurfaceVScale, "VScale", "VScale", functor( *this, &CRiverObject::OnParamChange ) );
}

//////////////////////////////////////////////////////////////////////////
Vec3 CRiverObject::GetPremulFogColor() const
{
	return Vec3( mv_waterFogColor ) * max( float( mv_waterFogColorMultiplier ), 0.0f );
}

//////////////////////////////////////////////////////////////////////////
void CRiverObject::OnRiverParamChange( IVariable* var )
{
	for( size_t i( 0 ) ; i < m_sectors.size(); ++i )
	{
		if( m_sectors[i].m_pRoadSector )
		{
			IWaterVolumeRenderNode* pRenderNode( static_cast< IWaterVolumeRenderNode* >( m_sectors[i].m_pRoadSector ) );
			pRenderNode->SetFogDensity( mv_waterFogDensity );
			pRenderNode->SetFogColor( GetPremulFogColor() );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRiverObject::Physicalize()
{
	if( !m_sectors.empty() && m_sectors[0].m_pRoadSector )
	{
		IWaterVolumeRenderNode* pRenderNode( static_cast< IWaterVolumeRenderNode* >( m_sectors[0].m_pRoadSector ) );

		pRenderNode->Dephysicalize();

		//////////////////////////////////////////////////////////////////////////
		size_t numRiverSegments( m_sectors.size() );

		std::vector< Vec3 > outline;
		outline.reserve( 2 * ( numRiverSegments + 1 ) );
	
		for( size_t i( 0 ); i < numRiverSegments; ++i )
			outline.push_back( m_sectors[ i ].points[1] );

		outline.push_back( m_sectors[ numRiverSegments - 1 ].points[3] );
		outline.push_back( m_sectors[ numRiverSegments - 1 ].points[2] );
	
		for( size_t i( 0 ); i < numRiverSegments; ++i )
			outline.push_back( m_sectors[ numRiverSegments - 1 - i ].points[0] );

		pRenderNode->SetRiverPhysicsArea( &outline[0], outline.size(), mv_waterVolumeDepth, mv_waterStreamSpeed, true );				
		//////////////////////////////////////////////////////////////////////////

		pRenderNode->Physicalize();
	}
}

//////////////////////////////////////////////////////////////////////////
void CRiverObject::OnRiverPhysicsParamChange( IVariable* var )
{
	Physicalize();
}

//////////////////////////////////////////////////////////////////////////
void CRiverObject::UpdateSectors()
{
	IWaterVolumeRenderNode* pOriginatorProxy( 0 );
	if( !m_sectors.empty() )
	{
		const Matrix34& wtm( GetWorldTM() );
		m_fogPlane.SetPlane( wtm.GetColumn2(), m_sectors[0].points[0] );
	}

	if( m_waterVolumeID == -1 )
	{
		m_waterVolumeID = ToEntityGuid( GetId() );
		assert( m_waterVolumeID );
	}

	//////////////////////////////////////////////////////////////////////////
	// base class impl no longer compatible, as a fix we put in previous 
	// implementation of CRoadObject::UpdateSectors(...)
	
	//__super::UpdateSectors(); 
	
	for (int i = 0; i < m_sectors.size(); i++)
	{
		CRoadSector &sector = m_sectors[i];

		if (sector.points[0]!= sector.points[1] && 
			sector.points[0] != sector.points[2] &&
			sector.points[1] != sector.points[2] &&
			sector.points[2] != sector.points[3] &&
			sector.points[1] != sector.points[3])
		{
			UpdateSector( sector );
		}
	}
	//////////////////////////////////////////////////////////////////////////

	Physicalize();
}

//////////////////////////////////////////////////////////////////////////
void CRiverObject::UpdateSector( CRoadSector& sector )
{
  //if( !m_sectors.empty() && &sector != &m_sectors[0] )
  //  return;

	if( !sector.m_pRoadSector )
		sector.m_pRoadSector = GetIEditor()->Get3DEngine()->CreateRenderNode( eERType_WaterVolume );

	bool isHidden = CheckFlags(OBJFLAG_INVISIBLE);

	IWaterVolumeRenderNode* pWaterVolumeRN( static_cast< IWaterVolumeRenderNode* >( sector.m_pRoadSector ) );
	if( pWaterVolumeRN )
	{
		int renderFlags = 0;
		if( isHidden )
			renderFlags = ERF_HIDABLE | ERF_HIDDEN;
		pWaterVolumeRN->SetRndFlags( renderFlags );

		pWaterVolumeRN->SetFogDensity( mv_waterFogDensity );
		pWaterVolumeRN->SetFogColor( GetPremulFogColor() );

		pWaterVolumeRN->CreateRiver( m_waterVolumeID, &sector.points[0], sector.points.size(), 
			sector.t0, sector.t1, Vec2( mv_waterSurfaceUScale, mv_waterSurfaceVScale ), m_fogPlane, true );

		CMaterial* pMat( GetMaterial() );
		if( pMat )
			pMat->AssignToEntity( pWaterVolumeRN );
		else
			sector.m_pRoadSector->SetMaterial( 0 );
	}
}

//////////////////////////////////////////////////////////////////////////
void CRiverObject::Serialize( CObjectArchive& ar )
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
XmlNodeRef CRiverObject::Export( const CString& levelPath, XmlNodeRef& xmlNode )
{
	return 0;
}
