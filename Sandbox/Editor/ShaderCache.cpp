////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   ShaderCache.cpp
//  Version:     v1.00
//  Created:     6/11/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ShaderCache.h"
#include "GameEngine.h"

#include <IRenderer.h>

//////////////////////////////////////////////////////////////////////////
bool CLevelShaderCache::Reload()
{
	return Load( m_filename );
}

//////////////////////////////////////////////////////////////////////////
bool CLevelShaderCache::Load( const char *filename )
{
	FILE *f = fopen( filename,"rt" );
	if (!f)
		return false;

	int nNumLines = 0;
	m_entries.clear();
	m_filename = filename;
	char str[65535];
	while (fgets(str,sizeof(str),f) != NULL)
	{
		if (str[0] == '<')
		{
			m_entries.insert(str);
			nNumLines++;
		}
	}
	fclose(f);
	if (nNumLines == m_entries.size())
		m_bModified = false;
	else
		m_bModified = true;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLevelShaderCache::LoadBuffer( const CString &textBuffer,bool bClearOld )
{
	const char *separators = "\r\n,";
	CString resToken;
	int curPos= 0;

	int nNumLines = 0;
	if (bClearOld)
		m_entries.clear();
	m_filename = "";

	resToken= textBuffer.Tokenize(separators,curPos);
	while (resToken != "")
	{
		if (!resToken.IsEmpty() && resToken[0] == '<')
		{
			m_entries.insert(resToken);
			nNumLines++;
		}
		
		resToken = textBuffer.Tokenize(separators,curPos);
	}
	if (nNumLines == m_entries.size() && !bClearOld)
		m_bModified = false;
	else
		m_bModified = true;

	int numShaders = m_entries.size();
	CLogFile::FormatLine( "%d shader combination loaded for level %s",numShaders,(const char*)GetIEditor()->GetGameEngine()->GetLevelName() );

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLevelShaderCache::Save()
{
	if (m_filename.IsEmpty())
		return false;

	Update();

	FILE *f = fopen( m_filename,"wt" );
	if (f)
	{
		for (Entries::iterator it = m_entries.begin(); it != m_entries.end(); ++it)
		{
			const char *str = *it;
			fputs( str,f );
		}
		fclose(f);
	}
	m_bModified = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLevelShaderCache::SaveBuffer( CString &textBuffer )
{
	Update();

	textBuffer.Preallocate( m_entries.size()*1024 );
	for (Entries::iterator it = m_entries.begin(); it != m_entries.end(); ++it)
	{
		textBuffer += (*it);
		textBuffer += "\n";
	}
	m_bModified = false;
	
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CLevelShaderCache::Update()
{
	IRenderer *pRenderer = gEnv->pRenderer;
	{
		CString buf;
		char *str = (char *)pRenderer->EF_Query(EFQ_GetShaderCombinations);
		if (str)
		{
			buf = str;
			pRenderer->EF_Query(EFQ_DeleteMemoryArrayPtr, (INT_PTR)str );
		}
		LoadBuffer( buf,false ); // Merge additional shader combinations.
	}
}

//////////////////////////////////////////////////////////////////////////
void CLevelShaderCache::Clear()
{
	m_entries.clear();
	m_bModified = true;
}

//////////////////////////////////////////////////////////////////////////
void CLevelShaderCache::ActivateShaders()
{
	bool bPreload = false;
	ICVar *pSysPreload = gEnv->pConsole->GetCVar( "sys_preload" );
	if (pSysPreload && pSysPreload->GetIVal() != 0)
		bPreload = true;

	if (bPreload)
	{
		CString textBuffer;
		textBuffer.Preallocate( m_entries.size()*1024 );
		for (Entries::iterator it = m_entries.begin(); it != m_entries.end(); ++it)
		{
			textBuffer += (*it);
			textBuffer += "\n";
		}
		gEnv->pRenderer->EF_Query(EFQ_SetShaderCombinations, (INT_PTR)(const char*)textBuffer );
	}
}