/*=============================================================================
	GeomPrimitives.cpp :
	Copyright 2004 Crytek Studios. All Rights Reserved.

	Revision history:
	* Created by Nick Kasyan
=============================================================================*/

#include "stdafx.h"

#include "GeomPrimitives.h"
#include "IIndexedMesh.h"

//needs some inf constatns
#include "GeomCore.h"
#include "SceneContext.h"

CRLTriangle::CRLTriangle() 
{
	//FIX:: evaluate plane equation and major axis
	m_nMeshInfo = 0;
	m_nIsOpaque = 0;
	m_nIsDilated	=	0;
	//	m_nSessionID = ~0;
}

//===================================================================================
// Method					:	BaryInterpolate 
// Description		:	Interpolate position and normal given the Barycentric cooridnates.
//===================================================================================
void CRLTriangle::BaryInterpolate (f32 b1, f32 b2, f32 b3,
																	 Vec3& pos, Vec3& norm, const SMeshInformations *pMeshInfo)  const
{
	Vec3 vPos[3];
	Vec3 vNormal[3];

	GetNormals( vNormal, pMeshInfo );
	GetPositions( vPos, pMeshInfo );

	vNormal[0].NormalizeSafe();
	vNormal[1].NormalizeSafe();
	vNormal[2].NormalizeSafe();

	vPos[0] *= b1;
	vPos[1] *= b2;
	vPos[2] *= b3;

	vNormal[0] *= b1;
	vNormal[1] *= b2;
	vNormal[2] *= b3;

	pos = vPos[0];
	pos += vPos[1];
	pos += vPos[2];

	norm = vNormal[0];
	norm += vNormal[1];
	norm += vNormal[2];
}

Vec3	CRLTriangle::GetNormal( const int32 iVertId, const SMeshInformations *pMeshInfo ) const
{
	assert( iVertId >= 0 && iVertId < 3 );
	assert( pMeshInfo != NULL );
	int iNormalStride = 0;
	Vec3 *pNormals = NULL; //reinterpret_cast<Vec3 *> (pMeshInfo->m_pRenderMesh->GetNormalPtr(iNormalStride));
	assert( pNormals );
	Vec3 vTemp = pMeshInfo->m_mMatrix.TransformVector
		((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pNormals)[m_nInd[iVertId] * iNormalStride]))));

	if( _isnan(vTemp.x) )
		vTemp.x = FLT_MAX;

	if( _isnan(vTemp.y) )
		vTemp.y = FLT_MAX;

	if( _isnan(vTemp.z) )
		vTemp.z = FLT_MAX;

	FastKillDenormals(vTemp);

	return vTemp;
}

void	CRLTriangle::GetNormals( Vec3 *pNorm, const SMeshInformations *pMeshInfo ) const
{
	assert( pNorm );
	assert( pMeshInfo != NULL );
	int TanStride,BiTanStride;
	byte* pTangent = pMeshInfo->m_pRenderMesh->GetStridedTangentPtr(TanStride);
	byte* pBiTangent = pMeshInfo->m_pRenderMesh->GetStridedBinormalPtr(BiTanStride);
	Vec4sf& rTan0		=	*reinterpret_cast<Vec4sf*>(&pTangent[m_nInd[0]*TanStride]);
	Vec4sf& rTan1		=	*reinterpret_cast<Vec4sf*>(&pTangent[m_nInd[1]*TanStride]);
	Vec4sf& rTan2		=	*reinterpret_cast<Vec4sf*>(&pTangent[m_nInd[2]*TanStride]);
	Vec4sf& rBiTan0	=	*reinterpret_cast<Vec4sf*>(&pBiTangent[m_nInd[0]*BiTanStride]);
	Vec4sf& rBiTan1	=	*reinterpret_cast<Vec4sf*>(&pBiTangent[m_nInd[1]*BiTanStride]);
	Vec4sf& rBiTan2	=	*reinterpret_cast<Vec4sf*>(&pBiTangent[m_nInd[2]*BiTanStride]);


	Vec3	Tan0(tPackB2F(rTan0.x),tPackB2F(rTan0.y),tPackB2F(rTan0.z));
	Vec3	Tan1(tPackB2F(rTan1.x),tPackB2F(rTan1.y),tPackB2F(rTan1.z));
	Vec3	Tan2(tPackB2F(rTan2.x),tPackB2F(rTan2.y),tPackB2F(rTan2.z));
	Vec3	BiTan0(tPackB2F(rBiTan0.x),tPackB2F(rBiTan0.y),tPackB2F(rBiTan0.z));
	Vec3	BiTan1(tPackB2F(rBiTan1.x),tPackB2F(rBiTan1.y),tPackB2F(rBiTan1.z));
	Vec3	BiTan2(tPackB2F(rBiTan2.x),tPackB2F(rBiTan2.y),tPackB2F(rBiTan2.z));
	Vec3 Normal0	=	Tan0%BiTan0*tPackB2F(rTan0.w);
	Vec3 Normal1	=	Tan1%BiTan1*tPackB2F(rTan1.w);
	Vec3 Normal2	=	Tan2%BiTan2*tPackB2F(rTan2.w);

	pNorm[0] = pMeshInfo->m_mMatrix.TransformVector(Normal0);
	pNorm[1] = pMeshInfo->m_mMatrix.TransformVector(Normal1);
	pNorm[2] = pMeshInfo->m_mMatrix.TransformVector(Normal2);

/*
	assert( pNormals );
	pNorm[0] = pMeshInfo->m_mMatrix.TransformVector
		((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pNormals)[m_nInd[0] * iNormalStride]))));
	pNorm[1] = pMeshInfo->m_mMatrix.TransformVector
		((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pNormals)[m_nInd[1] * iNormalStride]))));
	pNorm[2] = pMeshInfo->m_mMatrix.TransformVector
		((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pNormals)[m_nInd[2] * iNormalStride]))));*/
	if( _isnan(pNorm[0].x) )
		pNorm[0].x = FLT_MAX;

	if( _isnan(pNorm[0].y) )
		pNorm[0].y = FLT_MAX;

	if( _isnan(pNorm[0].z) )
		pNorm[0].z = FLT_MAX;

	if( _isnan(pNorm[1].x) )
		pNorm[1].x = FLT_MAX;

	if( _isnan(pNorm[1].y) )
		pNorm[1].y = FLT_MAX;

	if( _isnan(pNorm[1].z) )
		pNorm[1].z = FLT_MAX;

	if( _isnan(pNorm[2].x) )
		pNorm[2].x = FLT_MAX;

	if( _isnan(pNorm[2].y) )
		pNorm[2].y = FLT_MAX;

	if( _isnan(pNorm[2].z) )
		pNorm[2].z = FLT_MAX;


	FastKillDenormals(pNorm[0]);
	FastKillDenormals(pNorm[1]);
	FastKillDenormals(pNorm[2]);
}

