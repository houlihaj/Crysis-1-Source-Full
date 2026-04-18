////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   COcclusionCuller.h
//  Version:     v1.00
//  Created:     13/8/2006 by Michael Kopietz
//  Compilers:   Visual Studio.NET
//  Description: Occlusion buffer
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "COcclusionCuller.h"
#include "COcclusionCullerPoly.h"
#include "COcclusionCullerClipper.h"
#include "StatObj.h"

COcculsionCuller::COcculsionCuller():
#ifdef  OC_ZEROCYCLECLEAR
m_ClearCounter(0),
#endif
m_Dirty(true),
m_OutdoorVisible(true)
{
#if defined(PS3)
	m_CurProjectedVertexCount = OC_MAX_VERTPERPOLY;
	m_ProjectedVertexCacheX		= (int32*)memalign(128, 3*OC_MAX_VERTPERPOLY*sizeof(int32));
	m_ProjectedVertexCacheY		= m_ProjectedVertexCacheX + OC_MAX_VERTPERPOLY;
	m_ProjectedVertexCacheZ		= (TOCZexel*)(m_ProjectedVertexCacheY + OC_MAX_VERTPERPOLY);
#else
	m_ProjectedVertexCacheX.PreAllocate(OC_MAX_VERTPERPOLY,OC_MAX_VERTPERPOLY);
	m_ProjectedVertexCacheY.PreAllocate(OC_MAX_VERTPERPOLY,OC_MAX_VERTPERPOLY);
	m_ProjectedVertexCacheZ.PreAllocate(OC_MAX_VERTPERPOLY,OC_MAX_VERTPERPOLY);
#endif
	m_TrisWritten		=
	m_ObjectsWritten=
	m_TrisTested		=
	m_ObjectsTested	=
	m_ObjectsTestedAndRejected	=	0;
	m_TopLeftX			=
	m_TopLeftY			=	0.f;
	m_BottomRightX	=
	m_BottomRightY	=	1.f;

#ifdef OC_LUTDIVISION
	for(int32 a=-255;a<256;a++)
		if(a)
			m_IDivLut[a+255]	=	256/a;
	m_UDivLut[0]	=	0xffffff;
	for(int32 a=1;a<256;a++)
			m_UDivLut[a]	=	(1<<14)/a;
#endif
#if defined(PS3)
	m_ZBuffer = NULL;
	m_LastSizeZ = 0;
	#ifdef OC_HIERARCHICAL_ZBUFFER
		m_YCBuffer	= NULL;
		m_CBuffer		= NULL;
	#endif
	m_CurVertexCacheSize = 1024;
	m_VertexCache = (Vec4*)memalign(128, m_CurVertexCacheSize * sizeof(Vec4));
	#ifdef OC_COHENSUTHERLAND
		m_VertexClipOutcodes = (uint32*)memalign(128, m_CurVertexCacheSize * sizeof(uint32));
	#endif
#endif
}

void COcculsionCuller::BeginFrame(const CCamera& rCam)
{
	FUNCTION_PROFILER_3DENGINE;

	m_FrameTime			=	0.f;
	m_ZNearInMeters	=	0.f;
	m_ZFarInMeters	=	1024.f;

	m_TrisWritten		=
	m_ObjectsWritten=
	m_TrisTested		=
	m_ObjectsTested	=
	m_ObjectsTestedAndRejected	=	0;
	m_Camera				=	rCam;
	Vec3 rCamPosition(rCam.GetPosition());
	m_EyePos				=	*reinterpret_cast<const Vec4*>(&rCamPosition);
	m_EyePos.w			=	1.f;
	m_TopLeftX			=
	m_TopLeftY			=	0.f;
	m_BottomRightX	=
	m_BottomRightY	=	1.f;
	m_SizeX					=	
	m_SizeY					= min(max(1, GetCVars()->e_cbuffer_resolution),256);
	m_SizeXMax			=	m_SizeX-1;
	m_InvSize				=	2.5f/static_cast<f32>(m_SizeX);//scale by pixelsize

	m_SizeShift	=	-1;
	while(!((1<<(++m_SizeShift))&m_SizeX));
	m_SizeZ			=	m_SizeX*m_SizeY;
	m_SizeZMask	=	m_SizeZ-1;
	m_SizeC			=	m_SizeZ>>OC_ZEXELSKIP_SHIFT;

#if defined(PS3)
	if(m_LastSizeZ != m_SizeZ)
	{
		m_LastSizeZ = m_SizeZ;
		free(m_ZBuffer);
		m_ZBuffer = (TOCZexel*)memalign(128, m_SizeZ * sizeof(TOCZexel));
	#ifdef OC_HIERARCHICAL_ZBUFFER
		free(m_CBuffer);
		m_CBuffer = (TOCCexel*)memalign(128, m_SizeC * sizeof(TOCCexel));
		free(m_YCBuffer);
		m_YCBuffer = (TOCCexel*)memalign(128, m_SizeY * sizeof(TOCCexel));
	#endif
	}
#else
	m_ZBuffer.PreAllocate(m_SizeZ,m_SizeZ);
	#ifdef OC_HIERARCHICAL_ZBUFFER
		m_CBuffer.PreAllocate(m_SizeC,m_SizeC);
		m_YCBuffer.PreAllocate(m_SizeY,m_SizeY);
	#endif
#endif

//	RasterizeWriteMMX(32,39,32,137,141,142,2146643972,2146643972,2146643972);

	if(!GetCVars()->e_cbuffer_debug_freeze)
	{
		CCamera tmpCam	= m_pRenderer->GetCamera();
		m_pRenderer->SetCamera(m_Camera);
		m_pRenderer->GetModelViewMatrix(reinterpret_cast<f32*>(&m_MatView));
		m_pRenderer->GetProjectionMatrix(reinterpret_cast<f32*>(&m_MatProj));
		m_MatViewProj		=	m_MatView*m_MatProj;
		m_MatViewProj.Transpose();
		m_pRenderer->SetCamera(tmpCam);

		m_FixedZFar			=	1.f/(m_Camera.GetFarPlane()-m_Camera.GetNearPlane());

		Clear();
	}
}

