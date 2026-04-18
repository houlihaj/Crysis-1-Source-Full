////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	28/09/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  Implementation of all software skinning & morphing functions
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <IRenderAuxGeom.h>
#include "Model.h"
#include "ModelMesh.h"
#include "ModelSkeleton.h"
#include "CharacterInstance.h"
#include "CharacterManager.h"
#include "CryModEffMorph.h"
#include "GeomQuery.h"



// returns the index of the morph target, in the indexation of the array of morph targets
int CModelMesh::FindMorphTarget (const char* szMorphTargetName)
{
	int numMophs = m_morphTargets.size();
	for (int i=0; i<numMophs; ++i)
		if (!stricmp(m_morphTargets[i]->m_name.c_str(), szMorphTargetName))
			return i;
	return -1;
}





CloseInfo CModelMesh::FindClosestPointOnMesh( const Vec3& RMWPosition ) 
{ 

	f32 distance=99999999.0f;
	uint32 face=0xffffffff;
	Vec3 ClosestPoint;

	// 
	//SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	//renderFlags.SetFillMode( e_FillModeWireframe );
	//renderFlags.SetDrawInFrontMode( e_DrawInFrontOn );
	//g_pAuxGeom->SetRenderFlags( renderFlags );


	LockFullRenderMesh(m_pModel->m_nBaseLOD);
	int32 numFaces = m_Indices / 3;

	CloseInfo cf;
	uint32 foundedPos(-1);
	for(int d=0; d<m_Indices; d += 3)	
	{
		Vec3 v0 = *(Vec3*)(m_pPositions + m_pIndices[d] * m_PosStride);//m_arrExtVertices[ m_arrExtFaces[d].i0 ].wpos1;
		Vec3 v1 = *(Vec3*)(m_pPositions + m_pIndices[d+1] * m_PosStride);//m_arrExtVertices[ m_arrExtFaces[d].i1 ].wpos1;
		Vec3 v2 = *(Vec3*)(m_pPositions + m_pIndices[d+2] * m_PosStride);//m_arrExtVertices[ m_arrExtFaces[d].i2 ].wpos1;

		//g_pAuxGeom->DrawLine( v0,RGBA8(0xff,0x00,0x00,0x00), v1,RGBA8(0x00,0xff,0x00,0x00) );
		//g_pAuxGeom->DrawLine( v1,RGBA8(0x00,0xff,0x00,0x00), v2,RGBA8(0x00,0x00,0xff,0x00) );
		//g_pAuxGeom->DrawLine( v2,RGBA8(0x00,0x00,0xff,0x00), v0,RGBA8(0xff,0x00,0x00,0x00) );

		//f32 sqdist=Distance::Point_Triangle(GetRMWPosition(),Triangle(v0,v1,v2));
		Vec3 TriMiddle=(v0+v1+v2)/3.0f;
		f32 sqdist=(TriMiddle-RMWPosition)|(TriMiddle-RMWPosition);
		if 	(distance>sqdist) 
		{
			cf.tv0		= v0;
			cf.tv1		= v1;
			cf.tv2		= v2;

			cf.middle	= TriMiddle;
			distance=sqdist;
			foundedPos = d;
			//			face=d;
			ClosestPoint=TriMiddle;
		}
	}


	for (uint32 i = 0, end = m_arrExtVerticesCached.size(); i < end; i += 3)
	{
		Vec3 v0 = m_arrExtVerticesCached[i].wpos1;
		Vec3 v1 = m_arrExtVerticesCached[i+ 1].wpos1;
		Vec3 v2 = m_arrExtVerticesCached[i + 2].wpos1;

		//	g_pAuxGeom->DrawLine( v0,RGBA8(0xff,0x00,0x00,0x00), v1,RGBA8(0x00,0xff,0x00,0x00) );
		//	g_pAuxGeom->DrawLine( v1,RGBA8(0x00,0xff,0x00,0x00), v2,RGBA8(0x00,0x00,0xff,0x00) );
		//	g_pAuxGeom->DrawLine( v2,RGBA8(0x00,0x00,0xff,0x00), v0,RGBA8(0xff,0x00,0x00,0x00) );

		if (m_arrExtVerticesCached[i].wpos1 == cf.tv0 && m_arrExtVerticesCached[i+1].wpos1 == cf.tv1 && m_arrExtVerticesCached[i+2].wpos1 == cf.tv2)
		{
			face = i/3;
			break;
		}
	}

	if (face == -1)
	{

		AttSkinVertex vertex;
		ExtSkinVertex skin;

		skin = GetSkinVertex(foundedPos);
		vertex.FromExtSkinVertex(skin);

		uint32 id0 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx0;
		uint32 id1 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx1;
		uint32 id2 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx2;
		uint32 id3 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx3;

		vertex.boneIDs[0] = id0;
		vertex.boneIDs[1] = id1;
		vertex.boneIDs[2] = id2;
		vertex.boneIDs[3] = id3;

		m_arrExtVerticesCached.push_back(vertex);

		skin = GetSkinVertex(foundedPos+1);
		vertex.FromExtSkinVertex(skin);

		id0 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx0;
		id1 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx1;
		id2 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx2;
		id3 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx3;

		vertex.boneIDs[0] = id0;
		vertex.boneIDs[1] = id1;
		vertex.boneIDs[2] = id2;
		vertex.boneIDs[3] = id3;

		m_arrExtVerticesCached.push_back(vertex);

		skin = GetSkinVertex(foundedPos+2);
		vertex.FromExtSkinVertex(skin);

		id0 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx0;
		id1 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx1;
		id2 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx2;
		id3 = m_arrExtBoneIDs[m_pIndices[foundedPos]].idx3;

		vertex.boneIDs[0] = id0;
		vertex.boneIDs[1] = id1;
		vertex.boneIDs[2] = id2;
		vertex.boneIDs[3] = id3;

		m_arrExtVerticesCached.push_back(vertex);

		face = (m_arrExtVerticesCached.size() - 1) / 3;

	}
	UnlockFullRenderMesh(m_pModel->m_nBaseLOD);

	cf.FaceNr	=	face;

	/*
	Vec3 v0 = m_arrExtVerticesCached[face *3].wpos1;
	Vec3 v1 = m_arrExtVerticesCached[face *3+ 1].wpos1;
	Vec3 v2 = m_arrExtVerticesCached[face *3 + 2].wpos1;
	g_pAuxGeom->DrawLine( v0,RGBA8(0xff,0xff,0xff,0x00), v1,RGBA8(0xff,0xff,0xff,0x00) );
	g_pAuxGeom->DrawLine( v1,RGBA8(0xff,0xff,0xff,0x00), v2,RGBA8(0xff,0xff,0xff,0x00) );
	g_pAuxGeom->DrawLine( v2,RGBA8(0xff,0xff,0xff,0x00), v0,RGBA8(0xff,0xff,0xff,0x00) );
	*/

	return cf;
}


volatile int meshGuard;

void CModelMesh::LockFullRenderMesh(int lod)
{
	WriteLock lock(meshGuard);

	++m_iThreadMeshAccessCounter;

	IRenderMesh * pMesh = m_pModel->GetRenderMesh(/*m_pModel->m_nBaseLOD*//*m_nLOD*/lod);

	m_pIndices = pMesh->GetIndices(&m_Indices);
	m_pPositions = pMesh->GetStridedPosPtr(m_PosStride, 0);
	m_pSkinningInfo = pMesh->GetStridedHWSkinPtr(m_SkinStride, 0);
	m_pMorphingInfo = pMesh->GetStridedShapePtr(m_ShapeStride, 0);
	m_pUVs = pMesh->GetStridedUVPtr(m_UVStride, 0);
	m_pColors = pMesh->GetStridedColorPtr(m_ColorStride, 0);
	m_pTangents = pMesh->GetStridedTangentPtr(m_TangentStride, 0);
	m_pBinormals = pMesh->GetStridedBinormalPtr(m_BinormalStride, 0);
}

