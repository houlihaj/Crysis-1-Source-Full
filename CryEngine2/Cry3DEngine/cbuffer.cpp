////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   cbuffer.cpp
//  Version:     v1.00
//  Created:     30/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Occlusion (coverage) buffer
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "cbuffer.h"
#include "StatObj.h"

CCoverageBuffer::CCoverageBuffer()
{ 
	m_nTrisWriten=m_nObjectsWriten=m_nTrisTested=m_nObjectsTested=m_nObjectsTestedAndRejected=0;
	m_nSelRes = GetCVars()->e_cbuffer_resolution;
	m_fCurrentZNearMeters = 0;
	//		m_pTree = NULL;
	//		m_bTreeIsReady = false;
}

void CCoverageBuffer::TransformPoint(float out[4], const float m[16], const float in[4])
{
#define M(row,col)  m[col*4+row]
  out[0] = M(0, 0) * in[0] + M(0, 1) * in[1] + M(0, 2) * in[2] + M(0, 3) * in[3];
  out[1] = M(1, 0) * in[0] + M(1, 1) * in[1] + M(1, 2) * in[2] + M(1, 3) * in[3];
  out[2] = M(2, 0) * in[0] + M(2, 1) * in[1] + M(2, 2) * in[2] + M(2, 3) * in[3];
  out[3] = M(3, 0) * in[0] + M(3, 1) * in[1] + M(3, 2) * in[2] + M(3, 3) * in[3];
#undef M
}

void CCoverageBuffer::MatMul4( float *product, const float *a, const float *b )
{
#define A(row,col)  a[(col<<2)+row]
#define B(row,col)  b[(col<<2)+row]
#define P(row,col)  product[(col<<2)+row]
  int i;
  for (i=0; i<4; i++)
  {
    float ai0=A(i,0),  ai1=A(i,1),  ai2=A(i,2),  ai3=A(i,3);
    P(i,0) = ai0 * B(0,0) + ai1 * B(1,0) + ai2 * B(2,0) + ai3 * B(3,0);
    P(i,1) = ai0 * B(0,1) + ai1 * B(1,1) + ai2 * B(2,1) + ai3 * B(3,1);
    P(i,2) = ai0 * B(0,2) + ai1 * B(1,2) + ai2 * B(2,2) + ai3 * B(3,2);
    P(i,3) = ai0 * B(0,3) + ai1 * B(1,3) + ai2 * B(2,3) + ai3 * B(3,3);
  }
#undef A
#undef B
#undef P
}

Point2d CCoverageBuffer::ProjectToScreen(const Vec3 & vIn, float * pMatrix)
{
  Point2d res;

  float _in[4], out[4];
  _in[0] = vIn.x;
  _in[1] = vIn.y;
  _in[2] = vIn.z;
  _in[3] = 1.0;

	TransformPoint(out, pMatrix, _in);

	if(out[3]>0)
	{
		if(out[3] < 0.00001f)
			out[3] = 0.00001f;
	}
	else
	{
		if(out[3] > -0.00001f)
			out[3] = -0.00001f;
	}

  res.x = out[0] / out[3];
  res.y = out[1] / out[3];
  res.z = out[2];// / out[3];

  res.x = (1.f + res.x) * m_fHalfRes;
  res.y = (1.f + res.y) * m_fHalfRes;

	if(res.z<0.001f)
		res.z = 0.001f;

	res.z = 1.f/res.z;

  return res;
}

Vec3 CCoverageBuffer::ProjectFromScreen(const float & x, const float & y, const float & z)
{
	Vec3 vRes;
	CCamera tmpCam = m_pRenderer->GetCamera();
	m_pRenderer->SetCamera(m_Camera);
	m_pRenderer->UnProjectFromScreen( 
		x*m_pRenderer->GetWidth()/m_nSelRes, 
		y*m_pRenderer->GetHeight()/m_nSelRes, 
		z, &vRes.x, &vRes.y, &vRes.z );
	m_pRenderer->SetCamera(tmpCam);
	return vRes;
}

void CCoverageBuffer::AddRenderMesh(IRenderMesh * pRM, Matrix34* pTranRotMatrix, IMaterial * pMaterial, bool bOutdoorOnly, bool bCompletelyInFrustum,bool bNoCull)
{
	FUNCTION_PROFILER_3DENGINE;

	// check material
	if(!pMaterial)
		pMaterial = pRM->GetMaterial();
	assert(pMaterial);
	if(!pMaterial)
		return;

	uint32 nVisibleChunksMask = 0;

	const int nVertCount = pRM->GetVertCount();
	int nIndCount = 0, nPosStride=0;
	const ushort * pIndices = pRM->GetIndices(&nIndCount);
	const byte * pPos = pRM->GetStridedPosPtr(nPosStride);
	assert(pIndices && pPos);
	if(!pIndices || !pPos)
		return;

	m_arrVertsWS_AddMesh.Clear();
	m_arrVertsWS_AddMesh.PreAllocate(nVertCount,nVertCount);

	m_arrVertsSS_AddMesh.Clear();
	m_arrVertsSS_AddMesh.PreAllocate(nVertCount,nVertCount);

	if (bCompletelyInFrustum)
	{ // clipping will be not needed
		// don't prepare WS positions
		Matrix44 matCombined(*((Matrix44*)m_matCombined));
		Matrix44 matTM(*pTranRotMatrix);
		matTM.Transpose();
		matCombined = matTM*matCombined;

		// transform all verts directly into SS
		for(int i=0; i<nVertCount; i++)
			m_arrVertsSS_AddMesh[i] = ProjectToScreen(*(Vec3*)&pPos[nPosStride*i], &matCombined.m00);
	}
	else
	{
		// transform vertices
		for(int i=0; i<nVertCount; i++)
		{
			Vec3 vPosOS = *(Vec3*)&pPos[nPosStride*i];
			m_arrVertsWS_AddMesh[i] = pTranRotMatrix->TransformPoint(vPosOS);
			m_arrVertsSS_AddMesh[i] = ProjectToScreen(m_arrVertsWS_AddMesh[i], m_matCombined);
		}
	}

	// render tris
	PodArray<CRenderChunk> *	pChunks = pRM->GetChunks();
  const uint32 VertexCount = pRM->GetVertCount();
	for(int nChunkId=0; nChunkId<pChunks->Count(); nChunkId++)
	{
		CRenderChunk * pChunk = pChunks->Get(nChunkId);
		if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
			continue;

		// skip transparent and alpha test
		const SShaderItem &shaderItem = pMaterial->GetShaderItem(pChunk->m_nMatID);
		if (!shaderItem.IsZWrite())
			continue;

    if(shaderItem.m_pShader && shaderItem.m_pShader->GetFlags()&EF_DECAL)
      continue;

		ECull nCullMode = shaderItem.m_pShader->GetCull();
		if (bNoCull)
			nCullMode = eCULL_None;

		nVisibleChunksMask |= (1 << (nChunkId));

		int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
    int nIndexStep = (pRM->GetPrimetiveType() == R_PRIMV_MULTI_STRIPS) ? 1 : 3;
		for(int i=pChunk->nFirstIndexId; i<nLastIndexId-2; i+=nIndexStep)
		{
      int32 I0	=	pIndices[i];
      int32 I1	=	pIndices[i+1];
      int32 I2	=	pIndices[i+2];
      if(nIndexStep==1 && (i&1))
      {
        I1	=	pIndices[i+2];
        I2	=	pIndices[i+1];
      }
      assert(I0<VertexCount);
      assert(I1<VertexCount);
      assert(I2<VertexCount);

			if(bCompletelyInFrustum)
				ScanTriangle(m_arrVertsSS_AddMesh.GetElements(), I0, I1, I2,
					true, nCullMode, bOutdoorOnly);
			else
				ScanTriangleWithCliping(m_arrVertsSS_AddMesh.GetElements(), m_arrVertsWS_AddMesh.GetElements(),
					I0, I1, I2,
					true, nCullMode, bOutdoorOnly, bCompletelyInFrustum);
		}
	}

	if (GetCVars()->e_cbuffer_draw_occluders)
	{
		int nFlags = 0;
		if (bNoCull)
			nFlags |= 1;
		pRM->DebugDraw( *pTranRotMatrix,nFlags,nVisibleChunksMask );
	}

	m_nObjectsWriten++;
}

