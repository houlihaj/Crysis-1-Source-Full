////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   PathExpansion.h
//  Version:     v1.00
//  Created:     25/5/2007 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __PATHEXPANSION_H__
#define __PATHEXPANSION_H__

namespace PathExpansion
{
	// Expand patterns into paths. An example of a pattern before expansion:
	// animations/facial/idle_{0,1,2,3}.fsq
	// {} is used to specify options for parts of the string.
	// output must point to buffer that is at least as large as the pattern.
	void SelectRandomPathExpansion(const char* pattern, char* output);
	void EnumeratePathExpansions(const char* pattern, void (*enumCallback)(void* userData, const char* expansion), void* userData);
}

#endif //__PATHEXPANSION_H__