void COcculsionCuller::Clear()
{
	FUNCTION_PROFILER_3DENGINE;
#ifdef  OC_ZEROCYCLECLEAR
	if(!m_ClearCounter)
#endif

#ifdef OC_MMX
		int32*	pZBuffer	=	m_ZBuffer.GetElements();
	__m64 Clear64		=	_mm_cvtsi32_si64(OC_ZEXEL_CLEARVALUEHIGH);
	Clear64	=	_mm_or_si64(_mm_slli_si64(Clear64,32),Clear64);

//	__m64 OutdoorVisible	=	_mm_or_si64(_mm_slli_si64(_mm_cvtsi32_si64(0),32),_mm_cvtsi32_si64(0));

	TOCZexel* pBuffer	=	&m_ZBuffer[0];
	for(uint32 a=0;a<m_SizeZ;a+=4)
	{
//		__m64 ZBuffer0	=	*(__m64*)&pZBuffer[a];
//		__m64 ZBuffer1	=	*(__m64*)&pZBuffer[a+2];
//		OutdoorVisible	=_mm_or_si64(OutdoorVisible,_mm_or_si64(_mm_cmpeq_pi32(ZBuffer1,Clear64),_mm_cmpeq_pi32(ZBuffer0,Clear64)));
		*(__m64*)&pZBuffer[a]	=	Clear64;
		*(__m64*)&pZBuffer[a+2]	=	Clear64;
	}

//	m_OutdoorVisible	=	(OutdoorVisible.m64_i32[0]|OutdoorVisible.m64_i32[1])?true:false;
	_mm_empty();
#else
#if defined(PS3) && defined(PS3_OPT)
	const vec_uint4 cClearVal = (vec_uint4){OC_ZEXEL_CLEARVALUEHIGH};
	vec_uint4* const __restrict pBuffer	=	(vec_uint4*)&m_ZBuffer[0];
	const uint32 cCnt = m_SizeZ * sizeof(TOCZexel) / sizeof(vec_uint4) / 4;//clear via 4 vec_uint4 at once
	for(uint32 a=0;a<cCnt;a+=4)
	{
		pBuffer[a]		=	cClearVal;
		pBuffer[a+1]	=	cClearVal;
		pBuffer[a+2]	=	cClearVal;
		pBuffer[a+3]	=	cClearVal;
	}
#else
	TOCZexel* pBuffer	=	&m_ZBuffer[0];
	for(uint32 a=0;a<m_SizeZ;a+=4)
	{
			pBuffer[a]		=	OC_ZEXEL_CLEARVALUEHIGH;
			pBuffer[a+1]	=	OC_ZEXEL_CLEARVALUEHIGH;
			pBuffer[a+2]	=	OC_ZEXEL_CLEARVALUEHIGH;
			pBuffer[a+3]	=	OC_ZEXEL_CLEARVALUEHIGH;
	}
#endif//PS3
#endif
#ifdef OC_HIERARCHICAL_ZBUFFER
	#if defined(PS3)
		const vec_uint4 cClearValC = (vec_uint4){OC_ZEXEL_CLEARVALUELOW};
		vec_uint4* const __restrict pBufferC	=	(vec_uint4*)&m_CBuffer[0];
		const uint32 cCntC = m_SizeC * sizeof(TOCZexel) / sizeof(vec_uint4) / 4;//clear via 4 vec_uint4 at once
		for(uint32 a=0;a<cCntC;a+=4)
		{
			pBufferC[a]		=	cClearValC;
			pBufferC[a+1]	=	cClearValC;
			pBufferC[a+2]	=	cClearValC;
			pBufferC[a+3]	=	cClearValC;
		}
	#else
		for(uint32 a=0;a<m_SizeC;a++)
			m_CBuffer[a]	=	OC_ZEXEL_CLEARVALUELOW;
	#endif//PS3
#endif
#ifdef  OC_ZEROCYCLECLEAR
	m_ClearCounter	=	(m_ClearCounter-(1<<24))&OC_ZEXEL_CLEARVALUEHIGH;
	m_Nearest	=	OC_ZEXEL_MAXVALUEHIGH|m_ClearCounter;
#else
	m_Nearest	=	OC_ZEXEL_MAXVALUEHIGH;
#endif
}

#ifdef OC_MMX
void COcculsionCuller::RasterizeWriteMMX(int32 X1,int32 X2,int32 X3,int32 Y1,int32 Y2,int32 Y3,int32 Z1,int32 Z2,int32 Z3)
{
  if(Z1<m_Nearest)
    m_Nearest	=	Z1;
  if(Z2<m_Nearest)
    m_Nearest	=	Z2;
  if(Z3<m_Nearest)
    m_Nearest	=	Z3;

	if(!(Y1<=Y2 && Y2<=Y3))
	{
		int32 S;
		if(Y2<Y1)
		{
			if(Y1<Y3)
			{//just swap 1,2
				S=X1;X1=X2;X2=S;
				S=Y1;Y1=Y2;Y2=S;
				S=Z1;Z1=Z2;Z2=S;
			}
			else//Y1 is biggest
			{
				if(Y2<Y3)//Y2 is smallest
				{//rotate 2,3,1
					S=X1;X1=X2;X2=X3;X3=S;
					S=Y1;Y1=Y2;Y2=Y3;Y3=S;
					S=Z1;Z1=Z2;Z2=Z3;Z3=S;
				}
				else
				{//just swap 1,3
					S=X1;X1=X3;X3=S;
					S=Y1;Y1=Y3;Y3=S;
					S=Z1;Z1=Z3;Z3=S;
				}
			}
		}
		else//Y1<Y2
		{
			if(Y3<Y1)
			{//rotate 3,1,2
				S=X1;X1=X3;X3=X2;X2=S;
				S=Y1;Y1=Y3;Y3=Y2;Y2=S;
				S=Z1;Z1=Z3;Z3=Z2;Z2=S;
			}
			else
			{//just swap 2,3
				S=X2;X2=X3;X3=S;
				S=Y2;Y2=Y3;Y3=S;
				S=Z2;Z2=Z3;Z3=S;
			}
		}
	}
	if(Y1==Y3)
		return;

//	Z1/=16;
//	Z2/=16;
//	Z3/=16;
	int32 ZL	=	Z1;
	int32 ZR	=	Z1;
	int32 PtL	=	((Y1<<m_SizeShift)|min(X1,(int32)m_SizeX-1))<<OC_FIXPS;
	int32 PtR	=	PtL;
	int32 Pt2	=	((Y2<<m_SizeShift)|min(X2,(int32)m_SizeX-1))<<OC_FIXPS;
	int32 Pt3	=	((Y3<<m_SizeShift)|min(X3,(int32)m_SizeX-1))<<OC_FIXPS;
	const int32 Add13		=	OC_DIVU32S14((Pt3-PtL),(Y3-Y1));	//already tested for Y1==Y3
	const int32 Add13z	=	OC_DIVU32S14((Z3-Z1),(Y3-Y1));

/*	if(X1>=m_SizeX)
	{
		PtL--;
		PtR--;
	}
	if(X2>=m_SizeX)
		Pt2--;
	if(X3>=m_SizeX)
		Pt3--;*/

	if(Y2==Y1)
	{
		//Y1=Y2;
		Y2=Y3;
		ZR=Z2;
		Z2=Z3;
		PtR=Pt2;
		Pt2=Pt3;
	}

#if defined(PS3)
	int32*	pZBuffer	=	m_ZBuffer;
#else
	int32*	pZBuffer	=	m_ZBuffer.GetElements();
#endif
	while(Y2>Y1)
	{
		//prefetch of m_ZBuffer[PtL] here!
		const int32 Add12		=	OC_DIVU32S14((Pt2-PtR),(Y2-Y1));
		const int32 Add12z	=	OC_DIVU32S14((Z2-ZR),(Y2-Y1));
		do
		{
			int32 DeltaX=(PtR>>OC_FIXPS)-(PtL>>OC_FIXPS);//must be shifted down first to get correct filling
			if(DeltaX!=0)
			{
				const int32 AddZ=OC_DIVI32((ZR-ZL),DeltaX);
				int32 Pt,PtZ;
				if(DeltaX>0)
				{
					Pt=PtL>>OC_FIXPS;
					PtZ=ZL;
				}
				else
				{
					DeltaX=-DeltaX;
					Pt=PtR>>OC_FIXPS;
					PtZ=ZR;
				}
				if(DeltaX&1)
				{
					--DeltaX;
					//																			if(PtZ<m_ZBuffer[Pt++])
					//																				m_ZBuffer[Pt-1]	=	PtZ;
					const int32 Mask=(PtZ-m_ZBuffer[Pt])>>31;
					pZBuffer[Pt]	&=	~Mask;
					pZBuffer[Pt++]	|=	PtZ&Mask;
				}
				if(DeltaX)
				{
					__m64 AddZ64	=	_mm_cvtsi32_si64(AddZ<<1);//   *2 cause of double stepsize
					__m64 PtZ64		=	_mm_cvtsi32_si64(PtZ);
					PtZ64	=	_mm_or_si64(_mm_add_pi32(_mm_slli_si64(PtZ64,32),_mm_slli_si64(_mm_cvtsi32_si64(AddZ),32)),PtZ64);
					AddZ64	=	_mm_or_si64(_mm_slli_si64(AddZ64,32),AddZ64);
					for(int32 a=0;a<DeltaX;a+=2,Pt+=2)
					{
						__m64 ZBuffer	=	*(__m64*)&pZBuffer[Pt];
						const __m64 Mask	=	_mm_cmpgt_pi32(ZBuffer,PtZ64);
						ZBuffer	=_mm_or_si64(_mm_andnot_si64(Mask,ZBuffer),_mm_and_si64(Mask,PtZ64));
						*(__m64*)&pZBuffer[Pt]	=	ZBuffer;
						PtZ64=_mm_add_pi32(PtZ64,AddZ64);
					}
				}
			}
			PtL+=Add13;
			PtR+=Add12;
			ZL+=Add13z;
			ZR+=Add12z;
		}while(++Y1<Y2);
		Y1=Y2;
		Y2=Y3;
		ZR=Z2;
		Z2=Z3;
		PtR=Pt2;
		Pt2=Pt3;
	}

}
#endif

