/*=============================================================================
  IllumHandler.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __ILLUMHANDLER_H__
#define __ILLUMHANDLER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Cry_Math.h"

#define NUM_COMPONENTS 4 //max components per IllumHandler

// Spectrum Declarations
class  CIllumHandler 
{
public:

	CIllumHandler(f32 fInitFlux = 0.0f) 
	{
		for (int i = 0; i < NUM_COMPONENTS; ++i)
		{
			m_vLightFlux[i].x = 
			m_vLightFlux[i].y = 
			m_vLightFlux[i].z = 
			m_vLightFlux[i].w =  fInitFlux;
		}

	}

	CIllumHandler(Vec4& vInitFlux) 
	{
		for (int i = 0; i < NUM_COMPONENTS; ++i)
			m_vLightFlux[i] = vInitFlux;
	}

	inline void Clear()
	{
		memset( m_vLightFlux, 0, sizeof(Vec4)*NUM_COMPONENTS );
	}

  CIllumHandler& Add(const Vec3& spectrum, const Vec3& vIrradDir);
	CIllumHandler& Add(const Vec4& spectrum);

	void SetTangentBasis(Matrix33& mInvRot);
	void SetTangentBasis(Vec3& vTangent, Vec3& vBinormal, Vec3& vNormal);

	CIllumHandler operator +(const CIllumHandler& illumHandler) const;
	CIllumHandler& operator +=(const CIllumHandler& illumHandler);
 
	void Scale(Vec4& vScaleFact)
	{
		for (int32 i = 0; i < NUM_COMPONENTS; ++i)
		{
			m_vLightFlux[i].x *= vScaleFact.x;
			m_vLightFlux[i].y *= vScaleFact.y;
			m_vLightFlux[i].z *= vScaleFact.z;
			m_vLightFlux[i].w *= vScaleFact.w;
		}
	}

	void Scale(f32 fScaleFact)
	{
		for (int32 i = 0; i < NUM_COMPONENTS; ++i)
		{
			m_vLightFlux[i].x *= fScaleFact;
			m_vLightFlux[i].y *= fScaleFact;
			m_vLightFlux[i].z *= fScaleFact;
			m_vLightFlux[i].w *= fScaleFact;
		}
	}


/*	void AddWeighted(float w, const Spectrum &s) 
	{
		for (int i = 0; i < NUM_COMPONENT; ++i)
			c[i] += w * s.c[i];
	}

	Spectrum Sqrt() const 
	{
		Spectrum ret;
		for (int i = 0; i < NUM_COMPONENTS; ++i)
			ret.c[i] = sqrtf(c[i]);
		return ret;
	}


			void XYZ(float xyz[3]) const {
				xyz[0] = xyz[1] = xyz[2] = 0.;
				for (int i = 0; i < COLOR_SAMPLES; ++i) {
					xyz[0] += XWeight[i] * c[i];
					xyz[1] += YWeight[i] * c[i];
					xyz[2] += ZWeight[i] * c[i];
				}
			}
			float y() const {
				float v = 0.;
				for (int i = 0; i < COLOR_SAMPLES; ++i)
					v += YWeight[i] * c[i];
				return v;
			}
			bool operator<(const Spectrum &s2) const {
				return y() < s2.y();
			}*/

public:
	static const Vec3 m_vBasisComponents[NUM_COMPONENTS]; //dir components in tangent space

	Vec3 m_vDirComponents[NUM_COMPONENTS]; //dir components in world space

	// Spectrum Private Data
	Vec4 m_vLightFlux[NUM_COMPONENTS];

	//hack: remove
	Vec3 m_vNormal;

};
#endif //__ILLUMHANDLER_H__
