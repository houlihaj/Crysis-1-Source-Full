////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   COcclusionCullerClipper.h
//  Version:     v1.00
//  Created:     29/8/2006 by Michael Kopietz
//  Compilers:   Visual Studio.NET
//  Description: Occlusion Culler Clipper
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef _COCCLUSIONCULLERCLIPPERPIMPL_H
#define _COCCLUSIONCULLERCLIPPERPIMPL_H

#include "COcclusionCullerPoly.h"

enum EOC_COMPERATOR
{
	EOCC_LESS,
	EOCC_GREATHER,
	EOCC_GREATHERTHANZERO,
};

template<UINT IDX=0,EOC_COMPERATOR COMPERATOR=EOCC_LESS>
class COCClipperPimpl
{
public:
	ILINE static bool		Included(const Vec4& rV,f32 ClipValue)
											{
												const f32 Value	=	rV[IDX];
												if(COMPERATOR==EOCC_GREATHERTHANZERO)
													return Value>0.f;
												if(COMPERATOR==EOCC_GREATHER)
													return rV.w*ClipValue<=Value;
												return Value<=rV.w*ClipValue;
											}

	ILINE static Vec4		Clip(const Vec4& rV1,const Vec4& rV2,f32 ClipValue)
											{
												const f32 f1	=	(COMPERATOR==EOCC_GREATHERTHANZERO?0.f:rV1.w*ClipValue)-rV1[IDX];
												const f32 f2	=	(COMPERATOR==EOCC_GREATHERTHANZERO?0.f:rV2.w*ClipValue)-rV2[IDX];
												const f32 lrp	=	f1/(f1-f2);
												return rV1+(rV2-rV1)*lrp;
											}

	ILINE static void		Clip(COCPoly& rClippedPoly,const COCPoly& rPoly,f32 ClipValue)
											{
												rClippedPoly.Clear();

												for(uint a=0,b=rPoly.Count()-1;a<rPoly.Count();b=a++)
												{
													const Vec4& rV1	=	rPoly.Vec(b);
													const Vec4& rV2	=	rPoly.Vec(a);

													const bool I1	=	COCClipperPimpl<IDX,COMPERATOR>::Included(rV1,ClipValue);
													const bool I2	=	COCClipperPimpl<IDX,COMPERATOR>::Included(rV2,ClipValue);

													if(I1)
														rClippedPoly.Add()	=	rV1;
													if(I1!=I2) 
														rClippedPoly.Add()	=	rV1[IDX]*rV2.w<rV2[IDX]*rV1.w?COCClipperPimpl<IDX,COMPERATOR>::Clip(rV1,rV2,ClipValue):
																																								COCClipperPimpl<IDX,COMPERATOR>::Clip(rV2,rV1,ClipValue);
												}
											}

	ILINE static void		Clip(COCPoly& rClippedPoly,const COCPoly& rPoly)
											{
												const f32	ClipValue	=	1.f;
												COCPoly TmpPoly;

												COCClipperPimpl<2,EOCC_GREATHERTHANZERO>::Clip(TmpPoly,rPoly,0.f);
												COCClipperPimpl<2,EOCC_LESS>::Clip(rClippedPoly,TmpPoly,ClipValue);
												COCClipperPimpl<1,EOCC_GREATHER>::Clip(TmpPoly,rClippedPoly,-ClipValue);
												COCClipperPimpl<1,EOCC_LESS>::Clip(rClippedPoly,TmpPoly,ClipValue);
												COCClipperPimpl<0,EOCC_GREATHER>::Clip(TmpPoly,rClippedPoly,-ClipValue);
												COCClipperPimpl<0,EOCC_LESS>::Clip(rClippedPoly,TmpPoly,ClipValue);
											}
};

#endif 