void CModelMesh::UnlockFullRenderMesh(int lod)
{
	WriteLock lock(meshGuard);

	--m_iThreadMeshAccessCounter;
	if (m_iThreadMeshAccessCounter == 0) {
		IRenderMesh * pMesh = m_pModel->GetRenderMesh(lod/*m_nLOD*/);
		pMesh->UnlockStream(VSF_GENERAL);
		pMesh->UnlockStream(VSF_HWSKIN_SHAPEDEFORM_INFO);
		pMesh->UnlockStream(VSF_HWSKIN_INFO);
		pMesh->UnlockStream(VSF_TANGENTS);
	}
}

ExtSkinVertex CModelMesh::GetSkinVertexNoInd(int pos)
{
	ExtSkinVertex vertex;

	//	if (m_pPositions) {
	vertex.wpos1 = *(Vec3*)(m_pPositions + pos * m_PosStride);
	//	}

	//	if (m_pBinormals) {
	vertex.binormal = *(Vec4sf*)(m_pBinormals + pos * m_BinormalStride);
	vertex.tangent = *(Vec4sf*)(m_pTangents + pos * m_TangentStride);
	//	}

	//	if (m_pSkinningInfo) {
	vertex.boneIDs = *(ColorB*)&((struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F*)(m_pSkinningInfo + pos * m_SkinStride))->indices;
	vertex.weights = *(ColorB*)&((struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F*)(m_pSkinningInfo + pos * m_SkinStride))->weights;
	//	}

	//vertex.color = *((ColorB*)(m_pColors + pos * m_ColorStride));
	vertex.u = ((Vec2*)(m_pUVs + pos * m_UVStride))->x;
	vertex.v = ((Vec2*)(m_pUVs + pos * m_UVStride))->y;

	//	if (m_pMorphingInfo) {
	vertex.wpos0 = ((struct_VERTEX_FORMAT_2xP3F_INDEX4UB*)(m_pMorphingInfo + pos * m_ShapeStride))->thin;
	vertex.wpos2 = ((struct_VERTEX_FORMAT_2xP3F_INDEX4UB*)(m_pMorphingInfo + pos * m_ShapeStride))->fat;
	vertex.color = *(ColorB*)&((struct_VERTEX_FORMAT_2xP3F_INDEX4UB*)(m_pMorphingInfo + pos * m_ShapeStride))->index;

/*#if !defined(XENON)
	assert(vertex.color.a<8);
	std::swap(vertex.color.r, vertex.color.b);
#endif
*/

#if defined(PS3)
	uint32 col = *(uint32*)&vertex.color;
	SwapEndian(col);
	vertex.color = *(ColorB*)&col;
	assert(vertex.color.a<8);
#elif !defined(XENON)
	assert(vertex.color.a<8);
	std::swap(vertex.color.r, vertex.color.b);
#endif

	//	}

	return vertex;
}

void  AttSkinVertex::FromExtSkinVertex(const ExtSkinVertex& skin)
{
	wpos0=skin.wpos0;
	wpos1=skin.wpos1;
	wpos2=skin.wpos2;
	color=skin.color;
	weights[0]=skin.weights[0]/255.0f;
	weights[1]=skin.weights[1]/255.0f;
	weights[2]=skin.weights[2]/255.0f;
	weights[3]=skin.weights[3]/255.0f;
	boneIDs[0]=skin.boneIDs[0];
	boneIDs[1]=skin.boneIDs[1];
	boneIDs[2]=skin.boneIDs[2];
	boneIDs[3]=skin.boneIDs[3];
}


void CModelMesh::GetSingleSkinnedVertex(CCharInstance* pMaster, int nV, Vec3* pVert, Vec3* pNorm, bool bLockUnlock)
{
#ifdef DEFINE_PROFILER_FUNCTION
	DEFINE_PROFILER_FUNCTION();
#endif

	int nFrameId = g_pCharacterManager->m_nUpdateCounter;//g_pIRenderer->GetFrameID(false);
	if (nFrameId != m_FrameIdSkinInit)
	{
		m_FrameIdSkinInit = nFrameId;
		InitSkinningExtSW(&pMaster->m_Morphing,pMaster->m_arrShapeDeformValues,0,m_pModel->m_nBaseLOD);
	}
	if (bLockUnlock)
		LockFullRenderMesh(m_pModel->m_nBaseLOD);

	if (m_pIndices)
		nV = m_pIndices[nV];

	const ExtSkinVertex ExtVert = GetSkinVertexNoInd(nV);

	//create the final vertex for skinning (blend between 3 characters)
	uint8 idx	=	ExtVert.color.a; 
	Vec3 v		= ExtVert.wpos0*VertexRegs[idx].x + ExtVert.wpos1*VertexRegs[idx].y + ExtVert.wpos2*VertexRegs[idx].z;

	//apply morph-targets
	if (m_arrExtMorphStream.size())
		v+=m_arrExtMorphStream[nV];

	uint32 id0 = m_arrExtBoneIDs[nV].idx0;
	uint32 id1 = m_arrExtBoneIDs[nV].idx1;
	uint32 id2 = m_arrExtBoneIDs[nV].idx2;
	uint32 id3 = m_arrExtBoneIDs[nV].idx3;
	f32 w0 = ExtVert.weights[0]/255.0f;
	f32 w1 = ExtVert.weights[1]/255.0f;
	f32 w2 = ExtVert.weights[2]/255.0f;
	f32 w3 = ExtVert.weights[3]/255.0f;



	if (Console::GetInst().ca_SphericalSkinning)
	{
		QuatD const& q0 = (const QuatD&)pMaster->m_arrNewSkinQuat[pMaster->m_iActiveFrame][id0];
		QuatD const& q1 = (const QuatD&)pMaster->m_arrNewSkinQuat[pMaster->m_iActiveFrame][id1];
		QuatD const& q2 = (const QuatD&)pMaster->m_arrNewSkinQuat[pMaster->m_iActiveFrame][id2];
		QuatD const& q3 = (const QuatD&)pMaster->m_arrNewSkinQuat[pMaster->m_iActiveFrame][id3];

		QuatD wquat		=	(q0*w0 +  q1*w1 + q2*w2 +	q3*w3);

		f32 l=1.0f/wquat.nq.GetLength();
		wquat.nq*=l;
		wquat.dq*=l;

		Matrix34 m34	= Matrix34(wquat);  
		if (pVert)
			*pVert = m34*v;

		if (pNorm)
		{
			Vec3 binormal	= Vec3( tPackB2F(ExtVert.binormal.x),tPackB2F(ExtVert.binormal.y),tPackB2F(ExtVert.binormal.z) );
			Vec3 tangent	= Vec3( tPackB2F(ExtVert.tangent.x), tPackB2F(ExtVert.tangent.y), tPackB2F(ExtVert.tangent.z) );
			*pNorm = Matrix33(m34) * (tangent%binormal) * tPackB2F(ExtVert.tangent.w);
		}
	}
	else
	{
		QuatTS const& q0 = pMaster->m_arrNewSkinQuat[pMaster->m_iActiveFrame][id0];
		QuatTS const& q1 = pMaster->m_arrNewSkinQuat[pMaster->m_iActiveFrame][id1];
		QuatTS const& q2 = pMaster->m_arrNewSkinQuat[pMaster->m_iActiveFrame][id2];
		QuatTS const& q3 = pMaster->m_arrNewSkinQuat[pMaster->m_iActiveFrame][id3];

		//do skinning&weighting for the vertex 
		if (pVert)
		{
			Vec3 v0 = q0*v*w0;
			Vec3 v1 = q1*v*w1;
			Vec3 v2 = q2*v*w2;
			Vec3 v3 = q3*v*w3;
			*pVert = v0+v1+v2+v3;
		}

		if (pNorm)
		{
			//do skinning & weighting for the binormal 
			Vec3 binormal	= Vec3( tPackB2F(ExtVert.binormal.x),tPackB2F(ExtVert.binormal.y),tPackB2F(ExtVert.binormal.z) );
			Vec3 b0 = q0.q*(binormal)*w0;
			Vec3 b1 = q1.q*(binormal)*w1;
			Vec3 b2 = q2.q*(binormal)*w2;
			Vec3 b3 = q3.q*(binormal)*w3;
			Vec3 b = b0+b1+b2+b3;

			//do skinning & weighting for the tangent 
			Vec3 tangent	 = Vec3( tPackB2F(ExtVert.tangent.x), tPackB2F(ExtVert.tangent.y), tPackB2F(ExtVert.tangent.z) );
			Vec3 t0 = q0.q*(tangent)*w0;
			Vec3 t1 = q1.q*(tangent)*w1;
			Vec3 t2 = q2.q*(tangent)*w2;
			Vec3 t3 = q3.q*(tangent)*w3;
			Vec3 t = t0+t1+t2+t3;

			*pNorm = (t%b) * tPackB2F(ExtVert.tangent.w);
		}
	}

	if (bLockUnlock)
		UnlockFullRenderMesh(m_pModel->m_nBaseLOD);
}