inline bool IsZWriteMMX(const SShaderItem& rSI)
{
    IShader *pSH = rSI.m_pShader;
    if (pSH->GetFlags2() & EF2_NODRAW)
      return false;
    if (pSH->GetFlags2() & EF2_FORCE_ZPASS)
      return true;
    if (pSH->GetFlags() & EF_DECAL)
      return false;
    if (rSI.m_pShaderResources && *reinterpret_cast<int32*>(&rSI.m_pShaderResources->GetDiffuseColor()[3]) != 0x3f800000)
      return false;
    return true;
}
void COcculsionCuller::AddRenderMesh(IRenderMesh * pRM, Matrix34* pTranRotMatrix, IMaterial * pMaterial, bool bOutdoorOnly, bool bCompletelyInFrustum,bool bNoCull)
{
	FUNCTION_PROFILER_3DENGINE;

	// check material
	if(!pMaterial)
		pMaterial = pRM->GetMaterial();
	assert(pMaterial);
	if(!pMaterial)
		return;

	m_ObjectsWritten++;
	if(GetCVars()->e_cbuffer_debug_freeze)
	{
		m_TrisWritten++;
		return;
	}

	uint32 nVisibleChunksMask = 0;
	const uint32 VertexCount = pRM->GetVertCount();
	int32 IndexCount = 0;
	int32 VertexSize	= 0;
	const ushort* pIndices = pRM->GetIndices(&IndexCount);
	const byte* pPos = pRM->GetStridedPosPtr(VertexSize);
	assert(pIndices && pPos);

	Matrix44 MatWorld(*pTranRotMatrix);
	Matrix44 MatWorldViewProj	=	m_MatViewProj * MatWorld;
#if defined(PS3)
	if(m_CurVertexCacheSize < VertexCount)
	{
		free(m_VertexCache);
		m_VertexCache = (Vec4*)memalign(128, (VertexCount+1) * sizeof(Vec4));
		m_CurVertexCacheSize = VertexCount+1;
#ifdef OC_COHENSUTHERLAND
		free(m_VertexClipOutcodes);
		m_VertexClipOutcodes = (uint32*)memalign(128, (VertexCount+1) * sizeof(uint32));
#endif
	}
#else
	if(m_VertexCache.capacity()<VertexCount)
	{
		m_VertexCache.PreAllocate(VertexCount+1,VertexCount+1);
	#ifdef OC_COHENSUTHERLAND
		m_VertexClipOutcodes.PreAllocate(VertexCount+1,VertexCount+1);
	#endif
	}
#endif//PS3
#ifdef OC_SSE
	Vec4* pVertexCache	=	&m_VertexCache[0];
	uint32* pVertexClipOutcodes	=	&m_VertexClipOutcodes[0];
	_mm_prefetch((const char*)pPos,_MM_HINT_T0);
	_mm_prefetch((const char*)pVertexCache,_MM_HINT_NTA);
	_mm_prefetch((const char*)pVertexClipOutcodes,_MM_HINT_NTA);
	MatWorldViewProj.Transpose();
	float Mat[32];
	Matrix44& rMat=*reinterpret_cast<Matrix44*>(reinterpret_cast<size_t>(&Mat[15])&~0xf);
	rMat=MatWorldViewProj;//need to copy as the compiler kinda makes alligned load in _mm_loadu_ps on the first element ->crash
	register __m128	MX		=	_mm_load_ps(&((float*)&rMat)[0]);
	register __m128	MY		=	_mm_load_ps(&((float*)&rMat)[4]);
	register __m128	MZ		=	_mm_load_ps(&((float*)&rMat)[8]);
	register __m128	MW		=	_mm_load_ps(&((float*)&rMat)[12]);
	register __m128 WMask	=	_mm_set_ps(0.f,0.f,-1.f,-1.f);
	Vec4 OutPos;
#ifdef OC_COHENSUTHERLAND
	if(!bCompletelyInFrustum)
	{
		uint32 ObjClipMask=0;
		for(uint32 a=0;a<VertexCount;a++)
		{
			_mm_prefetch((const char*)(&pPos[VertexSize*a])+128,_MM_HINT_T0);
			_mm_prefetch((const char*)(&pVertexCache[a])+128,_MM_HINT_NTA);
			_mm_prefetch((const char*)(&pVertexClipOutcodes[a])+128,_MM_HINT_NTA);
			const Vec3&	Pos	=	*reinterpret_cast<const Vec3*>(&pPos[VertexSize*a]);
			register __m128 OutPos	=	_mm_loadu_ps((const float*)&Pos);
#if defined(PS3)
			vec_float4 res;
			vec_float4 xxxx, yyyy, zzzz;
			xxxx		= vec_splat((vec_float4)OutPos, 0);
			yyyy		= vec_splat((vec_float4)OutPos, 1);
			zzzz		= vec_splat((vec_float4)OutPos, 2);
			OutPos	= (__m128)vec_madd((vec_float4)MX, xxxx, (vec_float4)MW);
			OutPos	= (__m128)vec_madd((vec_float4)MY, yyyy, (vec_float4)OutPos);
			OutPos	= (__m128)vec_madd((vec_float4)MZ, zzzz, (vec_float4)OutPos);
#else
			OutPos	=	_mm_add_ps(_mm_add_ps(
																											_mm_mul_ps(MX,_mm_shuffle_ps(OutPos,OutPos,0x0)),
																											_mm_mul_ps(MY,_mm_shuffle_ps(OutPos,OutPos,_MM_SHUFFLE(1,1,1,1)))),
																						_mm_add_ps(
																											_mm_mul_ps(MZ,_mm_shuffle_ps(OutPos,OutPos,_MM_SHUFFLE(2,2,2,2))),
																											MW));
#endif
			_mm_storeu_ps((float*)&pVertexCache[a],OutPos);
#if defined(PS3)
			//move w into all components
			__m128 OutPosW	=	_mm_shuffle_ps_w(OutPos);
#else
			__m128 OutPosW	=	_mm_shuffle_ps(OutPos,OutPos,0xff);
#endif
				
			uint32 cc								=(_mm_movemask_ps(_mm_cmpgt_ps(OutPos,OutPosW))<<4)|
																_mm_movemask_ps(_mm_cmplt_ps(OutPos,_mm_mul_ps(WMask,OutPosW)));
			ObjClipMask	|=	cc;
			pVertexClipOutcodes[a]	=	cc&0x77;
		}
		bCompletelyInFrustum	=	(ObjClipMask&0x77)?false:true;
	}
	else
#endif
	{
		for(uint32 a=0;a<VertexCount;a++)
		{
			_mm_prefetch((const char*)(&pPos[VertexSize*a])+256,_MM_HINT_T0);
			_mm_prefetch((const char*)(&pVertexCache[a])+256,_MM_HINT_NTA);
			const Vec3&	Pos	=	*reinterpret_cast<const Vec3*>(&pPos[VertexSize*a]);
			register __m128 OutPos	=	_mm_loadu_ps((const float*)&Pos);
#if defined(PS3)
			vec_float4 res;
			vec_float4 xxxx, yyyy, zzzz;
			xxxx		= vec_splat((vec_float4)OutPos, 0);
			yyyy		= vec_splat((vec_float4)OutPos, 1);
			zzzz		= vec_splat((vec_float4)OutPos, 2);
			OutPos	= (__m128)vec_madd((vec_float4)MX, xxxx, (vec_float4)MW);
			OutPos	= (__m128)vec_madd((vec_float4)MY, yyyy, (vec_float4)OutPos);
			OutPos	= (__m128)vec_madd((vec_float4)MZ, zzzz, (vec_float4)OutPos);
#else
			OutPos	=	_mm_add_ps(_mm_add_ps(
																											_mm_mul_ps(MX,_mm_shuffle_ps(OutPos,OutPos,0x0)),
																											_mm_mul_ps(MY,_mm_shuffle_ps(OutPos,OutPos,_MM_SHUFFLE(1,1,1,1)))),
																						_mm_add_ps(
																											_mm_mul_ps(MZ,_mm_shuffle_ps(OutPos,OutPos,_MM_SHUFFLE(2,2,2,2))),
																											MW));
#endif
			_mm_storeu_ps((float*)&pVertexCache[a],OutPos);
		}
	}
#else
	for(uint32 a=0;a<VertexCount;a++)
	{
		Vec4	Pos	=	*reinterpret_cast<const Vec4*>(&pPos[VertexSize*a]);
		Pos.w	=	1.f;
		m_VertexCache[a]	=	MatWorldViewProj*Pos;
	}
#endif


	m_Dirty	=	true;
	if(bCompletelyInFrustum)
	{
#if defined(PS3)	
		if(m_CurProjectedVertexCount < VertexCount)
		{
			m_CurProjectedVertexCount = VertexCount;
			free(m_ProjectedVertexCacheX);
			m_ProjectedVertexCacheX		= (int32*)memalign(128, 3*VertexCount*sizeof(int32));
			m_ProjectedVertexCacheY		= m_ProjectedVertexCacheX + VertexCount;
			m_ProjectedVertexCacheZ		= (TOCZexel*)(m_ProjectedVertexCacheY + VertexCount);
		}
#else
		if(m_ProjectedVertexCacheX.capacity()<VertexCount)
		{
			m_ProjectedVertexCacheX.PreAllocate(VertexCount,VertexCount);
			m_ProjectedVertexCacheY.PreAllocate(VertexCount,VertexCount);
			m_ProjectedVertexCacheZ.PreAllocate(VertexCount,VertexCount);
		}
#endif
		ProjectVertices(m_VertexCache,VertexCount);
		PodArray<CRenderChunk> *	pChunks = pRM->GetChunks();
    int nIndexStep = (pRM->GetPrimetiveType() == R_PRIMV_MULTI_STRIPS) ? 1 : 3;
		for(int32 nChunkId=0; nChunkId<pChunks->Count(); nChunkId++)
		{
			CRenderChunk * pChunk = pChunks->Get(nChunkId);
			if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
				continue;

			// skip transparent and alpha test
			const SShaderItem &shaderItem = pMaterial->GetShaderItem(pChunk->m_nMatID);
			if (!bNoCull && !IsZWriteMMX(shaderItem))
				continue;

			ECull nCullMode = shaderItem.m_pShader->GetCull();
			if (bNoCull)
				nCullMode = eCULL_None;

			nVisibleChunksMask |= (1 << (nChunkId));

			int32 nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
#ifndef OC_MMX
			int32 VerticesX[3];
			int32 VerticesY[3];
			int32 VerticesZ[3];
#endif
			m_TrisWritten	+=	(nLastIndexId-pChunk->nFirstIndexId)/3;
      for(int32 a=pChunk->nFirstIndexId; a<nLastIndexId-2; a+=nIndexStep)
      {
        int32 I0	=	pIndices[a];
        int32 I1	=	pIndices[a+1];
        int32 I2	=	pIndices[a+2];
        if(nIndexStep==1 && (a&1))
        {
          I1	=	pIndices[a+2];
          I2	=	pIndices[a+1];
        }
				assert(I0<VertexCount);
				assert(I1<VertexCount);
				assert(I2<VertexCount);

				const int32 Facing = (m_ProjectedVertexCacheX[I1]-m_ProjectedVertexCacheX[I0])*(m_ProjectedVertexCacheY[I2]-m_ProjectedVertexCacheY[I0])-
											(m_ProjectedVertexCacheX[I2]-m_ProjectedVertexCacheX[I0])*(m_ProjectedVertexCacheY[I1]-m_ProjectedVertexCacheY[I0]);
				if(	nCullMode == eCULL_None || (nCullMode == eCULL_Back && Facing>0 ) || (nCullMode == eCULL_Front && Facing<0))
				{
#ifdef OC_MMX
					if(Facing>0)
						RasterizeWriteMMX(	m_ProjectedVertexCacheX[I0],m_ProjectedVertexCacheX[I1],m_ProjectedVertexCacheX[I2],
																m_ProjectedVertexCacheY[I0],m_ProjectedVertexCacheY[I1],m_ProjectedVertexCacheY[I2],
																m_ProjectedVertexCacheZ[I0],m_ProjectedVertexCacheZ[I1],m_ProjectedVertexCacheZ[I2]);
					else
						RasterizeWriteMMX(	m_ProjectedVertexCacheX[I0],m_ProjectedVertexCacheX[I2],m_ProjectedVertexCacheX[I1],
																m_ProjectedVertexCacheY[I0],m_ProjectedVertexCacheY[I2],m_ProjectedVertexCacheY[I1],
																m_ProjectedVertexCacheZ[I0],m_ProjectedVertexCacheZ[I2],m_ProjectedVertexCacheZ[I1]);
#else
					if(Facing>0)
					{
						VerticesX[0]	=	m_ProjectedVertexCacheX[I0];
						VerticesX[1]	=	m_ProjectedVertexCacheX[I1];
						VerticesX[2]	=	m_ProjectedVertexCacheX[I2];
						VerticesY[0]	=	m_ProjectedVertexCacheY[I0];
						VerticesY[1]	=	m_ProjectedVertexCacheY[I1];
						VerticesY[2]	=	m_ProjectedVertexCacheY[I2];
						VerticesZ[0]	=	m_ProjectedVertexCacheZ[I0];
						VerticesZ[1]	=	m_ProjectedVertexCacheZ[I1];
						VerticesZ[2]	=	m_ProjectedVertexCacheZ[I2];
					}
					else
					{
						VerticesX[0]	=	m_ProjectedVertexCacheX[I0];
						VerticesX[1]	=	m_ProjectedVertexCacheX[I2];
						VerticesX[2]	=	m_ProjectedVertexCacheX[I1];
						VerticesY[0]	=	m_ProjectedVertexCacheY[I0];
						VerticesY[1]	=	m_ProjectedVertexCacheY[I2];
						VerticesY[2]	=	m_ProjectedVertexCacheY[I1];
						VerticesZ[0]	=	m_ProjectedVertexCacheZ[I0];
						VerticesZ[1]	=	m_ProjectedVertexCacheZ[I2];
						VerticesZ[2]	=	m_ProjectedVertexCacheZ[I1];
					}

					Rasterize<true>(VerticesX,VerticesY,VerticesZ,3);
#endif
				}
			}
		}
#ifdef OC_MMX
	_mm_empty();
#endif
	}
	else
	{
		COCPoly	PolyClipped;
		Matrix44 IMatWorld(pTranRotMatrix->GetInverted());
		const Vec4	TmpIEyePos	=	IMatWorld*m_EyePos;
		const Vec3&	IEyePos	=	*reinterpret_cast<const Vec3*>(&TmpIEyePos);
		PodArray<CRenderChunk> *	pChunks = pRM->GetChunks();
    int nIndexStep = (pRM->GetPrimetiveType() == R_PRIMV_MULTI_STRIPS) ? 1 : 3;
		for(int32 nChunkId=0; nChunkId<pChunks->Count(); nChunkId++)
		{
			CRenderChunk * pChunk = pChunks->Get(nChunkId);
			if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
				continue;

			// skip transparent and alpha test
			const SShaderItem &shaderItem = pMaterial->GetShaderItem(pChunk->m_nMatID);
			if (!bNoCull && !IsZWriteMMX(shaderItem))
				continue;

			ECull nCullMode = shaderItem.m_pShader->GetCull();
			if (bNoCull)
				nCullMode = eCULL_None;

			nVisibleChunksMask |= (1 << (nChunkId));

			int32 nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
			for(int32 a=pChunk->nFirstIndexId; a<nLastIndexId-2; a+=nIndexStep)
			{
				int32 I0	=	pIndices[a];
				int32 I1	=	pIndices[a+1];
				int32 I2	=	pIndices[a+2];
        if(nIndexStep==1 && (a&1))
        {
          I1	=	pIndices[a+2];
          I2	=	pIndices[a+1];
        }
				assert(I0<VertexCount);
				assert(I1<VertexCount);
				assert(I2<VertexCount);

				const Vec3& rV0	=	*reinterpret_cast<const Vec3*>(&pPos[VertexSize*I0]);
				const Vec3& rV1	=	*reinterpret_cast<const Vec3*>(&pPos[VertexSize*I1]);
				const Vec3& rV2	=	*reinterpret_cast<const Vec3*>(&pPos[VertexSize*I2]);
				const Vec3 Nrm	=	(rV2-rV0)^(rV1-rV0);

				const f32 Facing =	Nrm*(rV0-IEyePos);
				if(	nCullMode == eCULL_None || (nCullMode == eCULL_Back && Facing>0.f ) || (nCullMode == eCULL_Front && Facing<0.f))
				{
#ifdef OC_COHENSUTHERLAND
					if(!(m_VertexClipOutcodes[I0]&m_VertexClipOutcodes[I1]&m_VertexClipOutcodes[I2]))	//if not completely outside
					{
						if(!(m_VertexClipOutcodes[I0]|m_VertexClipOutcodes[I1]|m_VertexClipOutcodes[I2]))//if completely inside
						{
							Vec4 Vertices[3];
//							PolyClipped	=	COCPoly(m_VertexCache[I0],m_VertexCache[I1],m_VertexCache[I2]);
							Vertices[0]	=	m_VertexCache[I0];
							Vertices[1]	=	m_VertexCache[I1];
							Vertices[2]	=	m_VertexCache[I2];
							ProjectVertices(Vertices,3);
							m_TrisWritten++;
#ifdef OC_MMX
							if(Facing>=0.f)
								RasterizeWriteMMX(	m_ProjectedVertexCacheX[0],m_ProjectedVertexCacheX[1],m_ProjectedVertexCacheX[2],
																		m_ProjectedVertexCacheY[0],m_ProjectedVertexCacheY[1],m_ProjectedVertexCacheY[2],
																		m_ProjectedVertexCacheZ[0],m_ProjectedVertexCacheZ[1],m_ProjectedVertexCacheZ[2]);
							else
								RasterizeWriteMMX(	m_ProjectedVertexCacheX[0],m_ProjectedVertexCacheX[2],m_ProjectedVertexCacheX[1],
																		m_ProjectedVertexCacheY[0],m_ProjectedVertexCacheY[2],m_ProjectedVertexCacheY[1],
																		m_ProjectedVertexCacheZ[0],m_ProjectedVertexCacheZ[2],m_ProjectedVertexCacheZ[1]);

							_mm_empty();
#else
							Rasterize<true>(m_ProjectedVertexCacheX,m_ProjectedVertexCacheY,m_ProjectedVertexCacheZ,3);
#endif
						}
						else //else clipp it
						{
#endif
							if(Facing>=0.f)
								COCClipper::Clip(PolyClipped,COCPoly(m_VertexCache[I0],m_VertexCache[I1],m_VertexCache[I2]));
							else
								COCClipper::Clip(PolyClipped,COCPoly(m_VertexCache[I0],m_VertexCache[I2],m_VertexCache[I1]));
							if(PolyClipped.Count()>2)
							{
								m_TrisWritten++;
								RasterizeClip<true>(PolyClipped);
							}
#ifdef OC_COHENSUTHERLAND
						}
					}
#endif
				}

			}
		}
	}
#ifdef OC_MMX
	_mm_empty();
#endif
	if (GetCVars()->e_cbuffer_draw_occluders)
	{
		int nFlags = 0;
		if (bNoCull)
			nFlags |= 1;
		pRM->DebugDraw( *pTranRotMatrix,nFlags,nVisibleChunksMask );
	}
}

