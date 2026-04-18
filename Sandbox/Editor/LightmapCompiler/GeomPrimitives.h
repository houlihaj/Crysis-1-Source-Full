/*=============================================================================
  GeomPrimitives.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __GEOMPRIMITIVES_H__
#define __GEOMPRIMITIVES_H__

//dep
#include "Ray.h"
#include "Cry_Geo.h"


class	ITracable
{
public:
	virtual	void		Bound(AABB& ) const		=	0;

	//ray intersection
	virtual	bool	Intersect(CRay*)	=	0;
	virtual	bool	IntersectClosest(CRay* pRay)	= 0;
	virtual	bool	IntersectFrontClosest(CRay* pRay) const = 0;

	//bounding box intersection
	virtual bool	TriBoxIntersect (	const AABB& bBox ) const = 0;
	virtual	bool	TriBoxIntersect (	const Vec3&, const Vec3&) const = 0;

	//material settings
	virtual void	SetOpaque( const bool bOpaque ) = 0;
	virtual bool	IsOpaque() const = 0;

	int	nID;
};


struct	SRLVertex 
{
//	Vec3		vPos;
	//Vec3		vNormal;

	f32			u,v; //FIX:: to f32 uv[2];

	//fix
	//f32			fpX,fpY;

	//fix
	//Vec3		vTNormal;
	//Vec3		vBinormal;
	//Vec3		vTangent;

};

// RasterEdge structure.
//fix:: names
struct CRasterEdge
{
	int32 iMinY;
	int32 iMaxY;
	int32 iX;
	int32 iX1;
	int32 iIncrement;
	int32 iNumerator;
	int32 iDenominator;
} ;


struct SMeshInformations
{
	int32 iVtxStride;
	Vec3 *pVertices;
	IRenderMesh* m_pRenderMesh;
	Matrix34		 m_mMatrix;
	struct IRenderShaderResources* m_pMaterial;
};

class	CRLTriangleSmall
{
public:
	CRLTriangleSmall();

	void		Bound(AABB& bBox) const;

	void BaryInterpolate (f32 b1, f32 b2, f32 b3, Vec3& pos, Vec3& norm) const;

	//===================================================================================
	// Method					:	BaryInterpolate Pos
	// Description		:	Interpolate position given the Barycentric cooridnates.
	//===================================================================================
	inline void BaryInterpolatePos (const f32 b1, const f32 b2, const f32 b3, Vec3& pos) const
	{
		Vec3 vPos[3];
		GetPositions( vPos );

		vPos[0] *= b1;
		vPos[1] *= b2;
		vPos[2] *= b3;

		pos = vPos[0];
		pos += vPos[1];
		pos += vPos[2];
	}

	void BaryInterpolateNormal (f32 b1, f32 b2, f32 b3, Vec3& norm) const;
	void BaryInterpolateTangent (f32 b1, f32 b2, f32 b3, Vec3& binormal, Vec3& tangent)  const;


	//ray intersection
	bool	Intersect(CRay*);
	bool	IntersectClosest(CRay* pRay);
	bool	IntersectFrontClosest(CRay* pRay) const;

	bool		TriBoxIntersect (	const AABB& bBox ) const;
	bool		TriBoxIntersect (	const Vec3& vBoxCenter, const Vec3& boxhalfsize, Vec3* vPos ) const;

	//	fix: optimize mesh representation
	//	void			*v[3];		// Pointers to the vertices (v[0] contains the major axis, v[1] contains the face status, v[2] contains the front back)

	Vec3	GetPosition( const int32 iVertId ) const;
	Vec3	GetNormal( const int32 iVertId ) const;
	Vec3	GetTangent( const int32 iVertId ) const;
	Vec3	GetBinormal( const int32 iVertId ) const;
	void	GetPositions( Vec3 *pVec ) const;

	inline void	GetPositions( Vec3 *pVec, const SMeshInformations *pMeshInfo ) const
	{
		const Matrix34* pMatrix = &(pMeshInfo->m_mMatrix);

		Vec3* v[3];
		v[0] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pMeshInfo->pVertices)[m_nInd[0] * pMeshInfo->iVtxStride]));
		v[1] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pMeshInfo->pVertices)[m_nInd[1] * pMeshInfo->iVtxStride]));
		v[2] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pMeshInfo->pVertices)[m_nInd[2] * pMeshInfo->iVtxStride]));

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
		/*
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

		FastKillDenormals(pVec[0]);
		FastKillDenormals(pVec[1]);
		FastKillDenormals(pVec[2]);
		*/
	}

	void	GetNormals( Vec3 *pNorm ) const;
	void	GetTangents( Vec3 *pTang ) const;
	void	GetBinormals( Vec3 *pBiNorm ) const;

	const struct IRenderShaderResources* 	GetMaterial() const;


	//===================================================================================
	// Method					:	TriIntersect_Fast
	// Description		:	Main Triangle intersect routine (fast version - no intersection point )
	//===================================================================================
	inline bool TriIntersect_Fast(CRay* pRay, const SMeshInformations *pMeshInfo ) const
	{
		Vec3& orig = pRay->vFrom;
		Vec3& dir = pRay->vDir;
		Vec3& vOrigNormal = pRay->vOrigNormal;
		Vec3& facenormal = pRay->vN;

		Vec3 vPos[3];

//		GetPositions( vPos, pMeshInfo );

		const Matrix34* pMatrix = &(pMeshInfo->m_mMatrix);

		Vec3* v[3];
		v[0] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pMeshInfo->pVertices)[m_nInd[0] * pMeshInfo->iVtxStride]));
		v[1] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pMeshInfo->pVertices)[m_nInd[1] * pMeshInfo->iVtxStride]));
		v[2] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pMeshInfo->pVertices)[m_nInd[2] * pMeshInfo->iVtxStride]));

		float fM = pMatrix->m00;
		vPos[0].x = fM*v[0]->x;
		vPos[1].x = fM*v[1]->x;
		vPos[2].x = fM*v[2]->x;
		fM = pMatrix->m01;
		vPos[0].x += fM*v[0]->y;
		vPos[1].x += fM*v[1]->y;
		vPos[2].x += fM*v[2]->y;
		fM = pMatrix->m02;
		vPos[0].x += fM*v[0]->z;
		vPos[1].x += fM*v[1]->z;
		vPos[2].x += fM*v[2]->z;
		fM = pMatrix->m03;
		vPos[0].x += fM;
		vPos[1].x += fM;
		vPos[2].x += fM;

		fM = pMatrix->m10;
		vPos[0].y = fM*v[0]->x;
		vPos[1].y = fM*v[1]->x;
		vPos[2].y = fM*v[2]->x;
		fM = pMatrix->m11;
		vPos[0].y += fM*v[0]->y;
		vPos[1].y += fM*v[1]->y;
		vPos[2].y += fM*v[2]->y;
		fM = pMatrix->m12;
		vPos[0].y += fM*v[0]->z;
		vPos[1].y += fM*v[1]->z;
		vPos[2].y += fM*v[2]->z;
		fM = pMatrix->m13;
		vPos[0].y += fM;
		vPos[1].y += fM;
		vPos[2].y += fM;

		fM = pMatrix->m20;
		vPos[0].z = fM*v[0]->x;
		vPos[1].z = fM*v[1]->x;
		vPos[2].z = fM*v[2]->x;
		fM = pMatrix->m21;
		vPos[0].z += fM*v[0]->y;
		vPos[1].z += fM*v[1]->y;
		vPos[2].z += fM*v[2]->y;
		fM = pMatrix->m22;
		vPos[0].z += fM*v[0]->z;
		vPos[1].z += fM*v[1]->z;
		vPos[2].z += fM*v[2]->z;
		fM = pMatrix->m23;
		vPos[0].z += fM;
		vPos[1].z += fM;
		vPos[2].z += fM;