//////////////////////////////////////////////////////////////////////////
// skinning of internal vertices 
//////////////////////////////////////////////////////////////////////////
void CModelMesh::InitSkinningExtSW ( CMorphing* pMorphing, f32* pShapeDeform,uint32 nResetMode, int nLOD)
{
	//-----------------------------------------------------------------------
	//----           Create the internal morph-target stream             ----
	//-----------------------------------------------------------------------
	uint32 nNeedMorph=pMorphing->NeedMorph();
	if (nLOD==0 && nNeedMorph && Console::GetInst().ca_UseMorph)
	{

		uint32 numExtVertices = m_pModel->GetRenderMesh(0)->GetVertCount();//	assert(numExtVertices);
		if (m_arrExtMorphStream.size() != numExtVertices)
			m_arrExtMorphStream.resize(numExtVertices, Vec3(ZERO));
		else
			memset(&m_arrExtMorphStream[0],0,numExtVertices*sizeof(Vec3));

		uint32 num= pMorphing->m_arrMorphEffectors.size();
		for (uint32 nMorphEffector=0; nMorphEffector<num; ++nMorphEffector)
		{
			const CryModEffMorph& rMorphEffector = pMorphing->m_arrMorphEffectors[nMorphEffector];
			int nMorphTargetId = rMorphEffector.getMorphTargetId ();
			if (nMorphTargetId < 0)
				continue;
			CMorphTarget* rMorphSkin = this->m_pModel->getMorphSkin (0, nMorphTargetId);
			float fBlending = rMorphEffector.getBlending();
			uint32 num = rMorphSkin->m_vertices.size();
			for(uint32 i=0; i<num; i++) 
			{
				// add all morph-values to the stream to use subsequently in skinning
				uint32 idx = rMorphSkin->m_vertices[i].nVertexId;
				m_arrExtMorphStream[idx]+=rMorphSkin->m_vertices[i].m_MTVertex*fBlending;
			}
		}
	}

	//-----------------------------------------------------------------------
	//----    Create 8 Vec4 vectors that contain the blending values     ----
	//---                    to blend between 3 models                    ---
	//-----------------------------------------------------------------------
	f32 MorphArray[8] = { 0,0,0,0,0,0,0,0};
	if (nResetMode==0) 
	{ 
		MorphArray[0]=pShapeDeform[0];
		MorphArray[1]=pShapeDeform[1];
		MorphArray[2]=pShapeDeform[2];
		MorphArray[3]=pShapeDeform[3];
		MorphArray[4]=pShapeDeform[4];
		MorphArray[5]=pShapeDeform[5];
		MorphArray[6]=pShapeDeform[6];
		MorphArray[7]=pShapeDeform[7];
	}

	for (uint32 i=0; i<8; i++) 
	{
		if (MorphArray[i]<0.0f) 
		{
			VertexRegs[i].x	=	1-(MorphArray[i]+1);
			VertexRegs[i].y	=	MorphArray[i]+1;
			VertexRegs[i].z	=	0;
		} 
		else 
		{
			VertexRegs[i].x	=	0;
			VertexRegs[i].y	=	1-MorphArray[i];
			VertexRegs[i].z	=	MorphArray[i];
		}
	}

}


//////////////////////////////////////////////////////////////////////////
// skinning of external internal vertices and tangents 
//////////////////////////////////////////////////////////////////////////
void CModelMesh::SkinningExtSW ( CCharInstance* pMaster, CAttachment* pAttachment, int nLOD)
{

#ifdef DEFINE_PROFILER_FUNCTION
	DEFINE_PROFILER_FUNCTION();
#endif
	InitSkinningExtSW(&pMaster->m_Morphing,pMaster->m_arrShapeDeformValues,0,nLOD);

	QuatTS arrNewSkinQuat[MAX_JOINT_AMOUNT+1];	//bone quaternions for skinning (entire skeleton)
	uint32 iActiveFrame		=	pMaster->m_iActiveFrame;
	if(pAttachment)
	{
		uint32 numRemapJoints	= pAttachment->m_arrRemapTable.size();
		for (uint32 i=0; i<numRemapJoints; i++)	arrNewSkinQuat[i]=pMaster->m_arrNewSkinQuat[iActiveFrame][pAttachment->m_arrRemapTable[i]];
	}
	else
	{
		uint32 numJoints	= pMaster->m_SkeletonPose.m_AbsolutePose.size();
		cryMemcpy(arrNewSkinQuat, &pMaster->m_arrNewSkinQuat[iActiveFrame][0],	sizeof(QuatTS)*numJoints);
	}


	uint32 numExtVertices = m_pModel->GetRenderMesh(nLOD)->GetVertCount();//m_arrTransformCache.size();	assert(numExtVertices);
	uint32 numFacesTotal=0;

	if (m_arrExtSkinnedStream.size() != numExtVertices)
		m_arrExtSkinnedStream.resize(numExtVertices);


	LockFullRenderMesh(nLOD);

	if (m_arrTangents.empty())
		m_arrTangents.resize(numExtVertices);

	if (Console::GetInst().ca_SphericalSkinning) 
	{
		for (uint32 i=0; i<numExtVertices; ++i)
			VertexShader_SBS(arrNewSkinQuat, i);
	}
	else 
	{
		for (uint32 i = 0; i <numExtVertices; ++i)
			VertexShader_LBS(arrNewSkinQuat, i);
	}
	UnlockFullRenderMesh(nLOD);



}




//---------------------------------------------------------------------
//----          this part of code belongs into hardware            ----
//---    this is a simulation of the spherical-vertex-blending      ---
//---------------------------------------------------------------------
void CModelMesh::VertexShader_SBS(QuatTS* parrNewSkinQuat,uint32 i) 
{
	ExtSkinVertex vertex = GetSkinVertexNoInd(i);
	//create the final vertex for skinning (blend between 3 characters)
	uint8 idx		=	vertex.color.a; 
	Vec3 v	= vertex.wpos0*VertexRegs[idx].x + vertex.wpos1*VertexRegs[idx].y + vertex.wpos2*VertexRegs[idx].z;
	//apply morph-targets
	if (m_arrExtMorphStream.size())
		v+=m_arrExtMorphStream[i];

	//get indices for bones (always 4 indices per vertex)
	uint32 id0 = m_arrExtBoneIDs[i].idx0;
	uint32 id1 = m_arrExtBoneIDs[i].idx1;
	uint32 id2 = m_arrExtBoneIDs[i].idx2;
	uint32 id3 = m_arrExtBoneIDs[i].idx3;
	//get weights for vertices (always 4 weights per vertex)
	f32 w0 = vertex.weights[0]/255.0f;
	f32 w1 = vertex.weights[1]/255.0f;
	f32 w2 = vertex.weights[2]/255.0f;
	f32 w3 = vertex.weights[3]/255.0f;
	assert(fabsf((w0+w1+w2+w3)-1.0f)<0.0001f);

	const QuatD& q0=(const QuatD&)parrNewSkinQuat[id0].q;
	const QuatD& q1=(const QuatD&)parrNewSkinQuat[id1].q;
	const QuatD& q2=(const QuatD&)parrNewSkinQuat[id2].q;
	const QuatD& q3=(const QuatD&)parrNewSkinQuat[id3].q;
	QuatD wquat		=	(q0*w0 +  q1*w1 + q2*w2 +	q3*w3);

	f32 l=1.0f/wquat.nq.GetLength();
	wquat.nq*=l;
	wquat.dq*=l;

	Matrix34 m34	= Matrix34(wquat);  
	m_arrExtSkinnedStream[i].pos	= m34*v;

	Vec3 binormal	= Vec3( tPackB2F(vertex.binormal.x),tPackB2F(vertex.binormal.y),tPackB2F(vertex.binormal.z) );
	Vec3 b = m34.TransformVector(binormal);
	m_arrTangents[i].Binormal = Vec4sf(tPackF2B(b.x),tPackF2B(b.y),tPackF2B(b.z), vertex.binormal.w);

	Vec3 tangent	= Vec3( tPackB2F(vertex.tangent.x), tPackB2F(vertex.tangent.y), tPackB2F(vertex.tangent.z) );
	Vec3 t = m34.TransformVector(tangent);
	m_arrTangents[i].Tangent	= Vec4sf(tPackF2B(t.x),tPackF2B(t.y),tPackF2B(t.z), vertex.tangent.w);
}; 


