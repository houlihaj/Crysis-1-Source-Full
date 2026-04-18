#include "StdAfx.h"

#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "CullBuffer.h"
#include "3dEngine.h"
#include "IndexedMesh.h"
#include "WaterVolumes.h"
#include "Brush.h"
#include "LMCompStructures.h"
#include "Vegetation.h"
#include "terrain.h"
#include "RenderMeshMerger.h"
#include "MeshCompiler/MeshCompiler.h"

CRenderMeshMerger::CRenderMeshMerger(void)
{
}

CRenderMeshMerger::~CRenderMeshMerger(void)
{
}

int CRenderMeshMerger::Cmp_Materials(IMaterial *pMat1, IMaterial *pMat2)
{
	if(!pMat1 && !pMat2)
		return 0;

	SShaderItem& shaderItem1 = pMat1->GetShaderItem();
	SShaderItem& shaderItem2 = pMat2->GetShaderItem();

#ifdef _DEBUG
	const char * pName1 = shaderItem1.m_pShader->GetName();
	const char * pName2 = shaderItem2.m_pShader->GetName();
#endif

	// vert format
	int nVertFormat1 = shaderItem1.m_pShader->GetVertexFormat();
	int nVertFormat2 = shaderItem2.m_pShader->GetVertexFormat();

	if(nVertFormat1 > nVertFormat2)
		return  1;
	if(nVertFormat1 < nVertFormat2)
		return -1;

	bool bDecal1 = (shaderItem1.m_pShader->GetFlags()&EF_DECAL)!=0;
	bool bDecal2 = (shaderItem2.m_pShader->GetFlags()&EF_DECAL)!=0;

	// shader
	if(bDecal1 > bDecal2)
		return  1;
	if(bDecal1 < bDecal2)
		return -1;

	// shader resources
	if(shaderItem1.m_pShaderResources > shaderItem2.m_pShaderResources)
		return  1;
	if(shaderItem1.m_pShaderResources < shaderItem2.m_pShaderResources)
		return -1;

	// shader
	if(shaderItem1.m_pShader > shaderItem2.m_pShader)
		return  1;
	if(shaderItem1.m_pShader < shaderItem2.m_pShader)
		return -1;

	// compare mats ptr
	if(pMat1 > pMat2)
		return  1;
	if(pMat1 < pMat2)
		return -1;

	return 0;
}

int CRenderMeshMerger::Cmp_RenderChunksInfo(const void* v1, const void* v2)
{
	SRenderMeshInfoInput *in1 = (SRenderMeshInfoInput*)v1;
	SRenderMeshInfoInput *in2 = (SRenderMeshInfoInput*)v2;

	//assert(pChunk1->LMInfo.iRAETex == pChunk2->LMInfo.iRAETex);

	IMaterial *pMat1 = in1->pMat;
	IMaterial *pMat2 = in2->pMat;

	return Cmp_Materials(pMat1, pMat2);
}

int CRenderMeshMerger::Cmp_RenderChunks_(const void* v1, const void* v2)
{
	CRenderMeshMerger::SMergedChunk *pChunk1 = (CRenderMeshMerger::SMergedChunk*)v1;
	CRenderMeshMerger::SMergedChunk *pChunk2 = (CRenderMeshMerger::SMergedChunk*)v2;

	IMaterial *pMat1 = pChunk1->pMaterial;
	IMaterial *pMat2 = pChunk2->pMaterial;

	return Cmp_Materials(pMat1, pMat2);
}

void CRenderMeshMerger::IsChunkValid(CRenderChunk & Ch, PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> & m_lstVerts, 
															 PodArray<uint> & m_lstIndices)
{
#ifdef _DEBUG
	assert(Ch.nFirstIndexId >= 0 && Ch.nFirstIndexId + Ch.nNumIndices <= m_lstIndices.Count());
	assert(Ch.nFirstVertId >= 0 && Ch.nFirstVertId + Ch.nNumVerts <= m_lstVerts.Count());

	for(int i=Ch.nFirstIndexId; i<Ch.nFirstIndexId+Ch.nNumIndices; i++)
	{
		assert(m_lstIndices[i]>=Ch.nFirstVertId && m_lstIndices[i]<Ch.nFirstVertId+Ch.nNumVerts);
		assert(m_lstIndices[i]>=0 && m_lstIndices[i]<m_lstVerts.Count());
	}
#endif				
}

