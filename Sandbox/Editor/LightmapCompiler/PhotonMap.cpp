/*=============================================================================
  PhotonMap.cpp : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#include "stdafx.h"
#include "SceneContext.h"
#include "PhotonMap.h"
#include "CryArray.h"

// Define static members in the PhotonMap's file scope
bool CPhotonMap::m_bTablesInited	=	false;
float		CPhotonMap::m_pCosTheta[256];
float		CPhotonMap::m_pSinTheta[256];
float		CPhotonMap::m_pCosPhi[256];
float		CPhotonMap::m_pSinPhi[256];

#define	PHOTON_CAUSTICS		1
#define	PHOTON_EXPLOSION	2
#define	PHOTON_POINT		4
#define	PHOTON_DISTANT		8
#define	PHOTON_AREA			16


//===================================================================================
// Method				:	CPhotonMap
CPhotonMap::CPhotonMap(const char *n,const Matrix34& world) : CMultiDimMap<CPhoton>() 
{

	//init conversion tables
	if (m_bTablesInited == false) 
	{
		m_bTablesInited	=	true;

		for (int i=0;i<256;i++) 
		{
			const double	dAngle	=	(double) i * (1.0 / 256.0) * g_PI;
			m_pCosTheta[i]		=	(float) cos(dAngle);
			m_pSinTheta[i]		=	(float) sin(dAngle);
			m_pCosPhi[i]		=	(float) cos(2 * dAngle);
			m_pSinPhi[i]		=	(float) sin(2 * dAngle);
		}
	}

	//FILE	*in		=	ropen(name,"rb",filePhotonMap,TRUE);

	m_root			=	NULL;
	m_nMaxDepth		=	0;
	m_nRefCount		=	0;
	m_bModifying		=	false;
	m_fMaxPower		=	0;

	{
		// Reset the transformation matrices
		//fix: choose right transformation
		m_mToCamera = world;
		m_mFromCamera = world;
		m_mFromCamera.Invert();
	}
}

//===================================================================================
// Method					:	~CPhotonMap
// Description		:	Destructor
CPhotonMap::~CPhotonMap() 
{
	if (m_root != NULL) 
	{
		CPhotonNode		**stackBase	=	(CPhotonNode **)	alloca(m_nMaxDepth*sizeof(CPhotonNode *)*8);
		CPhotonNode		**stack;
		CPhotonNode		*cNode;
		CPhotonSample	*cSample;
		int			i;

		stack		=	stackBase;
		*stack++	=	m_root;
		while(stack > stackBase) {
			cNode	=	*(--stack);

			while((cSample=cNode->samples) != NULL) {
				cNode->samples	=	cSample->next;
				delete cSample;
			}

			for (i=0;i<8;i++) {
				if (cNode->children[i] != NULL) *stack++	=	cNode->children[i];
			}

			delete cNode;
		}
	}
}


//===================================================================================
// Method				:	reset
// Description			:	Reset the photonmap
void	CPhotonMap::Reset() 
{
	CMultiDimMap<CPhoton>::Reset();
}



//===================================================================================
// Method				:	probe
// Description	:
bool CPhotonMap::Probe(Vec3* C,const Vec3& P,const Vec3& N) 
{
	CPhotonNode			*cNode;
	CPhotonNode			**stackBase	=	(CPhotonNode **)	alloca(m_nMaxDepth*sizeof(CPhotonNode *)*8);
	CPhotonNode			**stack;
	CPhotonSample		*cSample;
	float				totalWeight	=	0;
	int					i;

	if (m_root == NULL) return false;

	stack			=	stackBase;
	*stack++		=	m_root;
	C->Set(0,0,0);
	while(stack > stackBase) 
	{
		cNode	=	*(--stack);

		// Iterate over the samples
		for (cSample=cNode->samples;cSample!=NULL;cSample=cSample->next)
		{
			Vec3	D;
			float	d;
			
			D  = cSample->P - P;

			d	=	D * D;

			//fix::!!! unification of urand func
			if (d < (cSample->dP*cSample->dP)/**rand()*/) //fix rand
			{
				d	=	(float) sqrt(d);
				d	+=	(float) fabs( D * (cSample->N))*16;

				if (d < cSample->dP) 
				{
					float	weight	=	1 - d / cSample->dP;

					totalWeight		+=	weight;
					C->x			+=	cSample->C.x * weight;
					C->y			+=	cSample->C.y * weight;
					C->z			+=	cSample->C.z * weight;
				}
			}
		}

		// Check the children
		for (i=0;i<8;i++) 
		{
			CPhotonNode	*tNode	=	cNode->children[i];

			if (tNode != NULL) 
			{
				const	float	tSide	=	tNode->side;

				if (	((tNode->center[0] + tSide) > P.x)	&&
						((tNode->center[1] + tSide) > P.y)	&&
						((tNode->center[2] + tSide) > P.z)	&&
						((tNode->center[0] - tSide) < P.x)	&&
						((tNode->center[1] - tSide) < P.y)	&&
						((tNode->center[2] - tSide) < P.z) )
				{
					*stack++	=	tNode;
				}
			}
		}
	}

	if (totalWeight > 0) 
	{
		//fix by references
		(*C) *= (1.0f / totalWeight);
		return true;
	}

	return false;
}