//---------------------------------------------------------------------
//----            this part of code belongs into hardware          ----
//---   this is a simulation of the smoothe-skinning vertex-shader  ---
//---------------------------------------------------------------------
void CModelMesh::VertexShader_LBS(QuatTS* parrNewSkinQuat,uint32 i) 
{
	//--------------------------------------------------------
	//----   this part of code belongs into hardware      ----
	//---  this loop is a simulation of the vertex-shader  ---
	//--------------------------------------------------------

	ExtSkinVertex vertex = GetSkinVertexNoInd(i);
	//create the final vertex for skinning (blend between 3 characters)
	uint8 idx	=	vertex.color.a; 
	Vec3 v		= vertex.wpos0*VertexRegs[idx].x + vertex.wpos1*VertexRegs[idx].y + vertex.wpos2*VertexRegs[idx].z;
	//apply morph-targets
	if (m_arrExtMorphStream.size())
		v+=m_arrExtMorphStream[i];
	//get indices for bones (always 4 indices per vertex)
	uint32 id0 = m_arrExtBoneIDs[i].idx0;
	uint32 id1 = m_arrExtBoneIDs[i].idx1;
	uint32 id2 = m_arrExtBoneIDs[i].idx2;
	uint32 id3 = m_arrExtBoneIDs[i].idx3;

	f32 w0 = vertex.weights[0]/255.0f;
	f32 w1 = vertex.weights[1]/255.0f;
	f32 w2 = vertex.weights[2]/255.0f;
	f32 w3 = vertex.weights[3]/255.0f;

	const QuatTS& qt0=parrNewSkinQuat[id0];
	const QuatTS& qt1=parrNewSkinQuat[id1];
	const QuatTS& qt2=parrNewSkinQuat[id2];
	const QuatTS& qt3=parrNewSkinQuat[id3];
	Vec3 v0=qt0*v*w0;
	Vec3 v1=qt1*v*w1;
	Vec3 v2=qt2*v*w2;
	Vec3 v3=qt3*v*w3;

	m_arrExtSkinnedStream[i].pos = v0+v1+v2+v3;

	//do skinning & weighting for the binormal 
	Vec3 binormal	= Vec3( tPackB2F(vertex.binormal.x),tPackB2F(vertex.binormal.y),tPackB2F(vertex.binormal.z) );
	Vec3 b0=qt0.q*(binormal)*w0;
	Vec3 b1=qt1.q*(binormal)*w1;
	Vec3 b2=qt2.q*(binormal)*w2;
	Vec3 b3=qt3.q*(binormal)*w3;
	Vec3 b = b0+b1+b2+b3;
	m_arrTangents[i].Binormal = Vec4sf(tPackF2B(b.x),tPackF2B(b.y),tPackF2B(b.z), vertex.binormal.w);

	//do skinning & weighting for the tangent 
	Vec3 tangent	= Vec3( tPackB2F(vertex.tangent.x), tPackB2F(vertex.tangent.y), tPackB2F(vertex.tangent.z) );
	Vec3 t0=qt0.q*(tangent)*w0;
	Vec3 t1=qt1.q*(tangent)*w1;
	Vec3 t2=qt2.q*(tangent)*w2;
	Vec3 t3=qt3.q*(tangent)*w3;
	Vec3 t = t0+t1+t2+t3;
	m_arrTangents[i].Tangent	= Vec4sf(tPackF2B(t.x),tPackF2B(t.y),tPackF2B(t.z), vertex.tangent.w);
}
//#endif




//---------------------------------------------------------------------
//----            this part of code belongs into hardware          ----
//---   this is a simulation of the smoothe-skinning vertex-shader  ---
//---------------------------------------------------------------------
Vec3 CModelMesh::GetSkinnedExtVertex2( QuatTS* parrNewSkinQuat, uint32 i) 
{
	//--------------------------------------------------------
	//----   this part of code belongs into hardware      ----
	//---  this loop is a simulation of the vertex-shader  ---
	//--------------------------------------------------------
	//create the final vertex for skinning (blend between 3 characters)
	ExtSkinVertex vertex = GetSkinVertexNoInd(i);

	uint8 idx		=	vertex.color.a; 
	Vec3 v	= vertex.wpos0*VertexRegs[idx].x + vertex.wpos1*VertexRegs[idx].y + vertex.wpos2*VertexRegs[idx].z;
	//apply morph-targets
	if (m_arrExtMorphStream.size())
		v+=m_arrExtMorphStream[i];
	//get indices for bones (always 4 indices per vertex)
	uint32 id0 = m_arrExtBoneIDs[i].idx0;
	uint32 id1 = m_arrExtBoneIDs[i].idx1;
	uint32 id2 = m_arrExtBoneIDs[i].idx2;
	uint32 id3 = m_arrExtBoneIDs[i].idx3;
	//get weights for vertices (always 4 weights per vertex)
	f32 w0 = vertex.weights[0]/255.0f;
	f32 w1 = vertex.weights[1]/255.0f;
	f32 w2 = vertex.weights[2]/255.0f;
	f32 w3 = vertex.weights[3]/255.0f;
	assert(fabsf((w0+w1+w2+w3)-1.0f)<0.0001f);


	if (Console::GetInst().ca_SphericalSkinning) 
	{
		//do the dual quaternion skinning 
		const QuatD& q0=(const QuatD&)parrNewSkinQuat[id0].q;
		const QuatD& q1=(const QuatD&)parrNewSkinQuat[id1].q;
		const QuatD& q2=(const QuatD&)parrNewSkinQuat[id2].q;
		const QuatD& q3=(const QuatD&)parrNewSkinQuat[id3].q;
		QuatD wquat		=	(q0*w0 +  q1*w1 + q2*w2 +	q3*w3);
		f32 l=1.0f/wquat.nq.GetLength();
		wquat.nq*=l;
		wquat.dq*=l;
		return QuatT(wquat)*v;
	} 
	else 
	{
		//do the linear-blend-skinning 
		Vec3 v0=parrNewSkinQuat[id0]*v*w0;
		Vec3 v1=parrNewSkinQuat[id1]*v*w1;
		Vec3 v2=parrNewSkinQuat[id2]*v*w2;
		Vec3 v3=parrNewSkinQuat[id3]*v*w3;
		return v0+v1+v2+v3;
	}

}

