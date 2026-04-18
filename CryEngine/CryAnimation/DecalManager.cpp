//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:DecalManager.cpp
//  Implementation of DecalManager class
//
//	History:
//	August 16, 2004: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <CryHeaders.h>

#include "CryEngineDecalInfo.h"
#include "DecalManager.h"
#include "Model.h"
#include "CharacterInstance.h"




// the array of transformed vertices (where 0,0,0 is the hit point, and 0,0,1 is the direction of the bullet)
// in other words, it's the Vertices in Bullet Coordinate System
std::vector<Vec3> g_arrVerticesBS;
// the mapping from the character vertices (in the external indexation) to the decal vertices
// -1 means the vertex has not yet been mapped
// It is always true that an element is >= -1 and < m_arrDecalVertices.size()
std::vector<int32> g_arrDecalVertexMapping;
std::vector<int32> g_arrDecalFaceMapping;



//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

size_t CAnimDecalManager::SizeOfThis ()
{
	size_t DecalInfoMem = sizeof(CryEngineDecalInfo)*m_arrDecalRequests.size();
	size_t DecalsMem=0;
	size_t numDecals = m_arrDecals.size();
	for (uint32 i=0; i<numDecals; i++)
		DecalsMem	+=	m_arrDecals[i].GetSize();

	return DecalInfoMem+DecalsMem;
}