Vec3	CRLTriangle::GetTangent( const int32 iVertId, const SMeshInformations *pMeshInfo ) const
{
	assert( iVertId >= 0 && iVertId < 3 );
	assert( pMeshInfo != NULL );
	int iTangentStride = 0;
	Vec4sf *pTangents = reinterpret_cast<Vec4sf *> (pMeshInfo->m_pRenderMesh->GetStridedTangentPtr(iTangentStride));
	assert( pTangents );
	Vec4sf sfTangent = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pTangents)[m_nInd[iVertId] * iTangentStride]) );
	Vec3 vTemp = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfTangent.x),tPackB2F(sfTangent.y),tPackB2F(sfTangent.z)));

	if( _isnan(vTemp.x) )
		vTemp.x = FLT_MAX;

	if( _isnan(vTemp.y) )
		vTemp.y = FLT_MAX;

	if( _isnan(vTemp.z) )
		vTemp.z = FLT_MAX;

	return vTemp;
}

void	CRLTriangle::GetTangents( Vec3 *vTang, const SMeshInformations *pMeshInfo ) const
{
	assert( vTang );
	assert( pMeshInfo != NULL );
	int iTangentStride = 0;
	Vec4sf *pTangents = reinterpret_cast<Vec4sf *> (pMeshInfo->m_pRenderMesh->GetStridedTangentPtr(iTangentStride));
	assert( pTangents );
	Vec4sf sfTangent = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pTangents)[m_nInd[0] * iTangentStride]) );
	vTang[0] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfTangent.x),tPackB2F(sfTangent.y),tPackB2F(sfTangent.z)));
	sfTangent = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pTangents)[m_nInd[1] * iTangentStride]) );
	vTang[1] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfTangent.x),tPackB2F(sfTangent.y),tPackB2F(sfTangent.z)));
	sfTangent = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pTangents)[m_nInd[2] * iTangentStride]) );
	vTang[2] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfTangent.x),tPackB2F(sfTangent.y),tPackB2F(sfTangent.z)));

	if( _isnan(vTang[0].x) )
		vTang[0].x = FLT_MAX;

	if( _isnan(vTang[0].y) )
		vTang[0].y = FLT_MAX;

	if( _isnan(vTang[0].z) )
		vTang[0].z = FLT_MAX;

	if( _isnan(vTang[1].x) )
		vTang[1].x = FLT_MAX;

	if( _isnan(vTang[1].y) )
		vTang[1].y = FLT_MAX;

	if( _isnan(vTang[1].z) )
		vTang[1].z = FLT_MAX;

	if( _isnan(vTang[2].x) )
		vTang[2].x = FLT_MAX;

	if( _isnan(vTang[2].y) )
		vTang[2].y = FLT_MAX;

	if( _isnan(vTang[2].z) )
		vTang[2].z = FLT_MAX;

}

Vec3	CRLTriangle::GetBinormal( const int32 iVertId, const SMeshInformations *pMeshInfo ) const
{
	assert( iVertId >= 0 && iVertId < 3 );
	assert( pMeshInfo != NULL );
	int iBinormalStride = 0;
	Vec4sf *pBinormals = reinterpret_cast<Vec4sf *> (pMeshInfo->m_pRenderMesh->GetStridedBinormalPtr(iBinormalStride));
	assert( pBinormals );
	Vec4sf sfBinormal = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pBinormals)[m_nInd[iVertId] * iBinormalStride]) );
	Vec3 vTemp = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfBinormal.x),tPackB2F(sfBinormal.y),tPackB2F(sfBinormal.z)));

	if( _isnan(vTemp.x) )
		vTemp.x = FLT_MAX;

	if( _isnan(vTemp.y) )
		vTemp.y = FLT_MAX;

	if( _isnan(vTemp.z) )
		vTemp.z = FLT_MAX;

	return vTemp;
}

void	CRLTriangle::GetBinormals( Vec3 *pBiNorm, const SMeshInformations *pMeshInfo ) const
{
	assert( pMeshInfo != NULL );
	int iBinormalStride = 0;
	Vec4sf *pBinormals = reinterpret_cast<Vec4sf *> (pMeshInfo->m_pRenderMesh->GetStridedBinormalPtr(iBinormalStride));
	assert( pBinormals );
	Vec4sf sfBinormal = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pBinormals)[m_nInd[0] * iBinormalStride]) );
	pBiNorm[0] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfBinormal.x),tPackB2F(sfBinormal.y),tPackB2F(sfBinormal.z)));
	sfBinormal = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pBinormals)[m_nInd[1] * iBinormalStride]) );
	pBiNorm[1] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfBinormal.x),tPackB2F(sfBinormal.y),tPackB2F(sfBinormal.z)));
	sfBinormal = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pBinormals)[m_nInd[2] * iBinormalStride]) );
	pBiNorm[2] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfBinormal.x),tPackB2F(sfBinormal.y),tPackB2F(sfBinormal.z)));

	if( _isnan(pBiNorm[0].x) )
		pBiNorm[0].x = FLT_MAX;

	if( _isnan(pBiNorm[0].y) )
		pBiNorm[0].y = FLT_MAX;

	if( _isnan(pBiNorm[0].z) )
		pBiNorm[0].z = FLT_MAX;

	if( _isnan(pBiNorm[1].x) )
		pBiNorm[1].x = FLT_MAX;

	if( _isnan(pBiNorm[1].y) )
		pBiNorm[1].y = FLT_MAX;

	if( _isnan(pBiNorm[1].z) )
		pBiNorm[1].z = FLT_MAX;

	if( _isnan(pBiNorm[2].x) )
		pBiNorm[2].x = FLT_MAX;

	if( _isnan(pBiNorm[2].y) )
		pBiNorm[2].y = FLT_MAX;

	if( _isnan(pBiNorm[2].z) )
		pBiNorm[2].z = FLT_MAX;

}

