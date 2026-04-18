/*=============================================================================
AmbientOcclusionIntegrator.h : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef __AMBIENTOCCLUSIONINTEGRATOR_H__
#define __AMBIENTOCCLUSIONINTEGRATOR_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IlluminanceIntegrator.h"
#include "SceneContext.h"

#include "CryArray.h"

class	CAmbientOcclusionIntegrator : public CIlluminanceIntegrator
{
public:
	CAmbientOcclusionIntegrator () : m_pBoundingCell(NULL)
	{
	};

	virtual	~CAmbientOcclusionIntegrator()
	{
	};

	void Init();
	//Illuminance statements
	//fix: replace return value by multu-samplies color
	bool Illuminance(CIllumHandler&, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache);
	bool Illuminance(CIllumHandler&, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache );
protected:

	inline void CreateUniformDistribution( const f32 fsquareX, const f32 fsquareY, f32& fTheta, f32& fPhi ) const
	{
		f32 x,y,fOctant;
		//from [0,1] to [-1,1]
		f32 fX = fsquareX*2-1;
		f32 fY = fsquareY*2-1;

		//check the octants
		if( fY > -fX )
		{
			if( fX > fY )
			{
				x = fX;
				if( 0 < fY )
				{
					fOctant = 0;
					y = fY;
				}
				else
				{
					fOctant = 7;
					y = fY + fX;
				}
			}
			else
			{
				x = fY;
				if( 0 < fX )
				{
					fOctant = 1;
					y = fY-fX;
				}
				else
				{
					fOctant = 2;
					y = -fX;
				}
			}
		}
		else
		{
			if( fX < fY )
			{
				x = -fX;
				if( 0 < fY )
				{
					fOctant = 3;
					y = -fY-fX;
				}
				else
				{
					fOctant = 4;
					y = -fY;
				}
			}
			else
			{
				x = -fY;
				if( 0 < fX )
				{
					fOctant = 6;
					y = fX;
				}
				else
				{
					if( fY != 0 )
					{
						fOctant = 5;
						y = fX-fY;
					}
					else
					{
						x = 0;
						y = 0;
						fOctant = 0;
					}
				}
			}
		}

		if(fabsf(x)<=FLT_EPSILON)
			x	=	FLT_EPSILON;
		fTheta = acos_tpl( 1 - x*x );
		fPhi = (fOctant + y / x ) * gf_PI* 0.25f;
	}

	virtual void SetBoundingCell( COctreeCell* pCell )
	{
		m_pBoundingCell = pCell;
	};

	COctreeCell* m_pBoundingCell;
	CGeomCore*	pGeomCore;
	int32 numSamples;
	f32		fMinDistance,fMaxDistance;
	int32 iM1,iM2;
	f32		fOnePerNumSamples,fDivider;
	CIntersectionCacheCluster m_pIntersectionCache;
};

#endif