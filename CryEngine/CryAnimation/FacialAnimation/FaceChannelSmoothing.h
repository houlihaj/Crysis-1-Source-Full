////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   FaceChannelSmoothing.h
//  Version:     v1.00
//  Created:     12/2/2007 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __FACECHANNELSMOOTHING_H__
#define __FACECHANNELSMOOTHING_H__

class CFacialAnimChannelInterpolator;

namespace FaceChannel
{
	void GaussianBlurKeys(CFacialAnimChannelInterpolator* pSpline, float sigma);
	void RemoveNoise(CFacialAnimChannelInterpolator* pSpline, float sigma, float threshold);
}

#endif //__FACECHANNELSMOOTHING_H__