/*void CCoverageBuffer::AddRenderMeshLC(IRenderMesh * pRM, Matrix34* pTranRotMatrix, IMaterial * pMaterial, bool bOutdoorOnly, bool bCompletelyInFrustum)
{
	// check material
	if(!pMaterial)
		pMaterial = pRM->GetMaterial();
	assert(pMaterial);
	if(!pMaterial)
		return;

	// transform camera frustum into object space for clipping
	Plane osFrustum[6];
	Matrix34 matInv = pTranRotMatrix->GetInverted();
	if(!bCompletelyInFrustum)
	{
		for(int i=0; i<GetCVars()->e_cbuffer_clip_planes_num; i++)
		{
			Plane PlaneWS = *m_Camera.GetFrustumPlane(i);
			Vec3 vPointOnPlaneWS = -PlaneWS.n*PlaneWS.d;
			Vec3 vNorm = matInv.TransformVector(PlaneWS.n).GetNormalized();
			Vec3 vPos  = matInv.TransformPoint(vPointOnPlaneWS);
			osFrustum[i] = Plane::CreatePlane(vNorm,vPos);
		}
	}
	
	Vec3 vCamPosOS = matInv.TransformPoint(m_Camera.GetPosition());

	// get access to geometry
	int nIndCount = 0, nPosStride=0;
	ushort * pIndices = pRM->GetIndices(&nIndCount);
	const byte * pPos = pRM->GetPosPtr(nPosStride,0,true);
	int nVertCount = pRM->GetSysVertCount();

	// prepare OS to SS matrix (world space not used at all)
	Matrix44 matCombined(*((Matrix44*)m_matCombined));
	Matrix44 matTM(*pTranRotMatrix);
	matTM.Transpose();
	matCombined = matTM*matCombined;

	// transform all verts into SS
	static PodArray<Vec3> arrVertsSS; arrVertsSS.PreAllocate(nVertCount,nVertCount);
	for(int i=0; i<nVertCount; i++)
		arrVertsSS[i] = ProjectToScreen(*(Vec3*)&pPos[nPosStride*i], &matCombined.m00);

	// go thou list of triangles
	PodArray<CRenderChunk> *	pChunks = pRM->GetChunks();
	for(int nChunkId=0; nChunkId<pChunks->Count(); nChunkId++)
	{
		CRenderChunk * pChunk = pChunks->Get(nChunkId);

		// skip transparent or no draw
		if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
			continue;
		const SShaderItem &shaderItem = pMaterial->GetShaderItem(pChunk->m_nMatID);
		if (!shaderItem.IsZWrite())
			continue;

		ECull nCullMode = shaderItem.m_pShader->GetCull();

		int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
		for(int i=pChunk->nFirstIndexId; i<nLastIndexId; i+=3)
		{
			assert(pIndices[i+0]<nVertCount);
			assert(pIndices[i+1]<nVertCount);
			assert(pIndices[i+2]<nVertCount);

			// SS verts
			const Vec3 & SS0 = arrVertsSS[pIndices[i+0]];
			const Vec3 & SS1 = arrVertsSS[pIndices[i+1]];
			const Vec3 & SS2 = arrVertsSS[pIndices[i+2]];

			// OS verts
			const Vec3 & OS0 = *(Vec3*)&pPos[nPosStride*pIndices[i+0]];
			const Vec3 & OS1 = *(Vec3*)&pPos[nPosStride*pIndices[i+1]];
			const Vec3 & OS2 = *(Vec3*)&pPos[nPosStride*pIndices[i+2]];

			{
				// 3d back face culling
				Vec3 vNorm = (OS0-OS1).Cross(OS0-OS2);
				float fDot = (vCamPosOS-OS0).Dot(vNorm);

				if(nCullMode == eCULL_Back)
				{
					if(fDot<0)
						continue;
				}
				else if(nCullMode == eCULL_Front)
				{
					if(fDot>0)
						continue;
				}
			}

			// check if triangle clipping is needed
			bool bClipingNeeded = !bCompletelyInFrustum;
			if(bClipingNeeded)
			{
				for(int p=0; p<GetCVars()->e_cbuffer_clip_planes_num ;p++)
				{
					if(	osFrustum[p].DistFromPlane(OS0)>0 || 
							osFrustum[p].DistFromPlane(OS1)>0 || 
							osFrustum[p].DistFromPlane(OS2)>0 )
					{
						bClipingNeeded = true;
						break;
					}
				}
			}

			if(!bClipingNeeded)
			{
				ScanTriangle(SS0, SS1, SS2, true, eCULL_Back, bOutdoorOnly);
				continue;
			}

			// make polygon for clipping
			static PodArray<Vec3> arrTriangle; 
			arrTriangle.Clear();
			arrTriangle.Add(OS0);
			arrTriangle.Add(OS1);
			arrTriangle.Add(OS2);

			// clip polygon
			for(int p=0; p<GetCVars()->e_cbuffer_clip_planes_num && arrTriangle.Count()>=3;p++)
				ClipPolygon(&arrTriangle, osFrustum[p]);

			if(arrTriangle.Count()<3)
				continue;

			// remove duplicates
			for(int i=1; i<arrTriangle.Count(); i++)
			{
				if(	arrTriangle.GetAt(i).IsEquivalent(arrTriangle.GetAt(i-1), .025f ) )
				{
					arrTriangle.Delete(i);
					i--;
				}
			}

			assert(arrTriangle.Count()<16);

			// scan
			if(arrTriangle.Count()>2 && arrTriangle.Count()<16)
			{
				Point2d arrVerts[16];

				for( int i=0; i<arrTriangle.Count(); i++ )
				{ // transform into screen space
					arrVerts[i]= ProjectToScreen(arrTriangle[i],&matCombined.m00);
					assert(arrVerts[i].x>-m_nSelRes);
					assert(arrVerts[i].y>-m_nSelRes);
					assert(arrVerts[i].x< m_nSelRes*2);
					assert(arrVerts[i].y< m_nSelRes*2);
				}

				for(int t=2; t<arrTriangle.Count(); t++)
					ScanTriangle(arrVerts[0], arrVerts[t-1], arrVerts[t], true, nCullMode, bOutdoorOnly);
			}
		}
	}

	m_nObjectsWriten++;
}*/

