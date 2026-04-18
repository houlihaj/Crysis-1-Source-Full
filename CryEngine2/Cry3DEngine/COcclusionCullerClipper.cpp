#include "StdAfx.h"
#include "COcclusionCullerClipper.h"
#include "COcclusionCullerClipperPimpl.h"

void COCClipper::Clip(COCPoly& rClippedPoly,const COCPoly& rPoly)
{
	COCClipperPimpl<>::Clip(rClippedPoly,rPoly);
}