Vec3	CRLTriangle::GetPosition( const int32 iVertId, const SMeshInformations *pMeshInfo ) const
{
	assert( iVertId >= 0 && iVertId < 3 );
	assert( pMeshInfo != NULL );
	int iVtxStride = 0;
	Vec3 *pVertices = reinterpret_cast<Vec3 *> (pMeshInfo->m_pRenderMesh->GetStridedPosPtr(iVtxStride));
	assert( pVertices );
	Vec3 vTemp = pMeshInfo->m_mMatrix.TransformPoint
		((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pVertices)[m_nInd[iVertId] * iVtxStride]))));

	if( _isnan(vTemp.x) )
	{
		vTemp.x = pMeshInfo->m_mMatrix.m03;
		if( _isnan(vTemp.x) )
			vTemp.x = FLT_MAX;
	}
	if( _isnan(vTemp.y) )
	{
		vTemp.y = pMeshInfo->m_mMatrix.m13;
		if( _isnan(vTemp.y) )
			vTemp.y = FLT_MAX;
	}
	if( _isnan(vTemp.z) )
	{
		vTemp.z = pMeshInfo->m_mMatrix.m23;
		if( _isnan(vTemp.z) )
			vTemp.z = FLT_MAX;
	}
	return vTemp;
}

const struct IRenderShaderResources* 	CRLTriangle::GetMaterial(const SMeshInformations *pMeshInfo) const
{
	assert( pMeshInfo != NULL );
	return pMeshInfo->m_pMaterial;
}


//===================================================================================
// Method					:	BaryInterpolateNormal
// Description		:	Interpolate normal given the Barycentric cooridnates.
//===================================================================================
void CRLTriangle::BaryInterpolateNormal (f32 b1, f32 b2, f32 b3,
																				 Vec3& norm, const SMeshInformations *pMeshInfo)  const
{
	Vec3 vNormal[3];

	GetNormals( vNormal, pMeshInfo );

	vNormal[0].NormalizeSafe();
	vNormal[1].NormalizeSafe();
	vNormal[2].NormalizeSafe();

	vNormal[0] *= b1;
	vNormal[1] *= b2;
	vNormal[2] *= b3;

	norm = vNormal[0];
	norm += vNormal[1];
	norm += vNormal[2];
}

//===================================================================================
// Method					:	BaryInterpolate 
// Description		:	Interpolate binormal and tangent vectors given the Barycentric cooridnates.
//===================================================================================
void CRLTriangle::BaryInterpolateTangent (f32 b1, f32 b2, f32 b3,
																					Vec3& binormal, Vec3& tangent, const SMeshInformations *pMeshInfo) const
{
	Vec3 vBinormals[3];
	Vec3 vTangents[3];

	GetBinormals( vBinormals, pMeshInfo );

	GetTangents( vTangents, pMeshInfo );

	vBinormals[0] *= b1;
	vBinormals[1] *= b2;
	vBinormals[2] *= b3;

	vTangents[0] *= b1;
	vTangents[1] *= b2;
	vTangents[2] *= b3;

	binormal = vBinormals[0];
	binormal += vBinormals[1];
	binormal += vBinormals[2];

	tangent  = vTangents[0];
	tangent  += vTangents[1];
	tangent  += vTangents[2];

	binormal.NormalizeSafe();
	tangent.NormalizeSafe();
}


////////////////////////////////////////////////////////////////////////////////////
// Dedicated octree triangle
////////////////////////////////////////////////////////////////////////////////////

CRLTriangleSmall::CRLTriangleSmall() 
{
	m_nMeshInfo = 0;
}

// Method				:	Bound
void	CRLTriangleSmall::Bound(AABB& bBox) const 
{	

	bBox.Add( GetPosition( 0 ) );
	bBox.Add( GetPosition( 1 ) );
	bBox.Add( GetPosition( 2 ) );
}



//===================================================================================
// Method				:	TriBoxIntersect
// Description	:	Check if the triangle intersects a given box
//===================================================================================
#define TBM_ZERO 0.0f

#define X 0
#define Y 1
#define Z 2

#define CROSS(dest,v1,v2) \
          dest[0]=v1[1]*v2[2]-v1[2]*v2[1]; \
          dest[1]=v1[2]*v2[0]-v1[0]*v2[2]; \
          dest[2]=v1[0]*v2[1]-v1[1]*v2[0];

#define DOT(v1,v2) (v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2])

#define SUB(dest,v1,v2) \
          dest[0]=v1[0]-v2[0]; \
          dest[1]=v1[1]-v2[1]; \
          dest[2]=v1[2]-v2[2];

#define ADD(dest,v1,v2) \
	dest[0]=v1[0]+v2[0]; \
	dest[1]=v1[1]+v2[1]; \
	dest[2]=v1[2]+v2[2];

#define FINDMINMAX(x0,x1,x2,min,max) \
  min = max = x0;   \
  if(x1<min) min=x1;\
  if(x1>max) max=x1;\
  if(x2<min) min=x2;\
  if(x2>max) max=x2;

//===================================================================================
bool CRLTriangleSmall::PlaneBoxOverlap (Vec3& normal, f32 d, const Vec3& maxbox) const
{
   f32 vmin[3];
   f32 vmax[3];
   for (int32 q = X; q <= Z; q++)
   {
      if (normal[q] > TBM_ZERO)
      {
         vmin[q] = -maxbox[q];
         vmax[q] = maxbox[q];
      }
      else
      {
         vmin[q] = maxbox[q];
         vmax[q] = -maxbox[q];
      }
   }
   if (DOT (normal, vmin) + d > TBM_ZERO)
   {
      return false;
   }
   if (DOT (normal, vmax) + d >= TBM_ZERO)
   {
      return true;
   }
   return false;
}

