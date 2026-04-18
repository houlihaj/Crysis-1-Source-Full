////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   indirectlighting.h
//  Version:     v1.00
//  Created:     13/6/2005 by Michael Glueck
//  Compilers:   Visual Studio.NET
//  Description: Declaration of Indirect Lighting engine
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef INDIRECT_LIGHTING_H
#define INDIRECT_LIGHTING_H

#include <I3DEngine.h>
#undef min
#undef max

//! implementation of interface to indirect lighting engine 
// Summary:
//     indirect lighting engine 
class CIndirectLighting : public IIndirectLighting, Cry3DEngineBase
{
public:

	// Description:
	//		 default ctor
	CIndirectLighting();

	// Description:
	//		 retrieves the required information about terrain voxels
	//		 pass NULL to retrieve just the count
	virtual void GetTerrainVoxelInfo(SVoxelInfo *ppVoxels, uint32& rCount);

	// Description:
	//		 retrieves existing terrain accessibility data
	virtual void RetrieveTerrainAcc(uint8 *pDst, const uint32 cWidth, const uint32 cHeight);
};

#endif // INDIRECT_LIGHTING_H

