////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   COcclusionCuller.h
//  Version:     v1.00
//  Created:     13/8/2006 by Michael Kopietz
//  Compilers:   Visual Studio.NET
//  Description: Occlusion Culler
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef _OCCLUSIONCULLER_H
#define _OCCLUSIONCULLER_H

#include "COcclusionCullerPoly.h"

typedef ushort tdHMPixel;

#pragma warning(disable: 4799)

#if defined(_CPU_SSE)
	#define OC_SSE
	#define OC_COHENSUTHERLAND
#endif
#define OC_32BIT
//#define OC_DEBUG_OFOUTBOUNDRENDER
//#define OC_DEBUG_SHADOWVOLUMES
//#define OC_ZEROCYCLECLEAR
#define OC_GRADIENT_TRUNCATE
//#define OC_HIERARCHICAL_ZBUFFER
//#define OC_LUTDIVISION


#ifdef OC_LUTDIVISION
	#define OC_DIVI32(X,Y) (((X)>>8)*m_IDivLut[Y+255])
//	#define OC_DIVI32(X,Y)  static_cast<int32>(f32(X)/(f32)(Y))
//	#define OC_DIVU32S14(X,Y) (((X)>>14)*m_UDivLut[Y])
//#ifdef OC_SSE
//	#define OC_DIVU32S14(X,Y) _mm_cvtss_si32(_mm_mul_ss(_mm_cvtsi32_ss(__m128(),X),_mm_rcp_ss(_mm_cvtsi32_ss(__m128(),Y))))
//#else
//	#define OC_DIVU32S14(X,Y) static_cast<int32>(f32(X)/(f32)(Y))
//#endif
	#define OC_DIVU32S14(X,Y) ((X)/(Y))
#else
	#define OC_DIVI32(X,Y) ((X)/(Y))
	#define OC_DIVU32S14(X,Y) ((X)/(Y))
#endif

#ifdef OC_GRADIENT_TRUNCATE
	#define OC_GRADIENT(X,D)	((X)/(int16)(D))
#else
	#define OC_GRADIENT(X,D)	(( (X)+((D)>>1) )/(int16)(D))
#endif

const				uint32					OC_FIXPS									=	14;
const				int32						OC_FIXPHALF								=	1<<(OC_FIXPS-2);

#ifdef  OC_ZEROCYCLECLEAR
	typedef			int32					TOCZexel;
	const				int32					OC_ZEXELHIGH_SHIFT			=	0;
	const				int32					OC_ZEXELHIGH_2LOW				=	16;
	const				TOCZexel			OC_ZEXEL_CLEARVALUEHIGH	=	0x7fffffff;
	const				TOCZexel			OC_ZEXEL_MAXVALUEHIGH		=	0xffffff;
	#define ZLowMasking(X)			((((X)<(m_ClearCounter|OC_ZEXEL_MAXVALUEHIGH)?(X):(m_ClearCounter|OC_ZEXEL_MAXVALUEHIGH))>>(OC_ZEXELHIGH_2LOW+OC_ZEXELHIGH_SHIFT))&0xff)
#else
#ifdef  OC_32BIT
	typedef			int32					TOCZexel;
	const				int32					OC_ZEXELHIGH_SHIFT			=	0;
	const				int32					OC_ZEXELHIGH_2LOW				=	23;
	const				TOCZexel			OC_ZEXEL_CLEARVALUEHIGH	=	0x7fffffff;
	const				TOCZexel			OC_ZEXEL_MAXVALUEHIGH		=	OC_ZEXEL_CLEARVALUEHIGH;
	#define ZLowMasking(X)  ((X)>>(OC_ZEXELHIGH_2LOW+OC_ZEXELHIGH_SHIFT))
#if defined(_CPU_SSE) && !defined(PS3)
		#define OC_MMX
	#endif
#else
	typedef			int16					TOCZexel;
	const				int32					OC_ZEXELHIGH_SHIFT			=	16;
	const				int32					OC_ZEXELHIGH_2LOW				=	7;
	const				TOCZexel			OC_ZEXEL_CLEARVALUEHIGH	=	0x7fff;
	const				TOCZexel			OC_ZEXEL_MAXVALUEHIGH		=	OC_ZEXEL_CLEARVALUEHIGH;
	#define ZLowMasking(X)  ((X)>>(OC_ZEXELHIGH_2LOW+OC_ZEXELHIGH_SHIFT))
