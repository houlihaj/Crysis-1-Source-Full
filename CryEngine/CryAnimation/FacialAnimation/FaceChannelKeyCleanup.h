////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   FaceChannelKeyCleanup
//  Version:     v1.00
//  Created:     7/10/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __FACECHANNELKEYCLEANUP_H__
#define __FACECHANNELKEYCLEANUP_H__

class CFacialAnimChannelInterpolator;

namespace FaceChannel
{
	void CleanupKeys(CFacialAnimChannelInterpolator* pSpline, float errorMax);
}

#endif //__FACECHANNELKEYCLEANUP_H__