// ======================== X-tests ========================
#define AXISTEST_X01(a, b, fa, fb)             \
    p0 = a*v0[Y] - b*v0[Z];                    \
    p2 = a*v2[Y] - b*v2[Z];                    \
        if(p0<p2) {min=p0; max=p2;} else {min=p2; max=p0;} \
    rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];   \
    if(min>rad || max<-rad) return false;

#define AXISTEST_X2(a, b, fa, fb)              \
    p0 = a*v0[Y] - b*v0[Z];                    \
    p1 = a*v1[Y] - b*v1[Z];                    \
        if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
    rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];   \
    if(min>rad || max<-rad) return false;

// ======================== Y-tests ========================
#define AXISTEST_Y02(a, b, fa, fb)             \
    p0 = -a*v0[X] + b*v0[Z];                   \
    p2 = -a*v2[X] + b*v2[Z];                       \
        if(p0<p2) {min=p0; max=p2;} else {min=p2; max=p0;} \
    rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];   \
    if(min>rad || max<-rad) return false;

#define AXISTEST_Y1(a, b, fa, fb)              \
    p0 = -a*v0[X] + b*v0[Z];                   \
    p1 = -a*v1[X] + b*v1[Z];                       \
        if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
    rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];   \
    if(min>rad || max<-rad) return false;

// ======================== Z-tests ========================

#define AXISTEST_Z12(a, b, fa, fb)             \
    p1 = a*v1[X] - b*v1[Y];                    \
    p2 = a*v2[X] - b*v2[Y];                    \
        if(p2<p1) {min=p2; max=p1;} else {min=p1; max=p2;} \
    rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y];   \
    if(min>rad || max<-rad) return false;

#define AXISTEST_Z0(a, b, fa, fb)              \
    p0 = a*v0[X] - b*v0[Y];                \
    p1 = a*v1[X] - b*v1[Y];                    \
        if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
    rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y];   \
    if(min>rad || max<-rad) return false;

//fix: convert to Vec3 routines
//===================================================================================
bool CRLTriangleSmall::TriBoxIntersect (	const Vec3& vBoxCenter, const Vec3& boxhalfsize, Vec3* vPos ) const
{
	// use separating axis theorem to test overlap between triangle and box
	// need to test for overlap in these directions:
	// 1) the {x,y,z}-directions (actually, since we use the AABB of the triangle
	//    we do not even need to test these)
	// 2) normal of the triangle
	// 3) crossproduct(edge from tri, {x,y,z}-directin)
	//    this gives 3x3=9 more tests
	f32 v0[3], v1[3], v2[3];
	f32 min, max, d, p0, p1, p2, rad, fex, fey, fez;
	f32 e0[3], e1[3], e2[3];

	Vec3 normal;

	// This is the fastest branch on Sun
	// move everything so that the boxcenter is in (0,0,0)
	SUB (v0, vPos[0], vBoxCenter);
	SUB (v1, vPos[1], vBoxCenter);
	SUB (v2, vPos[2], vBoxCenter);

	// compute triangle edges
	SUB (e0, v1, v0);      // tri edge 0
	SUB (e1, v2, v1);      // tri edge 1
	SUB (e2, v0, v2);      // tri edge 2

	// Bullet 3:
	//  test the 9 tests first (this was faster)
	fex = (f32)fabs (e0[X]);
	fey = (f32)fabs (e0[Y]);
	fez = (f32)fabs (e0[Z]);
	AXISTEST_X01 (e0[Z], e0[Y], fez, fey);
	AXISTEST_Y02 (e0[Z], e0[X], fez, fex);
	AXISTEST_Z12 (e0[Y], e0[X], fey, fex);

	fex = (f32)fabs (e1[X]);
	fey = (f32)fabs (e1[Y]);
	fez = (f32)fabs (e1[Z]);
	AXISTEST_X01 (e1[Z], e1[Y], fez, fey);
	AXISTEST_Y02 (e1[Z], e1[X], fez, fex);
	AXISTEST_Z0 (e1[Y], e1[X], fey, fex);

	fex = (f32)fabs (e2[X]);
	fey = (f32)fabs (e2[Y]);
	fez = (f32)fabs (e2[Z]);
	AXISTEST_X2 (e2[Z], e2[Y], fez, fey);
	AXISTEST_Y1 (e2[Z], e2[X], fez, fex);
	AXISTEST_Z12 (e2[Y], e2[X], fey, fex);

	// Bullet 1:
	//  first test overlap in the {x,y,z}-directions
	//  find min, max of the triangle each direction, and test for overlap in
	//  that direction -- this is equivalent to testing a minimal AABB around
	//  the triangle against the AABB

	// test in X-direction
	FINDMINMAX (v0[X], v1[X], v2[X], min, max);
	if (min > boxhalfsize[X] || max < -boxhalfsize[X])
	{
		return false;
	}

	// test in Y-direction
	FINDMINMAX (v0[Y], v1[Y], v2[Y], min, max);
	if (min > boxhalfsize[Y] || max < -boxhalfsize[Y])
	{
		return false;
	}

	// test in Z-direction
	FINDMINMAX (v0[Z], v1[Z], v2[Z], min, max);
	if (min > boxhalfsize[Z] || max < -boxhalfsize[Z])
	{
		return false;
	}

	// Bullet 2:
	//  test if the box intersects the plane of the triangle
	//  compute plane equation of triangle: normal*x+d=0
	CROSS (normal, e0, e1);
	d = -DOT (normal, v0);  // plane eq: normal.x+d=0

	return 
		PlaneBoxOverlap (normal, d, boxhalfsize);
}