//----------------------------------------------------------------------------------
//-------     Spawn decal on the base-model and on all slave-model           -------
//----------------------------------------------------------------------------------
void CAnimDecalManager::CreateDecalsOnInstance(QuatTS* parrNewSkinQuat, CCharacterModel* pModel, CMorphing* pMorphing, f32* pShapeDeform, CryEngineDecalInfo& DecalLCS, const QuatT& rPhysEntityLocation )
{

	FUNCTION_PROFILER( gEnv->pSystem, PROFILE_ANIMATION );

	// The decal info in the local coordinate system
	CryEngineDecalInfo DecalLocal = DecalLCS;
	DecalLocal.fSize *= Console::GetInst().ca_DecalSizeMultiplier * float(0.4 + (rand()&0x7F)*0.001953125);
	DecalLocal.fAngle = (rand()&0xFF)*float(2*gf_PI/256.0f);
	m_arrDecalRequests.push_back(DecalLocal);

	OBB obb;
	uint32 count = pModel->m_arrCollisions.size();
	for (uint32 i=1; i<count; i++)
	{
		int32 id	=	pModel->m_arrCollisions[i].m_iBoneId;
		AABB aabb	=	pModel->m_arrCollisions[i].m_aABB;
		QuatTS wjoint=rPhysEntityLocation*parrNewSkinQuat[id];
		obb.SetOBBfromAABB(wjoint.q, aabb);
		pModel->m_arrCollisions[i].m_OBB = obb;
		pModel->m_arrCollisions[i].m_Pos = wjoint.t;
	//	g_pAuxGeom->DrawOBB(obb,wjoint.t,0,RGBA8(0xff,0xff,0xff,0xff),eBBD_Extremes_Color_Encoded);
	}


	// if there are decal requests, then converts them into the decal objects
	// reserves the vertex/updates the index buffers, if need to be
	// sets m_bNeedUpdateIndices to true if it has added something (and therefore refresh of indices is required)
	uint32 numDecalRequests = m_arrDecalRequests.size();
	if (numDecalRequests==0)
		CryError("Decals Zero2");

	IMaterialManager *pMatMan = g_pI3DEngine->GetMaterialManager();

	CModelMesh* pMesh = pModel->GetModelMesh(pModel->m_nBaseLOD);
	uint32 numIntVertices	= pMesh->GetVertextCount();//arrSkinnedVertices.size();
	uint32 numIntFaces		= pMesh->m_numExtTriangles;
	uint32 numVMappings = g_arrDecalVertexMapping.size();
	if (numVMappings<numIntVertices)
		g_arrDecalVertexMapping.resize(numIntVertices);

	uint32 numVerticesBCS = g_arrVerticesBS.size();
	if (numVerticesBCS<numIntVertices)
		g_arrVerticesBS.resize(numIntVertices);

	if (g_arrDecalFaceMapping.size() <numIntFaces* 3)
		g_arrDecalFaceMapping.resize(numIntFaces * 3);

	bool bFirstLock = true;

	// realize each unrealized decal, then clean up the array of unrealized decals
	for (uint32 nDecal=0; nDecal<numDecalRequests; ++nDecal)
	{
		CryEngineDecalInfo& rDecalInfo = m_arrDecalRequests[nDecal];
		CAnimDecal NewDecal;
		bool bFirstMaterial = true;
		assert((fabs_tpl(1-(rDecalInfo.vHitDirection|rDecalInfo.vHitDirection)))<0.0001);		//check if unit-vector

		// transform the bullet position to the local coordinates
		NewDecal.m_ptSource = rDecalInfo.vPos - rPhysEntityLocation.t;

		// Z axis looks in the direction of the hit
		Matrix34 m_matBullet34=Matrix33::CreateRotationV0V1(Vec3(0,0,-1),rDecalInfo.vHitDirection);
		m_matBullet34.SetTranslation(NewDecal.m_ptSource);

		//m_matBullet34 = Matrix33::CreaterRotationVDir(m_ptHitDirectionLCS, 0);
		//m_matBullet34.SetTranslation(m_ptSourceLCS);
		m_matBullet34.m02 *= 4.0f;
		m_matBullet34.m12 *= 4.0f;
		m_matBullet34.m22 *= 4.0f;

		Matrix34 m_matInvBullet34 = m_matBullet34.GetInverted();

		memset(&g_arrDecalVertexMapping[0],-1, g_arrDecalVertexMapping.size() * sizeof(int));
		memset(&g_arrDecalFaceMapping[0],-1, g_arrDecalFaceMapping.size() * sizeof(int));
	
		Vec3 vec;
		uint32 count = pModel->m_arrCollisions.size();
		for (uint32 bb = 0; bb < count; ++bb)
		{
			//inline uint8 Ray_OBB( const Ray& ray,const Vec3& pos,const OBB& obb, Vec3& output1 )
			if (Intersect::Ray_OBB(Ray(rDecalInfo.vPos, rDecalInfo.vHitDirection), pModel->m_arrCollisions[bb].m_Pos, pMesh->m_pModel->m_arrCollisions[bb].m_OBB, vec) != 0) 
			{
				if (bFirstLock) 
				{
					pMesh->InitSkinningExtSW(pMorphing,pShapeDeform,0,pModel->m_nBaseLOD);
					pMesh->LockFullRenderMesh(pModel->m_nBaseLOD);
					bFirstLock = false;
				}

				if (bFirstMaterial) 
				{
					if (rDecalInfo.szMaterialName[0] != 0) 
					{
						NewDecal.m_pMaterial =pMatMan->FindMaterial(rDecalInfo.szMaterialName);
						if (!NewDecal.m_pMaterial)
							NewDecal.m_pMaterial = pMatMan->LoadMaterial( rDecalInfo.szMaterialName, true, true );
					} 
					else 
					{
						IMaterial* pDefaultDecalMaterial( pMatMan->FindMaterial("Materials/Decals/CharacterDecal"));
						if( !pDefaultDecalMaterial ) 
						{
							pDefaultDecalMaterial = pMatMan->LoadMaterial( "Materials/Decals/CharacterDecal", true, true );
							if (!pDefaultDecalMaterial ) 
								pDefaultDecalMaterial = g_pI3DEngine->GetMaterialManager()->GetDefaultMaterial();
						}

						NewDecal.m_pMaterial = pDefaultDecalMaterial;
					}
				}

				Matrix34 m34=m_matInvBullet34*Matrix33(rPhysEntityLocation.q);	
				Vec3 tmp, tmpn;
				for (uint32 v=0, end = pMesh->m_pModel->m_arrCollisions[bb].m_arrIndexes.size(); v< end; v++)
				{
					int ind = pMesh->m_pModel->m_arrCollisions[bb].m_arrIndexes[v];
					int i0 = pMesh->m_pIndices[ind*3+0];
					int i1 = pMesh->m_pIndices[ind*3+1];
					int i2 = pMesh->m_pIndices[ind*3+2];
					g_arrVerticesBS[i0] = m34* pMesh->GetSkinnedExtVertex2(parrNewSkinQuat, i0 );//arrSkinnedVertices[i].pos;
					g_arrVerticesBS[i1] = m34* pMesh->GetSkinnedExtVertex2(parrNewSkinQuat, i1 );//arrSkinnedVertices[i].pos;
					g_arrVerticesBS[i2] = m34* pMesh->GetSkinnedExtVertex2(parrNewSkinQuat, i2 );//arrSkinnedVertices[i].pos;
				}

				// find the vertices that participate in decal generation and add them to the array of vertices
				//for (uint32 i=0; i<numIntVertices; i++)
				//	g_arrDecalVertexMapping[i]=-1;


				// FIXME: [CarstenW & MichaelR]: TexScale was always set to 0.030f regardless of fSize.
				// Changed this to allow decals on large characters (on hunter the Overlap tests failed), 
				// without messing up appearance of currently used smaller ones (e.g. bullet decals)    
				f32 TexScale = (rDecalInfo.fSize < 0.25f) ? 0.030f : rDecalInfo.fSize;
				//TexScale = 0.030f; 

				// find the faces to which the distance near enough
				for (uint32 nFace=0, end = pMesh->m_pModel->m_arrCollisions[bb].m_arrIndexes.size() ; nFace<end; ++nFace)
				{
					// vertices belonging to the face, in internal indexation
					TFace Face;// = arrFaces[nFace];
					int ind = pMesh->m_pModel->m_arrCollisions[bb].m_arrIndexes[nFace];
					if (g_arrDecalFaceMapping[ind] != -1)
						continue;
					//if (ind > g_arrDecalFaceMapping.size())
					//{
	
					//	g_arrDecalFaceMapping.resize(ind+1);
					//}
					g_arrDecalFaceMapping[ind] = 0;
					int i0 = pMesh->m_pIndices[ind * 3];
					int i1 = pMesh->m_pIndices[ind * 3 + 1];
					int i2 = pMesh->m_pIndices[ind * 3 + 2];

					Face.i0 = i0;//pMesh->m_pModel->m_arrCollisions[bb].m_arrIndexes[nFace * 3 + 0];//nFace * 3 + 0;//pMesh->m_pModel->m_arrCollisions[i].m_arrIndexes[nFace * 3];
					Face.i1 = i1;//pMesh->m_pModel->m_arrCollisions[bb].m_arrIndexes[nFace * 3 + 1];//nFace * 3 + 1;//pMesh->m_pModel->m_arrCollisions[i].m_arrIndexes[nFace * 3 + 1];
					Face.i2 = i2;//pMesh->m_pModel->m_arrCollisions[bb].m_arrIndexes[nFace * 3 + 2];//nFace * 3 + 2;//pMesh->m_pModel->m_arrCollisions[i].m_arrIndexes[nFace * 3 + 2];
					//Face.i0 = pMesh->m_pIndices[nFace * 3];
					//Face.i1 = pMesh->m_pIndices[nFace * 3 + 1];
					//Face.i2 = pMesh->m_pIndices[nFace * 3 + 2];

					Triangle tri;
					tri.v0=g_arrVerticesBS[Face.i0];
					tri.v1=g_arrVerticesBS[Face.i1];
					tri.v2=g_arrVerticesBS[Face.i2];

					//backface test
					f32 b0 =  (tri.v1.x-tri.v0.x) * (tri.v2.y-tri.v0.y) - (tri.v2.x-tri.v0.x) * (tri.v1.y-tri.v0.y);
					if (b0>0) 
					{
						uint32 t = Overlap::Sphere_Triangle( Sphere(Vec3(ZERO),TexScale), tri );
						if (t)
						{
							CDecalVertex DecalVertex;
							f32 ts = /*0.25f*/1.0f/TexScale;
							uint32 i0,i1,i2;

							if (g_arrDecalVertexMapping[Face.i0] != -1)	{	i0=g_arrDecalVertexMapping[Face.i0]; }
							else 
							{
								DecalVertex.nVertex = Face.i0; 
								DecalVertex.u	= 0.5f+(g_arrVerticesBS[Face.i0].x*ts); 
								DecalVertex.v	=	0.5f+(g_arrVerticesBS[Face.i0].y*ts);
								//DecalVertex.u = Clamp(DecalVertex.u, 0.0f, 1.0f);
								//DecalVertex.v = Clamp(DecalVertex.v, 0.0f, 1.0f);
								NewDecal.m_arrVertices.push_back(DecalVertex);
								g_arrDecalVertexMapping[Face.i0] = int(NewDecal.m_arrVertices.size()-1);
								i0=g_arrDecalVertexMapping[Face.i0];
							}

							if (g_arrDecalVertexMapping[Face.i1] != -1)	{ i1=g_arrDecalVertexMapping[Face.i1];	}
							else 
							{
								DecalVertex.nVertex = Face.i1; 
								DecalVertex.u	= 0.5f+(g_arrVerticesBS[Face.i1].x*ts); 
								DecalVertex.v	=	0.5f+(g_arrVerticesBS[Face.i1].y*ts);
								//DecalVertex.u = Clamp(DecalVertex.u, 0.0f, 1.0f);
								//DecalVertex.v = Clamp(DecalVertex.v, 0.0f, 1.0f);

								NewDecal.m_arrVertices.push_back(DecalVertex);
								g_arrDecalVertexMapping[Face.i1] = int(NewDecal.m_arrVertices.size()-1);
								i1=g_arrDecalVertexMapping[Face.i1];
							}

							if (g_arrDecalVertexMapping[Face.i2] != -1)	{ i2=g_arrDecalVertexMapping[Face.i2];}
							else 
							{
								DecalVertex.nVertex = Face.i2; 
								DecalVertex.u	= 0.5f+(g_arrVerticesBS[Face.i2].x*ts); 
								DecalVertex.v	=	0.5f+(g_arrVerticesBS[Face.i2].y*ts);
								//DecalVertex.u = Clamp(DecalVertex.u, 0.0f, 1.0f);
								//DecalVertex.v = Clamp(DecalVertex.v, 0.0f, 1.0f);

								NewDecal.m_arrVertices.push_back(DecalVertex);
								g_arrDecalVertexMapping[Face.i2] = int(NewDecal.m_arrVertices.size()-1);
								i2=g_arrDecalVertexMapping[Face.i2];
							}

							NewDecal.m_arrFaces.push_back( TFace(i0,i1,i2) );
						}
					}
				}
			}
		}

		uint32 num = NewDecal.m_arrFaces.size();
		if (num==0)
			continue; // we're not interested in this decal: we don't have any decals

		NewDecal.m_fBuildTime		= g_fCurrTime;
		NewDecal.m_fFadeInTime	= rDecalInfo.fSize / (0.04f + 0.02f*(rand()%0xFF)/256.0f); // suppose the speed is like this
		NewDecal.m_fDeathTime		= NewDecal.m_fBuildTime + rDecalInfo.fLifeTime; //remove deacal after 180 seconds
		NewDecal.m_fFadeOutTime = -1; // no fade out
		m_arrDecals.push_back( NewDecal );
	}


	if (!bFirstLock)
		pMesh->UnlockFullRenderMesh(pMesh->m_pModel->m_nBaseLOD);
	// after we realized the decal request, we don't need it anymore
	m_arrDecalRequests.clear();
}