//===================================================================================
// Method				:	insert
// Description			:	Insert a sample
void	CPhotonMap::Insert(const Vec3& C,const Vec3&  P,const Vec3& N,float dP) 
{
	CPhotonSample	*cSample	=	new CPhotonSample;
	CPhotonNode		*cNode		=	m_root;
	int				depth		=	0;
	int				i,j;

	cSample->C = C;
	cSample->P = P;
	cSample->N = N;
	cSample->dP	=	dP;

	while(cNode->side > (2*dP)) 
	{
		depth++;

		for (j=0,i=0;i<3;i++) 
		{
			if (P[i] > cNode->center[i]) 
			{
				j	|=	1 << i;
			}
		}

		if (cNode->children[j] == NULL)	{
			CPhotonNode	*nNode	=	(CPhotonNode *) new CPhotonNode;

			for (i=0;i<3;i++)
			{
				if (P[i] > cNode->center[i]) 
				{
					nNode->center[i]	=	cNode->center[i] + cNode->side / (float) 4;
				} else 
				{
					nNode->center[i]	=	cNode->center[i] - cNode->side / (float) 4;
				}
			}

			cNode->children[j]	=	nNode;
			nNode->side			=	cNode->side / 2.0f;
			nNode->samples		=	NULL;
			for (i=0;i<8;i++)	nNode->children[i]	=	NULL;
		}

		cNode			=	cNode->children[j];
	}

	cSample->next	=	cNode->samples;
	cNode->samples	=	cSample;
	m_nMaxDepth		=	max(m_nMaxDepth,depth);
}

//===================================================================================
// Method				:	lookup
// Description			:	Locate the nearest maxFoundPhoton photons
//===================================================================================
void	CPhotonMap::Lookup(Vec3* Cl,const Vec3& Pl,const Vec3& Nl,int maxFound) 
{
	int		numFound	=	0;
	const CPhoton	**indices	=	(const CPhoton **)	alloca((maxFound+1)*sizeof(CPhoton *)); 
	float			*distances	=	(float	*)			alloca((maxFound+1)*sizeof(float)); 
	CLookup			l;

	f32 fRadiusAngle = CSceneContext::GetInstance().m_fMaxPhotonSearchRadius;

	m_fSearchRadius		=	(float) (sqrt(maxFound*m_fMaxPower / (float) fRadiusAngle) / gf_PI)*(float) 0.5;

	distances[0]		=	m_fSearchRadius*m_fSearchRadius;

	l.nMaxFound			=	maxFound;
	l.nNumFound			=	0;
	l.P = /*m_mToCamera * */Pl;
	l.N = /*m_mFromCamera * */Nl;
	l.bGotHeap			=	false;
	l.indices			=	indices;
	l.pfDistances			=	distances;

	if (!Probe(Cl,l.P,l.N)) 
	{
		CMultiDimMap<CPhoton>::LookupWithN(&l,1);

		Cl->Set(0,0,0);

		//fix:: add constant
		if (l.nNumFound < 2)	return;

		numFound	=	l.nNumFound;

		//Fix:: actual integration stuff
		//include light handler here for integration accomulation!
		for (int i=1;i<=numFound;i++) 
		{
			const	CPhoton	*p	=	indices[i];
			Vec3	I;

			assert(distances[i] <= distances[0]);

			PhotonToDir(I,p->theta,p->phi);

			if ((I * Nl) < 0) 
			{
				*Cl  += p->C;
			}
		}
		
		const	float	tmp	=	(float) (1.0 / gf_PI)/distances[0];

		(*Cl) *= tmp;

		Insert((*Cl),l.P,l.N,(float) sqrt(distances[0])*(float) 0.2);
	}
}