/*bool CCoverageBuffer::IsBBoxVisible(const AABB objBox)
{
  if(!m_nTrisInBuffer)
    return true;

	FUNCTION_PROFILER_3DENGINE;

  Point2d verts[8] = 
  { // transform into screen space
    // todo: check only front faces
    ProjectToScreen(mins.x,mins.y,mins.z),
    ProjectToScreen(mins.x,maxs.y,mins.z),
    ProjectToScreen(maxs.x,mins.y,mins.z),
    ProjectToScreen(maxs.x,maxs.y,mins.z),
    ProjectToScreen(mins.x,mins.y,maxs.z),
    ProjectToScreen(mins.x,maxs.y,maxs.z),
    ProjectToScreen(maxs.x,mins.y,maxs.z),
    ProjectToScreen(maxs.x,maxs.y,maxs.z)
  };

  // find 2d bounds to use it as 2d quad
  Point2d min2d = verts[0], max2d = verts[0];
	float fNearestZ = 0;
  for(int i=0; i<8; i++)
  {
    if(verts[i].x < min2d.x)
      min2d.x = verts[i].x;
    if(verts[i].x > max2d.x)
      max2d.x = verts[i].x;
    if(verts[i].y < min2d.y)
      min2d.y = verts[i].y;
    if(verts[i].y > max2d.y)
      max2d.y = verts[i].y;
		if(verts[i].z>fNearestZ)
			fNearestZ = verts[i].z;
  }

//  if(min2d.x<-900 || min2d.y<-900 || max2d.x<-900 || max2d.y<-900)
  //  return true; // object intersect near plane

  // make it little bigger to be sure that it's bigger than 1 pixel
  min2d.x -= 0.25f;
  min2d.y -= 0.25f;
  max2d.x += 0.25f;
  max2d.y += 0.25f;

  return IsQuadVisible(min2d, max2d, fNearestZ);
}*/

bool CCoverageBuffer::IsObjectVisible(const AABB _objBox, EOcclusionObjectType eOcclusionObjectType, float fDistance)
{
	AABB objBox = _objBox;
	float fExt = fDistance*0.025f;
	objBox.min -= Vec3(fExt,fExt,fExt);
	objBox.max += Vec3(fExt,fExt,fExt);

//  DrawBBox(objBox);

	switch(eOcclusionObjectType)
	{
	case eoot_OCCLUDER:
		return IsBoxVisible_OCCLUDER(objBox);
	case eoot_OCEAN:
		return IsBoxVisible_OCEAN(objBox);
	case eoot_OCCELL:
		return IsBoxVisible_OCCELL(objBox);			
	case eoot_OCCELL_OCCLUDER:
		return IsBoxVisible_OCCELL_OCCLUDER(objBox);
	case eoot_OBJECT:
		return IsBoxVisible_OBJECT(objBox);
	case eoot_OBJECT_TO_LIGHT:
		return IsBoxVisible_OBJECT_TO_LIGHT(objBox);
	case eoot_TERRAIN_NODE:
		return IsBoxVisible_TERRAIN_NODE(objBox);
	case eoot_PORTAL:
		return IsBoxVisible_PORTAL(objBox);
	}

	assert(!"Undefined occluder type");
	
	return true;
}

bool CCoverageBuffer::IsBoxVisible_TERRAIN_NODE(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool CCoverageBuffer::IsBoxVisible_OCCELL_OCCLUDER(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool CCoverageBuffer::IsBoxVisible_OCCLUDER(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool CCoverageBuffer::IsBoxVisible_OCEAN(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool CCoverageBuffer::IsBoxVisible_OCCELL(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool CCoverageBuffer::IsBoxVisible_OBJECT(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool CCoverageBuffer::IsBoxVisible_OBJECT_TO_LIGHT(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool CCoverageBuffer::IsBoxVisible_PORTAL(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool CCoverageBuffer::IsBoxVisible(const AABB objBox)
{
	if(!m_nTrisWriten || !GetCVars()->e_cbuffer)
		return true;

	// if camera is inside of box
	if(objBox.IsContainSphere(m_Camera.GetPosition(), -m_Camera.GetNearPlane()*2.f))
		return true;

	/*if(m_bTreeIsReady && GetCVars()->e_cbuffer_test_mode&1 && m_pTree)
	{ // world space test
//		FRAME_PROFILER( "CB::IsBoxVisible_WS", GetSystem(), PROFILE_3DENGINE );

		static PodArray<CWSNode*> lstNodes; 
		lstNodes.Clear();

		bool bVisible = m_pTree->IsBoxVisible( objBox, this, lstNodes );

		if(bVisible || !lstNodes.Count())
			return bVisible;

		if(GetCVars()->e_cbuffer_tree_debug)
			for(int i=0; i<lstNodes.Count(); i++)
				lstNodes[i]->DrawDebug(this, true);
	}

	if(GetCVars()->e_cbuffer_test_mode&2)*/
	{ // image space test
//		FRAME_PROFILER( "CB::IsBoxVisible_IS", GetSystem(), PROFILE_3DENGINE );

		if(GetCVars()->e_cbuffer_debug == 32)
			DrawBBox(objBox);

		m_nObjectsTested++;

		Vec3 arrVerts3d[8] = 
		{
			Vec3(objBox.min.x,objBox.min.y,objBox.min.z),
			Vec3(objBox.min.x,objBox.max.y,objBox.min.z),
			Vec3(objBox.max.x,objBox.min.y,objBox.min.z),
			Vec3(objBox.max.x,objBox.max.y,objBox.min.z),
			Vec3(objBox.min.x,objBox.min.y,objBox.max.z),
			Vec3(objBox.min.x,objBox.max.y,objBox.max.z),
			Vec3(objBox.max.x,objBox.min.y,objBox.max.z),
			Vec3(objBox.max.x,objBox.max.y,objBox.max.z)
		};

		Point2d arrVerts[8];
		
		// transform into screen space
		for(int i=0; i<8; i++)
			arrVerts[i] = ProjectToScreen(arrVerts3d[i], m_matCombined);

		Vec3 vCamPos = m_Camera.GetPosition();

		{
//			FRAME_PROFILER( "CB::IsBoxVisible_IS_Scan", GetSystem(), PROFILE_3DENGINE );

			// render only front faces of box
			if(vCamPos.x>objBox.max.x)
			{
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 2, 3, 6, false, eCULL_Back, false))
					return true;
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 6, 3, 7, false, eCULL_Back, false))
					return true;
			}                                                                              
			else if(vCamPos.x<objBox.min.x)
			{                                                                              
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 1, 0, 4, false, eCULL_Back, false))
					return true;
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 1, 4, 5, false, eCULL_Back, false))
					return true;
			}                                                                              

			if(vCamPos.y>objBox.max.y)                                                           
			{                                                                              
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 3, 1, 7, false, eCULL_Back, false))
					return true;
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 7, 1, 5, false, eCULL_Back, false))
					return true;
			}                                                                              
			else if(vCamPos.y<objBox.min.y)                                                           
			{                                                                              
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 0, 2, 4, false, eCULL_Back, false))
					return true;
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 4, 2, 6, false, eCULL_Back, false))
					return true;
			}

			if(vCamPos.z>objBox.max.z)                                                           
			{                                                                              
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 5, 4, 6, false, eCULL_Back, false))
					return true;
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 5, 6, 7, false, eCULL_Back, false))
					return true;
			}                                                                              
			else if(vCamPos.z<objBox.min.z)                                                           
			{                                                                              
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 0, 1, 2, false, eCULL_Back, false))
					return true;
				if(ScanTriangleWithCliping(arrVerts, arrVerts3d, 2, 1, 3, false, eCULL_Back, false))
					return true;
			}
		}
	}
