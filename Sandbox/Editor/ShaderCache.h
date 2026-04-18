////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   ShaderCache.h
//  Version:     v1.00
//  Created:     6/11/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ShaderCache_h__
#define __ShaderCache_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
class CLevelShaderCache
{
public:
	CLevelShaderCache()
	{
		m_bModified = false;
	}
	bool Load( const char *filename );
	bool LoadBuffer( const CString &textBuffer,bool bClearOld=true );
	bool SaveBuffer( CString &textBuffer );
	bool Save();
	bool Reload();
	void Clear();

	void Update();
	void ActivateShaders();

private:
	//////////////////////////////////////////////////////////////////////////
	bool m_bModified;
	CString m_filename;
	typedef std::set<CString> Entries;
	Entries m_entries;
};


#endif // __ShaderCache_h__