//===================================================================================
// Method				:	lookup
// Description			:	Locate the nearest maxFoundPhoton photons
void	CPhotonMap::Lookup(Vec3* Cl,const Vec3& Pl,int maxFound) 
{
	int				numFound	=	0;
	const CPhoton	**indices	=	(const CPhoton **)	alloca((maxFound+1)*sizeof(CPhoton *)); 
	float			*distances	=	(float	*)			alloca((maxFound+1)*sizeof(float)); 
	CLookup			l;

	f32 fRadiusAngle = CSceneContext::GetInstance().m_fMaxPhotonSearchRadius;

	m_fSearchRadius		=	(float) (sqrt(maxFound*m_fMaxPower / (float) fRadiusAngle) / gf_PI)*(float) 0.5;

	distances[0]		=	m_fSearchRadius*m_fSearchRadius;

	l.nMaxFound			=	maxFound;
	l.nNumFound			=	0;

	l.P = /*m_mToCamera * */Pl;
	l.N.Set(0,0,0);
	l.bGotHeap			=	false;
	l.indices			=	indices;
	l.pfDistances			=	distances;

	if (!Probe(Cl,l.P,l.N)) 
	{
 		CMultiDimMap<CPhoton>::Lookup(&l,1);

		Cl->Set(0,0,0);

		if (l.nNumFound < 2)	return;
		else
		{
			Cl->Set(0,0,0);
		}


		numFound	=	l.nNumFound;

		for (int i=1;i<=numFound;i++) 
		{
			const	CPhoton	*p	=	indices[i];

			assert(distances[i] <= distances[0]);

		 (*Cl)+=(p->C);
		}
		
		const	float	tmp	=	(float) (1.0 / gf_PI)/distances[0];

		(*Cl) *= tmp;

		//fix:: unification of references
		Insert(*Cl,l.P,l.N,(float) sqrt(distances[0])*(float) 0.2);
	}
}

short		CPhotonMap::ShadowLookup(const Vec3& Pl,const Vec3& Nl)
{
	const CPhoton	*indices[2]; 
	float			distances[2]; 
	CLookup			l;

	f32 fRadiusAngle = CSceneContext::GetInstance().m_fMaxPhotonSearchRadius;
	m_fSearchRadius		=	(float) (sqrt(1 / (float) fRadiusAngle) / gf_PI)*(float) 0.5;

	distances[0]		=	m_fSearchRadius*m_fSearchRadius;

	l.nMaxFound			=	1;
	l.nNumFound			=	0;
	l.P							= Pl;
	l.N							= Nl;
	l.bGotHeap			=	false;
	l.indices				=	indices;
	l.pfDistances		=	distances;

	CMultiDimMap<CPhoton>::Lookup(&l,1);
//	CMultiDimMap<CPhoton>::LookupWithN(&l,1);
	return l.nNumFound < 1 ? 0 : indices[1]->flags;
}


