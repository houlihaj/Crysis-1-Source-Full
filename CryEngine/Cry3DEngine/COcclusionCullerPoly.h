////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   COcclusionCuller.h
//  Version:     v1.00
//  Created:     13/8/2006 by Michael Kopietz
//  Compilers:   Visual Studio.NET
//  Description: Occlusion Culler Poly
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef _COCCLUSIONCULLERPOLY_H
#define _COCCLUSIONCULLERPOLY_H

const uint32		OC_MAX_VERTPERPOLY	=	32;

class COCPoly
{
protected:
	Vec4									m_Vertices[OC_MAX_VERTPERPOLY];
	uint									m_Count;
public:
	ILINE 								COCPoly():
												m_Count(0)
												{
												}
	ILINE 								COCPoly(const COCPoly& rPoly):
												m_Count(rPoly.m_Count)
												{
													for(uint	a=0;a<m_Count;a++)
														m_Vertices[a]	=	rPoly.m_Vertices[a];
												}

	ILINE 								COCPoly(const Vec4& rV1,const Vec4& rV2,const Vec4& rV3):
												m_Count(3)
												{
													m_Vertices[0]	=	rV1;
													m_Vertices[1]	=	rV2;
													m_Vertices[2]	=	rV3;
												}
	ILINE 								COCPoly(const Vec4& rV1,const Vec4& rV2,const Vec4& rV3,const Vec4& rV4):
												m_Count(4)
												{
													m_Vertices[0]	=	rV1;
													m_Vertices[1]	=	rV2;
													m_Vertices[2]	=	rV3;
													m_Vertices[3]	=	rV4;
												}


	ILINE uint						Count()	const	{return m_Count;}
	ILINE void						Clear()	{m_Count	=	0;}
	ILINE const Vec4&			Vec(uint Pt)	const	{assert(Pt<m_Count);return m_Vertices[Pt];}
	ILINE Vec4&						Add(){assert(m_Count+1<=OC_MAX_VERTPERPOLY);return m_Vertices[m_Count++];}
	ILINE const Vec4*			Vecs()	const	{return m_Vertices;}

};

#endif 
