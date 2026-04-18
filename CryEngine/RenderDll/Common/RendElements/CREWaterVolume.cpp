#include "StdAfx.h"
#include "CREWaterVolume.h"


CREWaterVolume::CREWaterVolume()
: CRendElement()
, m_pParams( 0 )
, m_pOceanParams( 0 )
, m_drawWaterSurface( false )
{
	mfSetType( eDATA_WaterVolume );
	mfUpdateFlags( FCEF_TRANSFORM );
}


CREWaterVolume::~CREWaterVolume()
{
}


void CREWaterVolume::mfGetPlane(Plane& pl)
{
  pl = m_pParams->m_fogPlane;
  pl.d = -pl.d;
}

void CREWaterVolume::mfCenter(Vec3& vCenter, CRenderObject *pObj)
{
  vCenter = m_pParams->m_center;
  if (pObj)
    vCenter += pObj->GetTranslation();
}

void CREWaterVolume::mfPrepare()
{
  gRenDev->EF_CheckOverflow( 0, 0, this );
  gRenDev->m_RP.m_pRE = this;
  gRenDev->m_RP.m_RendNumIndices = 0;
  gRenDev->m_RP.m_RendNumVerts = 0;
}


float CREWaterVolume::mfDistanceToCameraSquared( Matrix34& matInst )
{
	assert( m_pParams );
	if( !m_pParams )
		return 0;

	CRenderer* rd( gRenDev );
	Vec3 delta( rd->GetRCamera().Orig - m_pParams->m_center );
	return delta.GetLengthSquared();
}
