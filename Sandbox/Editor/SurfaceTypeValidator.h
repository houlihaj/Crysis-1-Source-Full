////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   SurfaceTypeValidator.h
//  Version:     v1.00
//  Created:     15/8/2006 by MichaelS.
//  Compilers:   Visual Studio .NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __SURFACETYPEVALIDATOR_H__
#define __SURFACETYPEVALIDATOR_H__

struct pe_params_part;

class CSurfaceTypeValidator
{
public:
	void Validate();

private:
	unsigned GetUsedSubMaterialFlags(pe_params_part* pPart);
};

#endif //__SURFACETYPEVALIDATOR_H__