//	else
	//	return true;

	m_nObjectsTestedAndRejected++;
	return false;
}

void CCoverageBuffer::DrawDebug(int nStep)
{ // project buffer to the screen
  if(!nStep)
    return;

  m_pRenderer->Set2DMode(true,m_nSelRes,m_nSelRes);

	Vec3 vSize(.4f,.4f,.4f);
	if(nStep==1)
		vSize = Vec3(.5f,.5f,.5f);

  for(int x=0; x<m_nSelRes; x+=nStep)
  for(int y=0; y<m_nSelRes; y+=nStep)
	{
		Vec3 vPos((float)x,m_nSelRes-(float)y,0);
		vPos += Vec3(0.5f,-0.5f,0);
		float v = Ffabs(m_farrBuffer[y][x]);
		if(v < m_fFixedZFar)
			v = m_fFixedZFar;
		float fBr = 1.f / v * m_fFixedZFar;
		fBr = cry_sqrtf(fBr)*GetCVars()->e_cbuffer_debug_draw_scale;
		if(fBr>1.f)
			fBr=1.f;
		ColorB col((uint8)(fBr*255), (uint8)(fBr*255), /*(m_farrBuffer[x][y]>0) ? */(uint8)(fBr*255)/* : 255*/, (uint8)255);
		GetRenderer()->GetIRenderAuxGeom()->DrawAABB(AABB(vPos-vSize, vPos+vSize), nStep==1, col, eBBD_Faceted);
	}

  m_pRenderer->Set2DMode(false,m_nSelRes,m_nSelRes);
}

// return number of vertices to add
int CCoverageBuffer::ClipEdge(const Vec3 & v1, const Vec3 & v2, const Plane & ClipPlane, Vec3 & vRes1, Vec3 & vRes2)
{
  float d1 = -ClipPlane.DistFromPlane(v1);
  float d2 = -ClipPlane.DistFromPlane(v2);
  if(d1<0 && d2<0)
    return 0; // all clipped = do not add any vertices

  if(d1>=0 && d2>=0)
  {
    vRes1 = v2;
    return 1; // both not clipped - add second vertex
  }

  // calculate new vertex
  Vec3 vIntersectionPoint = v1 + (v2-v1)*(Ffabs(d1)/(Ffabs(d2)+Ffabs(d1)));

#ifdef _DEBUG
  float fNewDist = -ClipPlane.DistFromPlane(vIntersectionPoint);
  assert(Ffabs(fNewDist)<0.01f);
#endif

  if(d1>=0 && d2<0)
  { // from vis to no vis
    vRes1 = vIntersectionPoint;
    return 1;
  }
  else if(d1<0 && d2>=0)
  { // from not vis to vis
    vRes1 = vIntersectionPoint;
    vRes2 = v2;
    return 2;
  }

  assert(0);
  return 0;
}

void CCoverageBuffer::ClipPolygon(PodArray<Vec3> * pPolygon, const Plane & ClipPlane)
{
  static PodArray<Vec3> PolygonOut; // Keep this list static to not perform reallocation every time.
  PolygonOut.Clear();
  // clip edges, make list of new vertices
  for(int i=0; i<pPolygon->Count(); i++)
  {
    Vec3 vNewVert1(0,0,0), vNewVert2(0,0,0);
    if(int nNewVertNum = ClipEdge(pPolygon->GetAt(i), pPolygon->GetAt((i+1)%pPolygon->Count()), ClipPlane, vNewVert1, vNewVert2))
    {
      PolygonOut.Add(vNewVert1);
      if(nNewVertNum>1)
        PolygonOut.Add(vNewVert2);
    }
  }

  // check result
  for(int i=0; i<PolygonOut.Count(); i++)
  {
    float d1 = -ClipPlane.DistFromPlane(PolygonOut.GetAt(i));
    assert(d1>=-0.01f);
  }

  assert(PolygonOut.Count()==0 || PolygonOut.Count() >= 3);

	pPolygon->Clear();
	pPolygon->AddList( PolygonOut );
}