#endif
#endif

	typedef			uint8					TOCCexel;
	const				int32					OC_ZEXELSKIP_COUNT			=	8;
	const				int32					OC_ZEXELSKIP_SHIFT			=	3;
	const				int32					OC_ZEXELLOW_MASK				=	((1<<OC_ZEXELSKIP_SHIFT)-1);
	const				TOCCexel			OC_ZEXEL_CLEARVALUELOW	=	(~(TOCCexel)0);

class CCamera;


class COcculsionCuller : public Cry3DEngineBase
{
protected:
	uint32										m_SizeShift;
	int32											m_SizeXMax;
	uint32										m_SizeX;
	uint32										m_SizeY;
	uint32										m_SizeZ;
	uint32										m_SizeZMask;
	uint32										m_SizeC;
	f32												m_InvSize;
	f32												m_TopLeftX;
	f32												m_TopLeftY;
	f32												m_BottomRightX;
	f32												m_BottomRightY;
#ifdef OC_ZEROCYCLECLEAR
	uint32										m_ClearCounter;
#endif


	f32												m_FrameTime;
	f32												m_FixedZFar;
	f32												m_ZNearInMeters;
	f32												m_ZFarInMeters;
	uint32										m_TrisWritten;
	uint32										m_ObjectsWritten;
	uint32										m_TrisTested;
	uint32										m_ObjectsTested;
	uint32										m_ObjectsTestedAndRejected;

	CCamera										m_Camera;
	Vec4											m_EyePos;
#ifdef OC_HIERARCHICAL_ZBUFFER
	#if defined(PS3)
	TOCCexel*									m_YCBuffer;
	TOCCexel*									m_CBuffer;
	#else
		PodArray<TOCCexel>			m_YCBuffer;
		PodArray<TOCCexel>			m_CBuffer;
	#endif
#endif
	TOCCexel									m_CMin;
	TOCZexel									m_Nearest;
	bool											m_Dirty;
#if defined(PS3)
	uint32										m_LastSizeZ;
	uint32										m_CurVertexCacheSize;
	TOCZexel*									m_ZBuffer;
	Vec4*											m_VertexCache;
	#ifdef OC_COHENSUTHERLAND
		uint32*									m_VertexClipOutcodes;
	#endif
	uint32										m_CurProjectedVertexCount;
	int32*										m_ProjectedVertexCacheX;
	int32*										m_ProjectedVertexCacheY;
	TOCZexel*									m_ProjectedVertexCacheZ;
#else
	PodArray<TOCZexel>				m_ZBuffer;
	PodArray<Vec4>						m_VertexCache;
	#ifdef OC_COHENSUTHERLAND
		PodArray<uint32>				m_VertexClipOutcodes;
	#endif
	PodArray<int32>						m_ProjectedVertexCacheX;
	PodArray<int32>						m_ProjectedVertexCacheY;
	PodArray<TOCZexel>				m_ProjectedVertexCacheZ;
#endif
//	PodArray<uint8>						m_VertexClipCache;
	Matrix44									m_MatProj;
	Matrix44									m_MatView;
	Matrix44									m_MatViewProj;
	bool											m_OutdoorVisible;
#ifdef OC_LUTDIVISION
	int32											m_IDivLut[256*2];
	int32											m_UDivLut[256];
#endif


	bool											RayBox(const AABB objBox,const Vec3& Origin,const Vec3& Dir)const;

	bool											IsBoxVisible_OCCLUDER(const AABB objBox);
	bool											IsBoxVisible_OCEAN(const AABB objBox);
	bool											IsBoxVisible_OCCELL(const AABB objBox);
	bool											IsBoxVisible_OCCELL_OCCLUDER(const AABB objBox);
	bool											IsBoxVisible_OBJECT(const AABB objBox);
	bool											IsBoxVisible_OBJECT_TO_LIGHT(const AABB objBox);
	bool											IsBoxVisible_TERRAIN_NODE(const AABB objBox);
	bool											IsBoxVisible_PORTAL(const AABB objBox);
	bool											IsBoxVisible(const AABB objBox);

