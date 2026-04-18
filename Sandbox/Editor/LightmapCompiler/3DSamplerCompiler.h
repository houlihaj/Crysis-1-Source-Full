/*=============================================================================
3DSamplerCompiler.h : 
Copyright 2006 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef __3DSAMPLERCOMPILER_H__
#define __3DSAMPLERCOMPILER_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include <I3DSampler.h>

class C3DSamplerCompiler
{
public:
	C3DSamplerCompiler():m_pSampler(NULL)
	{}

	~C3DSamplerCompiler()
	{
		SAFE_DELETE(m_pSampler);
	}

	I3DSampler* GetSampler() { return m_pSampler; }

	bool	Save( const char* szFilePath ) 
	{
		if( NULL == szFilePath || NULL == m_pSampler )
			return false;

		return m_pSampler->Save( szFilePath );
	}

	//Calculation
	bool	Prepare( const f32 fDistBetween2Points, e3DSamplingType SamplingType );	// Generate a full list of points
protected:
	bool	GeneratePointList( const f32 fDistBetween2Points );																							// Generate point list's based on the visareas
	I3DSampler* m_pSampler;																																					// The temporally sampler to calculations
};

#endif//__3DSAMPLEROCTREECOMPILER_H__