float CModelMesh::ComputeExtent(GeomQuery& geo, EGeomForm eForm)
{
	if (eForm == GeomForm_Vertices)
		return (float)GetVertextCount();//(float)m_arrExtSkinnedStream.size();
	else
	{
		IRenderMesh* pMesh = m_pModel->GetRenderMesh(m_nLOD);
		if (!pMesh)
			return 0.f;
		int nIndices, posStride;
		unsigned short int* pIndices = pMesh->GetIndices(&nIndices);
		byte* pPositions = pMesh->GetStridedPosPtr(posStride, 0);

		assert(nIndices%3 == 0);
		int nTris = nIndices/3;
		geo.SetNumParts(nTris);
		for (int i = 0; i < nTris; i++)
		{
			Vec3 const& v0 = *(Vec3*)(pPositions + pIndices[i*3] * posStride);
			Vec3 const& v1 = *(Vec3*)(pPositions + pIndices[i*3+1] * posStride);
			Vec3 const& v2 = *(Vec3*)(pPositions + pIndices[i*3+2] * posStride);

			geo.SetPartExtent(i, TriExtent(eForm, v0, v1, v2));
		}

		pMesh->UnlockStream(0);
		return geo.GetExtent();
	}
}

void CModelMesh::GetRandomPos(CCharInstance* pMaster, RandomPos& ran, GeomQuery& geo, EGeomForm eForm)
{
	if (eForm == GeomForm_Vertices)
	{
		int nV = Random((int)GetVertextCount());
		GetSingleSkinnedVertex(pMaster, nV, &ran.vPos, &ran.vNorm, true);
	}
	else
	{
		if (geo.SetOwner(this, eForm))
			ComputeExtent(geo, eForm);
		int iFace = geo.GetRandomPart(this, eForm);

		int32 numFaces = m_numExtTriangles;

		if (iFace < 0 || iFace >= (int)m_numExtTriangles) {
			AnimWarning("model '%s'. GetRandomPos requested iFace %i, Total faces %i", m_pModel->GetModelFilePath(), iFace, m_numExtTriangles);
			iFace = Random(m_numExtTriangles);
		}


		//TFace const& face = m_arrExtFaces[iFace];

		RandomPos ranv[3];
		LockFullRenderMesh(m_pModel->m_nBaseLOD);
		GetSingleSkinnedVertex(pMaster, iFace*3+0, &ranv[0].vPos, &ranv[0].vNorm, false);
		GetSingleSkinnedVertex(pMaster, iFace*3+1, &ranv[1].vPos, &ranv[1].vNorm, false);
		GetSingleSkinnedVertex(pMaster, iFace*3+2, &ranv[2].vPos, &ranv[2].vNorm, false);
		UnlockFullRenderMesh(m_pModel->m_nBaseLOD);

		// Generate interpolators for verts. Volume gen not supported (use surface).
		float t[3];
		TriRandomPoint(t, eForm);

		ran.vPos = ranv[0].vPos * t[0] +
			ranv[1].vPos * t[1] +
			ranv[2].vPos * t[2];
		ran.vNorm = ranv[0].vNorm * t[0] +
			ranv[1].vNorm * t[1] +
			ranv[2].vNorm * t[2];
		ran.vNorm.Normalize();
	}
}


void CModelMesh::DrawWireframeStatic( CMorphing* pMorphing, const Matrix34& m34, int nLOD, uint32 color)
{
	if (nLOD < m_pModel->m_nBaseLOD)
		nLOD = m_pModel->m_nBaseLOD;

	f32 arrShapeDeformValues[8] = {0,0,0,0, 0,0,0,0};
	InitSkinningExtSW(pMorphing,arrShapeDeformValues,1,nLOD);

	uint32 numExtIndices	= m_pModel->GetRenderMesh(nLOD)->GetSysIndicesCount();
	uint32 numExtVertices	=	m_pModel->GetRenderMesh(nLOD)->GetVertCount();
	assert(numExtVertices);

	static std::vector<ColorB> arrExtVColors;
	uint32 csize = arrExtVColors.size();
	if (csize<numExtVertices) arrExtVColors.resize( numExtVertices );	

	static std::vector<Vec3> arrExtSkinnedStream;
	uint32 vsize = arrExtSkinnedStream.size();
	if (vsize<numExtVertices) arrExtSkinnedStream.resize( numExtVertices );	
	LockFullRenderMesh(nLOD); 
	for(uint32 e=0; e<numExtVertices; e++) 
	{	
		//create the final vertex for skinning (blend between 3 characters)
		ExtSkinVertex vertex = GetSkinVertexNoInd(e);
		arrExtVColors[e]=vertex.color;
		uint8 idx	=	vertex.color.a; 
		Vec3 v		= vertex.wpos0*VertexRegs[idx].x + vertex.wpos1*VertexRegs[idx].y + vertex.wpos2*VertexRegs[idx].z;
		if (m_arrExtMorphStream.size())
			v+=m_arrExtMorphStream[e];
		arrExtSkinnedStream[e] = m34*v;
	}
	UnlockFullRenderMesh(nLOD);

	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	renderFlags.SetFillMode( e_FillModeWireframe );
	renderFlags.SetDrawInFrontMode( e_DrawInFrontOn );
	renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
	g_pAuxGeom->SetRenderFlags( renderFlags );
	g_pAuxGeom->DrawTriangles(&arrExtSkinnedStream[0],numExtVertices, m_pIndices,numExtIndices,color);		
}



void CModelMesh::DrawWireframe ( CMorphing* pMorphing, QuatTS* parrNewSkinQuat, f32* pShapeDeform,uint32 nResetMode, int nLOD,const Matrix34& rRenderMat34 )
{

	InitSkinningExtSW(pMorphing,pShapeDeform,nResetMode,nLOD);

	uint32 numExtIndices	= m_pModel->GetRenderMesh(nLOD)->GetSysIndicesCount();
	uint32 numExtVertices	=	m_pModel->GetRenderMesh(nLOD)->GetVertCount();
	assert(numExtVertices);

	static std::vector<ColorB> arrExtVColors;
	uint32 csize = arrExtVColors.size();
	if (csize<numExtVertices) arrExtVColors.resize( numExtVertices );	


	static std::vector<Vec3> arrExtSkinnedStream;
	uint32 vsize = arrExtSkinnedStream.size();
	if (vsize<numExtVertices) arrExtSkinnedStream.resize( numExtVertices );	


	LockFullRenderMesh(nLOD);
	if (Console::GetInst().ca_SphericalSkinning) 
	{
		f32 fUniformScaling = parrNewSkinQuat[0].s;
		for(uint32 e=0; e<numExtVertices; e++) 
		{	
			//create the final vertex for skinning (blend between 3 characters)

			ExtSkinVertex vertex = GetSkinVertexNoInd(e);
			arrExtVColors[e]=vertex.color;
			uint8 idx	=	vertex.color.a; 
			assert(idx<8); 
			Vec3 v		= vertex.wpos0*VertexRegs[idx].x + vertex.wpos1*VertexRegs[idx].y + vertex.wpos2*VertexRegs[idx].z;
			//apply morph-targets
			if (m_arrExtMorphStream.size())
				v+=m_arrExtMorphStream[e];

			//get indices for bones (always 4 indices per vertex)
			uint32 id0 = m_arrExtBoneIDs[e].idx0;
			uint32 id1 = m_arrExtBoneIDs[e].idx1;
			uint32 id2 = m_arrExtBoneIDs[e].idx2;
			uint32 id3 = m_arrExtBoneIDs[e].idx3;

			f32 w0 = vertex.weights[0]/255.0f;
			f32 w1 = vertex.weights[1]/255.0f;
			f32 w2 = vertex.weights[2]/255.0f;
			f32 w3 = vertex.weights[3]/255.0f;

			const QuatD& q0=(const QuatD&)parrNewSkinQuat[id0].q;
			const QuatD& q1=(const QuatD&)parrNewSkinQuat[id1].q;
			const QuatD& q2=(const QuatD&)parrNewSkinQuat[id2].q;
			const QuatD& q3=(const QuatD&)parrNewSkinQuat[id3].q;
			QuatD wquat		=	(q0*w0 +  q1*w1 + q2*w2 +	q3*w3);

			f32 l=1.0f/wquat.nq.GetLength();
			wquat.nq*=l;
			wquat.dq*=l;

			Vec3 sv			= QuatT(wquat)*v;
			arrExtSkinnedStream[e] = rRenderMat34*sv;
		}
	}
	else
	{

		for(uint32 e=0; e<numExtVertices; e++) 
		{	
			//create the final vertex for skinning (blend between 3 characters) 

			ExtSkinVertex vertex = GetSkinVertexNoInd(e); 
			arrExtVColors[e]=vertex.color;
			uint8 idx	=	vertex.color.a; 
			assert(idx<8); 

			Vec3 v		= vertex.wpos0*VertexRegs[idx].x + vertex.wpos1*VertexRegs[idx].y + vertex.wpos2*VertexRegs[idx].z;

			//apply morph-targets
			if (m_arrExtMorphStream.size())
				v+=m_arrExtMorphStream[e]; 

	//		v*=fUniformScaling;

			//get indices for bones (always 4 indices per vertex) 
			uint32 id0 = m_arrExtBoneIDs[e].idx0;
			uint32 id1 = m_arrExtBoneIDs[e].idx1;
			uint32 id2 = m_arrExtBoneIDs[e].idx2;
			uint32 id3 = m_arrExtBoneIDs[e].idx3;


			f32 w0 = vertex.weights[0]/255.0f;
			f32 w1 = vertex.weights[1]/255.0f;
			f32 w2 = vertex.weights[2]/255.0f;
			f32 w3 = vertex.weights[3]/255.0f;

			const QuatTS& qt0=parrNewSkinQuat[id0];
			const QuatTS& qt1=parrNewSkinQuat[id1];
			const QuatTS& qt2=parrNewSkinQuat[id2];
			const QuatTS& qt3=parrNewSkinQuat[id3];
			Vec3 v0=qt0*v*w0;
			Vec3 v1=qt1*v*w1;
			Vec3 v2=qt2*v*w2;
			Vec3 v3=qt3*v*w3;
			Vec3 final = (v0+v1+v2+v3);
			arrExtSkinnedStream[e] = rRenderMat34*final;
		}
	}
	UnlockFullRenderMesh(nLOD);
	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	renderFlags.SetFillMode( e_FillModeWireframe );
	renderFlags.SetDrawInFrontMode( e_DrawInFrontOn );
	g_pAuxGeom->SetRenderFlags( renderFlags );
	g_pAuxGeom->DrawTriangles(&arrExtSkinnedStream[0],numExtVertices, m_pIndices,numExtIndices,&arrExtVColors[0]);		
}