/*//	debug triangle, ray
		vPos[0].x	= vPos[0].y	= vPos[0].z	= 0.f;
		vPos[1].y	= vPos[1].z	= 0.f;
		vPos[2].x	= vPos[2].z	= 0.f;
		vPos[1].x	= vPos[2].y	= 1.f;
		orig.x	=	0.25f;
		orig.y	=	0.25f;
		orig.z	=	1.f;
		dir.x	=	
		dir.y	=	0.f;
		dir.z	=	-1.f;*/

		{
			const Vec3	D10			=	(vPos[1]-vPos[0]);
			const Vec3	D02			=	(vPos[0]-vPos[2]);
			const Vec3	Normal	=	D02^D10;	//usually D10^D20, but we flipped one vector, so we have 
																			//to flip the normal by swapping the cross product factors

			const f32	tDiv	=	(Normal*dir);
			const f32 tDot	=	-(orig*Normal-Normal*vPos[0]);
			const f32 t	=	tDot/tDiv;
			if(!(t>0.01f) || (t>pRay->fT))
				return false;

			const Vec3 Pos	=	orig	+	dir*t;

			const Vec3 N0	=	Normal^D10;
			const bool CMP0	=	(N0*Pos<vPos[0]*N0);
//			if(CMP0)
//				return false;
			const Vec3 N1	=	Normal^D02;//(vPos[0]-vPos[2]);
			const bool CMP1	=	(N1*Pos<vPos[2]*N1);
//			if(CMP1)
//				return false;
			const Vec3 N2	=	Normal^(vPos[2]-vPos[1]);
			const	bool CMP2	=	(N2*Pos<vPos[1]*N2);
//			if(CMP2)
//				return false;
			if((CMP0 & CMP1 & CMP2)|(!(CMP0 | CMP1 | CMP2)))
			{
				pRay->fTmin	=	t;
				pRay->HitNormal(Normal);
				return true;
			}
			return false;
		}

