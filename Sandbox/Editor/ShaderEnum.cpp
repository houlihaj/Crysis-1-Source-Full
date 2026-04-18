////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   ShaderEnum.cpp
//  Version:     v1.00
//  Created:     7/12/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Enumerate Installed Shaders.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ShaderEnum.h"

//////////////////////////////////////////////////////////////////////////
CShaderEnum::CShaderEnum()
{
	m_bEnumerated = false;
}

CShaderEnum::~CShaderEnum()
{
}


inline bool ShaderLess( const CShaderEnum::ShaderDesc &s1,const CShaderEnum::ShaderDesc &s2 )
{
	return stricmp( s1.name,s2.name ) < 0;
}
/*
struct StringLess {
	bool operator()( const CString &s1,const CString &s2 )
	{
		return stricmp( s1,s2 ) < 0;
	}
};
*/

//! Enum shaders.
int CShaderEnum::EnumShaders()
{
	IRenderer *renderer = GetIEditor()->GetSystem()->GetIRenderer();
	if (!renderer)
		return 0;

	m_bEnumerated = true;

	int numShaders = 0;

	m_shaders.clear();
	m_shaders.reserve( 100 );
	//! Enumerate Shaders.
  int nNumShaders = 0;
	string *files = renderer->EF_GetShaderNames(nNumShaders);
	for (int i = 0; i < nNumShaders; i++)
	{
		ShaderDesc sd;
		sd.name = files[i].c_str();
		sd.file = files[i].c_str();
		if (!sd.name.IsEmpty())
		{
			// Capitalize first character of the string.
			char shadername[1024];
			strcpy(shadername,sd.name);
			shadername[0] = toupper(shadername[0]);
			sd.name = shadername;
		}
		m_shaders.push_back( sd );
	}
	std::sort( m_shaders.begin(),m_shaders.end(),ShaderLess );
	return m_shaders.size();
}
