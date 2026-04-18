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
#ifndef _COCCLUSIONCULLERCLIPPER_H
#define _COCCLUSIONCULLERCLIPPER_H

class COCPoly;


class COCClipper
{
public:
	static void				Clip(COCPoly& rClippedPoly,const COCPoly& rPoly);
};

#endif 