//===================================================================================
// Method				:	TriBoxIntersect
// Description	: Do the triangle/box intersection test
bool CRLTriangleSmall::TriBoxIntersect (	const AABB& bBox ) const
{
	Vec3	vBoxCenter,vBoxHalfsize;
	Vec3	vPos[3];

	GetPositions( vPos );

	vBoxCenter = bBox.max + bBox.min;
	vBoxHalfsize = bBox.max - bBox.min;

	vBoxCenter *= 0.5;
	vBoxHalfsize *= 0.5;

	return
		TriBoxIntersect (vBoxCenter, vBoxHalfsize, vPos);
}

//===================================================================================
// Method					:	BaryInterpolate 
// Description		:	Interpolate position and normal given the Barycentric cooridnates.
//===================================================================================
void CRLTriangleSmall::BaryInterpolate (f32 b1, f32 b2, f32 b3,
																	 Vec3& pos, Vec3& norm)  const
{
	Vec3 vPos[3];
	Vec3 vNormal[3];

	GetNormals( vNormal );
	GetPositions( vPos );

	vNormal[0].NormalizeSafe();
	vNormal[1].NormalizeSafe();
	vNormal[2].NormalizeSafe();

	vPos[0] *= b1;
	vPos[1] *= b2;
	vPos[2] *= b3;

	vNormal[0] *= b1;
	vNormal[1] *= b2;
	vNormal[2] *= b3;

	pos = vPos[0];
	pos += vPos[1];
	pos += vPos[2];

	norm = vNormal[0];
	norm += vNormal[1];
	norm += vNormal[2];
}


//===================================================================================
// Method				:	Intersect
// Description	: Do the triangle/ray intersection test here
// Note					:	CRay::fT should be set to Infinity for first intersection's handling
bool	CRLTriangleSmall::Intersect(CRay* pRay)	
{	
	f32 dT, dU, dV;
	if( false == TriIntersect(pRay->vFrom, pRay->vDir, &dT, &dU, &dV, pRay->fT, pRay->fTmin) )
		return false;

	//assign triangle material
	pRay->pMaterial = (struct IRenderShaderResources*)GetMaterial();

	pRay->fT = static_cast<float> (dT);
	pRay->fU = static_cast<float> (dU);
	pRay->fV = static_cast<float> (dV);

	// Figure out new normal and position
	f32 b0 = 1.0 - dU - dV;

	//fix:: optimize: use this instead barycentric interpolation
	//Pl =  (cRay.vDir * cRay.t) + cRay.vFrom;
	BaryInterpolate (b0, dU, dV, pRay->vP, pRay->vN);

	//pRay->object	=	this; //fix: coherency check

	return true;
}

//===================================================================================
// Method				:	IntersectClosest
// Description	: Do the triangle/ray intersection test here
// Note					:	CRay::fT should be set to Infinity for first intersection's handling
//===================================================================================
bool CRLTriangleSmall::IntersectClosest(CRay* pRay)
{	
	f32 dT, dU, dV;
	if( false == TriIntersect(pRay->vFrom, pRay->vDir, &dT, &dU, &dV, pRay->fT, pRay->fTmin) )
		return false;


	//fix: implement better closest intersection's strategy 
	if (	(fabs(dT) < CGeomCore::m_dInf) &&
				(fabs(dT) < fabs(pRay->fT)) &&
				(fabs(dT) > fabs(pRay->fTmin)) )
	{

		pRay->fT = static_cast<float> (dT);
		pRay->fU = static_cast<float> (dU);
		pRay->fV = static_cast<float> (dV);

		//assign triangle material
		pRay->pMaterial = (struct IRenderShaderResources*)GetMaterial();

		// Figure out new normal and position
		f32 b0 = 1.0 - dU - dV;

		//fix:: optimize: use this instead barycentric interpolation
		//Pl =  (cRay.vDir * cRay.t) + cRay.vFrom;
		BaryInterpolate (b0, dU, dV, pRay->vP, pRay->vN);

		//pRay->object	=	this; //fix: coherency check

		return true;
	}

	return false;
}

//===================================================================================
// Method				:	IntersectFrontClosest
// Description	: Do the triangle/ray intersection test here
// Note					:	CRay::fT should be set to Infinity for first intersection's handling
//===================================================================================
bool CRLTriangleSmall::IntersectFrontClosest(CRay* pRay) const
{	
	f32 dT, dU, dV;
	if( false == TriIntersect(pRay->vFrom, pRay->vDir, &dT, &dU, &dV, pRay->fT, pRay->fTmin) )
		return false;


	//fix: implement better closest intersection's strategy 
  if (	(dT >= -CGeomCore::m_dEps) &&
				(fabs(dT) < CGeomCore::m_dInf) &&
				(fabs(dT) < fabs(pRay->fT)) &&
				(fabs(dT) > fabs(pRay->fTmin)) )
	{

		pRay->fT = static_cast<float> (dT);
		pRay->fU = static_cast<float> (dU);
		pRay->fV = static_cast<float> (dV);

		//assign triangle material
		pRay->pMaterial = (struct IRenderShaderResources*)GetMaterial();

		// Figure out new normal and position
		f32 b0 = 1.0 - dU - dV;

		//fix:: optimize: use this instead barycentric interpolation
		//Pl =  (cRay.vDir * cRay.t) + cRay.vFrom;
		BaryInterpolate (b0, dU, dV, pRay->vP, pRay->vN);

		//pRay->object	=	this; //fix: coherency check

		return true;
	}

	return false;
}