uint CCoverageBuffer::ScanTriangleWithCliping(const Point2d * pVerts2d, const Vec3 * pVerts3d, int i1, int i2, int i3, 
																							bool bWriteAllowed, ECull nCullMode, bool bOutdoorOnly, bool bCompletelyInFrustum)
{
//	FUNCTION_PROFILER_3DENGINE;

  if(pVerts3d[i1].IsEquivalent(pVerts3d[i2],VEC_EPSILON) || pVerts3d[i1].IsEquivalent(pVerts3d[i3],VEC_EPSILON) || pVerts3d[i2].IsEquivalent(pVerts3d[i3],VEC_EPSILON))
    return 0; // skip degenerative triangles

	// 3d back face culling
	Vec3 vNorm = (pVerts3d[i1]-pVerts3d[i2]).Cross(pVerts3d[i1]-pVerts3d[i3]);
	float fDot = (m_Camera.GetPosition()-pVerts3d[i1]).Dot(vNorm);

  if(nCullMode == eCULL_Back)
  {
    if(fDot<0)
      return 0;
  }
  else if(nCullMode == eCULL_Front)
  {
    if(fDot>0)
      return 0;
  }

	if(bCompletelyInFrustum)
		return ScanTriangle(pVerts2d, i1, i2, i3, bWriteAllowed, nCullMode, bOutdoorOnly);

  // test if clipping needed 
  // todo: try to clip all triangles without this test (cache transformed vertices)
  bool bClipingNeeded = false;
	int p=0;
  for(; p<GetCVars()->e_cbuffer_clip_planes_num ;p++)
  {
    if(	m_Camera.GetFrustumPlane(p)->DistFromPlane(pVerts3d[i1])>0 || 
				m_Camera.GetFrustumPlane(p)->DistFromPlane(pVerts3d[i2])>0 || 
				m_Camera.GetFrustumPlane(p)->DistFromPlane(pVerts3d[i3])>0 )
    {
      bClipingNeeded = true;
      break;
    }
  }

  if(!bClipingNeeded) // use already calculated 2d points
    return ScanTriangle(pVerts2d, i1, i2, i3, bWriteAllowed, nCullMode, bOutdoorOnly);

  // make polygon
  static PodArray<Vec3> arrTriangle;
  arrTriangle.Clear();
  arrTriangle.Add(pVerts3d[i1]);
  arrTriangle.Add(pVerts3d[i2]);
  arrTriangle.Add(pVerts3d[i3]);

  // clip polygon
  for(; p<GetCVars()->e_cbuffer_clip_planes_num ;p++)
  { 
    ClipPolygon(&arrTriangle, *m_Camera.GetFrustumPlane(p));
  }

	if(!arrTriangle.Count())
		return 0;

/*
	assert(
		!IsEquivalent(arrTriangle[0],arrTriangle[1],0.1f) && 
		!IsEquivalent(arrTriangle[0],arrTriangle[2],0.1f) && 
		!IsEquivalent(arrTriangle[1],arrTriangle[2],0.1f)
		);
*/
  // remove duplicates
  for(int i=1; i<arrTriangle.Count(); i++)
  {
    if(	arrTriangle.GetAt(i).IsEquivalent(arrTriangle.GetAt(i-1), .025f ) )
    {
      arrTriangle.Delete(i);
      i--;
    }
  }

  assert(arrTriangle.Count()<16);

	int nPixelsDrawn = 0;

  // scan
  if(arrTriangle.Count()>2 && arrTriangle.Count()<16)
  {
    Point2d arrVerts[16];

    for( int i=0; i<arrTriangle.Count(); i++ )
    { // transform into screen space
      arrVerts[i] = ProjectToScreen(arrTriangle[i], m_matCombined);

      assert(arrVerts[i].z<1000.f);
			assert(arrVerts[i].x>-m_nSelRes);
			assert(arrVerts[i].y>-m_nSelRes);
			assert(arrVerts[i].x< m_nSelRes*2);
			assert(arrVerts[i].y< m_nSelRes*2);
    }

		for(int t=2; t<arrTriangle.Count(); t++)
		{
			nPixelsDrawn += ScanTriangle(arrVerts, 0, t-1, t, bWriteAllowed, nCullMode, bOutdoorOnly);
			if(!bWriteAllowed && nPixelsDrawn)
				break;
		}
  }

	return nPixelsDrawn;
}

uint CCoverageBuffer::ScanTriangle(const Point2d * pVerts2d, int i1, int i2, int i3, 
																	 bool bWriteAllowed, ECull nCullMode, bool bOutdoorOnly )
{
	// 2d back face culling
	float fDot = (pVerts2d[i1].x-pVerts2d[i2].x)*(pVerts2d[i3].y-pVerts2d[i2].y)-(pVerts2d[i1].y-pVerts2d[i2].y)*(pVerts2d[i3].x-pVerts2d[i2].x);        

	if(nCullMode == eCULL_Back)
	{
		if(fDot>0)
			return 0;
	}
	else if(nCullMode == eCULL_Front)
	{
		if(fDot<0)
			return 0;
	}

	if(bWriteAllowed)
		m_nTrisWriten++;
	else
		m_nTrisTested++;

	float fOutdoorOnly = bOutdoorOnly ? -1.f : 1.f;

	return Render2dTriangle(pVerts2d, i1, i2, i3, fOutdoorOnly, bWriteAllowed);
}

void CCoverageBuffer::BeginFrame(const CCamera & _camera)
{
	FUNCTION_PROFILER_3DENGINE;

//	m_bTreeIsReady = false;
	m_nSelRes = max(1, GetCVars()->e_cbuffer_resolution);
	m_farrBuffer.Allocate(m_nSelRes);

	m_Camera = _camera;

  m_fHalfRes = 0.5f*m_nSelRes;

  // make matrices
  float matModel[16]; memset(matModel, 0, sizeof(matModel));
  float matProj[16]; memset(matProj, 0, sizeof(matProj));

	CCamera tmpCam = m_pRenderer->GetCamera();
	m_pRenderer->SetCamera(m_Camera);
  m_pRenderer->GetModelViewMatrix(matModel);
  m_pRenderer->GetProjectionMatrix(matProj);
  MatMul4(m_matCombined,matProj,matModel);
	m_pRenderer->SetCamera(tmpCam);

	m_fFixedZFar = 1.f/(m_Camera.GetFarPlane()-m_Camera.GetNearPlane());

	if(!GetCVars()->e_cbuffer_hw)
	{
		// reset buffer
		for(int x=0; x<m_nSelRes; x+=1)
			for(int y=0; y<m_nSelRes; y+=1)
				m_farrBuffer[x][y] = m_fFixedZFar;
	}

	m_nTrisWriten=m_nObjectsWriten=m_nTrisTested=m_nObjectsTested=m_nObjectsTestedAndRejected=0;
}

void CCoverageBuffer::UpdateDepthTree()
{
	FUNCTION_PROFILER_3DENGINE;

	if(!m_farrBuffer.GetSize() || !GetCVars()->e_cbuffer || !m_nTrisWriten)
		return;

	{
		float fCurrentZNear = 0.0001f;

		for(int x=0; x<m_farrBuffer.GetSize(); x++)
		{
			for(int y=0; y<m_farrBuffer.GetSize(); y++)
			{
				float fVal = m_farrBuffer[x][y];

				float fValAbs = Ffabs(fVal);

				if(fValAbs > fCurrentZNear)
					fCurrentZNear = fValAbs;
			}
		}

		m_fCurrentZNearMeters = 1.f/fCurrentZNear;
	}

	// update tree
/*	if(!m_pTree || GetCVars()->e_cbuffer_tree_depth != m_pTree->m_nTreeLevel+1)
	{
		SAFE_DELETE(m_pTree);
		m_pTree = new CCoverageBuffer::CWSNode(GetCVars()->e_cbuffer_tree_depth);
	}

	SPlaneObject splitPlane;
	splitPlane.plane.SetPlane(m_Camera.GetPosition(), m_Camera.GetPosition()+m_Camera.GetViewdir(), m_Camera.GetPosition()+Vec3(0,0,-1));
	splitPlane.Update();

	m_pTree->Update(splitPlane, 0, m_nSelRes, 0, m_nSelRes, this, true, GetCVars()->e_cbuffer_tree_depth);

	m_bTreeIsReady = true;*/
}