void COcculsionCuller::AddHeightMap(const SRangeInfo & m_rangeInfo, float X1, float Y1, float X2, float Y2)
{

}

bool COcculsionCuller::IsObjectVisible(const AABB _objBox, EOcclusionObjectType eOcclusionObjectType, float fDistance)
{
	AABB objBox = _objBox;
	float fExt = fDistance*m_InvSize;
	objBox.min -= Vec3(fExt,fExt,fExt);
	objBox.max += Vec3(fExt,fExt,fExt);

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

bool COcculsionCuller::IsBoxVisible_TERRAIN_NODE(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool COcculsionCuller::IsBoxVisible_OCCELL_OCCLUDER(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool COcculsionCuller::IsBoxVisible_OCCLUDER(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool COcculsionCuller::IsBoxVisible_OCEAN(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool COcculsionCuller::IsBoxVisible_OCCELL(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;
	if(GetCVars()->e_cbuffer_debug_freeze)
		return true;

	return IsBoxVisible(objBox);
}

bool COcculsionCuller::IsBoxVisible_OBJECT(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool COcculsionCuller::IsBoxVisible_OBJECT_TO_LIGHT(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool COcculsionCuller::IsBoxVisible_PORTAL(const AABB objBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return IsBoxVisible(objBox);
}

bool COcculsionCuller::RayBox(const AABB objBox,const Vec3& rOrigin,const Vec3& rDir)const
{
	f32 txmax,txmin,tymax,tymin,tzmax,tzmin;

	if(rDir.x>=0.f)
	{
		const f32 IDirX	=	1.f/max(rDir.x,FLT_EPSILON);
		txmin	=	(objBox.min.x-rOrigin.x)*IDirX;
		txmax	=	(objBox.max.x-rOrigin.x)*IDirX;
	}
	else
	{
		const f32 IDirX	=	1.f/min(rDir.x,-FLT_EPSILON);
		txmin	=	(objBox.max.x-rOrigin.x)*IDirX;
		txmax	=	(objBox.min.x-rOrigin.x)*IDirX;
	}
	if(rDir.y>=0.f)
	{
		const f32 IDirY	=	1.f/max(rDir.y,FLT_EPSILON);
		tymin	=	(objBox.min.y-rOrigin.y)*IDirY;
		tymax	=	(objBox.max.y-rOrigin.y)*IDirY;
	}
	else
	{
		const f32 IDirY	=	1.f/min(rDir.y,-FLT_EPSILON);
		tymin	=	(objBox.max.y-rOrigin.y)*IDirY;
		tymax	=	(objBox.min.y-rOrigin.y)*IDirY;
	}
	if(txmin>tymax || tymin>txmax)
		return false;

	if(rDir.z>=0.f)
	{
		const f32 IDirZ	=	1.f/max(rDir.x,FLT_EPSILON);
		tzmin	=	(objBox.min.z-rOrigin.z)*IDirZ;
		tzmax	=	(objBox.max.z-rOrigin.z)*IDirZ;
	}
	else
	{
		const f32 IDirZ	=	1.f/min(rDir.z,-FLT_EPSILON);
		tzmin	=	(objBox.max.z-rOrigin.z)*IDirZ;
		tzmax	=	(objBox.min.z-rOrigin.z)*IDirZ;
	}

	if(tymin>txmin)
		txmin=tymin;
	if(tymax<txmax)
		txmax=tymax;

	return !(txmin>tzmax || tzmin>txmax);
}

bool COcculsionCuller::IsShadowcasterVisible(const AABB& objBox,Vec3 rExtrusionDir)
{
#if !defined(PS3)
	FUNCTION_PROFILER_3DENGINE;
	if(!m_TrisWritten || !GetCVars()->e_cbuffer || !(GetCVars()->e_cbuffer_test_mode&2))
		return true;

	if(objBox.IsContainSphere(m_Camera.GetPosition(), -m_Camera.GetNearPlane()*2.f))
		return true;

	if(RayBox(objBox,m_Camera.GetPosition(),rExtrusionDir))
		return true;

	//if below the zero level, cut it to save fillrate-testing and get better results
	if(GetCVars()->e_shadows_occ_cutCaster>0 && objBox.max.z+rExtrusionDir.z<-1.f)
	{
		const f32 invHeight	=	(objBox.max.z-GetCVars()->e_shadows_occ_cutCaster)/(rExtrusionDir.z+0.000001f);
		rExtrusionDir*=-invHeight;
	}
	else
		rExtrusionDir*=-30.f;

	Vec4 arrVerts[16] = 
	{
		m_MatViewProj*Vec4(objBox.min.x,objBox.min.y,objBox.min.z,1.f),
		m_MatViewProj*Vec4(objBox.min.x,objBox.max.y,objBox.min.z,1.f),//1
		m_MatViewProj*Vec4(objBox.max.x,objBox.min.y,objBox.min.z,1.f),
		m_MatViewProj*Vec4(objBox.max.x,objBox.max.y,objBox.min.z,1.f),//3
		m_MatViewProj*Vec4(objBox.min.x,objBox.min.y,objBox.max.z,1.f),
		m_MatViewProj*Vec4(objBox.min.x,objBox.max.y,objBox.max.z,1.f),//5
		m_MatViewProj*Vec4(objBox.max.x,objBox.min.y,objBox.max.z,1.f),
		m_MatViewProj*Vec4(objBox.max.x,objBox.max.y,objBox.max.z,1.f),//7
		m_MatViewProj*Vec4(objBox.min.x+rExtrusionDir.x,objBox.min.y+rExtrusionDir.y,objBox.min.z+rExtrusionDir.z,1.f),
		m_MatViewProj*Vec4(objBox.min.x+rExtrusionDir.x,objBox.max.y+rExtrusionDir.y,objBox.min.z+rExtrusionDir.z,1.f),
		m_MatViewProj*Vec4(objBox.max.x+rExtrusionDir.x,objBox.min.y+rExtrusionDir.y,objBox.min.z+rExtrusionDir.z,1.f),
		m_MatViewProj*Vec4(objBox.max.x+rExtrusionDir.x,objBox.max.y+rExtrusionDir.y,objBox.min.z+rExtrusionDir.z,1.f),
		m_MatViewProj*Vec4(objBox.min.x+rExtrusionDir.x,objBox.min.y+rExtrusionDir.y,objBox.max.z+rExtrusionDir.z,1.f),
		m_MatViewProj*Vec4(objBox.min.x+rExtrusionDir.x,objBox.max.y+rExtrusionDir.y,objBox.max.z+rExtrusionDir.z,1.f),
		m_MatViewProj*Vec4(objBox.max.x+rExtrusionDir.x,objBox.min.y+rExtrusionDir.y,objBox.max.z+rExtrusionDir.z,1.f),
		m_MatViewProj*Vec4(objBox.max.x+rExtrusionDir.x,objBox.max.y+rExtrusionDir.y,objBox.max.z+rExtrusionDir.z,1.f)
	};

	const Vec3 vCamPos = m_Camera.GetPosition();

	// render only front faces of box
	COCPoly PolyClipped;


	//+x
	if(vCamPos.x>objBox.max.x)
	{
		if(rExtrusionDir.x<=-FLT_EPSILON)
		{//no extrusion
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[2],arrVerts[3],arrVerts[7],arrVerts[6]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
		}
//		else
		{//extrusion needed
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[2+8],arrVerts[3+8],arrVerts[7+8],arrVerts[6+8]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
			if(rExtrusionDir.y>0.f)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[2],arrVerts[2+8],arrVerts[6+8],arrVerts[6]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[3+8],arrVerts[3],arrVerts[7],arrVerts[7+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}

			if(rExtrusionDir.z>0.f)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[2],arrVerts[3],arrVerts[3+8],arrVerts[2+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[6+8],arrVerts[7+8],arrVerts[7],arrVerts[6]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
		}
	}

	//-x                                                                            
	if(vCamPos.x<objBox.min.x)
	{                                                                              
		if(rExtrusionDir.x>=FLT_EPSILON)
		{//no extrusion
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[1],arrVerts[0],arrVerts[4],arrVerts[5]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
		}
//		else
		{//extrusion needed
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[0+8],arrVerts[1+8],arrVerts[5+8],arrVerts[4+8]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
			if(rExtrusionDir.y>FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[0+8],arrVerts[0],arrVerts[4],arrVerts[4+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			if(rExtrusionDir.y<-FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[1],arrVerts[1+8],arrVerts[5+8],arrVerts[5]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			if(rExtrusionDir.z>FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[1],arrVerts[0],arrVerts[0+8],arrVerts[1+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			if(rExtrusionDir.z<-FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[4+8],arrVerts[4],arrVerts[5],arrVerts[5+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
		}
	}

	//+y
	if(vCamPos.y>objBox.max.y)                                                           
	{                                                                              
		if(rExtrusionDir.y<=-FLT_EPSILON)
		{//no extrusion
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[3],arrVerts[1],arrVerts[5],arrVerts[7]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
		}
//		else
		{//extrusion needed
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[3+8],arrVerts[1+8],arrVerts[5+8],arrVerts[7+8]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
			if(rExtrusionDir.x>0.f)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[1],arrVerts[3],arrVerts[3+8],arrVerts[1+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[5],arrVerts[7],arrVerts[7+8],arrVerts[5+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			if(rExtrusionDir.z>0.f)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[1],arrVerts[5],arrVerts[5+8],arrVerts[1+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[7+8],arrVerts[7],arrVerts[3],arrVerts[3+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
		}
	}//-y
	if(vCamPos.y<objBox.min.y)                                                           
	{                                                                              
		if(rExtrusionDir.y>=FLT_EPSILON)
		{//no extrusion
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[0],arrVerts[2],arrVerts[6],arrVerts[4]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
		}
//		else
		{//extrusion needed
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[0+8],arrVerts[2+8],arrVerts[6+8],arrVerts[4+8]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
			if(rExtrusionDir.x>0.f)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[4],arrVerts[0],arrVerts[0+8],arrVerts[4+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[2],arrVerts[6],arrVerts[6+8],arrVerts[2+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			if(rExtrusionDir.z>0.f)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[0+8],arrVerts[0],arrVerts[2],arrVerts[2+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[6],arrVerts[4],arrVerts[4+8],arrVerts[6+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
		}
	}
	//+z
	if(vCamPos.z>objBox.max.z)                                                           
	{                                                                              
		if(rExtrusionDir.z<=-FLT_EPSILON)
		{//no extrusion
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[5],arrVerts[4],arrVerts[6],arrVerts[7]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
		}
//		else
		{//extrusion needed
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[5+8],arrVerts[4+8],arrVerts[6+8],arrVerts[7+8]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
			if(rExtrusionDir.x>FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[5],arrVerts[4],arrVerts[4+8],arrVerts[5+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			if(rExtrusionDir.x<-FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[7+8],arrVerts[6+8],arrVerts[6],arrVerts[7]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			if(rExtrusionDir.y>FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[4],arrVerts[6],arrVerts[6+8],arrVerts[4+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			if(rExtrusionDir.y<-FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[5],arrVerts[5+8],arrVerts[7+8],arrVerts[7+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
		}
	}
	//-z
	if(vCamPos.z<objBox.min.z)                                                           
	{                                                                              
		if(rExtrusionDir.z>=FLT_EPSILON)
		{//no extrusion
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[0],arrVerts[1],arrVerts[3],arrVerts[2]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
		}
//		else
		{//extrusion needed
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[0+8],arrVerts[1+8],arrVerts[3+8],arrVerts[2+8]));
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
			if(rExtrusionDir.x>FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[0],arrVerts[1],arrVerts[1+8],arrVerts[0+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			if(rExtrusionDir.x<-FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[2+8],arrVerts[3+8],arrVerts[3],arrVerts[2]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			if(rExtrusionDir.y>FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[1],arrVerts[3],arrVerts[3+8],arrVerts[1+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
			else
			if(rExtrusionDir.y<-FLT_EPSILON)
			{
				COCClipper::Clip(PolyClipped,COCPoly(arrVerts[2],arrVerts[0],arrVerts[0+8],arrVerts[2+8]));
				if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
			}
		}
	}

#endif//PS3
	return false;
}

void COcculsionCuller::UpdateHierarchy()
{
#ifdef OC_HIERARCHICAL_ZBUFFER
	m_Dirty	=	false;
	m_CMin	=	m_CBuffer[0];
	for(uint32 y=0,pt=0;y<m_SizeY;y++)
	{
		TOCCexel MinC=m_CBuffer[pt++];
		for(uint32 x=1;x<(m_SizeX>>OC_ZEXELSKIP_SHIFT);x++)
		{
			TOCCexel	C=m_CBuffer[pt++];
			if(C>MinC)
				MinC=C;
		}
		m_YCBuffer[y]	=	MinC;
		if(MinC>m_CMin)
			m_CMin	=	MinC;
	}
#endif
}

bool COcculsionCuller::IsBoxVisible(const AABB objBox)
{
#ifdef OC_DEBUG_SHADOWVOLUMES
	if(GetCVars()->e_cbuffer_debug&32)
		return true;
#endif
	if(!m_TrisWritten || !GetCVars()->e_cbuffer)
		return true;

	// if camera is inside of box
  if(objBox.IsContainSphere(m_Camera.GetPosition(), -m_Camera.GetNearPlane()*2.f))
    return true;
/*
	if(m_bTreeIsReady && GetCVars()->e_cbuffer_test_mode&1 && m_pTree)
	{ // world space test
//		FRAME_PROFILER( "CB::IsBoxVisible_WS", GetSystem(), PROFILE_3DENGINE );

		static PodArray<CWSNode*> lstNodes; 
		lstNodes.Clear();

		bool bVisible = m_pTree->IsBoxVisible( objBox, this, lstNodes );

		if(bVisible || !lstNodes.Count())
			return bVisible;

		if(GetCVars()->e_cbuffer_tree_debug)
			for(int32 i=0; i<lstNodes.Count(); i++)
				lstNodes[i]->DrawDebug(this, true);
	}
*/

	if(!(GetCVars()->e_cbuffer_test_mode&2))
		return true;

#ifdef OC_HIERARCHICAL_ZBUFFER
	if(m_Dirty)
		UpdateHierarchy();
#endif

	// image space test
	m_ObjectsTested++;

	Vec4 arrVerts[8] = 
	{
		m_MatViewProj*Vec4(objBox.min.x,objBox.min.y,objBox.min.z,1.f),//0
		m_MatViewProj*Vec4(objBox.min.x,objBox.max.y,objBox.min.z,1.f),//1
		m_MatViewProj*Vec4(objBox.max.x,objBox.min.y,objBox.min.z,1.f),//2
		m_MatViewProj*Vec4(objBox.max.x,objBox.max.y,objBox.min.z,1.f),//3
		m_MatViewProj*Vec4(objBox.min.x,objBox.min.y,objBox.max.z,1.f),//4
		m_MatViewProj*Vec4(objBox.min.x,objBox.max.y,objBox.max.z,1.f),//5
		m_MatViewProj*Vec4(objBox.max.x,objBox.min.y,objBox.max.z,1.f),//6
		m_MatViewProj*Vec4(objBox.max.x,objBox.max.y,objBox.max.z,1.f)//7
	};

//	Point2d arrVerts[8];
	
	// transform into screen space
//	for(int32 i=0; i<8; i++)
//		arrVerts[i] = m_MatViewProj*arrVerts[i];

	Vec3 vCamPos = m_Camera.GetPosition();

	{
//			FRAME_PROFILER( "CB::IsBoxVisible_IS_Scan", GetSystem(), PROFILE_3DENGINE );

		// render only front faces of box
		COCPoly PolyClipped;
		if(vCamPos.x>objBox.max.x)
		{
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[2],arrVerts[3],arrVerts[7],arrVerts[6]));
//			Rasterize<true,false>(PolyClipped);
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
					return true;
		}                                                                              
		else if(vCamPos.x<objBox.min.x)
		{                                                                              
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[1],arrVerts[0],arrVerts[4],arrVerts[5]));
//			Rasterize<true,false>(PolyClipped);
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
		}                                                                              

		if(vCamPos.y>objBox.max.y)                                                           
		{                                                                              
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[3],arrVerts[1],arrVerts[5],arrVerts[7]));
//			Rasterize<true,false>(PolyClipped);
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
		}                                                                              
		else if(vCamPos.y<objBox.min.y)                                                           
		{                                                                              
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[0],arrVerts[2],arrVerts[6],arrVerts[4]));
//			Rasterize<true,false>(PolyClipped);
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
		}

		if(vCamPos.z>objBox.max.z)                                                           
		{                                                                              
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[5],arrVerts[4],arrVerts[6],arrVerts[7]));
//			Rasterize<true,false>(PolyClipped);
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
		}                                                                              
		else if(vCamPos.z<objBox.min.z)                                                           
		{                                                                              
			COCClipper::Clip(PolyClipped,COCPoly(arrVerts[0],arrVerts[1],arrVerts[3],arrVerts[2]));
//			Rasterize<true,false>(PolyClipped);
			if(PolyClipped.Count()>2 && RasterizeClip<false>(PolyClipped))
				return true;
		}
	}

	m_ObjectsTestedAndRejected++;
	return false;
}

void COcculsionCuller::DrawDebug(int32 nStep)
{ // project buffer to the screen
	nStep	%=	32;
  if(!nStep)
    return;

	if(GetCVars()->e_cbuffer_debug_freeze)
	{
		Matrix44	iVP;
		mathMatrixInverse(reinterpret_cast<f32*>(&iVP),reinterpret_cast<f32*>(&m_MatViewProj),CPUF_SSE);

		Vec3 vSize(5.f,5.f,5.f);
		const f32 Addf	=	2.f/static_cast<f32>(nStep);
		f32 yf	=	-1.f;
		const	f32	fSizeZ		=	1.f/static_cast<f32>(OC_ZEXEL_MAXVALUEHIGH-64);//bias to prevent overflows or sign missmatches with int2float2int
		for(int32 y=0; y<m_SizeY; y+=nStep,yf+=Addf)
		{
			f32 xf	=	-1.f;
			for(int32 x=0; x<m_SizeX; x+=nStep,xf+=Addf)
			{
				const f32 Z=1.f-m_ZBuffer[x+y*m_SizeX]*fSizeZ;
				Vec4 vPos4	=	iVP*Vec4(xf,yf,Z,1.f);
				vPos4	*=	1.f/vPos4.w;
				Vec3 vPos(vPos4.x,vPos4.y,vPos4.z);
				int32 Value	=	(int32)(Z*0xff);
				ColorB col(Value,Value,Value, (uint8)255);
				GetRenderer()->GetIRenderAuxGeom()->DrawAABB(AABB(vPos-vSize*Z, vPos+vSize*Z), nStep==1, col, eBBD_Faceted);
			}
		}
		return;
	}

	m_pRenderer->Set2DMode(true,m_SizeX,m_SizeY);
#ifdef 		OC_ZEROCYCLECLEAR
	if(nStep==1)
	{
		Vec3 vSize(.4f,.4f,.4f);
		if(nStep==1)
			vSize = Vec3(.5f,.5f,.5f);
		for(int32 y=0; y<m_SizeY; y+=nStep)
		for(int32 x=0; x<m_SizeX; x+=nStep)
		{
			Vec3 vPos((float)x,(float)(m_SizeY-y-1),0);
			vPos += Vec3(0.5f,-0.5f,0);
			int32 Value	=	m_ZBuffer[x+y*m_SizeX]>>(OC_ZEXELHIGH_2LOW+8);
			ColorB col(Value,Value,Value, (uint8)255);
			GetRenderer()->GetIRenderAuxGeom()->DrawAABB(AABB(vPos-vSize, vPos+vSize), nStep==1, col, eBBD_Faceted);
		}
		m_pRenderer->Set2DMode(false,m_SizeX,m_SizeY);
		return;
	}
	else
		nStep--;
#endif
#ifdef OC_HIERARCHICAL_ZBUFFER
	if(nStep==1)
	{
		Vec3 vSize(.5f*(1<<OC_ZEXELSKIP_SHIFT),.5f,.5f);
		for(int32 y=0; y<m_SizeY; y+=nStep)
			for(int32 x=0; x<m_SizeX; x+=(nStep<<OC_ZEXELSKIP_SHIFT))
			{
				Vec3 vPos((float)x,(float)(m_SizeY-y-1),0);
				vPos += Vec3(0.5f*(1<<OC_ZEXELSKIP_SHIFT),-0.5f,0);
				int32 Value =	m_CBuffer[(x+y*m_SizeX)>>OC_ZEXELSKIP_SHIFT];
				ColorB col(Value,Value,Value, (uint8)255);
				GetRenderer()->GetIRenderAuxGeom()->DrawAABB(AABB(vPos-vSize, vPos+vSize), 1/*TRUE*/, col, eBBD_Faceted);
			}
	}
	else
	{
		nStep--;
#else
	{
#endif

		Vec3 vSize(.4f,.4f,.4f);
		if(nStep==1)
			vSize = Vec3(.5f,.5f,.5f);
		for(int32 y=0; y<m_SizeY; y+=nStep)
		for(int32 x=0; x<m_SizeX; x+=nStep)
		{
			Vec3 vPos((float)x,(float)(m_SizeY-y-1),0);
			vPos += Vec3(0.5f,-0.5f,0);
			int32 Value	=	m_ZBuffer[x+y*m_SizeX]==OC_ZEXEL_CLEARVALUEHIGH?0:(static_cast<int32>((m_ZBuffer[x+y*m_SizeX])*GetCVars()->e_cbuffer_debug_draw_scale)>>OC_ZEXELHIGH_2LOW)&0xff;
			ColorB col(Value,Value,Value, (uint8)255);
			GetRenderer()->GetIRenderAuxGeom()->DrawAABB(AABB(vPos-vSize, vPos+vSize), nStep==1, col, eBBD_Faceted);
		}
	}


  m_pRenderer->Set2DMode(false,m_SizeX,m_SizeY);
}

void COcculsionCuller::GetMemoryUsage(ICrySizer * pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "CoverageBuffer");
	pSizer->AddObject(this, sizeof(COcculsionCuller));
#if !defined(PS3)
	pSizer->AddContainer(m_ZBuffer);
	pSizer->AddContainer(m_VertexCache);
	pSizer->AddContainer(m_ProjectedVertexCacheX);
	pSizer->AddContainer(m_ProjectedVertexCacheY);
	pSizer->AddContainer(m_ProjectedVertexCacheZ);
#endif
#ifdef OC_HIERARCHICAL_ZBUFFER
	#if !defined(PS3)
		pSizer->AddContainer(m_CBuffer);
		pSizer->AddContainer(m_YCBuffer);
	#endif
#endif
//	pSizer->AddContainer(m_VertexClipCache);
}

void COcculsionCuller::UpdateDepthTree()
{
}