//===================================================================================
// Method					:	TriIntersect
// Description		:	Main Triangle intersect routine
// Fix: evaluate performance without defines
//===================================================================================
bool CRLTriangleSmall::TriIntersect(Vec3& orig, Vec3& dir,
														 f32 *t, f32 *u, f32 *v, f32 fTmax, f32 fTmin	) const
{
	f32 edge1[3],edge2[3],facenormal[3];
	Vec3 vPos[3];

	GetPositions( vPos );
	SUB(edge1, vPos[1], vPos[0]);
	SUB(edge2, vPos[2], vPos[0]);

	CROSS(facenormal, edge1, edge2);
	f32 fLen = DOT( facenormal, facenormal );
	if( fLen > 10e-6 )
	{
		facenormal[0] /= fLen;
		facenormal[1] /= fLen;
		facenormal[2] /= fLen;
	}

	f32 dR1 = DOT( dir, facenormal );
	f32 dR0 = DOT( orig, facenormal ) - DOT( vPos[0], facenormal );

	if( fabsf( dR1 ) < 10e-5 )
		return false;

	//same side of the triangle - don't need to do anything with it... (have same sign...)
	if( (dR0+dR1*(0.05f+fTmin))*(dR0+dR1*(fTmax-0.05f)) >= 0 )
		return false;

	f32 v0[3],v1[3],v2[3],temp[3];

	SUB( v0, vPos[0], orig );
	ADD( v1, v0, edge1 );
	ADD( v2, v0, edge2 );

	CROSS( temp, v0,v1 );
	f32 fDot0 = DOT( temp, dir );
	CROSS( temp, v1,v2 );
	f32 fDot1 = DOT( temp, dir );
	CROSS( temp, v2,v0 );
	f32 fDot2 = DOT( temp, dir );


	int nSign0 = fDot0 < 0.f ? -1 : (fDot0 > 0.f ? 1 : 0);
	int nSign1 = fDot1 < 0.f ? -1 : (fDot1 > 0.f ? 1 : 0);
	int nSign2 = fDot2 < 0.f ? -1 : (fDot2 > 0.f ? 1 : 0);

	if((0 == nSign0 && 0 == nSign1) ||
		 (0 == nSign1 && 0 == nSign2) ||
		 (0 == nSign2 && 0 == nSign0) ||
		 (0 == nSign0 && nSign1 == nSign2) ||
		 (0 == nSign1 && nSign0 == nSign2) ||
		 (0 == nSign2 && nSign1 == nSign0) ||
		 (nSign0 == nSign1 && nSign1 == nSign2))
	{
			//the intersection point - distance from the plane
			*t = -dR0 / dR1;

			f32 pvec[3], tvec[3], qvec[3];

			CROSS(pvec, dir, edge2);
			SUB(tvec, orig, vPos[0]);
			CROSS(qvec, tvec, edge1);

			*u = DOT(tvec, pvec);
			*v = DOT(dir, qvec);
			return true;
	}

	return false;
}


Vec3	CRLTriangleSmall::GetPosition( const int32 iVertId ) const
{
	assert( iVertId >= 0 && iVertId < 3 );
	CGeomCore *pGeomCore = CSceneContext::GetInstance().GetGeomCore();
	assert( pGeomCore != NULL );
	const SMeshInformations *pMeshInfo = pGeomCore->GetMeshInfo( m_nMeshInfo );
	assert( pMeshInfo != NULL );
	int iVtxStride = 0;
	Vec3 *pVertices = reinterpret_cast<Vec3 *> (pMeshInfo->m_pRenderMesh->GetStridedPosPtr(iVtxStride));
	assert( pVertices );
	Vec3 vTemp = pMeshInfo->m_mMatrix.TransformPoint
		((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pVertices)[m_nInd[iVertId] * iVtxStride]))));

	if( _isnan(vTemp.x) )
	{
		vTemp.x = pMeshInfo->m_mMatrix.m03;
		if( _isnan(vTemp.x) )
			vTemp.x = FLT_MAX;
	}
	if( _isnan(vTemp.y) )
	{
		vTemp.y = pMeshInfo->m_mMatrix.m13;
		if( _isnan(vTemp.y) )
			vTemp.y = FLT_MAX;
	}
	if( _isnan(vTemp.z) )
	{
		vTemp.z = pMeshInfo->m_mMatrix.m23;
		if( _isnan(vTemp.z) )
			vTemp.z = FLT_MAX;
	}
	return vTemp;
}

Vec3	CRLTriangleSmall::GetNormal( const int32 iVertId ) const
{
	assert( iVertId >= 0 && iVertId < 3 );
	CGeomCore *pGeomCore = CSceneContext::GetInstance().GetGeomCore();
	assert( pGeomCore != NULL );
	const SMeshInformations *pMeshInfo = pGeomCore->GetMeshInfo( m_nMeshInfo );
	assert( pMeshInfo != NULL );
	int iNormalStride = 0;
	Vec3 *pNormals = NULL; //reinterpret_cast<Vec3 *> (pMeshInfo->m_pRenderMesh->GetNormalPtr(iNormalStride));
	assert( pNormals );
	Vec3 vTemp = pMeshInfo->m_mMatrix.TransformVector
		((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pNormals)[m_nInd[iVertId] * iNormalStride]))));

	if( _isnan(vTemp.x) )
		vTemp.x = FLT_MAX;

	if( _isnan(vTemp.y) )
		vTemp.y = FLT_MAX;

	if( _isnan(vTemp.z) )
		vTemp.z = FLT_MAX;

	FastKillDenormals(vTemp);

	return vTemp;
}

void	CRLTriangleSmall::GetNormals( Vec3 *pNorm ) const
{
	assert( pNorm );
	CGeomCore *pGeomCore = CSceneContext::GetInstance().GetGeomCore();
	assert( pGeomCore != NULL );
	const SMeshInformations *pMeshInfo = pGeomCore->GetMeshInfo( m_nMeshInfo );
	assert( pMeshInfo != NULL );
	int iNormalStride = 0;
	Vec3 *pNormals = NULL; //reinterpret_cast<Vec3 *> (pMeshInfo->m_pRenderMesh->GetNormalPtr(iNormalStride));
	assert( pNormals );
	pNorm[0] = pMeshInfo->m_mMatrix.TransformVector
		((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pNormals)[m_nInd[0] * iNormalStride]))));
	pNorm[1] = pMeshInfo->m_mMatrix.TransformVector
		((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pNormals)[m_nInd[1] * iNormalStride]))));
	pNorm[2] = pMeshInfo->m_mMatrix.TransformVector
		((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pNormals)[m_nInd[2] * iNormalStride]))));

	if( _isnan(pNorm[0].x) )
			pNorm[0].x = FLT_MAX;

	if( _isnan(pNorm[0].y) )
			pNorm[0].y = FLT_MAX;

	if( _isnan(pNorm[0].z) )
			pNorm[0].z = FLT_MAX;

	if( _isnan(pNorm[1].x) )
			pNorm[1].x = FLT_MAX;

	if( _isnan(pNorm[1].y) )
			pNorm[1].y = FLT_MAX;

	if( _isnan(pNorm[1].z) )
			pNorm[1].z = FLT_MAX;

	if( _isnan(pNorm[2].x) )
			pNorm[2].x = FLT_MAX;

	if( _isnan(pNorm[2].y) )
			pNorm[2].y = FLT_MAX;

	if( _isnan(pNorm[2].z) )
			pNorm[2].z = FLT_MAX;


	FastKillDenormals(pNorm[0]);
	FastKillDenormals(pNorm[1]);
	FastKillDenormals(pNorm[2]);
}