	//#define OC_DEBUG_SHADOWVOLUMES
	void											DebugLine(int32 X1,int32 Y1,int32 Z1,int32 X2,int32 Y2,int32 Z2)
														{
															if(abs(X2-X1)>1 || abs(Y2-Y1)>1)
															{
																DebugLine(X1,Y1,Z1,(X1+X2)>>1,(Y1+Y2)>>1,(Z1+Z2)>>1);
																DebugLine(X2,Y2,Z2,(X1+X2)>>1,(Y1+Y2)>>1,(Z1+Z2)>>1);
															}
															else
															{
																if(X1>=0 && X1<(int32)m_SizeX && Y1>=0 && Y1<(int32)m_SizeY)
																	m_ZBuffer[X1+Y1*m_SizeX]	=	(X1^Y1)&1?Z1:OC_ZEXEL_CLEARVALUEHIGH-Z1;
//																	m_ZBuffer[X1+Y1*m_SizeX]	=	OC_ZEXEL_CLEARVALUEHIGH-10;
//																m_ZBuffer[X1+Y1*m_SizeX]	=	(X1^Y1^X2^Y2)&1?OC_ZEXEL_CLEARVALUEHIGH-1:1;
															}
														}

#ifdef OC_HIERARCHICAL_ZBUFFER
	template<bool WRITE>
	bool											HLine(int32 PLl,int32 PRl,int32 PLz,int32 PRz)
														{
															if(PLl>=PRl)	//frontfacing and at least one pixel to set? else reject it
																return false;

															const int32 ADDz=OC_GRADIENT(PRz-PLz,PRl-PLl);

															//prefix
															const int32 PLl8Start=PLl>>OC_ZEXELSKIP_SHIFT;
															while(PLl<PRl && (PLl&OC_ZEXELLOW_MASK))
															{
																if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl])
																	return true;
																if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl])
																		m_ZBuffer[PLl]=PLz>>OC_ZEXELHIGH_SHIFT;
																PLl++;
																PLz+=ADDz;
															}

															if(WRITE)	//update CBuffer
															{
																const int32 a=PLl8Start<<OC_ZEXELSKIP_SHIFT;
																TOCZexel Z0	=	m_ZBuffer[a];
																TOCZexel Z1	=	m_ZBuffer[a+1];
																TOCZexel Z2	=	m_ZBuffer[a+2];
																TOCZexel Z3	=	m_ZBuffer[a+3];
																TOCZexel Z4	=	m_ZBuffer[a+4];
																TOCZexel Z5	=	m_ZBuffer[a+5];
																TOCZexel Z6	=	m_ZBuffer[a+6];
																TOCZexel Z7	=	m_ZBuffer[a+7];
																if(Z1>Z0)	Z0	=	Z1;
																if(Z3>Z2)	Z2	=	Z3;
																if(Z5>Z4)	Z4	=	Z5;
																if(Z7>Z6)	Z6	=	Z7;

																if(Z2>Z0)	Z0	=	Z2;
																if(Z6>Z4)	Z4	=	Z6;

																if(Z4>Z0)	Z0	=	Z4;
																m_CBuffer[PLl8Start]	=	ZLowMasking(Z0);
															}

															if(PLl==PRl)
																return false;


															//body
															assert(!(PLl&OC_ZEXELLOW_MASK));
															int32 PLl8=PLl>>OC_ZEXELSKIP_SHIFT;
															const int32 PRl8=(PRl>>OC_ZEXELSKIP_SHIFT);
															const int32 ADDz7=(ADDz<<OC_ZEXELSKIP_SHIFT)-ADDz;

															if(ADDz7>0)
															{	//increasing
																while(PLl8<PRl8)
																{
																	if(ZLowMasking(PLz+ADDz7)<=m_CBuffer[PLl8])
																	{
																		if(WRITE)
																			m_CBuffer[PLl8]=ZLowMasking(PLz+ADDz7);

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl])	m_ZBuffer[PLl]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+1])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+1])	m_ZBuffer[PLl+1]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+2])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+2])	m_ZBuffer[PLl+2]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+3])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+3])	m_ZBuffer[PLl+3]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+4])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+4])	m_ZBuffer[PLl+4]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+5])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+5])	m_ZBuffer[PLl+5]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+6])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+6])	m_ZBuffer[PLl+6]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+7])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+7])	m_ZBuffer[PLl+7]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;
																	}
																	else
																		PLz+=ADDz7+ADDz;

																	PLl+=(1<<OC_ZEXELSKIP_SHIFT);
																	PLl8++;
																}
															}
															else
															{	//decreasing
																while(PLl8<PRl8)
																{
																	if(ZLowMasking(PLz)<=m_CBuffer[PLl8])
																	{
																		if(WRITE)
																			m_CBuffer[PLl8]=ZLowMasking(PLz);

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl])	m_ZBuffer[PLl]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+1])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+1])	m_ZBuffer[PLl+1]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+2])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+2])	m_ZBuffer[PLl+2]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+3])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+3])	m_ZBuffer[PLl+3]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+4])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+4])	m_ZBuffer[PLl+4]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+5])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+5])	m_ZBuffer[PLl+5]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+6])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+6])	m_ZBuffer[PLl+6]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;

																		if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl+7])	return true;
																		if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl+7])	m_ZBuffer[PLl+7]=PLz>>OC_ZEXELHIGH_SHIFT;
																		PLz+=ADDz;
																	}
																	else
																		PLz+=ADDz7+ADDz;

																	PLl+=(1<<OC_ZEXELSKIP_SHIFT);
																	PLl8++;
																}
															}
															assert(!(PLl&OC_ZEXELLOW_MASK));
															assert((PLl>=PRl)==(!(PRl&OC_ZEXELLOW_MASK)));

															if(PLl>=PRl || ZLowMasking(min(PLz,PLz+ADDz7))>m_CBuffer[PLl8])
																return false;

															//postfix
															while(PLl<PRl)
															{
																if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl])
																	return true;
																if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl])
																		m_ZBuffer[PLl]=PLz>>OC_ZEXELHIGH_SHIFT;
																PLl++;
																PLz+=ADDz;
															}

															if(WRITE)	//update CBuffer
															{
																const int32 a=PLl&~OC_ZEXELLOW_MASK;
																m_CBuffer[PLl8Start]	=	ZLowMasking(max(max(max(m_ZBuffer[a],m_ZBuffer[a+1]),
																																						max(m_ZBuffer[a+2],m_ZBuffer[a+3])),
																																				max(max(m_ZBuffer[a+4],m_ZBuffer[a+5]),
																																						max(m_ZBuffer[a+6],m_ZBuffer[a+7]))));
															}
															return false;
														}