void	CPhotonMap::PreprocessShadowPhotons()
{
	//check all photons if needed...
//	int32 maxFound = CSceneContext::GetInstance().m_numEmitPhotons;
	int32 maxFound = CSceneContext::GetInstance().m_nPhotonEstimator;
	const CPhoton	**indices	=	(const CPhoton **)	alloca((maxFound+1)*sizeof(CPhoton *)); 
	float			*distances	=	(float	*)			alloca((maxFound+1)*sizeof(float)); 
	Vec3	I;
	CLookup			l;

	f32 fRadiusAngle = CSceneContext::GetInstance().m_fMaxPhotonSearchRadius;
	m_fSearchRadius		=	(float) (sqrt(maxFound / (float) fRadiusAngle) / gf_PI)*(float) 0.5;

	const PodArray<CDLight*>*	pListLights	=	CSceneContext::GetInstance().GetLights();
	CDLight* pLight	=	pListLights->GetAt( 0 );

//	m_fSearchRadius		=	(float) (sqrt(maxFound*m_fMaxPower / (float) fRadiusAngle) / gf_PI)*(float) 0.5;

	for( int32 nPNum = 1;  nPNum < m_numPhotons; ++nPNum )
	{
		CPhoton *pActualPhoton = &m_pPhotons[nPNum];

//		if( pLight->m_vAreaSize.x )
//			m_fSearchRadius = (pLight->m_BaseOrigin - pActualPhoton->P).GetLength() * pLight->m_vAreaSize.x;

		//############## BROKEN ##########################
		m_fSearchRadius = (pLight->m_BaseOrigin - pActualPhoton->P).GetLength() * 0.5f;
		//############## BROKEN ##########################
//			m_fSearchRadius = pLight->m_vAreaSize.x;

		PhotonToDir(I,pActualPhoton->theta,pActualPhoton->phi);

		distances[0]		=	m_fSearchRadius*m_fSearchRadius;

		l.nMaxFound			=	maxFound;
		l.nNumFound			=	0;
		l.P							= pActualPhoton->P;
		l.N							= I;
		l.bGotHeap			=	false;
		l.indices				=	indices;
		l.pfDistances		=	distances;

		//the photon itself
		bool			bAllInLight = ( pActualPhoton->flags & 0x10 );
		bool			bAllInShadow = ( pActualPhoton->flags & 0x20 );

//		CMultiDimMap<CPhoton>::LookupWithN(&l,1);
		CMultiDimMap<CPhoton>::Lookup(&l,1);

		for (int i=1;i<=l.nNumFound;i++) 
		{
			const	CPhoton	*p	=	indices[i];
			if( p->flags & 0x10 )
				bAllInShadow = false;
			else
				if( p->flags & 0x20 )
						bAllInLight = false;
		}

		if( bAllInLight )
			pActualPhoton->flags |= 0x100;
		else 
			if( bAllInShadow )
				pActualPhoton->flags |= 0x200;
			else
				pActualPhoton->flags |= 0x300;
	}
}