bool IsABBBVisibleInFrontOfPlane_FAST(const AABB & objBox, const SPlaneObject & clipPlane)
{
	const f32* p = &objBox.min.x;
	if ( (clipPlane.plane|Vec3(p[clipPlane.vIdx2.x],p[clipPlane.vIdx2.y],p[clipPlane.vIdx2.z])) > 0) 
		return true;	

	return false;
}

bool IsABBBCompletelyInFrontOfPlane_FAST(const AABB & objBox, const SPlaneObject & clipPlane)
{
	const f32* p = &objBox.min.x;
	if ( (clipPlane.plane|Vec3(p[clipPlane.vIdx1.x],p[clipPlane.vIdx1.y],p[clipPlane.vIdx1.z])) > 0) 
		return true;	

	return false;
}

/*
bool IsABBBVisibleInFrontOfPlane(const AABB & objBox, const Plane & clipPlane)
{
	float dx = max(objBox.max.x-objBox.min.x,0.01f);
	float dy = max(objBox.max.y-objBox.min.y,0.01f);
	float dz = max(objBox.max.z-objBox.min.z,0.01f);

	for(float x=objBox.min.x; x<=objBox.max.x; x+=dx)
	for(float y=objBox.min.y; y<=objBox.max.y; y+=dy)
	for(float z=objBox.min.z; z<=objBox.max.z; z+=dz)
	{
		if(clipPlane.DistFromPlane(Vec3(x,y,z))>0)
			return true;
	}

	return false;
}
*/
#define CULLBOXBYPLANE_IN_FRONT		2
#define CULLBOXBYPLANE_BEHIND			1
#define CULLBOXBYPLANE_INTERSECT	0