#endif

	template<class T>
	static ILINE uint32				TopMost(const T& rTable,const uint32 VCount)
														{
															uint32 Pt	=	0;
															for(uint32 a=1;a<VCount;a++)
																if(rTable[a]<rTable[Pt])
																	Pt	=	a;
															return Pt;
														}
	template<class T>
	static ILINE uint32				BottomMostValue(const T& rTable,const uint32 VCount)
														{
															int32 Value =	rTable[0];
															for(uint32 a=1;a<VCount;a++)
																if(rTable[a]>Value)
																	Value	=	rTable[a];
															return Value;
														}

#ifdef OC_MMX
	void											RasterizeWriteMMX(int32 X1,int32 X2,int32 X3,int32 Y1,int32 Y2,int32 Y3,int32 Z1,int32 Z2,int32 Z3);
#endif
	template<bool WRITE,class T1, class T2>
	bool											Rasterize(const T1&			rVerticesX,
																			const T1&			rVerticesY,
																			const T2&			rVerticesZ,
																			const	uint32	VCount)
														{
#ifdef OC_DEBUG_SHADOWVOLUMES
															if(!WRITE && GetCVars()->e_cbuffer_debug&64)
															{
/*
																int32 X[4],Y[4],Z[4];//flip faces
																X[0]	=	rVerticesX[1];
																X[1]	=	rVerticesX[0];
																X[2]	=	rVerticesX[3];
																X[3]	=	rVerticesX[2];
																Y[0]	=	rVerticesY[1];
																Y[1]	=	rVerticesY[0];
																Y[2]	=	rVerticesY[3];
																Y[3]	=	rVerticesY[2];
																Z[0]	=	rVerticesZ[1];
																Z[1]	=	rVerticesZ[0];
																Z[2]	=	rVerticesZ[3];
																Z[3]	=	rVerticesZ[2];
																//Rasterize<true>(X,Y,Z,VCount);*/
																//Rasterize<true>(rVerticesX,rVerticesY,rVerticesZ,VCount);
																																	for(int32 a=0,b=VCount-1;a<VCount;b=a++)
																																		DebugLine(m_ProjectedVertexCacheX[a],m_ProjectedVertexCacheY[a],m_ProjectedVertexCacheZ[a],
																																							m_ProjectedVertexCacheX[b],m_ProjectedVertexCacheY[b],m_ProjectedVertexCacheZ[b]);

															}

#endif
															if(WRITE)
															{
																for(uint32 a=0;a<VCount;a++)
																{
																	const int32 ZValue	=	rVerticesZ[a];
																	if(ZValue<m_Nearest)
																		m_Nearest	=	ZValue;
																}
															}
															if(!WRITE)
															{
																m_TrisTested++;
#ifdef OC_HIERARCHICAL_ZBUFFER
																bool	Pass=false;
#endif
																bool	Nearer=true;
																for(uint32 a=0;a<VCount;a++)
																{
																	const int32 ZValue	=	rVerticesZ[a];
#ifdef OC_HIERARCHICAL_ZBUFFER
																	Pass|=(ZLowMasking(ZValue)<=m_CMin);
#endif
																	Nearer&=(ZValue<m_Nearest);
																}
#ifdef OC_HIERARCHICAL_ZBUFFER
																if(!Pass)
																	return false; 
#endif
																if(Nearer)
																	return true;
															}
															const int32 MAX_X	=	(m_SizeX<<OC_FIXPS)-1;
															int32 LPt,RPt;
															int32 LX1,LY1,LX2,LY2;
															int32 RX1,RY1,RX2,RY2;
															int32 LZ1,RZ1,RZ2,LZ2;
															int32 LAddX,LAddZ,RAddX,RAddZ;

															LPt	=	RPt	=	TopMost(rVerticesY,VCount);
															if(rVerticesY[LPt]==BottomMostValue(rVerticesY,VCount))
																return false;

															LY1	=	RY1	=	LY2	=	RY2	=	rVerticesY[LPt];
//															LX1	=	RX1	=	LX2	=	RX2	=	((LY1<<m_SizeShift)+min(m_SizeXMax,rVerticesX[LPt]))<<OC_FIXPS;
															LX1	=	LX2	=	(LY1<<(m_SizeShift+OC_FIXPS))+max((rVerticesX[LPt]<<OC_FIXPS)-OC_FIXPHALF,int32(0));
															RX1	=	RX2	=	(LY1<<(m_SizeShift+OC_FIXPS))+min((rVerticesX[LPt]<<OC_FIXPS)+OC_FIXPHALF,int32(m_SizeX<<OC_FIXPS));
															LZ1	=	RZ1	=	LZ2	=	RZ2	=	rVerticesZ[LPt]<<OC_ZEXELHIGH_SHIFT;
//															LX1	=	max(LX1-OC_FIXPHALF,0);
//															LX2	=	max(LX2-OC_FIXPHALF,0);
//															RX1	+=	OC_FIXPHALF;
//															RX2	+=	OC_FIXPHALF;

															while(true) 
															{
																if(LY1==LY2)
																{
																	do
																	{
																		LPt--;
																		LX1	=	LX2;
																		LZ1	=	LZ2;
																		if(LPt<0)
																			LPt	=	VCount-1;
																		LY2	=	rVerticesY[LPt];
//																		LX2	=	max((((LY2<<m_SizeShift)+min(rVerticesX[LPt],m_SizeXMax))<<OC_FIXPS)-OC_FIXPHALF,0);
																		LX2	=	(LY2<<(m_SizeShift+OC_FIXPS))+max((rVerticesX[LPt]<<OC_FIXPS)-OC_FIXPHALF,int32(0));
																		LZ2	=	rVerticesZ[LPt]<<OC_ZEXELHIGH_SHIFT;
																	}while(LY1==LY2);
																	if(LY1>LY2)
																		return false;
																	LAddX	=	OC_GRADIENT(LX2-LX1,LY2-LY1);
																	LAddZ	=	OC_GRADIENT(LZ2-LZ1,LY2-LY1);
																}
																if(RY1==RY2)
																{
																	do
																	{
																		RPt++;
																		RX1	=	RX2;
																		RZ1	=	RZ2;
																		if(RPt>=VCount)
																			RPt	=	0;
																		RY2	=	rVerticesY[RPt];
//																		RX2	=	(((RY2<<m_SizeShift)+min(rVerticesX[RPt],m_SizeXMax))<<OC_FIXPS)+OC_FIXPHALF;
																		RX2	=	(RY2<<(m_SizeShift+OC_FIXPS))+min((rVerticesX[RPt]<<OC_FIXPS)+OC_FIXPHALF,int32(m_SizeX<<OC_FIXPS));
																		RZ2	=	rVerticesZ[RPt]<<OC_ZEXELHIGH_SHIFT;
																	}while(RY1==RY2);
																	if(RY1>RY2)
																		return false;
																	RAddX	=	OC_GRADIENT(RX2-RX1,RY2-RY1);
																	RAddZ	=	OC_GRADIENT(RZ2-RZ1,RY2-RY1);
																}
																const int32 MaxY	=	min(LY2,RY2);
																while(LY1<MaxY)
																{
#ifndef OC_HIERARCHICAL_ZBUFFER
																	int32 PLl=LX1>>OC_FIXPS;
																	const int32 PRl=RX1>>OC_FIXPS;
																	if(PLl<PRl)
																	{
																		if(!WRITE && (LZ1>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl])
																			return true;
																		if(WRITE && (LZ1>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl])
																			m_ZBuffer[PLl]=LZ1>>OC_ZEXELHIGH_SHIFT;
																		if(PLl+1<PRl)
																		{
																			int32 PLz=LZ1;
																			const int32 ADDz=OC_DIVI32(RZ1-PLz,PRl-PLl);
																			PLl++;
																			PLz+=ADDz;
																			do
																			{
																				if(!WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<=m_ZBuffer[PLl])
																					return true;
																				if(WRITE && (PLz>>OC_ZEXELHIGH_SHIFT)<m_ZBuffer[PLl])
																					m_ZBuffer[PLl]=PLz>>OC_ZEXELHIGH_SHIFT;
																				PLl++;
																				PLz+=ADDz;
																			}while(PLl<PRl);
																		}
																	}
#else
																	if(HLine<WRITE>(LX1>>OC_FIXPS,RX1>>OC_FIXPS,LZ1,RZ1))
																		return true;
#endif
																	LY1++;
																	LX1	+=	LAddX;
																	LZ1	+=	LAddZ;
																	RX1	+=	RAddX;
																	RZ1	+=	RAddZ;
																}
																RY1	=	LY1;
															};
															return false;
														}