/*/
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

		f32 edge1[3],edge2[3];

		SUB(edge1, vPos[1], vPos[0]);
		SUB(edge2, vPos[2], vPos[0]);
		CROSS(facenormal, edge1, edge2);

		f32 fR1 = DOT( dir, facenormal );
		f32 fMinLen = DOT( facenormal, facenormal );
		f32 fAbsR1 = fabsf( fR1 ) * ( 1.f / 10e-5f );

		int nWrong = ( fMinLen < 10e-5f ) ? 1 : 0;
		nWrong += ( fAbsR1 < fMinLen ) ? 1 : 0;

		fR1 = ( fAbsR1 < fMinLen ) ? 1 : fR1;		//Div by zero

		f32 fR0 = DOT( orig, facenormal ) - DOT( vPos[0], facenormal );
		f32 t = -fR0 / fR1;
		f32 fNormalDiffDist = fabs(t*DOT(dir, vOrigNormal ));

		//prevent shadow leaking - non valid tests..
		nWrong += t < 0.001f + pRay->fTmin ? 1 : 0;
		nWrong += t > pRay->fT ? 1 : 0;
		nWrong += fNormalDiffDist < 10e-5f ? 1 : 0;

		if( nWrong )
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

		int nGood = ( (!nSign0 && ( !nSign1 || !nSign2 ) ) || (!nSign1 && !nSign2) ) ? 1 : 0;
		nGood += (!nSign0 && nSign1 == nSign2 ) ? 1 : 0;
		nGood += (!nSign1 && nSign0 == nSign2 ) ? 1 : 0;
		nGood += (!nSign2 && nSign1 == nSign0 ) ? 1 : 0;
		nGood += (nSign0 == nSign1 && nSign0 == nSign2 ) ? 1 : 0;
		pRay->fTmin = nGood ? t : pRay->fTmin;
		return nGood ? true : false;
#undef CROSS
#undef DOT
#undef SUB
#undef ADD
/**/
	}


	_inline void KillDenormals( float &f ) const
	{
#ifndef WIN64
		const int nFloat = *reinterpret_cast <const int *> (&f);
		f = ( nFloat & 0x007FFFFF && !(nFloat & 0x7F800000)) ? 0 : f;
#endif
	}

	_inline void KillDenormals( Vec3 &v ) const
	{
#ifndef WIN64
		const int nFloat = *reinterpret_cast <const int *> (&v.x);
		v.x = ( nFloat & 0x007FFFFF && !(nFloat & 0x7F800000)) ? 0 : v.x;
		const int nFloat2 = *reinterpret_cast <const int *> (&v.y);
		v.y = ( nFloat2 & 0x007FFFFF && !(nFloat2 & 0x7F800000)) ? 0 : v.y;
		const int nFloat3 = *reinterpret_cast <const int *> (&v.z);
		v.z = ( nFloat3 & 0x007FFFFF && !(nFloat3 & 0x7F800000)) ? 0 : v.z;
#endif
	}

	_inline void FastKillDenormals( float &f ) const
	{
		static const float fSmallNumber = FLT_MIN*264;
		f += fSmallNumber;
		f -= fSmallNumber;
	}

	_inline void FastKillDenormals( Vec3 &v ) const
	{
		static const float fSmallNumber = FLT_MIN*264;
		v.x += fSmallNumber;
		v.x -= fSmallNumber;
		v.y += fSmallNumber;
		v.y -= fSmallNumber;
		v.z += fSmallNumber;
		v.z -= fSmallNumber;
	}

private:
	bool PlaneBoxOverlap (Vec3& normal, f32 d, const Vec3& maxbox) const;
	bool TriIntersect(Vec3& orig, Vec3& dir, f32 *t, f32 *u, f32 *v, f32 fTmax, f32 fTmin) const;

public:
	uint16		m_nInd[3]; //Relation of vert to the vertex buffer of render mesh 
	int32			m_nMeshInfo;
};


class	CRLTriangle //: public ITracable 
{
public:

	CRLTriangle();

	void BaryInterpolate (f32 b1, f32 b2, f32 b3, Vec3& pos, Vec3& norm, const SMeshInformations *pMeshInfo ) const;