void CRenderMeshMerger::MakeRenderMeshInfoListOfAllChunks(SRenderMeshInfoInput * pRMIArray, int nRMICount,SMergeInfo &info )
{
	for(int nEntityId = 0; nEntityId<nRMICount; nEntityId++)
	{
		SRenderMeshInfoInput* pRMI	= &pRMIArray[nEntityId];
		IRenderMesh *pRM = pRMI->pMesh;

		for(int m=0; m<pRM->GetChunks()->Count(); m++)
		{
			CRenderChunk newMatInfo = *pRM->GetChunks()->Get(m);
			if(newMatInfo.m_nMatFlags & MTL_FLAG_NODRAW || !newMatInfo.pRE)
				continue;

			IMaterial *pCustMat;        
			if(pRMI->pMat && newMatInfo.m_nMatID>=0 && newMatInfo.m_nMatID < pRMI->pMat->GetSubMtlCount())
				pCustMat = pRMI->pMat->GetSubMtl(newMatInfo.m_nMatID);
			else
				pCustMat = pRMI->pMat;

			if(!pCustMat)
				continue;

			IShader * pShader = pCustMat->GetShaderItem().m_pShader;

			if(!pShader)
				continue;

			if(pShader->GetFlags2()&EF2_NODRAW)
				continue;

			if(info.pDecalClipInfo)
				if(pShader->GetFlags()&EF_DECAL)
					continue;

			SRenderMeshInfoInput RMIChunk = *pRMI;
			RMIChunk.pMat = info.pDecalClipInfo ? NULL : pCustMat;
			RMIChunk.nChunkId = m;
			m_lstRMIChunks.Add(RMIChunk);
		}
	}
}