//	bool											RasterizeClipfalse(const COCPoly& PolyClipped)
//														{
//															return RasterizeClip<false>(PolyClipped);
//														}
	template<bool WRITE>
	bool											RasterizeClip(const COCPoly& rPoly)
														{
															const int32	VCount	=	rPoly.Count();
															ProjectVertices(rPoly.Vecs(),VCount);
															return Rasterize<WRITE>(	m_ProjectedVertexCacheX,
																														m_ProjectedVertexCacheY,
																														m_ProjectedVertexCacheZ,
																														VCount);
														}

	void											Clear();
	template<class T>
	void											ProjectVertices(const T& rVertices,const uint32	VCount)
														{
															int32*		pProjectedVertexCacheX = (int32*)&m_ProjectedVertexCacheX[0];
															int32*		pProjectedVertexCacheY = (int32*)&m_ProjectedVertexCacheY[0];
															TOCZexel*	pProjectedVertexCacheZ = &m_ProjectedVertexCacheZ[0];
#ifdef OC_SSE
															_mm_prefetch((const char*)pProjectedVertexCacheX,_MM_HINT_T0);
															_mm_prefetch((const char*)pProjectedVertexCacheY,_MM_HINT_T0);
															_mm_prefetch((const char*)pProjectedVertexCacheZ,_MM_HINT_T0);
#endif
															const f32 fSizeX		=	static_cast<f32>(static_cast<int32>(m_SizeX)-1);//casting uint2int is for free, int2float is faster then uint2float
															const f32 fSizeY		=	static_cast<f32>(static_cast<int32>(m_SizeY)-1);
//															const	f32	fSizeZ		=	static_cast<f32>(OC_ZEXEL_MAXVALUEHIGH-64);//bias to prevent overflows or sign missmatches with int2float2int
															const	f32	fSizeZ		=	static_cast<f32>(OC_ZEXEL_MAXVALUEHIGH-10*1024*1024);//new bias to prevent overflows or sign missmatches with int2float2int with SSE optimization (see "Multiple floating point traps")

															for(uint32 a=0;a<VCount;a++)
															{
#ifdef OC_SSE
																_mm_prefetch((const char*)(&pProjectedVertexCacheX[a])+64,_MM_HINT_T0);
																_mm_prefetch((const char*)(&pProjectedVertexCacheY[a])+64,_MM_HINT_T0);
																_mm_prefetch((const char*)(&pProjectedVertexCacheZ[a])+64,_MM_HINT_T0);
#endif
																const Vec4&	rVec=	rVertices[a];
																const f32 InvW	=	1.f/rVec.w;
																pProjectedVertexCacheX[a]	=	static_cast<int32>((rVec.x*InvW*0.5f+0.5f)*fSizeX+0.5f);
																pProjectedVertexCacheY[a]	=	static_cast<int32>((rVec.y*InvW*0.5f+0.5f)*fSizeY+0.5f);
//																float Z	=	min(max(rVec.z,0.f)*InvW,1.f)*fSizeZ;
//																float Z	=	(1.f-min(max(rVec.z,rVec.w)/max(rVec.z,FLT_EPSILON)-1.f,1.f))*fSizeZ;	//LINEAR SPACE INTERPOLATION
																float Z	=	(2.f-min((rVec.w/max(rVec.z,FLT_EPSILON)),2.f))*fSizeZ;	//LINEAR SPACE INTERPOLATION
#ifdef  OC_ZEROCYCLECLEAR
																pProjectedVertexCacheZ[a]	=	static_cast<int32>(Z)|m_ClearCounter;
#else
																pProjectedVertexCacheZ[a]	=	static_cast<int32>(Z);
#endif

#ifdef OC_DEBUG_OFOUTBOUNDRENDER
																if(! (m_ProjectedVertexCacheX[a]>=0 && m_ProjectedVertexCacheY[a]>=0 &&
																			m_ProjectedVertexCacheZ[a]>=0 && m_ProjectedVertexCacheX[a]<=m_SizeX &&
																      m_ProjectedVertexCacheY[a]<=m_SizeY))
																{
																	if(m_ProjectedVertexCacheX[a]<0)
																	{
																		m_ProjectedVertexCacheX[a]	=	0;
																		m_ProjectedVertexCacheZ[a]	=	m_ClearCounter;
																	}
																	if(m_ProjectedVertexCacheX[a]>=m_SizeX)
																	{
																		m_ProjectedVertexCacheX[a]	=	m_SizeX-1;
																		m_ProjectedVertexCacheZ[a]	=	m_ClearCounter;
																	}
																	if(m_ProjectedVertexCacheY[a]<0)
																	{
																		m_ProjectedVertexCacheY[a]	=	0;
																		m_ProjectedVertexCacheZ[a]	=	m_ClearCounter;
																	}
																	if(m_ProjectedVertexCacheY[a]>=m_SizeY)
																	{
																		m_ProjectedVertexCacheY[a]	=	m_SizeY-1;
																		m_ProjectedVertexCacheZ[a]	=	m_ClearCounter;
																	}
																	if(m_ProjectedVertexCacheZ[a]<0)
																		m_ProjectedVertexCacheZ[a]	=	m_ClearCounter;
																}
#else
//																assert(m_ProjectedVertexCacheX[a]>=0);
//																assert(m_ProjectedVertexCacheY[a]>=0);
//																assert(m_ProjectedVertexCacheZ[a]>=0);
//																assert(m_ProjectedVertexCacheX[a]<=m_SizeX);
//																assert(m_ProjectedVertexCacheY[a]<=m_SizeY);
#endif
																if(pProjectedVertexCacheX[a]<0) //to make it deterministic, as requested by martin
																	pProjectedVertexCacheX[a]	=	0;
																else
																if(pProjectedVertexCacheX[a]>m_SizeX) 
																	pProjectedVertexCacheX[a]	=	m_SizeX;
																if(pProjectedVertexCacheY[a]<0)
																	pProjectedVertexCacheY[a]	=	0;
																else
																if(pProjectedVertexCacheY[a]>m_SizeY)
																	pProjectedVertexCacheY[a]	=	m_SizeY;
#ifdef  OC_ZEROCYCLECLEAR
																assert((m_ProjectedVertexCacheZ[a]^m_ClearCounter)<=OC_ZEXEL_MAXVALUEHIGH);
#else
																assert(m_ProjectedVertexCacheZ[a]<=OC_ZEXEL_MAXVALUEHIGH);
#endif
															}
														}

	void											UpdateHierarchy();