	//===================================================================================
	// Method					:	BaryInterpolate Pos
	// Description		:	Interpolate position given the Barycentric cooridnates.
	//===================================================================================
	inline void BaryInterpolatePos (const f32 b1, const f32 b2, const f32 b3, Vec3& pos, const SMeshInformations *pMeshInfo) const
	{
		Vec3 vPos[3];
		GetPositions( vPos, pMeshInfo );

		vPos[0] *= b1;
		vPos[1] *= b2;
		vPos[2] *= b3;

		pos = vPos[0];
		pos += vPos[1];
		pos += vPos[2];
	}

	void BaryInterpolateNormal (f32 b1, f32 b2, f32 b3, Vec3& norm, const SMeshInformations *pMeshInfo) const;
	void BaryInterpolateTangent (f32 b1, f32 b2, f32 b3, Vec3& binormal, Vec3& tangent, const SMeshInformations *pMeshInfo)  const;


	Vec3	GetPosition( const int32 iVertId, const SMeshInformations *pMeshInfo ) const;
	Vec3	GetNormal( const int32 iVertId, const SMeshInformations *pMeshInfo ) const;
	Vec3	GetTangent( const int32 iVertId, const SMeshInformations *pMeshInfo ) const;
	Vec3	GetBinormal( const int32 iVertId, const SMeshInformations *pMeshInfo ) const;

	inline void	GetPositions( Vec3 *pVec, const SMeshInformations *pMeshInfo ) const
	{
		const Matrix34* pMatrix = &(pMeshInfo->m_mMatrix);

		Vec3* v[3];
		v[0] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pMeshInfo->pVertices)[m_nInd[0] * pMeshInfo->iVtxStride]));
		v[1] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pMeshInfo->pVertices)[m_nInd[1] * pMeshInfo->iVtxStride]));
		v[2] =  reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pMeshInfo->pVertices)[m_nInd[2] * pMeshInfo->iVtxStride]));

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
/*
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

		FastKillDenormals(pVec[0]);
		FastKillDenormals(pVec[1]);
		FastKillDenormals(pVec[2]);
*/
	}

	void	GetNormals( Vec3 *pNorm, const SMeshInformations *pMeshInfo ) const;
	void	GetTangents( Vec3 *pTang, const SMeshInformations *pMeshInfo ) const;
	void	GetBinormals( Vec3 *pBiNorm, const SMeshInformations *pMeshInfo ) const;

	const struct IRenderShaderResources* 	GetMaterial( const SMeshInformations *pMeshInfo) const;


	_inline void KillDenormals( float &f ) const
	{
#ifndef WIN64
		const int nFloat = *reinterpret_cast <const int *> (&f);
		f = ( nFloat & 0x007FFFFF && !(nFloat & 0x7F800000)) ? 0 : f;
#endif
	}

	_inline void KillDenormals( Vec3 &v ) const
	{
#ifndef WIN64
		const int nFloat = *reinterpret_cast <const int *> (&v.x);
		v.x = ( nFloat & 0x007FFFFF && !(nFloat & 0x7F800000)) ? 0 : v.x;
		const int nFloat2 = *reinterpret_cast <const int *> (&v.y);
		v.y = ( nFloat2 & 0x007FFFFF && !(nFloat2 & 0x7F800000)) ? 0 : v.y;
		const int nFloat3 = *reinterpret_cast <const int *> (&v.z);
		v.z = ( nFloat3 & 0x007FFFFF && !(nFloat3 & 0x7F800000)) ? 0 : v.z;
#endif
	}

	_inline void FastKillDenormals( float &f ) const
	{
		static const float fSmallNumber = FLT_MIN*264;
		f += fSmallNumber;
		f -= fSmallNumber;
	}

	_inline void FastKillDenormals( Vec3 &v ) const
	{
		static const float fSmallNumber = FLT_MIN*264;
		v.x += fSmallNumber;
		v.x -= fSmallNumber;
		v.y += fSmallNumber;
		v.y -= fSmallNumber;
		v.z += fSmallNumber;
		v.z -= fSmallNumber;
	}

private:
	bool PlaneBoxOverlap (Vec3& normal, f32 d, const Vec3& maxbox) const;
	bool TriIntersect(Vec3& orig, Vec3& dir, f32 *t, f32 *u, f32 *v, f32 fTmax, f32 fTmin) const;

public:
	SRLVertex	m_vVert[3];  
  uint16		m_nInd[3]; //Relation of vert to the vertex buffer of render mesh 
	int32			m_nMeshInfo;

	//Plane	N;
//	SRLVertex*	m_pVert;
	uint8			m_nIsOpaque:1;
	uint8			m_nIsDilated:1;
//	int32		m_nSessionID;				//speedup - only 1 check / session
};


#endif