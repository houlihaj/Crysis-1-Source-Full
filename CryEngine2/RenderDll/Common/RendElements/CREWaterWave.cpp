#include "StdAfx.h"
#include "CREWaterWave.h"


CREWaterWave::CREWaterWave(): CRendElement(), m_pParams( 0 )
{
	mfSetType( eDATA_WaterWave );
	mfUpdateFlags( FCEF_TRANSFORM );
}


CREWaterWave::~CREWaterWave()
{

}

void CREWaterWave::mfGetPlane(Plane& pl)
{
  //pl = m_pParams->m_fogPlane;
  //pl.d = -pl.d;
}

void CREWaterWave::mfCenter(Vec3& pCenter, CRenderObject *pObj)
{
  pCenter = m_pParams->m_pCenter;
  if (pObj)
    pCenter += pObj->GetTranslation();
}

void CREWaterWave::mfPrepare()
{
  gRenDev->EF_CheckOverflow( 0, 0, this );
  gRenDev->m_RP.m_pRE = this;
  gRenDev->m_RP.m_RendNumIndices = 0;
  gRenDev->m_RP.m_RendNumVerts = 0;
}


float CREWaterWave::mfDistanceToCameraSquared( Matrix34& matInst )
{
	assert( m_pParams );
	if( !m_pParams )
		return 0;

	CRenderer* rd( gRenDev );
	Vec3 delta( rd->GetRCamera().Orig - m_pParams->m_pCenter );
	return delta.GetLengthSquared();
}