// starts fading out all decals that are close enough to the given point
// NOTE: the radius is m^2 - it's the square of the radius of the sphere
void CAnimDecalManager::fadeOutCloseDecals (const Vec3& ptCenter, float fRadius2)
{
	uint32 numDecals = m_arrDecals.size();
	for (uint32 i=0; i<numDecals; ++i)
	{
		if ( (m_arrDecals[i].m_ptSource-ptCenter).GetLengthSquared() < fRadius2)
			m_arrDecals[i].startFadeOut (2);
	}
}

// if we delete one of the decals
void CAnimDecalManager::RemoveObsoleteDecals()
{
	uint32 numDecals = m_arrDecals.size();
	for (uint32 i=0; i<numDecals; i++)
	{
		if ( m_arrDecals[i].isDead() )
		{
			m_arrDecals[i] = m_arrDecals[numDecals-1];
			m_arrDecals.pop_back();
			numDecals--;
		}
	}
}



// cleans up all decals, destroys the vertex buffer
void CAnimDecalManager::clear()
{
	m_arrDecalRequests.clear();
	m_arrDecals.clear();
}




// returns the decal multiplier: 0 - no decal, 1 - full decal size
f32 CAnimDecal::getIntensity()const
{
	if (g_fCurrTime >= m_fDeathTime)
		// we've faded out
		return 0;

	if (g_fCurrTime > m_fDeathTime - m_fFadeOutTime)
		// we're fading out
		return 1-sqr(1-(m_fDeathTime - g_fCurrTime) / m_fFadeOutTime);


	f32 fLifeTime = (g_fCurrTime - m_fBuildTime);
	if (fLifeTime > m_fFadeInTime)
		// we've faded in
		return 1;
	else
		// we're fading in
		return 1 - sqr(1 - fLifeTime / m_fFadeInTime);
}

// returns true if this is a dead/empty decal and should be discarded
bool CAnimDecal::isDead()
{
	return g_fCurrTime >= m_fDeathTime;
}

// starts fading out the decal from this moment on
void CAnimDecal::startFadeOut(f32 fTimeout)
{
	m_fFadeOutTime = fTimeout;
	m_fDeathTime = g_fCurrTime + fTimeout;
}