//===================================================================================
// Method				:	IllumLookup
// Description			:	Locate the nearest maxFoundPhoton photons
//===================================================================================
void	CPhotonMap::IllumLookup(CIllumHandler& illumHandler,const Vec3& Pl,const Vec3& Nl,int maxFound) 
{
	int		numFound	=	0;
	const CPhoton	**indices	=	(const CPhoton **)	alloca((maxFound+1)*sizeof(CPhoton *)); 
	float			*distances	=	(float	*)			alloca((maxFound+1)*sizeof(float)); 
	CLookup			l;
	Vec3 Cl;

	f32 fRadiusAngle = CSceneContext::GetInstance().m_fMaxPhotonSearchRadius;

	m_fSearchRadius		=	(float) (sqrt(maxFound*m_fMaxPower / (float) fRadiusAngle) / gf_PI)*(float) 0.5;

	distances[0]		=	m_fSearchRadius*m_fSearchRadius;

	l.nMaxFound			=	maxFound;
	l.nNumFound			=	0;
	l.P = /*m_mToCamera * */Pl;
	l.N = /*m_mFromCamera * */Nl;
	l.bGotHeap			=	false;
	l.indices			=	indices;
	l.pfDistances			=	distances;

	//if (!Probe(Cl,l.P,l.N)) 
	//{
		//FIX: The N parameter is working wrongly, need time to investigate...
		//CMultiDimMap<CPhoton>::LookupWithN(&l,1);
		CMultiDimMap<CPhoton>::Lookup(&l,1);

		//Cl->Set(0,0,0);

		//fix:: add constant
		if (l.nNumFound < 2)	return;

		numFound	=	l.nNumFound;

		//Fix:: actual integration stuff
		//include light handler here for integration accomulation!
		for (int i=1;i<=numFound;i++) 
		{
			const	CPhoton	*p	=	indices[i];
			Vec3	I;

			assert(distances[i] <= distances[0]);

			PhotonToDir(I,p->theta,p->phi);

			if ((I * Nl) < 0) 
			{
				//*Cl  += p->C;
				Vec3 vCScaled = (p->C) * ((float) (1.0 / gf_PI)/distances[0]);
				illumHandler.Add(vCScaled, I);
			}
		}

		//const	float	tmp	=	(float) (1.0 / gf_PI)/distances[0];

		//(*Cl) *= tmp;

		//illumHandler.Scale();

		//Insert((*Cl),l.P,l.N,(float) sqrt(distances[0])*(float) 0.2);
	//}
}

//===================================================================================
// Method				:	balance
// Description			:	Balance the map
void	CPhotonMap::Balance() 
{
	// If we have no photons in the map, add a dummy one to avoid an if statement during the lookup
	if (m_numPhotons == 0) {
		Vec3	P(0,0,0);
		Vec3	I(0,0,1);
		CPhoton	*photon	=	CMultiDimMap<CPhoton>::Store(P,I);
		photon->C.Set(0,0,0);
	}

	CMultiDimMap<CPhoton>::Balance();

	//////////////////////////
	//set root
	m_root	=	new CPhotonNode;
	m_root->center = m_BBox.min + m_BBox.max;
	m_root->center *=1.0f/2.0f;
	m_root->side		=	max(max(m_BBox.max.x - m_BBox.min.x, 
													m_BBox.max.y - m_BBox.min.y),
													m_BBox.max.z - m_BBox.min.z );
	m_root->samples	=	NULL;
	for (int i=0;i<8;i++) 
		m_root->children[i]	=	NULL;

}

//fix: implement
bool CPhotonMap::Illuminance(CIllumHandler& illumHandler, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache)
{
	return true;
}

bool CPhotonMap::Illuminance(CIllumHandler& illumHandler, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache )
{
/*
	short nResult = ShadowLookup( vPosition,vAxis ) & 0xfff00;


	switch( nResult )
	{
case 0:
	//photon not found
		illumHandler.Add( Vec4( 1,0,0,1 ) );
		break;
	case 0x100:
		//illuminated
		illumHandler.Add( Vec4( 1,1,1,1 ) );
		break;
	case 0x200:
		//shadowed
		illumHandler.Add( Vec4( 0,1,0,1 ) );
		break;
	case 0x300:
		//preumbra
		illumHandler.Add( Vec4( 0,0,1,1 ) );
		break;
	}

		return;
*/


	//fix: handle this params in future GI compiling
	int32 nPhotonEstimator = CSceneContext::GetInstance().m_nPhotonEstimator;
	//f32 fMaxBrightness = CSceneContext::GetInstance().m_fMaxBrightness;

	IllumLookup(illumHandler, vPosition, vAxis, nPhotonEstimator);
	//Lookup(&vCl, vPosition, nPhotonEstimator);

	//Avoid too bright spots
	//f32 fTmp	=	max(max(vCl.x,vCl.y),vCl.z);
	//if (fTmp > fMaxBrightness)	
	//	vCl *= ((fMaxBrightness)/fTmp);

	//spectrum = vCl ;

	return true;
}