#define VLENGTH (0.03f)
//--------------------------------------------------------------------
//---             draw the Tangents of a character                  ---
//--------------------------------------------------------------------
void CModelMesh::DrawTangents ( const Matrix34& rRenderMat34)
{

//	uint32 numExtIndices	= m_pModel->GetRenderMesh(m_pModel->m_nBaseLOD)->GetSysIndicesCount();
	uint32 numExtVertices	=	m_arrExtSkinnedStream.size();
	assert(numExtVertices);

	static std::vector<ColorB> arrExtVColors;
	uint32 csize = arrExtVColors.size();
	if (csize<(numExtVertices*2)) arrExtVColors.resize( numExtVertices*2 );	
	for(uint32 i=0; i<numExtVertices*2; i=i+2)	
	{
		arrExtVColors[i+0] = RGBA8(0x3f,0x00,0x00,0x00);
		arrExtVColors[i+1] = RGBA8(0xff,0x7f,0x7f,0x00);
	}

	Matrix34 WMat34 = rRenderMat34;
	Matrix33 WMat33 = Matrix33(rRenderMat34);
	static std::vector<Vec3> arrExtSkinnedStream;
	uint32 vsize = arrExtSkinnedStream.size();
	if (vsize<(numExtVertices*2)) arrExtSkinnedStream.resize( numExtVertices*2 );	
	for(uint32 i=0,t=0; i<numExtVertices; i++)	
	{
		Vec3 tangent  = Vec3( tPackB2F(m_arrTangents[i].Tangent.x), tPackB2F(m_arrTangents[i].Tangent.y), tPackB2F(m_arrTangents[i].Tangent.z) )*VLENGTH;
		arrExtSkinnedStream[t+0] = WMat34*m_arrExtSkinnedStream[i].pos; 
		arrExtSkinnedStream[t+1] = WMat33*tangent+arrExtSkinnedStream[t];
		t=t+2;
	}

	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	g_pAuxGeom->SetRenderFlags( renderFlags );
	g_pAuxGeom->DrawLines( &arrExtSkinnedStream[0],numExtVertices*2, &arrExtVColors[0]);		
}


//--------------------------------------------------------------------
//---             draw the Binormals of a character                ---
//--------------------------------------------------------------------
void CModelMesh::DrawBinormals( const Matrix34& rRenderMat34)
{
	//uint32 numExtIndices	= m_pModel->GetRenderMesh(m_pModel->m_nBaseLOD)->GetSysIndicesCount();
	uint32 numExtVertices	=	m_arrExtSkinnedStream.size();
	assert(numExtVertices);

	static std::vector<ColorB> arrExtVColors;
	uint32 csize = arrExtVColors.size();
	if (csize<(numExtVertices*2)) arrExtVColors.resize( numExtVertices*2 );	
	for(uint32 i=0; i<numExtVertices*2; i=i+2)	
	{
		arrExtVColors[i+0] = RGBA8(0x00,0x3f,0x00,0x00);
		arrExtVColors[i+1] = RGBA8(0x7f,0xff,0x7f,0x00);
	}

	Matrix34 WMat34 = rRenderMat34;
	Matrix33 WMat33 = Matrix33(rRenderMat34);
	static std::vector<Vec3> arrExtSkinnedStream;
	uint32 vsize = arrExtSkinnedStream.size();
	if (vsize<(numExtVertices*2)) arrExtSkinnedStream.resize( numExtVertices*2 );	
	for(uint32 i=0,t=0; i<numExtVertices; i++)	
	{
		Vec3 binormal = Vec3( tPackB2F(m_arrTangents[i].Binormal.x), tPackB2F(m_arrTangents[i].Binormal.y), tPackB2F(m_arrTangents[i].Binormal.z) )*VLENGTH;
		arrExtSkinnedStream[t+0] = WMat34*m_arrExtSkinnedStream[i].pos; 
		arrExtSkinnedStream[t+1] = WMat33*binormal+arrExtSkinnedStream[t];
		t=t+2;
	}

	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	g_pAuxGeom->SetRenderFlags( renderFlags );
	g_pAuxGeom->DrawLines( &arrExtSkinnedStream[0],numExtVertices*2, &arrExtVColors[0]);		
}

//--------------------------------------------------------------------
//---             draw the normals of a character                  ---
//--------------------------------------------------------------------
void CModelMesh::DrawNormals ( const Matrix34& rRenderMat34)
{
	uint32 numExtVertices	=	m_arrExtSkinnedStream.size();
	assert(numExtVertices);

	static std::vector<ColorB> arrExtVColors;
	uint32 csize = arrExtVColors.size();
	if (csize<(numExtVertices*2)) arrExtVColors.resize( numExtVertices*2 );	
	for(uint32 i=0; i<numExtVertices*2; i=i+2)	
	{
		arrExtVColors[i+0] = RGBA8(0x00,0x00,0x3f,0x00);
		arrExtVColors[i+1] = RGBA8(0x7f,0x7f,0xff,0x00);
	}

	Matrix34 WMat34 = rRenderMat34;
	Matrix33 WMat33 = Matrix33(rRenderMat34);
	static std::vector<Vec3> arrExtSkinnedStream;
	uint32 vsize = arrExtSkinnedStream.size();
	if (vsize<(numExtVertices*2)) arrExtSkinnedStream.resize( numExtVertices*2 );	
	for(uint32 i=0,t=0; i<numExtVertices; i++)	
	{
		f32  flip	=     tPackB2F(m_arrTangents[i].Binormal.w)*VLENGTH;
		Vec3 binormal	= Vec3( tPackB2F(m_arrTangents[i].Binormal.x),tPackB2F(m_arrTangents[i].Binormal.y),tPackB2F(m_arrTangents[i].Binormal.z) );
		Vec3 tangent  = Vec3( tPackB2F(m_arrTangents[i].Tangent.x), tPackB2F(m_arrTangents[i].Tangent.y), tPackB2F(m_arrTangents[i].Tangent.z) );
		Vec3 normal = (tangent%binormal)*flip;
		arrExtSkinnedStream[t+0] = WMat34*m_arrExtSkinnedStream[i].pos; 
		arrExtSkinnedStream[t+1] = WMat33*normal+arrExtSkinnedStream[t];
		t=t+2;
	}

	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	g_pAuxGeom->SetRenderFlags( renderFlags );
	g_pAuxGeom->DrawLines( &arrExtSkinnedStream[0],numExtVertices*2, &arrExtVColors[0]);		
}



