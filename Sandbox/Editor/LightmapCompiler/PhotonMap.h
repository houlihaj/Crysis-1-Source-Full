/*=============================================================================
  PhotonMap.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __PHOTONMAP_H__
#define __PHOTONMAP_H__

#if _MSC_VER > 1000
# pragma once
#endif

//#define _DEBUG

#include "MultiDimMap.h"
#include "IlluminanceIntegrator.h"

//===================================================================================
// Class				:	CPhotonMap
// Description			:	A Photon map implementation
class	CPhotonMap : public CMultiDimMap<CPhoton> , public CIlluminanceIntegrator
{

	class	CPhotonSample 
	{
	public:
		Vec3			C,P,N;
		float			dP;
		CPhotonSample	*next;
	};

	class	CPhotonNode 
	{
	public:
		Vec3			center;
		float			side;
		CPhotonSample	*samples;
		CPhotonNode		*children[8];
	};

protected:

	//===================================================================================
	// Method			:	DirToPhoton 
	// Description:	pack directions for photon map's data sorage
	//	should be defined in the header scope
	static void DirToPhoton(unsigned char& nTheta,unsigned char& nPhi,const Vec3& vD)	
	{
		int32 iT,iP;
		iT	=	(int32) (acos_tpl(vD.z)*(256.0 / g_PI));
		iP	=	(int32) (atan2(vD.y,vD.x)*(256.0 / (2.0*g_PI)));
		if (iT > 255)
			nTheta	=	(unsigned char) 255;
		else
			nTheta	=	(unsigned char) iT;

		if (iP > 255)
			nPhi		=	(unsigned char) 255;
		else if (iP < 0)
			nPhi		=	(unsigned char) (iP + 256);
		else
			nPhi		=	(unsigned char) iP;

		return;
	}

	//===================================================================================
	// Method			:	PhotonToDir 
	// Description:	unpack directions for photon map's data sorage
	//	should be defined in the header scope
	static void PhotonToDir(Vec3& vD, unsigned char nTheta,unsigned char nPhi)	
	{
		vD.x	=	CPhotonMap::m_pSinTheta[ nTheta ]*CPhotonMap::m_pCosPhi[ nPhi ];
		vD.y	=	CPhotonMap::m_pSinTheta[ nTheta ]*CPhotonMap::m_pSinPhi[ nPhi ];
		vD.z	=	CPhotonMap::m_pCosTheta[ nTheta ];

		return;
	}

public:
	CPhotonMap(const char *,const Matrix34&);
	~CPhotonMap();

	void		Attach()	{	m_nRefCount++;	}
	void		Detach()	{	m_nRefCount--; if (m_nRefCount <= 0) delete this; }
	void		Check()		{	if (m_nRefCount == 0)	delete this;			}

	void		Reset();
	//Fix:: implement
	//void		write(const Matrix34& *);

	bool		Probe(Vec3* C,const Vec3& P,const Vec3& N);
	void		Insert(const Vec3& C,const Vec3&  P,const Vec3& N,float dP);

	void		Lookup(Vec3* Cl,const Vec3& Pl,int maxFound);
	void		Lookup(Vec3* Cl,const Vec3& Pl,const Vec3& Nl,int maxFound);
	void		IllumLookup(CIllumHandler& illumHandler,const Vec3& Pl,const Vec3& Nl,int maxFound);
	short		ShadowLookup(const Vec3& Pl,const Vec3& Nl);
	void		Balance();
	void		PreprocessShadowPhotons();

	void		Store(const Vec3& P,const Vec3& N,const Vec3& I,const Vec3& C,const short nFlags = 0) 
	{
					CPhoton	*ton	=	CMultiDimMap<CPhoton>::Store(P,N);
					DirToPhoton(ton->theta,ton->phi,I);
					ton->C = C;
					ton->flags |= nFlags;
					m_fMaxPower	=	max(m_fMaxPower,C * C);
	}

	//Illumimance Intagrator's functions
	bool Illuminance(CIllumHandler& illumHandler, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache );
	bool Illuminance(CIllumHandler& illumHandler, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache );


public:
	bool			m_bModifying;
protected:
	CPhotonNode	*m_root;
	int				m_nMaxDepth;			// The maximum depth of the hierarchy
	int				m_nRefCount;
	Matrix34	m_mToCamera,m_mFromCamera;
	float		m_fMaxPower;			// The maximum photon power
	float		m_fSearchRadius;

private:
	static bool		m_bTablesInited;
	static float		m_pCosTheta[256];
	static float		m_pSinTheta[256];
	static float		m_pCosPhi[256];
	static float		m_pSinPhi[256];

};


#endif
