/*=============================================================================
	Ray.h :
	Copyright 2004 Crytek Studios. All Rights Reserved.

	Revision history:
	* Created by Nick Kasyan
=============================================================================*/
#ifndef __RAY_H__
#define __RAY_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Cry_Math.h"

class CRay  
{
	Vec3		m_HitNormal;
public:
	CRay():
	pMaterial(NULL)
	{
	}

	inline void CalcInvDir()
	{
		nSign[0] = vDir.x < 0.f ? 3 : 0;
		nSign[1] = vDir.y < 0.f ? 3 : 0;
		nSign[2] = vDir.z < 0.f ? 3 : 0;
		vInvDir.x = 1.f / vDir.x;
		vInvDir.y = 1.f / vDir.y;
		vInvDir.z = 1.f / vDir.z;
	}

public:

	//////////////////////////////////////////////////////////////////////////
	Vec3				vFrom;			// The ray in global space
	Vec3				vDir,vInvDir;			// 1 / dir
	int32				nSign[3];			// is the direction paralell to the main axises
	float				fT;							// The maximum intersection
	float				fTmin;					// The intersection bias

	Vec3				vOrigNormal;

	int					flags;					// The flags that must be on in the object's attributes
	//////////////////////////////////////////////////////////////////////////
	//	Intersection parameters
	//////////////////////////////////////////////////////////////////////////
	//ITracable			*object;			// The intersection object
	float				fU,fV;					// The parametric intersection coordinates
	Vec3				vN;							// The normal vector at the intersection
	Vec3				vP;							// Position of the intersection

	//////////////////////////////////////////////////////////////////////////
//	Vec3					vObjFrom,vObjTo,vObjDir;				// The ray in object space
	//Matrix34					*lastTransform;				// Pointer to the transform matrix for which oFrom,oTo,oDir are valid
//	unsigned long	ID;								// Last checked object ID
	struct IRenderShaderResources*	pMaterial;
//	CRay					*pChild;					// For keeping track of the opacity

	const Vec3&				HitNormal()	const{return m_HitNormal;}
	void							HitNormal(const Vec3& rHitNormal){m_HitNormal	=	rHitNormal;}
};

#endif