Vec3	CRLTriangleSmall::GetTangent( const int32 iVertId ) const
{
	assert( iVertId >= 0 && iVertId < 3 );
	CGeomCore *pGeomCore = CSceneContext::GetInstance().GetGeomCore();
	assert( pGeomCore != NULL );
	const SMeshInformations *pMeshInfo = pGeomCore->GetMeshInfo( m_nMeshInfo );
	assert( pMeshInfo != NULL );
	int iTangentStride = 0;
	Vec4sf *pTangents = reinterpret_cast<Vec4sf *> (pMeshInfo->m_pRenderMesh->GetStridedTangentPtr(iTangentStride));
	assert( pTangents );
	Vec4sf sfTangent = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pTangents)[m_nInd[iVertId] * iTangentStride]) );
	Vec3 vTemp = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfTangent.x),tPackB2F(sfTangent.y),tPackB2F(sfTangent.z)));

	if( _isnan(vTemp.x) )
			vTemp.x = FLT_MAX;

	if( _isnan(vTemp.y) )
			vTemp.y = FLT_MAX;

	if( _isnan(vTemp.z) )
			vTemp.z = FLT_MAX;

	return vTemp;
}

void	CRLTriangleSmall::GetTangents( Vec3 *vTang ) const
{
	assert( vTang );
	CGeomCore *pGeomCore = CSceneContext::GetInstance().GetGeomCore();
	assert( pGeomCore != NULL );
	const SMeshInformations *pMeshInfo = pGeomCore->GetMeshInfo( m_nMeshInfo );
	assert( pMeshInfo != NULL );
	int iTangentStride = 0;
	Vec4sf *pTangents = reinterpret_cast<Vec4sf *> (pMeshInfo->m_pRenderMesh->GetStridedTangentPtr(iTangentStride));
	assert( pTangents );
	Vec4sf sfTangent = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pTangents)[m_nInd[0] * iTangentStride]) );
	vTang[0] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfTangent.x),tPackB2F(sfTangent.y),tPackB2F(sfTangent.z)));
	sfTangent = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pTangents)[m_nInd[1] * iTangentStride]) );
	vTang[1] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfTangent.x),tPackB2F(sfTangent.y),tPackB2F(sfTangent.z)));
	sfTangent = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pTangents)[m_nInd[2] * iTangentStride]) );
	vTang[2] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfTangent.x),tPackB2F(sfTangent.y),tPackB2F(sfTangent.z)));

	if( _isnan(vTang[0].x) )
			vTang[0].x = FLT_MAX;

	if( _isnan(vTang[0].y) )
			vTang[0].y = FLT_MAX;

	if( _isnan(vTang[0].z) )
			vTang[0].z = FLT_MAX;

	if( _isnan(vTang[1].x) )
			vTang[1].x = FLT_MAX;

	if( _isnan(vTang[1].y) )
			vTang[1].y = FLT_MAX;

	if( _isnan(vTang[1].z) )
			vTang[1].z = FLT_MAX;

	if( _isnan(vTang[2].x) )
			vTang[2].x = FLT_MAX;

	if( _isnan(vTang[2].y) )
			vTang[2].y = FLT_MAX;

	if( _isnan(vTang[2].z) )
			vTang[2].z = FLT_MAX;

}

Vec3	CRLTriangleSmall::GetBinormal( const int32 iVertId ) const
{
	assert( iVertId >= 0 && iVertId < 3 );
	CGeomCore *pGeomCore = CSceneContext::GetInstance().GetGeomCore();
	assert( pGeomCore != NULL );
	const SMeshInformations *pMeshInfo = pGeomCore->GetMeshInfo( m_nMeshInfo );
	assert( pMeshInfo != NULL );
	int iBinormalStride = 0;
	Vec4sf *pBinormals = reinterpret_cast<Vec4sf *> (pMeshInfo->m_pRenderMesh->GetStridedBinormalPtr(iBinormalStride));
	assert( pBinormals );
	Vec4sf sfBinormal = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pBinormals)[m_nInd[iVertId] * iBinormalStride]) );
	Vec3 vTemp = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfBinormal.x),tPackB2F(sfBinormal.y),tPackB2F(sfBinormal.z)));

	if( _isnan(vTemp.x) )
			vTemp.x = FLT_MAX;

	if( _isnan(vTemp.y) )
			vTemp.y = FLT_MAX;

	if( _isnan(vTemp.z) )
			vTemp.z = FLT_MAX;

	return vTemp;
}