//--------------------------------------------------------------------
//---              hex-dump of model                               ---
//--------------------------------------------------------------------
void CModelMesh::ExportModel()
{


	// If need this one call me. Alexey

	//--------------------------------------------------------------------------------
	//-- dump all vertices
	//--------------------------------------------------------------------------------
	//uint32 numExtVertices = m_arrExtVertices.size();
	//FILE* vstream = fopen( "c:\\VBuffer.txt", "w+b" );
	//for (uint32 v=0; v<numExtVertices; v++)
	//{
	//	int16 tx = m_arrExtVertices[v].tangent.x;
	//	int16 ty = m_arrExtVertices[v].tangent.y;
	//	int16 tz = m_arrExtVertices[v].tangent.z;
	//	int16 tw = m_arrExtVertices[v].tangent.w;

	//	int16 bx = m_arrExtVertices[v].binormal.x;
	//	int16 by = m_arrExtVertices[v].binormal.y;
	//	int16 bz = m_arrExtVertices[v].binormal.z;
	//	int16 bw = m_arrExtVertices[v].binormal.w;

	//	fprintf(vstream, " {%15.10ff,%15.10ff,%15.10ff,  ",m_arrExtVertices[v].wpos1.x,m_arrExtVertices[v].wpos1.y,m_arrExtVertices[v].wpos1.z);  

	//	if (tx<0)	fprintf(vstream, "-0x%04x,",-tx);
	//	else      fprintf(vstream, "+0x%04x,",+tx);
	//	if (ty<0)	fprintf(vstream, "-0x%04x,",-ty);
	//	else      fprintf(vstream, "+0x%04x,",+ty);
	//	if (tz<0)	fprintf(vstream, "-0x%04x,",-tz);
	//	else      fprintf(vstream, "+0x%04x,",+tz);
	//	if (tw<0)	fprintf(vstream, "-0x%04x,  ",-tw);
	//	else      fprintf(vstream, "+0x%04x,  ",+tw);

	//	if (bx<0)	fprintf(vstream, "-0x%04x,",-bx);
	//	else      fprintf(vstream, "+0x%04x,",+bx);
	//	if (by<0)	fprintf(vstream, "-0x%04x,",-by);
	//	else      fprintf(vstream, "+0x%04x,",+by);
	//	if (bz<0)	fprintf(vstream, "-0x%04x,",-bz);
	//	else      fprintf(vstream, "+0x%04x,",+bz);
	//	if (bw<0)	fprintf(vstream, "-0x%04x,  ",-bw);
	//	else      fprintf(vstream, "+0x%04x,  ",+bw);

	//	f32 fu = m_arrExtVertices[v].u;
	//	f32 fv = m_arrExtVertices[v].v;
	//	if (fu>+1) fu=+1;
	//	if (fu<-1) fu=-1;
	//	if (fv>+1) fv=+1;
	//	if (fv<-1) fv=-1;
	//	int16 iu = tPackF2B(fu);
	//	int16 iv = tPackF2B(fv);
	//	if (iu<0)	fprintf(vstream, "-0x%04x,",-iu);
	//	else      fprintf(vstream, "+0x%04x,",+iu);
	//	if (iv<0)	fprintf(vstream, "-0x%04x ",-iv);
	//	else      fprintf(vstream, "+0x%04x ",+iv);

	//	fprintf(vstream, " }, //%04x",	v); 
	//	fprintf(vstream, "\r\n");
	//}
	//fprintf(vstream, "\r\n\r\n");
	//fclose( vstream );

	////--------------------------------------------------------------------------------
	////--   dump all indices
	////--------------------------------------------------------------------------------
	//uint32 numExtFaces = m_arrExtFaces.size();
	//FILE* istream = fopen( "c:\\IBuffer.txt", "w+b" );
	//for (uint32 f=0; f<numExtFaces; f++)
	//{
	//	fprintf(istream, "0x%04x,0x%04x,0x%04x, //0x%08x",m_arrExtFaces[f].i0,m_arrExtFaces[f].i1,m_arrExtFaces[f].i2,f);  
	//	fprintf(vstream, "\r\n");
	//}
	//fprintf(istream, "\r\n\r\n");
	//fclose(istream);


	////--------------------------------------------------------------------------------
	////--   dump all subsets
	////--------------------------------------------------------------------------------
	//uint32 numSubsets = m_arrSubsets.size();
	//FILE* sstream = fopen( "c:\\Subsets.txt", "w+b" );
	//for (uint32 s=0; s<numSubsets; s++)
	//{
	//	fprintf(sstream, "0x%08x,0x%08x, 0x%08x,0x%08x,  0x%04x, //0x%08x",  m_arrSubsets[s].nFirstVertId,m_arrSubsets[s].nNumVerts, m_arrSubsets[s].nFirstIndexId,m_arrSubsets[s].nNumIndices/3,   m_arrSubsets[s].nMatID, s);  
	//	fprintf(vstream, "\r\n");
	//}
	//fprintf(sstream, "\r\n\r\n");
	//fclose(sstream);

}

size_t CModelMesh::SizeOfThis(ICrySizer *pSizer) const
{
	uint32 nSize = 0;//sizeof(CModelMesh);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "Phys Bone meshes");

		for(uint32 i = 0, end = m_arrPhyBoneMeshes.size(); i < end; ++i )
		{
			nSize += m_arrPhyBoneMeshes[i].m_arrPoints.capacity() * sizeof(Vec3);
			nSize += m_arrPhyBoneMeshes[i].m_arrIndices.capacity() * sizeof(uint16);
			nSize += m_arrPhyBoneMeshes[i].m_arrMaterials.capacity();
		}

		pSizer->AddObject(&m_arrPhyBoneMeshes, nSize);

	}

	{

		SIZER_SUBCOMPONENT_NAME(pSizer, "Morph targets + subsets + VSQL(S)");
		nSize /*+*/= m_morphTargets.capacity() * sizeof(_smart_ptr<CMorphTarget>);
		//for (uint32 i = 0, end = m_arrSubsets.size(); i < end; ++i )
		//{
		//	nSize += sizeof(SMeshSubset) + m_arrSubsets[i].m_arrGlobalBonesPerSubset.size() * sizeof(uint16);
		//}

		for (uint32 i = 0, end = m_morphTargets.size(); i < end; ++i)
		{
			if (m_morphTargets[i]) {
				nSize += m_morphTargets[i]->m_vertices.capacity() * sizeof(CMorphTarget::Vertex) + sizeof(CMorphTarget) + m_morphTargets[i]->m_name.capacity();
			}
		}
		//		nSize += sizeof(m_arrVSQuatL);
		//		nSize += sizeof(m_arrVSQuatS);
		//		nSize += sizeof(m_arrExtVertices);
		pSizer->AddObject(&m_morphTargets , nSize);
	}


	//{
	//	SIZER_SUBCOMPONENT_NAME(pSizer, "TransformChache");
	//	nSize = m_arrTransformCache.size();
	//	pSizer->AddObject(this, nSize);
	//}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "ExtMorphStream");
		nSize = m_arrExtMorphStream.capacity() * sizeof(Vec3);
		pSizer->AddObject(&m_arrExtMorphStream, nSize);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "m_arrIntFaces");
		nSize = m_arrIntFaces.capacity() * sizeof(TFace);
		pSizer->AddObject(&m_arrIntFaces, nSize);
	}
		
	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "m_arrExtVerticesCached");
		nSize = m_arrExtVerticesCached.size() * sizeof(AttSkinVertex);
		pSizer->AddObject(&m_arrExtVerticesCached, nSize);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "m_arrIntVertices");
		nSize = m_arrIntVertices.capacity() * sizeof(IntSkinVertex);
		pSizer->AddObject(&m_arrIntVertices, nSize);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "ExtSkinnedStream");
		nSize = m_arrExtSkinnedStream.capacity() * sizeof(RVertex);
		pSizer->AddObject(&m_arrExtSkinnedStream, nSize);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "ExtBoneIDs");
		nSize = m_arrExtBoneIDs.capacity() * sizeof(BoneID);
		pSizer->AddObject(&m_arrExtBoneIDs, nSize);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "Tangents");
		nSize = m_arrTangents.capacity() * sizeof(SPipTangents);
		pSizer->AddObject(&m_arrTangents, nSize);
	}


	//	pSizer->AddObject(this, nSize);
	//}

	return /*nSize*/0;

}



