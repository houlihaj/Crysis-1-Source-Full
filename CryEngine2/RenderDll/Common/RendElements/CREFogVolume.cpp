#include "StdAfx.h"
#include "CREFogVolume.h"
#include "../../../CryCommon/IEntityRenderState.h"


CREFogVolume::CREFogVolume()
: CRendElement()
, m_pFogVolume( 0 )
, m_center( 0.0f, 0.0f, 0.0f )
, m_viewerInsideVolume( false )
, m_localAABB( Vec3( -1, -1, -1 ), Vec3( 1, 1, 1 ) )
, m_matWSInv()
, m_fogColor( 1, 1, 1, 1 )
, m_globalDensity( 1 )
, m_softEdgesLerp( 1, 0 )
, m_heightFallOffDirScaled( 0, 0, 1 )
, m_heightFallOffBasePoint( 0, 0, 0 )
, m_eyePosInWS( 0, 0, 0 )
, m_eyePosInOS( 0, 0, 0 )
{
	mfSetType( eDATA_FogVolume );
	mfUpdateFlags( FCEF_TRANSFORM );

	m_matWSInv.SetIdentity();
}


CREFogVolume::~CREFogVolume()
{
}


void CREFogVolume::mfPrepare()
{
	gRenDev->EF_CheckOverflow( 0, 0, this );
	gRenDev->m_RP.m_pRE = this;
	gRenDev->m_RP.m_RendNumIndices = 0;
	gRenDev->m_RP.m_RendNumVerts = 0;
}


float CREFogVolume::mfDistanceToCameraSquared( Matrix34& matInst )
{
	float distSq( 0.0f );
	if( 0 != m_pFogVolume )
		distSq = m_pFogVolume->GetDistanceToCameraSquared();
	return( distSq );
}