void	CRLTriangleSmall::GetBinormals( Vec3 *pBiNorm ) const
{
	assert( pBiNorm );
	CGeomCore *pGeomCore = CSceneContext::GetInstance().GetGeomCore();
	assert( pGeomCore != NULL );
	const SMeshInformations *pMeshInfo = pGeomCore->GetMeshInfo( m_nMeshInfo );
	assert( pMeshInfo != NULL );
	int iBinormalStride = 0;
	Vec4sf *pBinormals = reinterpret_cast<Vec4sf *> (pMeshInfo->m_pRenderMesh->GetStridedBinormalPtr(iBinormalStride));
	assert( pBinormals );
	Vec4sf sfBinormal = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pBinormals)[m_nInd[0] * iBinormalStride]) );
	pBiNorm[0] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfBinormal.x),tPackB2F(sfBinormal.y),tPackB2F(sfBinormal.z)));
	sfBinormal = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pBinormals)[m_nInd[1] * iBinormalStride]) );
	pBiNorm[1] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfBinormal.x),tPackB2F(sfBinormal.y),tPackB2F(sfBinormal.z)));
	sfBinormal = *reinterpret_cast<Vec4sf *>( &(reinterpret_cast<byte *>(pBinormals)[m_nInd[2] * iBinormalStride]) );
	pBiNorm[2] = pMeshInfo->m_mMatrix.TransformVector( Vec3(tPackB2F(sfBinormal.x),tPackB2F(sfBinormal.y),tPackB2F(sfBinormal.z)));

	if( _isnan(pBiNorm[0].x) )
			pBiNorm[0].x = FLT_MAX;

	if( _isnan(pBiNorm[0].y) )
			pBiNorm[0].y = FLT_MAX;

	if( _isnan(pBiNorm[0].z) )
			pBiNorm[0].z = FLT_MAX;

	if( _isnan(pBiNorm[1].x) )
			pBiNorm[1].x = FLT_MAX;

	if( _isnan(pBiNorm[1].y) )
			pBiNorm[1].y = FLT_MAX;

	if( _isnan(pBiNorm[1].z) )
			pBiNorm[1].z = FLT_MAX;

	if( _isnan(pBiNorm[2].x) )
			pBiNorm[2].x = FLT_MAX;

	if( _isnan(pBiNorm[2].y) )
			pBiNorm[2].y = FLT_MAX;

	if( _isnan(pBiNorm[2].z) )
			pBiNorm[2].z = FLT_MAX;

}

const struct IRenderShaderResources* 	CRLTriangleSmall::GetMaterial() const
{
	CGeomCore *pGeomCore = CSceneContext::GetInstance().GetGeomCore();
	assert( pGeomCore != NULL );
	const SMeshInformations *pMeshInfo = pGeomCore->GetMeshInfo( m_nMeshInfo );
	assert( pMeshInfo != NULL );
	return pMeshInfo->m_pMaterial;
}

void	CRLTriangleSmall::GetPositions( Vec3 *pVec ) const
{
	CGeomCore *pGeomCore = CSceneContext::GetInstance().GetGeomCore();
	assert( pGeomCore != NULL );
	const SMeshInformations *pMeshInfo = pGeomCore->GetMeshInfo( m_nMeshInfo );
	assert( pMeshInfo != NULL );
	int iVtxStride = 0;
	Vec3 *pVertices = reinterpret_cast<Vec3 *> (pMeshInfo->m_pRenderMesh->GetStridedPosPtr(iVtxStride));
	assert( pVertices );
	const Matrix34* pMatrix = &(pMeshInfo->m_mMatrix);

	Vec3* v[3];
	v[0] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pVertices)[m_nInd[0] * iVtxStride]));
	v[1] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pVertices)[m_nInd[1] * iVtxStride]));
	v[2] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pVertices)[m_nInd[2] * iVtxStride]));

	float fM = pMatrix->m00;
	pVec[0].x = fM*v[0]->x;
	pVec[1].x = fM*v[1]->x;
	pVec[2].x = fM*v[2]->x;
	fM = pMatrix->m01;
	pVec[0].x += fM*v[0]->y;
	pVec[1].x += fM*v[1]->y;
	pVec[2].x += fM*v[2]->y;
	fM = pMatrix->m02;
	pVec[0].x += fM*v[0]->z;
	pVec[1].x += fM*v[1]->z;
	pVec[2].x += fM*v[2]->z;
	fM = pMatrix->m03;
	pVec[0].x += fM;
	pVec[1].x += fM;
	pVec[2].x += fM;

	fM = pMatrix->m10;
	pVec[0].y = fM*v[0]->x;
	pVec[1].y = fM*v[1]->x;
	pVec[2].y = fM*v[2]->x;
	fM = pMatrix->m11;
	pVec[0].y += fM*v[0]->y;
	pVec[1].y += fM*v[1]->y;
	pVec[2].y += fM*v[2]->y;
	fM = pMatrix->m12;
	pVec[0].y += fM*v[0]->z;
	pVec[1].y += fM*v[1]->z;
	pVec[2].y += fM*v[2]->z;
	fM = pMatrix->m13;
	pVec[0].y += fM;
	pVec[1].y += fM;
	pVec[2].y += fM;

	fM = pMatrix->m20;
	pVec[0].z = fM*v[0]->x;
	pVec[1].z = fM*v[1]->x;
	pVec[2].z = fM*v[2]->x;
	fM = pMatrix->m21;
	pVec[0].z += fM*v[0]->y;
	pVec[1].z += fM*v[1]->y;
	pVec[2].z += fM*v[2]->y;
	fM = pMatrix->m22;
	pVec[0].z += fM*v[0]->z;
	pVec[1].z += fM*v[1]->z;
	pVec[2].z += fM*v[2]->z;
	fM = pMatrix->m23;
	pVec[0].z += fM;
	pVec[1].z += fM;
	pVec[2].z += fM;

	if( _isnan(pVec[0].x) )
		pVec[0].x = FLT_MAX;

	if( _isnan(pVec[0].y) )
		pVec[0].y = FLT_MAX;

	if( _isnan(pVec[0].z) )
		pVec[0].z = FLT_MAX;

	if( _isnan(pVec[1].x) )
		pVec[1].x = FLT_MAX;

	if( _isnan(pVec[1].y) )
		pVec[1].y = FLT_MAX;

	if( _isnan(pVec[1].z) )
		pVec[1].z = FLT_MAX;

	if( _isnan(pVec[2].x) )
		pVec[2].x = FLT_MAX;

	if( _isnan(pVec[2].y) )
		pVec[2].y = FLT_MAX;

	if( _isnan(pVec[2].z) )
		pVec[2].z = FLT_MAX;
}