uint32 CModelMesh::GetVertextCount() const 
{
	if (m_pModel && m_pModel->m_pRenderMeshs[m_pModel->m_nBaseLOD])
	{
		IRenderMesh * pMesh = m_pModel->GetRenderMesh(m_pModel->m_nBaseLOD/*m_nLOD*/);
		return pMesh->GetVertCount();
	} 

	return 0;
}


void CModelMesh::DrawDebugInfo(CCharInstance* pInstance, int nLOD,const Matrix34& rRenderMat34, int DebugMode, IMaterial* & pMaterial,  CRenderObject *pObj, const SRendParams& RendParams)
{
	bool bNoText = DebugMode < 0;

	int nTris = m_pModel->GetRenderMesh(nLOD)->GetVertCount();
	int32 numLODs = m_pModel->m_arrModelMeshes.size();
	int index = 0;
	Vec3 trans = rRenderMat34.GetTranslation();
	trans += (m_pModel->m_arrCollisions[index].m_aABB.max - m_pModel->m_arrCollisions[index].m_aABB.min)/2.0f + m_pModel->m_arrCollisions[index].m_aABB.min;
	float color[4] = {1,1,1,1};

	IRenderMesh * pRenderMesh = m_pModel->GetRenderMesh(nLOD);
	int nMats = (pRenderMesh && pRenderMesh->GetChunks()) ? pRenderMesh->GetChunks()->Count() : 0;
	int nRenderMats=0;

	string shortName = PathUtil::GetFile(m_pModel->m_strFilePath);

	IRenderAuxGeom *pAuxGeom = g_pIRenderer->GetIRenderAuxGeom();

	if(nMats)
	{
		for(int i=0;i<nMats;++i)
		{	
			CRenderChunk &rc = pRenderMesh->GetChunks()->GetAt(i);
			if(rc.pRE && rc.nNumIndices && rc.nNumVerts && ((rc.m_nMatFlags&MTL_FLAG_NODRAW)==0))
				++nRenderMats;
		}
	}

	switch (DebugMode)
	{
	case 1:
		g_pIRenderer->DrawLabelEx( trans,1.3f, color,true,true,"%s\n%d LOD(%i\\%i)", shortName.c_str(), nTris, nLOD+1, numLODs);
		pAuxGeom->DrawAABB( m_pModel->m_ModelAABB,rRenderMat34,false,ColorB(0,255,255,128),eBBD_Faceted );
		break;
	case 2:
		{

			IMaterialManager* pMatMan = g_pI3DEngine->GetMaterialManager();
			int fMult = 1;
			//int nTris = m_pModel->GetRenderMesh(nLOD)->GetSysVertCount();
			ColorB clr = ColorB(255,255,255,255);
			if (nTris >= 20000*fMult)
				clr = ColorB(255,0,0,255);
			else if (nTris >= 10000*fMult)
				clr = ColorB(255,255,0,255);
			else if (nTris >= 5000*fMult)
				clr = ColorB(0,255,0,255);
			else if (nTris >= 2500*fMult)
				clr = ColorB(0,255,255,255);
			else if (nTris > 1250*fMult)
				clr = ColorB(0,0,255,255);
			else
				clr = ColorB(nTris/10,nTris/10,nTris/10,255);

			//if (pMaterial)
			//	pMaterial = pMatMan->GetDefaultHelperMaterial();
			//if (pObj)
				pObj->m_II.m_AmbColor = ColorF(clr.r/*/155.0f*/,clr.g/*/155.0f*/,clr.b/*/155.0f*/,1);

			if (!bNoText)
				g_pIRenderer->DrawLabelEx( trans,1.3f,color,true,true,"%d",nTris );

		}
	break;
	case 3:
		{
			//////////////////////////////////////////////////////////////////////////
			// Show Lods
			//////////////////////////////////////////////////////////////////////////
			ColorB clr;
			if (numLODs < 2)
			{
				if (nTris <= 30)
				{
					clr = ColorB(50,50,50,255);
				}
				else
				{
					clr = ColorB(255,0,0,255);
					clr.g = 127 + (uint8)(sin(g_pISystem->GetITimer()->GetCurrTime()*10.0f)*120);
				}
			}
			else
			{
				if (nLOD == 1)
					clr = ColorB(255,0,0,255);
				else if (nLOD == 2)
					clr = ColorB(0,255,0,255);
				else if (nLOD == 3)
					clr = ColorB(0,0,255,255);
				else if (nLOD == 4)
					clr = ColorB(0,255,255,255);
				else
					clr = ColorB(255,255,255,255);
			}

			//if (pMaterial)
			//	pMaterial = GetMatMan()->GetDefaultHelperMaterial();
			if (pObj)
			{
				pObj->m_II.m_AmbColor = ColorF(clr.r,clr.g,clr.b,1);
			}

			if (numLODs > 1 && !bNoText)
				g_pIRenderer->DrawLabelEx( trans,1.3f,color,true,true,"%d/%d",nLOD+1,numLODs );
		}
		break;

	case 4:
		if(pRenderMesh)
		{
			int nTexMemUsage = pRenderMesh->GetTextureMemoryUsage( pMaterial );
			g_pIRenderer->DrawLabelEx( trans,1.3f,color,true,true,"%d",nTexMemUsage/1024 );
		}

		break;

	case 5:
		{
			ColorB clr;
			if (nRenderMats == 1)
				clr = ColorB(0,0,255,255);
			else if (nRenderMats == 2)
				clr = ColorB(0,255,255,255);
			else if (nRenderMats == 3)
				clr = ColorB(0,255,0,255);
			else if (nRenderMats == 4)
				clr = ColorB(255,0,255,255);
			else if (nRenderMats == 5)
				clr = ColorB(255,255,0,255);
			else if (nRenderMats >= 6)
				clr = ColorB(255,0,0,255);
			else if (nRenderMats >= 11)
				clr = ColorB(255,255,255,255);

			pObj->m_II.m_AmbColor = ColorF(clr.r,clr.g,clr.b,1);

			if (!bNoText)
				g_pIRenderer->DrawLabelEx( trans,1.3f,color,true,true,"%d",nRenderMats );


		}

		break;

	case 6:
		{

			g_pIRenderer->DrawLabelEx( trans,1.3f,color,true,true,"%d,%d,%d,%d",
				(int)(RendParams.AmbientColor.r*255.0f),(int)(RendParams.AmbientColor.g*255.0f),(int)(RendParams.AmbientColor.b*255.0f),(int)(RendParams.AmbientColor.a*255.0f)
				);
		}
		break;

	case 7:
		if(pRenderMesh)
		{
			int nTexMemUsage = pRenderMesh->GetTextureMemoryUsage( pMaterial );
			g_pIRenderer->DrawLabelEx( trans,1.3f,color,true,true,"%d,%d,%d",nTris, nRenderMats,nTexMemUsage/1024 );
		}
		break;

	case 10:
		{
			//DrawWireframe(pInstance, nLOD, rRenderMat34);
		}
			//if (pRenderMesh)
			//	pRenderMesh->DebugDraw( *RendParams.pMatrix,0 );
		break;

		default:
			break;
	}

}