public:
														COcculsionCuller();

	// start new frame
	void											BeginFrame(const CCamera& rCam);

	// render into buffer
	void											AddRenderMesh(IRenderMesh * pRM, Matrix34* pTranRotMatrix, IMaterial * pMaterial, bool bOutdoorOnly, bool bCompletelyInFrustum,bool bNoCull);
	void											AddHeightMap(const struct SRangeInfo & m_rangeInfo, float X1, float Y1, float X2, float Y2);

	// test visibility
	bool											IsObjectVisible(const AABB objBox, EOcclusionObjectType eOcclusionObjectType, float fDistance);

	bool											IsShadowcasterVisible(const AABB& objBox,Vec3 rExtrusionDir);
	// draw content to the screen for debug
	void											DrawDebug(int32 nStep);

	// update tree
	void											UpdateDepthTree();

	// return current camera
	const CCamera&						GetCamera() { return m_Camera; }

	//set the scissor for clipping (0.f|0.f to 1.f|1.f)
	void											Scissor(f32 TopLeftX,f32 TopLeftY,f32 BottomRightX,f32 BottomRightY)
														{
															m_TopLeftX			=	TopLeftX*2.f-1.f;
															m_TopLeftY			=	TopLeftY*2.f-1.f;
															m_BottomRightX	=	BottomRightX*2.f-1.f;
															m_BottomRightY	=	BottomRightY*2.f-1.f;
														}


	void											GetMemoryUsage(ICrySizer * pSizer);

	void											SetFrameTime(f32 fTime) { m_FrameTime = fTime; }
	f32												GetFrameTime() { return m_FrameTime; }
	bool											IsOutdooVisible(){return m_OutdoorVisible;}
	void											TrisWritten(int32){}
	int32											TrisWritten()const{return m_TrisWritten;}
	void											ObjectsWritten(int32){}
	int32											ObjectsWritten()const{return m_ObjectsWritten;}
	int32											TrisTested()const{return m_TrisTested;}
	int32											ObjectsTested()const{return m_ObjectsTested;}
	int32											ObjectsTestedAndRejected()const{return m_ObjectsTestedAndRejected;}
	int32											SelRes()const{return m_SizeX;}
	float											FixedZFar()const{return m_FixedZFar;}
	float											GetZNearInMeters()const{return m_ZNearInMeters;}
	float											GetZFarInMeters()const{return m_ZFarInMeters;}

};

#endif 