int CullBoxByPlane_FAST(const AABB & objBox, const SPlaneObject & clipPlane)
{
	const f32* p = &objBox.min.x;

	if ( (clipPlane.plane|Vec3(p[clipPlane.vIdx1.x],p[clipPlane.vIdx1.y],p[clipPlane.vIdx1.z])) > 0) 
		return CULLBOXBYPLANE_IN_FRONT;	

	if ( (clipPlane.plane|Vec3(p[clipPlane.vIdx2.x],p[clipPlane.vIdx2.y],p[clipPlane.vIdx2.z])) < 0) 
		return CULLBOXBYPLANE_BEHIND;	

	return CULLBOXBYPLANE_INTERSECT;
}
/*
int CullBoxByPlane(const AABB & objBox, const Plane & clipPlane)
{
	bool bFoundInFront=false;
	bool bFoundBehind=false;

	float dx = max(objBox.max.x-objBox.min.x,0.01f);
	float dy = max(objBox.max.y-objBox.min.y,0.01f);
	float dz = max(objBox.max.z-objBox.min.z,0.01f);

	for(float x=objBox.min.x; x<=objBox.max.x; x+=dx)
	for(float y=objBox.min.y; y<=objBox.max.y; y+=dy)
	for(float z=objBox.min.z; z<=objBox.max.z; z+=dz)
	{
		float fDist = clipPlane.DistFromPlane(Vec3(x,y,z));

		if(fDist>0)
			bFoundInFront = true;
		else if(fDist<0)
			bFoundBehind = true;
	}

	if(bFoundInFront && bFoundBehind)
		return CULLBOXBYPLANE_INTERSECT;

	if(bFoundInFront)
		return CULLBOXBYPLANE_IN_FRONT;

	if(bFoundBehind)
		return CULLBOXBYPLANE_BEHIND;

	assert(0);

	return CULLBOXBYPLANE_INTERSECT;
}*/
/*
CCoverageBuffer::CWSNode::CWSNode(int nRecursion)
{
	nRecursion--;

	memset(this,0,sizeof(*this));

	m_nTreeLevel = nRecursion;

	if(nRecursion)
	{		
		m_arrChilds[0] = new CWSNode(nRecursion);
		m_arrChilds[1] = new CWSNode(nRecursion);
	}
}

void CCoverageBuffer::CWSNode::Update( SPlaneObject p, int x1, int x2, int y1, int y2, CCoverageBuffer * pCB, bool bVertSplit, int nRecursion )
{
	m_x1 = x1;
	m_x2 = x2;
	m_y1 = y1;
	m_y2 = y2;

	nRecursion--;

	if(nRecursion)
	{		
		m_SplitPlane = p;

		int mid_x = (x2+x1)/2;
		int mid_y = (y2+y1)/2;

		assert(m_arrChilds[0] && m_arrChilds[1]);

		if(bVertSplit)
		{
			// make horizontal plane from mid point
			Vec3 pWSMidPos0 = pCB->ProjectFromScreen(mid_x, mid_y, 0);
			Vec3 pWSMidPos1 = pCB->ProjectFromScreen(mid_x, mid_y, 1);
			Vec3 pWSLeftPos = pCB->ProjectFromScreen(0, mid_y, 0);
			SPlaneObject nextSplitPlane;
			nextSplitPlane.plane.SetPlane(pWSMidPos0,pWSLeftPos,pWSMidPos1);
			nextSplitPlane.Update();

			m_arrChilds[0]->Update(nextSplitPlane, x1, mid_x, y1, y2, pCB, !bVertSplit, nRecursion);
			m_arrChilds[1]->Update(nextSplitPlane, mid_x, x2, y1, y2, pCB, !bVertSplit, nRecursion);
		}
		else
		{
			// make vertical plane from mid point
			Vec3 pWSMidPos0 = pCB->ProjectFromScreen(mid_x, mid_y, 0);
			Vec3 pWSMidPos1 = pCB->ProjectFromScreen(mid_x, mid_y, 1);
			Vec3 pWSBottomPos = pCB->ProjectFromScreen(mid_x, 0, 0);
			SPlaneObject nextSplitPlane;
			nextSplitPlane.plane.SetPlane(pWSMidPos0,pWSMidPos1,pWSBottomPos);
			nextSplitPlane.Update();

			m_arrChilds[0]->Update(nextSplitPlane, x1, x2, y1, mid_y, pCB, !bVertSplit, nRecursion);
			m_arrChilds[1]->Update(nextSplitPlane, x1, x2, mid_y, y2, pCB, !bVertSplit, nRecursion);
		}

		// take closest near plane
		float d0 = m_arrChilds[0]->m_NearPlane.plane.DistFromPlane(pCB->GetCamera().GetPosition());
		float d1 = m_arrChilds[1]->m_NearPlane.plane.DistFromPlane(pCB->GetCamera().GetPosition());
		if(d0<d1)
			m_NearPlane = m_arrChilds[0]->m_NearPlane;
		else
			m_NearPlane = m_arrChilds[1]->m_NearPlane;

		// take most far far plane
		d0 = m_arrChilds[0]->m_FarPlane.plane.DistFromPlane(pCB->GetCamera().GetPosition());
		d1 = m_arrChilds[1]->m_FarPlane.plane.DistFromPlane(pCB->GetCamera().GetPosition());
		if(d0>d1)
			m_FarPlane = m_arrChilds[0]->m_FarPlane;
		else
			m_FarPlane = m_arrChilds[1]->m_FarPlane;

		m_fCurrentZFarMeters = max(m_arrChilds[0]->m_fCurrentZFarMeters, m_arrChilds[1]->m_fCurrentZFarMeters);
		m_fCurrentZNearMeters = min(m_arrChilds[0]->m_fCurrentZNearMeters, m_arrChilds[1]->m_fCurrentZNearMeters);
		m_bOutdoorVisible = m_arrChilds[0]->m_bOutdoorVisible || m_arrChilds[1]->m_bOutdoorVisible;
	}
	else
	{
		assert(!m_arrChilds[0] && !m_arrChilds[1]);

		m_arrChilds[0] = m_arrChilds[1] = NULL;

		float fCurrentZFar = 1000.f;
		float fCurrentZNear = 0.f;
		m_bOutdoorVisible = false;

		for(int x=x1; x<x2; x++)
		{
			for(int y=y1; y<y2; y++)
			{
				float fVal = pCB->m_farrBuffer[x][y];

				if(fVal <= pCB->m_fFixedZFar) // outdoor
					m_bOutdoorVisible = true;

				float fValAbs = Ffabs(fVal);

				if(fValAbs < fCurrentZFar)
					fCurrentZFar = fValAbs;

				if(fValAbs > fCurrentZNear)
					fCurrentZNear = fValAbs;
			}
		}

		assert(fCurrentZFar>0);
		assert(fCurrentZNear>0);

		m_fCurrentZFarMeters = 1.f/fCurrentZFar;
		m_fCurrentZNearMeters = 1.f/fCurrentZNear;

		m_FarPlane.plane.SetPlane(-pCB->GetCamera().GetViewdir(),
			pCB->GetCamera().GetPosition() + pCB->GetCamera().GetViewdir()*m_fCurrentZFarMeters  + pCB->GetCamera().GetViewdir()*fPlanesBias);
		m_FarPlane.Update();

		m_NearPlane.plane.SetPlane(-pCB->GetCamera().GetViewdir(),
			pCB->GetCamera().GetPosition() + pCB->GetCamera().GetViewdir()*m_fCurrentZNearMeters + pCB->GetCamera().GetViewdir()*fPlanesBias);
		m_NearPlane.Update();

		if(m_fCurrentZFarMeters != m_fCurrentZNearMeters && pCB->GetCVars()->e_cbuffer_tree_debug)
			DrawDebug(pCB, false);
	}
}
*/
/*
bool CCoverageBuffer::CWSNode::IsBoxVisible( const AABB & objBox, CCoverageBuffer * pCB, PodArray<CWSNode*> & lstNodes)
{
//	assert(pCB->IsABBBVisibleInFrontOfPlane_FAST(objBox, m_NearPlane) == IsABBBVisibleInFrontOfPlane(objBox, m_NearPlane.plane));

	if(IsABBBVisibleInFrontOfPlane_FAST(objBox, m_NearPlane))
	{
		if(pCB->m_Camera.IsAABBVisible_FH(objBox) == CULL_INCLUSION)
			return true;

//		assert(pCB->m_Camera.IsAABBVisible_EH(objBox) != CULL_EXCLUSION);
	}

	if(!IsABBBVisibleInFrontOfPlane_FAST(objBox, m_FarPlane))
		return false;

	bool bVisible = false;

	if(m_arrChilds[0])
	{
		int nCull = CullBoxByPlane_FAST(objBox, m_SplitPlane);

//		assert(nCull == CullBoxByPlane(objBox, m_SplitPlane.plane));

		if(nCull == CULLBOXBYPLANE_IN_FRONT)
			bVisible |= m_arrChilds[0]->IsBoxVisible(objBox, pCB, lstNodes);
		else if(nCull == CULLBOXBYPLANE_BEHIND)
			bVisible |= m_arrChilds[1]->IsBoxVisible(objBox, pCB, lstNodes);
		else
		{
			bVisible |= m_arrChilds[0]->IsBoxVisible(objBox, pCB, lstNodes);
			bVisible |= m_arrChilds[1]->IsBoxVisible(objBox, pCB, lstNodes);
		}
	}
	else
	{
		// test these nodes later if needed
		lstNodes.Add(this);
	}

	return bVisible;
}

void CCoverageBuffer::CWSNode::DrawDebug(CCoverageBuffer * pCB, bool bDrawPixels)
{
	pCB->m_pRenderer->Set2DMode(true,pCB->m_nSelRes,pCB->m_nSelRes);

	Vec3 vShift(0.5f,-0.5f,0);
	Vec3 arrQuad[4];
	arrQuad[0].Set(m_x1-0.5f,pCB->m_nSelRes-m_y1+0.5f,0);
	arrQuad[1].Set(m_x1-0.5f,pCB->m_nSelRes-m_y2+0.5f,0);
	arrQuad[2].Set(m_x2-0.5f,pCB->m_nSelRes-m_y2+0.5f,0);
	arrQuad[3].Set(m_x2-0.5f,pCB->m_nSelRes-m_y1+0.5f,0);
	for(int i=0; i<4; i++)
		arrQuad[i] += vShift;
	pCB->m_pRenderer->GetIRenderAuxGeom()->DrawPolyline(arrQuad,4,true,ColorB(Col_White));

	if(bDrawPixels)
	{
		int nStep = 1;

		for(int x=m_x1; x<m_x2; x+=nStep)
		{
			for(int y=m_y1; y<m_y2; y+=nStep)
			{
				Vec3 vPos((float)x,pCB->m_nSelRes-(float)y,0);
				vPos += vShift;
				Vec3 vSize(.4f,.4f,.4f);
				float v = Ffabs(pCB->m_farrBuffer[x][y]);
				if(pCB->m_farrBuffer[x][y]<0)
					v = pCB->m_fFixedZFar;
				float fBr = (1.f/v) * pCB->m_fFixedZFar;
				fBr = cry_sqrtf(fBr)*GetCVars()->e_cbuffer_debug_draw_scale;
				if(fBr>1.f)
					fBr=1.f;
				ColorB col((uint8)(fBr*255), (uint8)(fBr*255), (pCB->m_farrBuffer[x][y]>0) ? (uint8)(fBr*255) : 255, (uint8)255);
				pCB->m_pRenderer->GetIRenderAuxGeom()->DrawAABB(AABB(vPos-vSize, vPos+vSize), false, col, eBBD_Faceted);
			}
		}
	}

	pCB->m_pRenderer->Set2DMode(false,pCB->m_nSelRes,pCB->m_nSelRes);
}
*/
float CCoverageBuffer::GetZNearInMeters() const
{ 
	//assert(m_pTree);
	return m_fCurrentZNearMeters;
}

float CCoverageBuffer::GetZFarInMeters() const
{ 
//	assert(m_pTree);
	return /*m_pTree ? m_pTree->m_fCurrentZFarMeters : */1024.f; 
}

bool CCoverageBuffer::IsOutdooVisible() 
{ 
//	assert(m_pTree);
	return /*m_pTree ? m_pTree->m_bOutdoorVisible : */true; 
}

