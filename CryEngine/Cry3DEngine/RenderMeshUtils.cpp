////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   RenderMeshUtils.cpp
//  Created:     14/11/2006 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "RenderMeshUtils.h"

namespace
{
	enum { MAX_CACHED_HITS = 8 };
	struct SCachedHit
	{
		IRenderMesh *pRenderMesh;
		SRayHitInfo hitInfo;
		Vec3 tri[3];
	};
	static SCachedHit last_hits[MAX_CACHED_HITS];
}

//////////////////////////////////////////////////////////////////////////
bool CRenderMeshUtils::RayIntersection( IRenderMesh *pRenderMesh, SRayHitInfo &hitInfo,IMaterial *pCustomMtl )
{
	FUNCTION_PROFILER_3DENGINE;

	//CTimeValue t0 = gEnv->pTimer->GetAsyncTime();

	float fMaxDist2 = hitInfo.fMaxHitDistance*hitInfo.fMaxHitDistance;

	Vec3 vHitPos(0,0,0);
	Vec3 vHitNormal(0,0,0);

	static bool bClearHitCache = true;
	if (bClearHitCache)
	{
		memset( last_hits,0,sizeof(last_hits) );
		bClearHitCache = false;
	}

	if (hitInfo.bUseCache)
	{
		Vec3 vOut;
		// Check for cached hits.
		for (int i = 0; i < MAX_CACHED_HITS; i++)
		{
			if (last_hits[i].pRenderMesh == pRenderMesh)
			{
				// If testing same render mesh, check if we hit the same triangle again.
				if (Intersect::Ray_Triangle( hitInfo.inRay,last_hits[i].tri[0], last_hits[i].tri[2], last_hits[i].tri[1],vOut ))
				{
					if (fMaxDist2)
					{
						float fDistance2 = hitInfo.inReferencePoint.GetSquaredDistance(vOut);
						if (fDistance2 > fMaxDist2)
							continue; // Ignore hits that are too far.
					}

					// Cached hit.
					hitInfo.vHitPos = vOut;
					hitInfo.vHitNormal = last_hits[i].hitInfo.vHitNormal;
					hitInfo.nHitMatID = last_hits[i].hitInfo.nHitMatID;
					hitInfo.nHitSurfaceID = last_hits[i].hitInfo.nHitSurfaceID;

					//CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
					//CryLogAlways( "TestTime :%.2f", (t1-t0).GetMilliSeconds() );
					//static int nCount = 0; CryLogAlways( "Cached Hit %d",++nCount );
          hitInfo.pRenderMesh = pRenderMesh;
					return true;
				}
			}
		}
	}

	int nVerts = pRenderMesh->GetVertCount();
	int nInds = pRenderMesh->GetSysIndicesCount();

	if (nInds == 0 || nVerts == 0)
		return false;

	// get position offset and stride
	int nPosStride = 0;
	uint8* pPos = (uint8*)pRenderMesh->GetStridedPosPtr(nPosStride);

	// get indices
	ushort *pInds = pRenderMesh->GetIndices(0);
	assert(nInds%3 == 0);

	float fMinDistance2 = FLT_MAX;

	IMaterial *pMtl = pRenderMesh->GetMaterial();
	if (pCustomMtl)
		pMtl = pCustomMtl;

	Ray inRay = hitInfo.inRay;

	bool bAnyHit = false;

	Vec3 vOut;
	Vec3 tri[3];

	// test tris
	PodArray<CRenderChunk>*	pChunks = pRenderMesh->GetChunks();
	int nChunkCount = pChunks->Count();
	for(int nChunkId = 0; nChunkId < nChunkCount; nChunkId++)
	{
		CRenderChunk * pChunk = pChunks->Get(nChunkId);
		if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
			continue;

		bool b2Sided = false;

    if (pMtl)
		{
			const SShaderItem &shaderItem = pMtl->GetShaderItem(pChunk->m_nMatID);
			if (hitInfo.bOnlyZWrite && !shaderItem.IsZWrite())
			  continue;
			if (!shaderItem.m_pShader || shaderItem.m_pShader->GetFlags2() & EF2_NODRAW || shaderItem.m_pShader->GetFlags() & EF_DECAL)
				continue;
			if (shaderItem.m_pShader->GetCull() & eCULL_None)
				b2Sided = true;
      if(shaderItem.m_pShaderResources && shaderItem.m_pShaderResources->GetResFlags() & MTL_FLAG_2SIDED) 
        b2Sided = true;
		}

		if (!b2Sided)
		{
			// make line triangle intersection

			int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
			for (int i = pChunk->nFirstIndexId; i < nLastIndexId; i+=3)
			{
				assert(pInds[i+0]<nVerts);
				assert(pInds[i+1]<nVerts);
				assert(pInds[i+2]<nVerts);

				// get tri vertices
				Vec3 &tv0 = *((Vec3*)&pPos[nPosStride*pInds[i+0]]);
				Vec3 &tv1 = *((Vec3*)&pPos[nPosStride*pInds[i+1]]);
				Vec3 &tv2 = *((Vec3*)&pPos[nPosStride*pInds[i+2]]);

				if (Intersect::Ray_Triangle( inRay,tv0, tv2, tv1,vOut))
				{
					float fDistance2 = hitInfo.inReferencePoint.GetSquaredDistance(vOut);
					if (fMaxDist2)
					{
						if (fDistance2 > fMaxDist2)
							continue; // Ignore hits that are too far.
					}

					bAnyHit = true;

					// for the fast test, we just need to know if there is an intersection.
					if (hitInfo.bInFirstHit)
					{
						vHitPos = vOut;
						hitInfo.nHitMatID = pChunk->m_nMatID;
						tri[0] = tv0; tri[1] = tv1; tri[2] = tv2;
						break;
					}
					if (fDistance2 < fMinDistance2)
					{
						fMinDistance2 = fDistance2;
						vHitPos = vOut;
						tri[0] = tv0; tri[1] = tv1; tri[2] = tv2;
						hitInfo.nHitMatID = pChunk->m_nMatID;
					}
				}
			}
		}
		else
		{
			// 2 Sided
			int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
			for (int i = pChunk->nFirstIndexId; i < nLastIndexId; i+=3)
			{
				assert(pInds[i+0]<nVerts);
				assert(pInds[i+1]<nVerts);
				assert(pInds[i+2]<nVerts);

				// get tri vertices
				Vec3 &tv0 = *((Vec3*)&pPos[nPosStride*pInds[i+0]]);
				Vec3 &tv1 = *((Vec3*)&pPos[nPosStride*pInds[i+1]]);
				Vec3 &tv2 = *((Vec3*)&pPos[nPosStride*pInds[i+2]]);

				// make line triangle intersection
				Vec3 vOut;
				if (Intersect::Ray_Triangle( inRay,tv0, tv2, tv1,vOut ))
				{
					float fDistance2 = hitInfo.inReferencePoint.GetSquaredDistance(vOut);
					if (fMaxDist2)
					{
						if (fDistance2 > fMaxDist2)
							continue; // Ignore hits that are too far.
					}

					bAnyHit = true;
					// Test front.
					if (hitInfo.bInFirstHit)
					{
						vHitPos = vOut;
						hitInfo.nHitMatID = pChunk->m_nMatID;
						tri[0] = tv0; tri[1] = tv1; tri[2] = tv2;
						break;
					}
					if (fDistance2 < fMinDistance2)
					{
						fMinDistance2 = fDistance2;
						vHitPos = vOut;
						hitInfo.nHitMatID = pChunk->m_nMatID;
						tri[0] = tv0; tri[1] = tv1; tri[2] = tv2;
					}
				}
				else if (Intersect::Ray_Triangle( inRay,tv0, tv1, tv2,vOut ))
				{
					float fDistance2 = hitInfo.inReferencePoint.GetSquaredDistance(vOut);
					if (fMaxDist2)
					{
						if (fDistance2 > fMaxDist2)
							continue; // Ignore hits that are too far.
					}

					bAnyHit = true;
					// Test back.
					if (hitInfo.bInFirstHit)
					{
						vHitPos = vOut;
						hitInfo.nHitMatID = pChunk->m_nMatID;
						tri[0] = tv0; tri[1] = tv2; tri[2] = tv1;
						break;
					}
					if (fDistance2 < fMinDistance2)
					{
						fMinDistance2 = fDistance2;
						vHitPos = vOut;
						hitInfo.nHitMatID = pChunk->m_nMatID;
						tri[0] = tv0; tri[1] = tv2; tri[2] = tv1;
					}
				}
			}
		}
	}

	if (bAnyHit)
	{ 
    hitInfo.pRenderMesh = pRenderMesh;

		// return closest to the shooter
		hitInfo.fDistance = (float)cry_sqrtf(fMinDistance2);
		hitInfo.vHitNormal = (tri[1]-tri[0]).Cross(tri[2]-tri[0]).GetNormalized();
		hitInfo.vHitPos = vHitPos;
		hitInfo.nHitSurfaceID = 0; 
		
		if (pRenderMesh->GetMaterial())
		{
			IMaterial *pMtl = pRenderMesh->GetMaterial()->GetSafeSubMtl(hitInfo.nHitMatID);
			if (pMtl)
				hitInfo.nHitSurfaceID = pMtl->GetSurfaceTypeId();
		}
		
		//////////////////////////////////////////////////////////////////////////
		// Add to cached results.
		memmove( &last_hits[1],&last_hits[0],sizeof(last_hits)-sizeof(last_hits[0]) ); // Move hits to the end of array, throwing out the last one.
		int last = MAX_CACHED_HITS-1;
		last_hits[0].pRenderMesh = pRenderMesh;
		last_hits[0].hitInfo = hitInfo;
		//this results in a internal compiler error currently on PS3, define will be thrown out with future 
		//	PS3 releases once it is fixed
#if !defined(PS3)
		memcpy( last_hits[0].tri,tri,sizeof(tri) );
#else
		last_hits[0].tri[0] = tri[0];
		last_hits[0].tri[1] = tri[1];
		last_hits[0].tri[2] = tri[2];
#endif
		//////////////////////////////////////////////////////////////////////////
	}
	//CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
	//CryLogAlways( "TestTime :%.2f", (t1-t0).GetMilliSeconds() );

	return bAnyHit;
}