void CRenderMeshMerger::MakeListOfAllCRenderChunks( SMergeInfo &info )
{
	m_nTotalVertexCount = 0;
	m_nTotalIndexCount = 0;

	for(int nEntityId = 0; nEntityId<m_lstRMIChunks.Count(); nEntityId++)
	{
		SRenderMeshInfoInput* pRMI	= &m_lstRMIChunks[nEntityId];

		bool bMatrixHasRotation = 
			pRMI->mat.m01 || pRMI->mat.m10 ||
			pRMI->mat.m02 || pRMI->mat.m20;

		Matrix34 matInv = pRMI->mat.GetInverted();

		IRenderMesh *pLMTexCoordsRM = pRMI->pLMTexCoordsRM;
		IRenderMesh *pRM = pRMI->pMesh;
		if(!pRM->GetVertCount())
			continue;

		int nInitVertCout = m_lstVerts.Count();

		// get vertices's
		int nPosStride=0;
		const byte * pPos = pRM->GetStridedPosPtr(nPosStride);
		int nTexStride=0;
		const byte * pTex = pRM->GetStridedUVPtr(nTexStride);

		// get tangent basis
		int nTangStride=0;
		const byte * pTang = pRM->GetStridedTangentPtr(nTangStride);
		int nBNormStride=0;
		const byte * pBNorm = pRM->GetStridedBinormalPtr(nBNormStride);
		int nColorStride=0;
		const byte * pColor = pRM->GetStridedColorPtr(nColorStride);

		Vec3 vMin, vMax;
		pRM->GetBBox(vMin, vMax);

		// get LM TexCoords
		int nLMStride=0;
		const byte * pLMTexCoords = pLMTexCoordsRM ? pLMTexCoordsRM->GetStridedPosPtr(nLMStride) : 0;

		// get indices
		int nIndCount=0;
		ushort *pSrcInds = pRM->GetIndices(&nIndCount);

		Vec3 vOSPos(0,0,0);
		Vec3 vOSProjDir(0,0,0);
		float fMatrixScale = 1.f;
		float fOSRadius = 0.f;

		if(info.pDecalClipInfo && info.pDecalClipInfo->fRadius)
		{
			vOSPos = matInv.TransformPoint(info.pDecalClipInfo->vPos);
			vOSProjDir = matInv.TransformVector(info.pDecalClipInfo->vProjDir);
			fMatrixScale = vOSProjDir.GetLength();
			fOSRadius = info.pDecalClipInfo->fRadius*fMatrixScale;
			if(vOSProjDir.GetLength()>0.01f)
				vOSProjDir.Normalize();
		}

		CRenderChunk newMatInfo = *pRM->GetChunks()->Get(pRMI->nChunkId);

#ifdef _DEBUG
		CRenderChunk Ch = newMatInfo;
		for(int i=Ch.nFirstIndexId; i<Ch.nFirstIndexId+Ch.nNumIndices; i++)
		{
			assert(i>=0 && i<nIndCount);
			assert(pSrcInds[i]>=Ch.nFirstVertId && pSrcInds[i]<Ch.nFirstVertId+Ch.nNumVerts);
			assert(pSrcInds[i]<pRM->GetVertCount());
		}
#endif				

		int nFirstIndexId = m_lstIndices.Count();

		// add indices
		for(int i=newMatInfo.nFirstIndexId; i<newMatInfo.nFirstIndexId+newMatInfo.nNumIndices; i+=3)
		{
			assert(pSrcInds[i+0]<pRM->GetVertCount());
			assert(pSrcInds[i+1]<pRM->GetVertCount());
			assert(pSrcInds[i+2]<pRM->GetVertCount());

			assert(pSrcInds[i+0]>=newMatInfo.nFirstVertId && pSrcInds[i+0]<newMatInfo.nFirstVertId+newMatInfo.nNumVerts);
			assert(pSrcInds[i+1]>=newMatInfo.nFirstVertId && pSrcInds[i+1]<newMatInfo.nFirstVertId+newMatInfo.nNumVerts);
			assert(pSrcInds[i+2]>=newMatInfo.nFirstVertId && pSrcInds[i+2]<newMatInfo.nFirstVertId+newMatInfo.nNumVerts);

			// skip not needed triangles for decals
			if(fOSRadius)
			{
				// get verts
				Vec3 v0 = *((Vec3*)&pPos[nPosStride*pSrcInds[i+0]]);
				Vec3 v1 = *((Vec3*)&pPos[nPosStride*pSrcInds[i+1]]);
				Vec3 v2 = *((Vec3*)&pPos[nPosStride*pSrcInds[i+2]]);

				if(vOSProjDir.IsZero())
				{ // explosion mode
					// test the face
					float fDot0 = (vOSPos-v0).Dot((v1-v0).Cross(v2-v0));
					float fTest = -0.15f;
					if(fDot0 < fTest)
						continue;
				}
				else
				{
					// get triangle normal
					Vec3 vNormal = (v1-v0).Cross(v2-v0);

					// test the face
					if(vNormal.Dot(vOSProjDir)<=0)
						continue;
				}

				// get bbox
				AABB triBox;
				triBox.min = v0; triBox.min.CheckMin(v1); triBox.min.CheckMin(v2);
				triBox.max = v0; triBox.max.CheckMax(v1); triBox.max.CheckMax(v2);

				if(!Overlap::Sphere_AABB(Sphere(vOSPos, fOSRadius), triBox))
					continue;
			}
			else if(info.pClipCellBox)
			{
				// get verts
				Vec3 v0 = *((Vec3*)&pPos[nPosStride*pSrcInds[i+0]]);
				Vec3 v1 = *((Vec3*)&pPos[nPosStride*pSrcInds[i+1]]);
				Vec3 v2 = *((Vec3*)&pPos[nPosStride*pSrcInds[i+2]]);

				Vec3 vCenter = (v0 + v1 + v2) * 0.3333f;
				vCenter = pRMI->mat.TransformPoint(vCenter);

				// get bbox
				AABB triBox;
				triBox.min = v0; triBox.min.CheckMin(v1); triBox.min.CheckMin(v2);
				triBox.max = v0; triBox.max.CheckMax(v1); triBox.max.CheckMax(v2);

				if(
					vCenter.x > info.pClipCellBox->max.x || 
					vCenter.y > info.pClipCellBox->max.y || 
					vCenter.z > info.pClipCellBox->max.z || 
					vCenter.x < info.pClipCellBox->min.x ||
					vCenter.y < info.pClipCellBox->min.y ||
					vCenter.z < info.pClipCellBox->min.z 
					)
					continue;
			}

			m_lstIndices.Add((int)(pSrcInds[i+0]) - newMatInfo.nFirstVertId + nInitVertCout);
			m_lstIndices.Add((int)(pSrcInds[i+1]) - newMatInfo.nFirstVertId + nInitVertCout);
			m_lstIndices.Add((int)(pSrcInds[i+2]) - newMatInfo.nFirstVertId + nInitVertCout);
		}

		newMatInfo.nFirstIndexId = nFirstIndexId;
		newMatInfo.nNumIndices = m_lstIndices.Count() - nFirstIndexId;

		if(!newMatInfo.nNumIndices)
			continue;

		// add vertices
		for(int v=newMatInfo.nFirstVertId; v<newMatInfo.nFirstVertId+newMatInfo.nNumVerts; v++)
		{
			assert(v>=0 && v<pRM->GetVertCount());

			struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F vert;

			// set pos
			Vec3 vPos = *(Vec3*)&pPos[nPosStride*v];				
			vPos = pRMI->mat.TransformPoint(vPos);
			vert.xyz = vPos;

			// set uv
      if(pTex)
      {
			  float * pUV = (float*)&pTex[nTexStride*v];				
			  vert.st[0] = pUV[0];
			  vert.st[1] = pUV[1];
      }
      else
      {
        vert.st[0] = 0;
        vert.st[1] = 0;
      }

			/*if(info.bPlaceInstancePositionIntoVertexNormal)
			{
				vert.normal = pRMI->mat.GetTranslation();
			}
			else if(pNorm)
			{
				Vec3 vNrm = pRMI->mat.TransformVector(*(Vec3*)&pNorm[nNormStride*v]);
				vert.normal = vNrm;
			}
			else
			{
				vert.normal.Set(0,0,1);
			}*/

			vert.color.dcolor = *(uint32*)&pColor[nColorStride*v+0];

			m_lstVerts.Add(vert);

			// get tangent basis
      SPipTangents basis;
      if(pTang && pBNorm)
      {
			  Vec4sf *Tangent = (Vec4sf*)&pTang[nTangStride*v];
			  Vec4sf *Binormal = (Vec4sf*)&pBNorm[nBNormStride*v];
			  if(bMatrixHasRotation)
			  {
				  Vec3 tangent	= Vec3(tPackB2F(Tangent->x), tPackB2F(Tangent->y), tPackB2F(Tangent->z));
				  tangent = pRMI->mat.TransformVector(tangent);
				  tangent.Normalize();
				  basis.Tangent	= Vec4sf(tPackF2B(tangent.x),tPackF2B(tangent.y),tPackF2B(tangent.z), Tangent->w);

				  Vec3 binormal	= Vec3(tPackB2F(Binormal->x), tPackB2F(Binormal->y), tPackB2F(Binormal->z));
				  binormal = pRMI->mat.TransformVector(binormal);
				  binormal.Normalize();
				  basis.Binormal	= Vec4sf(tPackF2B(binormal.x),tPackF2B(binormal.y),tPackF2B(binormal.z), Binormal->w);
			  }
			  else
			  {
				  basis.Tangent	= *Tangent;
				  basis.Binormal = *Binormal;
			  }
      }
      else
        memset(&basis,0,sizeof(basis));

			m_lstTangBasises.Add(basis);

			// add LM texcoords
			Vec2 vLMTC;
			if(pLMTexCoords)
				vLMTC = *(Vec2*)&pLMTexCoords[nLMStride*v];
			else
				vLMTC.x = vLMTC.y = 0;

			m_lstLMTexCoords.Add(vLMTC);
		}

		// set vert range
		newMatInfo.nFirstVertId = m_lstVerts.Count() - newMatInfo.nNumVerts;

		newMatInfo.pRE = NULL;

		//		assert(IsHeapValid());
		//		IsChunkValid(newMatInfo, m_lstVerts, m_lstIndices);

		if(m_lstChunks.Count())
			assert(m_lstChunks.Last().rChunk.nFirstVertId+m_lstChunks.Last().rChunk.nNumVerts == newMatInfo.nFirstVertId);

		if(newMatInfo.nNumIndices)
		{
			SMergedChunk mrgChunk;
			mrgChunk.rChunk = newMatInfo;
			mrgChunk.pMaterial = info.pDecalClipInfo ? NULL : pRMI->pMat;
			if (pRMI->pMat)
				mrgChunk.rChunk.m_nMatFlags = pRMI->pMat->GetFlags();
			m_lstChunks.Add(mrgChunk);
		}

		m_nTotalVertexCount += newMatInfo.nNumVerts;
		m_nTotalIndexCount += newMatInfo.nNumIndices;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderMeshMerger::CompactVertices( SMergeInfo &info )
{
	if (info.bPrintDebugMessages)
		PrintMessage("  Remove unused vertices . . .");

	PodArray<uint> lstVertUsage;
	lstVertUsage.PreAllocate(m_lstVerts.Count(),m_lstVerts.Count());
	for(int i=0; i<m_lstIndices.Count(); i++)
		lstVertUsage[m_lstIndices[i]] = 1;

	PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> lstVertsOptimized;
	lstVertsOptimized.PreAllocate(m_lstVerts.Count());
	PodArray<SPipTangents> lstTangBasisesOptimized;
	lstTangBasisesOptimized.PreAllocate(m_lstVerts.Count());
	PodArray<Vec2> lstLMTexCoordsOptimized;
	lstLMTexCoordsOptimized.PreAllocate(m_lstVerts.Count());

	int nCurChunkId=0;
	CRenderChunk * pChBack = &m_lstChunks[0].rChunk;
	int nVertsRemoved=0;
	PodArray<SMergedChunk> lstChunksBk;
	lstChunksBk.AddList(m_lstChunks);
	for(int i=0; i<m_lstVerts.Count();)
	{
		if(lstVertUsage[i])
		{
			lstVertsOptimized.Add(m_lstVerts[i]);
			lstTangBasisesOptimized.Add(m_lstTangBasises[i]);
			lstLMTexCoordsOptimized.Add(m_lstLMTexCoords[i]);
		}
		else
			nVertsRemoved++;

		lstVertUsage[i] = lstVertsOptimized.Count()-1;

		i++;

		if(i >= lstChunksBk[nCurChunkId].rChunk.nFirstVertId+lstChunksBk[nCurChunkId].rChunk.nNumVerts)
		{
			if(nVertsRemoved)
			{
				pChBack->nNumVerts-=nVertsRemoved;

				for(int nId = nCurChunkId+1; nId<m_lstChunks.Count(); nId ++)
				{ 
					CRenderChunk & ChBack = m_lstChunks[nId].rChunk;
					ChBack.nFirstVertId -= nVertsRemoved;
				}

				nVertsRemoved=0;
			}

			nCurChunkId++;
			if(nCurChunkId<m_lstChunks.Count())
				pChBack = &m_lstChunks[nCurChunkId].rChunk;
			else
				break;
		}
	}

	for(int i=0; i<m_lstIndices.Count(); i++)
		m_lstIndices[i] = lstVertUsage[m_lstIndices[i]];

	m_lstVerts = lstVertsOptimized;
	m_lstTangBasises = lstTangBasisesOptimized;
	m_lstLMTexCoords = lstLMTexCoordsOptimized;

	if(info.bPrintDebugMessages)
		PrintMessagePlus(" %d vertices left", m_lstVerts.Count());
}

//////////////////////////////////////////////////////////////////////////
void CRenderMeshMerger::ClipDecals( SMergeInfo &info )
{
	if(info.bPrintDebugMessages)
		PrintMessage("  Do clipping . . ."/*, m_lstChunks.Count()*/);

	{ // minimize range
		uint nMin = ~0;
		uint nMax = 0;

		for(int i=0; i<m_lstIndices.Count(); i++)
		{
			if(nMin>m_lstIndices[i]) nMin=m_lstIndices[i];
			if(nMax<m_lstIndices[i]) nMax=m_lstIndices[i];
		}

		for(int i=0; i<m_lstIndices.Count(); i++)
			m_lstIndices[i] -= nMin;

		if(m_lstVerts.Count()-nMax-1>0)
		{
			m_lstVerts.Delete(nMax+1, m_lstVerts.Count()-(nMax+1));
			m_lstTangBasises.Delete(nMax+1, m_lstTangBasises.Count()-(nMax+1));
			m_lstLMTexCoords.Delete(nMax+1, m_lstLMTexCoords.Count()-(nMax+1));
		}

		m_lstVerts.Delete(0,nMin);
		m_lstTangBasises.Delete(0,nMin);
		m_lstLMTexCoords.Delete(0,nMin);
	}

	// define clip planes
	Plane planes[6];
  float fClipRadius = info.pDecalClipInfo->fRadius*1.3f;
	planes[0].SetPlane(Vec3( 0, 0, 1 ), info.pDecalClipInfo->vPos + Vec3( 0, 0, fClipRadius ));
	planes[1].SetPlane(Vec3( 0, 0,-1 ), info.pDecalClipInfo->vPos + Vec3( 0, 0,-fClipRadius ));
	planes[2].SetPlane(Vec3( 0, 1, 0 ), info.pDecalClipInfo->vPos + Vec3( 0, fClipRadius, 0 ));
	planes[3].SetPlane(Vec3( 0,-1, 0 ), info.pDecalClipInfo->vPos + Vec3( 0,-fClipRadius, 0 ));
	planes[4].SetPlane(Vec3( 1, 0, 0 ), info.pDecalClipInfo->vPos + Vec3( fClipRadius, 0, 0 ));
	planes[5].SetPlane(Vec3(-1, 0, 0 ), info.pDecalClipInfo->vPos + Vec3(-fClipRadius, 0, 0 ));

	// clip triangles
	int nOrigCount = m_lstIndices.Count();
	for(int i=0; i<nOrigCount; i+=3)
	{
		if(ClipTriangle(i, planes, 6))
		{
			i-=3;
			nOrigCount-=3;
		}
	}

	if(m_lstIndices.Count()<3 || m_lstVerts.Count()<3)
		return;

	assert(m_lstTangBasises.Count() == m_lstVerts.Count());
	assert(m_lstLMTexCoords.Count() == m_lstVerts.Count());

	{ // minimize range
		uint nMin = ~0;
		uint nMax = 0;

		for(int i=0; i<m_lstIndices.Count(); i++)
		{
			if(nMin>m_lstIndices[i]) nMin=m_lstIndices[i];
			if(nMax<m_lstIndices[i]) nMax=m_lstIndices[i];
		}

		for(int i=0; i<m_lstIndices.Count(); i++)
			m_lstIndices[i] -= nMin;

		if(m_lstVerts.Count()-nMax-1>0)
		{
			m_lstVerts.Delete(nMax+1, m_lstVerts.Count()-(nMax+1));
			m_lstTangBasises.Delete(nMax+1, m_lstTangBasises.Count()-(nMax+1));
			m_lstLMTexCoords.Delete(nMax+1, m_lstLMTexCoords.Count()-(nMax+1));
		}

		m_lstVerts.Delete(0,nMin);
		m_lstTangBasises.Delete(0,nMin);
		m_lstLMTexCoords.Delete(0,nMin);
	}

#ifdef _DEBUG
	assert(m_lstTangBasises.Count() == m_lstVerts.Count());
	assert(m_lstLMTexCoords.Count() == m_lstVerts.Count());
	for(int i=0; i<m_lstIndices.Count(); i++)
		assert(m_lstIndices[i]>=0 && m_lstIndices[i]<m_lstVerts.Count());
#endif

	if(m_lstIndices.Count()<3 || m_lstVerts.Count()<3)
		return;

	m_lstChunks[0].rChunk.nNumIndices = m_lstIndices.Count();
	m_lstChunks[0].rChunk.nNumVerts = m_lstVerts.Count();
}

//////////////////////////////////////////////////////////////////////////
void CRenderMeshMerger::TryMergingChunks( SMergeInfo &info )
{
	PodArray<SMergedChunk> &lstChunksMerged = m_lstChunksMergedTemp;
	lstChunksMerged.clear();
	lstChunksMerged.reserve( m_lstChunks.size() );

	int nCurrVertFormat = -1;
	for(int nChunkId=0; nChunkId<m_lstChunks.Count(); nChunkId++)
	{
		//		IsChunkValid(m_lstChunks[nChunkId], m_lstVerts, m_lstIndices);

		int nChunkVertFormat;
		if(info.pDecalClipInfo || info.bMergeToOneRenderMesh)
			nChunkVertFormat = -1;
		else
		{
			IMaterial * pMat = m_lstChunks[nChunkId].pMaterial;
			SShaderItem& shaderItem = pMat->GetShaderItem();
			nChunkVertFormat = shaderItem.m_pShader->GetVertexFormat();
		}

		if (info.bMergeToOneRenderMesh)
		{
			nChunkVertFormat = nCurrVertFormat;
		}

		if (!nChunkId || 
				(nChunkVertFormat != nCurrVertFormat ||	Cmp_RenderChunks_(&m_lstChunks[nChunkId], &m_lstChunks[nChunkId-1]) ||
				(lstChunksMerged.Last().rChunk.nNumVerts + m_lstChunks[nChunkId].rChunk.nNumVerts) > 65000))
		{ // not equal materials - add new chunk
			lstChunksMerged.Add(m_lstChunks[nChunkId]);
		}
		else
		{
			lstChunksMerged.Last().rChunk.nNumIndices += m_lstChunks[nChunkId].rChunk.nNumIndices;
			lstChunksMerged.Last().rChunk.nNumVerts += m_lstChunks[nChunkId].rChunk.nNumVerts;
		}

		IsChunkValid(lstChunksMerged.Last().rChunk, m_lstVerts, m_lstIndices);

		nCurrVertFormat = nChunkVertFormat;
	}
	m_lstChunks = lstChunksMerged;
}

//////////////////////////////////////////////////////////////////////////
IRenderMesh *CRenderMeshMerger::MergeRenderMeshes(	SRenderMeshInfoInput * pRMIArray, int nRMICount,
																										PodArray<SRenderMeshInfoOutput> &outRenderMeshes,
																										SMergeInfo &info )
{
	FUNCTION_PROFILER_3DENGINE;

	if (info.bPrintDebugMessages)
		PrintMessage("******* CRenderMeshMerger::MergeRenderMeshs: %s, %d input brushes *******", info.sMeshName, nRMICount);

	assert(IsHeapValid());

	m_nTotalVertexCount = 0;
	m_nTotalIndexCount = 0;

	m_lstRMIChunks.clear();
	m_lstVerts.clear();
	m_lstTangBasises.clear();
	m_lstLMTexCoords.clear();
	m_lstIndices.clear();
	m_lstChunks.clear();

	// make list of all chunks
	MakeRenderMeshInfoListOfAllChunks(pRMIArray, nRMICount, info);

	if(info.bPrintDebugMessages)
		PrintMessage("  %d mesh chunks found", m_lstRMIChunks.Count());

	if(!m_lstRMIChunks.Count())
		return NULL;

	// sort by materials
	if(!info.pDecalClipInfo)
		qsort(m_lstRMIChunks.GetElements(), m_lstRMIChunks.Count(), sizeof(m_lstRMIChunks[0]), Cmp_RenderChunksInfo);

	// make list of all CRenderChunks
	MakeListOfAllCRenderChunks( info );

	assert(IsHeapValid());

	if(!m_lstVerts.Count() || !m_lstChunks.Count() || !m_lstIndices.Count())
		return NULL;

	if(info.bPrintDebugMessages)
		PrintMessage("  Merge %d chunks left after culling (%d verts, %d indices) . . .", m_lstChunks.Count(), m_lstVerts.Count(), m_lstIndices.Count());

	// Split chunks that does not fit together.
	TryMergingChunks( info );

	if(info.bPrintDebugMessages)
		PrintMessagePlus("  %d chunks left", m_lstChunks.Count());

	if(!m_lstChunks.Count())
		return NULL;

	// now we have list of merged/sorted chunks, indices and vertices's
	// overall amount of vertices's may be more than 65000

	if(m_lstChunks.Count()==1 && info.pDecalClipInfo && GetCVars()->e_decals_clip)
	{
		// clip decals if needed
		ClipDecals( info );
		if(m_lstIndices.Count()<3 || m_lstVerts.Count()<3)
			return NULL;

    // find AABB
    AABB aabb = AABB(m_lstVerts[0].xyz,m_lstVerts[0].xyz);
    for(int i=0; i<m_lstVerts.Count(); i++)
      aabb.Add(m_lstVerts[i].xyz);

    // weld positions
    mesh_compiler::CMeshCompiler meshCompiler;
    meshCompiler.WeldPos_VERTEX_FORMAT_P3F_COL4UB_TEX2F<uint>( m_lstVerts, m_lstTangBasises, m_lstIndices, VEC_EPSILON, aabb );
    m_lstLMTexCoords.PreAllocate(m_lstVerts.Count(),m_lstVerts.Count());

    // update chunk
    CRenderChunk & Ch0 = m_lstChunks[0].rChunk;
    Ch0.nFirstIndexId = 0;
    Ch0.nNumIndices = m_lstIndices.Count();
    Ch0.nFirstVertId = 0;
    Ch0.nNumVerts = m_lstVerts.Count();
	}
	
	if(GetCVars()->e_scene_merging_compact_vertices && info.bCompactVertBuffer)
	{ // remove gaps in vertex buffers
		CompactVertices( info );
	}

#ifdef _DEBUG
	for(int nChunkId = 0; nChunkId < m_lstChunks.Count(); nChunkId++)
	{
		CRenderChunk & Ch0 = m_lstChunks[nChunkId].rChunk;
		IsChunkValid(Ch0,m_lstVerts,m_lstIndices);
	}
#endif // _DEBUG

	if(info.bPrintDebugMessages)
		PrintMessage("  Making new RenderMeshes . . .");

	outRenderMeshes.clear();

	m_lstNewChunks.reserve( m_lstChunks.Count() );
	m_lstNewVerts.reserve( m_nTotalVertexCount );
	m_lstNewTangBasises.reserve( m_nTotalVertexCount );
	m_lstNewLMTexCoords.reserve( m_nTotalVertexCount );
	m_lstNewIndices.reserve( m_nTotalIndexCount );

	for(int nChunkId = 0; nChunkId < m_lstChunks.Count();)
	{
		AABB finalBox;
		finalBox.Reset();

		m_lstNewVerts.clear();
		m_lstNewTangBasises.clear();
		m_lstNewLMTexCoords.clear();
		m_lstNewIndices.clear();
		m_lstNewChunks.clear();

		int nVertsNum = 0;
		for(; nChunkId<m_lstChunks.Count(); )
		{
			CRenderChunk Ch = m_lstChunks[nChunkId].rChunk;
			SMergedChunk &mrgChunk = m_lstChunks[nChunkId];

			IsChunkValid(Ch, m_lstVerts, m_lstIndices);

			int nCurrIdxPos = m_lstNewIndices.Count();
			int nCurrVertPos = m_lstNewVerts.Count();

			m_lstNewVerts.AddList(&m_lstVerts[Ch.nFirstVertId], Ch.nNumVerts);
			m_lstNewTangBasises.AddList(&m_lstTangBasises[Ch.nFirstVertId], Ch.nNumVerts);
			m_lstNewLMTexCoords.AddList(&m_lstLMTexCoords[Ch.nFirstVertId], Ch.nNumVerts);

			for(int i=Ch.nFirstIndexId; i<Ch.nFirstIndexId+Ch.nNumIndices; i++)
			{
				uint32 nIndex = m_lstIndices[i] - Ch.nFirstVertId + nCurrVertPos;
				assert(nIndex>=0 && nIndex<65000 && nIndex<m_lstNewVerts.Count());
				nIndex = nIndex & 0xFFFF;
				m_lstNewIndices.Add( nIndex );
				finalBox.Add(m_lstNewVerts[nIndex].xyz);
			}

			Ch.nFirstIndexId = nCurrIdxPos;
			Ch.nFirstVertId = nCurrVertPos;

			nVertsNum += Ch.nNumVerts;

			{
				assert(Ch.nFirstIndexId + Ch.nNumIndices <= m_lstNewIndices.Count());
				assert(Ch.nFirstVertId + Ch.nNumVerts <= m_lstNewVerts.Count());

#ifdef _DEBUG
				for(int i=Ch.nFirstIndexId; i<Ch.nFirstIndexId+Ch.nNumIndices; i++)
				{
					assert(m_lstNewIndices[i]>=Ch.nFirstVertId && m_lstNewIndices[i]<Ch.nFirstVertId+Ch.nNumVerts);
					assert(m_lstNewIndices[i]<m_lstNewVerts.Count());
				}
#endif				

				int y=0;
			}

			SMergedChunk newMergedChunk;
			newMergedChunk.rChunk = Ch;
			newMergedChunk.pMaterial = mrgChunk.pMaterial;
			m_lstNewChunks.Add(newMergedChunk);

			nChunkId++;

			if (info.bMergeToOneRenderMesh)
				continue;

			if(nChunkId < m_lstChunks.Count())
			{
				if(nVertsNum + m_lstChunks[nChunkId].rChunk.nNumVerts > 65000)
					break;

				// detect vert format change
				SShaderItem& shaderItemCur = mrgChunk.pMaterial->GetShaderItem();
				int nNextChunkVertFormatCur = shaderItemCur.m_pShader->GetVertexFormat();

				SShaderItem& shaderItemNext = m_lstChunks[nChunkId].pMaterial->GetShaderItem();
				int nNextChunkVertFormatNext = shaderItemNext.m_pShader->GetVertexFormat();

				if(nNextChunkVertFormatNext != nNextChunkVertFormatCur)
					break;
			}
		}

		// make new RenderMesh
		IRenderMesh * pNewLB = GetRenderer()->CreateRenderMeshInitialized(
			m_lstNewVerts.GetElements(), m_lstNewVerts.Count(), VERTEX_FORMAT_P3F_COL4UB_TEX2F, 
			m_lstNewIndices.GetElements(), m_lstNewIndices.Count(), R_PRIMV_TRIANGLES,
			info.sMeshType,info.sMeshName, eBT_Static, m_lstNewChunks.Count(), 0, NULL, NULL, false, false);

		pNewLB->UpdateSysVertices(m_lstNewTangBasises.GetElements(), m_lstNewTangBasises.Count(), 0, VSF_TANGENTS);

		pNewLB->SetBBox(finalBox.min, finalBox.max);

		if (!info.pUseMaterial)
		{
			// make new parent material
			IMaterial * pParentMaterial = NULL;
			if(!info.pDecalClipInfo)
			{
				char szMatName[256]="";
				sprintf(szMatName, "%s_Material", info.sMeshName);
				pParentMaterial = GetMatMan()->CreateMaterial(szMatName, MTL_FLAG_MULTI_SUBMTL);
				pParentMaterial->AddRef();
				pParentMaterial->SetSubMtlCount( m_lstChunks.Count() );
			}

			// define chunks
			for(int i=0; i<m_lstNewChunks.Count(); i++)
			{
				CRenderChunk * pChunk = &m_lstNewChunks[i].rChunk;

				IMaterial * pMat = m_lstNewChunks[i].pMaterial;

				if(pParentMaterial)
				{
					assert(pMat);
					pParentMaterial->SetSubMtl( i, pMat );
				}

				assert(pChunk->nFirstIndexId + pChunk->nNumIndices <= m_lstNewIndices.Count());

				pChunk->m_nMatID = i;
				if (pMat)
					pChunk->m_nMatFlags = pMat->GetFlags();
				pNewLB->SetChunk( i,*pChunk,true );
			}

			if(pParentMaterial)
			{
				pNewLB->SetMaterial(pParentMaterial);
				pParentMaterial->AddRef();
			}
		}
		else
		{
			pNewLB->SetMaterial(info.pUseMaterial);

			// define chunks
			for(int i=0; i<m_lstNewChunks.Count(); i++)
			{
				CRenderChunk * pChunk = &m_lstNewChunks[i].rChunk;
				assert(pChunk->nFirstIndexId + pChunk->nNumIndices <= m_lstNewIndices.Count());

				IMaterial *pSubMtl = info.pUseMaterial->GetSafeSubMtl( pChunk->m_nMatID );
				pChunk->m_nMatFlags = pSubMtl->GetFlags();

				pNewLB->SetChunk( i,*pChunk,true );
			}
		}

		SRenderMeshInfoOutput rmi;
		rmi.pMesh = pNewLB;

		// output LM texcoords
		if(pRMIArray->pLMTexCoordsRM)
		{
			rmi.pLMTexCoords = new Vec2[m_lstNewLMTexCoords.Count()];
			memcpy(rmi.pLMTexCoords, m_lstNewLMTexCoords.GetElements(), m_lstNewLMTexCoords.Count()*sizeof(Vec2));
		}

		outRenderMeshes.push_back(rmi);
	}

	if(info.bPrintDebugMessages)
		PrintMessagePlus(" %d RenderMeshes created", outRenderMeshes.size());

	return outRenderMeshes.Count() ? outRenderMeshes[0].pMesh : NULL;
}

bool CRenderMeshMerger::ClipTriangle(int nStartIdxId, Plane * pPlanes, int nPlanesNum)
{
	static PodArray<Vec3> lstPolygon; lstPolygon.Clear();
	lstPolygon.Add(m_lstVerts[m_lstIndices[nStartIdxId+0]].xyz);
	lstPolygon.Add(m_lstVerts[m_lstIndices[nStartIdxId+1]].xyz);
	lstPolygon.Add(m_lstVerts[m_lstIndices[nStartIdxId+2]].xyz);

	for(int i=0; i<nPlanesNum && lstPolygon.Count()>=3; i++)
		CCoverageBuffer::ClipPolygon(&lstPolygon, pPlanes[i]);

	if(lstPolygon.Count() < 3)
	{
		m_lstIndices.Delete(nStartIdxId, 3);
		return true; // entire triangle is clipped away
	}

	if(lstPolygon.Count() == 3)
		if(lstPolygon[0].IsEquivalent(m_lstVerts[m_lstIndices[nStartIdxId+0]].xyz))
			if(lstPolygon[1].IsEquivalent(m_lstVerts[m_lstIndices[nStartIdxId+1]].xyz))
				if(lstPolygon[2].IsEquivalent(m_lstVerts[m_lstIndices[nStartIdxId+2]].xyz))
					return false; // entire triangle is in

	// replace old triangle with several new triangles
	int nStartId = m_lstVerts.Count();
	int nStartIndex = m_lstIndices[nStartIdxId+0];
	struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F fullVert = m_lstVerts[nStartIndex];
	SPipTangents fullTang = m_lstTangBasises[nStartIndex];
	Vec2 fullLM = m_lstLMTexCoords[nStartIndex];
	for(int i=0; i<lstPolygon.Count(); i++)
	{
		fullVert.xyz = lstPolygon[i];
		m_lstVerts.Add(fullVert);
		m_lstTangBasises.Add(fullTang);
		m_lstLMTexCoords.Add(fullLM);
	}

	// put first new triangle into position of original one
	m_lstIndices[nStartIdxId+0] = nStartId+0;
	m_lstIndices[nStartIdxId+1] = nStartId+1;
	m_lstIndices[nStartIdxId+2] = nStartId+2;

	// put others in the end
	for(int i=1; i<lstPolygon.Count()-2 ; i++)
	{
		m_lstIndices.Add(nStartId+0);
		m_lstIndices.Add(nStartId+i+1);
		m_lstIndices.Add(nStartId+i+2);
	}

	return false;
}