void CCoverageBuffer::GetMemoryUsage(ICrySizer * pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "CoverageBuffer");
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddContainer( m_arrVertsWS_AddMesh );
	pSizer->AddContainer( m_arrVertsSS_AddMesh );
	m_farrBuffer.GetMemoryUsage(pSizer);
}

int CCoverageBuffer::Render2dTriangle(const Point2d * pVerts, int i1, int i2, int i3, float fOutdoorOnly, bool bWrite)
{
	int nPixelsDrawn = 0;

	// sort verts by height
	if(pVerts[i1].y > pVerts[i2].y) mySwap(i1, i2);
	if(pVerts[i1].y > pVerts[i3].y) mySwap(i1, i3);
	if(pVerts[i2].y > pVerts[i3].y) mySwap(i2, i3);

	int x0 = (int)pVerts[i1].x, y0 = (int)pVerts[i1].y;
	float z0 = pVerts[i1].z;

	int x1 = (int)pVerts[i2].x, y1 = (int)pVerts[i2].y;
	float z1 = pVerts[i2].z;

	int x2 = (int)pVerts[i3].x, y2 = (int)pVerts[i3].y;
	float z2 = pVerts[i3].z;

	// test for easy cases, else split triangle in two and render both half's
	if(y1 == y2)
	{
		if (y0 == y1)
			return 0;
		if(x1 > x2) 
		{
			mySwap(x1, x2);
			mySwapF(z1, z2);
		}
		nPixelsDrawn += Render2dSubTriangle(
			x1, y1, z1,
			x2, y2, z2,
			x0, y0, z0,
			fOutdoorOnly, bWrite);
	}
	else if(y0 == y1)
	{
		if (y1 == y2)
			return 0;
		if(x0 > x1) 
		{
			mySwap(x0, x1);
			mySwapF(z0, z1);
		}
		nPixelsDrawn += Render2dSubTriangle(
			x0, y0, z0,
			x1, y1, z1,
			x2, y2, z2,
			fOutdoorOnly, bWrite);
	}
	else
	{
		// compute x pos of the vert that builds the splitting line with x1
		int tmp_x = x0 + (int)(0.5f + (float)(y1-y0) * (float)(x2-x0) / (float)(y2-y0));
		float tmp_z = z0 + (float)(/*0.5f + */(float)(y1-y0) * (float)(z2-z0) / (float)(y2-y0));

		if(x1 > tmp_x) 
		{
			mySwap(x1, tmp_x);
			mySwapF(z1, tmp_z);
		}
		nPixelsDrawn += Render2dSubTriangle(
			x1, y1, z1,
			tmp_x, y1, tmp_z,
			x0, y0, z0,
			fOutdoorOnly, bWrite);
		nPixelsDrawn += Render2dSubTriangle(
			x1, y1, z1,
			tmp_x, y1, tmp_z,
			x2, y2, z2,
			fOutdoorOnly, bWrite);
	}

	return nPixelsDrawn;
}

int CCoverageBuffer::Render2dSubTriangle(	int x0, int y0, float z0,
																					int x1, int y1, float z1, 
																					int x2, int y2, float z2,
																					float fOutdoorOnly, bool bWrite)
{
	y0 = CLAMP(y0, 0, m_nSelRes);
	y1 = CLAMP(y1, 0, m_nSelRes);
	y2 = CLAMP(y2, 0, m_nSelRes);

	// compute slopes
	float dx0 = (float)(x2 - x0) / (y2 - y0);
	float dx1 = (float)(x2 - x1) / (y2 - y1);

	float dz0 = (float)(z2 - z0) / (y2 - y0);
	float dz1 = (float)(z2 - z1) / (y2 - y1);

	float lx = (float) x0, rx = (float) x1;
	float lz = (float) z0, rz = (float) z1;

	float * pLine;
	int nLinesNum;
	int nPitch = m_farrBuffer.GetSize();

	if(y0 < y2)
	{ 
		pLine = m_farrBuffer.GetData() + y0 * nPitch; 
		nLinesNum = y2 - y0; 
	}
	else  
	{ 
		pLine = m_farrBuffer.GetData() + y2 * nPitch; 
		nLinesNum = y0 - y2; 

		lx = rx = (float)x2; 
		lz = rz = z2;
	}

	int nPixelsDrawn=0;

	float fBias = bWrite ?
		(1.f + (GetCVars()->e_cbuffer_bias-1.f)*0.1f) : GetCVars()->e_cbuffer_bias;

	for(int l=0; l<nLinesNum; ++l)
	{
		float dz = ((int)rx-(int)lx) ? (rz-lz)/((int)rx-(int)lx) : 0;
		float curr_z = lz;

		lx = max(lx,0.f);
		rx = min(rx,(float)m_nSelRes-1.f);

		float * pPixel = (pLine + (int)(lx));

		// render target bounds
		bool bNotFirstLine = pLine>m_farrBuffer.GetData();
		bool bNotPreLastLine = pLine<m_farrBuffer.GetDataEnd()-nPitch*2;
		bool bNotLastLine = pLine<m_farrBuffer.GetDataEnd()-nPitch;

		// triangle bounds
		bool bFirstOrLastLine = l==0 || l==nLinesNum-1;

		int lxi = (int)(lx);
		int rxi = (int)(rx);

		for(int j=lxi; j<=rxi; ++j)
		{
			assert((pLine + j)>=m_farrBuffer.GetData());
			assert((pLine + j)< m_farrBuffer.GetDataEnd());
			assert(curr_z>=0);

			float fNewValBiased = curr_z * fBias;
			if(fNewValBiased > Ffabs(*pPixel))
			{
				nPixelsDrawn++;

				if(bWrite)
				{
					*(pPixel) = curr_z;

					if(bFirstOrLastLine || j==lxi || j==rxi)
					{ // draw fat pixel on the edges of triangle
						// horizontal
						if(j>0 && fNewValBiased > Ffabs(*(pPixel-1)))
							*(pPixel-1) = curr_z;
						if(j<(m_nSelRes-1) && fNewValBiased > Ffabs(*(pPixel+1)))
							*(pPixel+1) = curr_z;

						// vert
						if(bNotFirstLine && fNewValBiased > Ffabs(*(pPixel-nPitch)))
							*(pPixel-nPitch) = curr_z;
						if(bNotLastLine && fNewValBiased > Ffabs(*(pPixel+nPitch)))
							*(pPixel+nPitch) = curr_z;
						if(bNotPreLastLine && fNewValBiased > Ffabs(*(pPixel+nPitch*2)))
							*(pPixel+nPitch*2) = curr_z;
					}
				}
				else
					return nPixelsDrawn;
			}

			curr_z += dz;
			pPixel ++;
		}

		lx  += dx0;
		rx  += dx1;
		lz  += dz0;
		rz  += dz1;
		pLine += nPitch;
	}

	return nPixelsDrawn;
}
