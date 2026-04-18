//////////////////////////////////////////////////////////////////////
//
//  CryFont Source Code
//
//  File: CryFont.cpp
//  Description: CCryFont class.
//
//  History:
//  - August 18, 2001: Created by Alberto Demichelis
//  - June	 28, 2003: Added r_DumpFontTexture and r_DumpFontNames CVARs
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CryFont.h"
#include "FBitmap.h"
#include "FFont.h"

static ICVar *r_DumpFontTexture = 0;
static ICVar *r_DumpFontNames = 0;

///////////////////////////////////////////////
CCryFont::CCryFont(ISystem *pSystem)
{
  m_pISystem = pSystem;
	m_mapFonts.clear();

	// CVar added by marcio

	if (!r_DumpFontTexture)
	{
		r_DumpFontTexture = pSystem->GetIConsole()->RegisterString("r_DumpFontTexture", "", 0, "Dumps the specified font's texture to a bitmap file!\n\nUsage: r_DumpFontTexture fontname");
	}

	if (!r_DumpFontNames)
	{
		r_DumpFontNames = pSystem->GetIConsole()->RegisterInt("r_DumpFontNames", 0, 0, "Displays a list of fonts currently loaded!");
	}
}

///////////////////////////////////////////////
CCryFont::~CCryFont()
{
	m_pISystem->GetIConsole()->UnregisterVariable("r_DumpFontTexture", 1);
	m_pISystem->GetIConsole()->UnregisterVariable("r_DumpFontNames", 1);
	r_DumpFontTexture = 0;
	r_DumpFontNames = 0;

	FontMap tmpMap;
	tmpMap.swap(m_mapFonts);

	for (FontMapItor itor=tmpMap.begin();itor!=tmpMap.end(); ++itor)
	{
		IFFont *pFont=itor->second;
		pFont->Release();
	}
}

///////////////////////////////////////////////
void CCryFont::Release()
{
	delete this;
}


///////////////////////////////////////////////
IFFont* CCryFont::NewFont(const char *pszName)
{
	string sName=pszName;
	sName.MakeLower();				// we will removed it soon

	// check if font already created, if so return it
	FontMapItor itor;
	itor=m_mapFonts.find(sName.c_str());
	if (itor!=m_mapFonts.end())
		return itor->second;
	CFFont *pFont=new CFFont(m_pISystem, this, sName.c_str());
	m_mapFonts.insert(FontMapItor::value_type(sName.c_str(), pFont));
	return (IFFont*)pFont;
}

///////////////////////////////////////////////
void CCryFont::UnregisterFont(const char* pszName)
{
	FontMapItor iter = m_mapFonts.find(CONST_TEMP_STRING(pszName));
	if (iter != m_mapFonts.end())
		m_mapFonts.erase(iter);
}

///////////////////////////////////////////////
IFFont* CCryFont::GetFont(const char *pszName)
{
	if (r_DumpFontTexture)
	{
		const char *pValue = r_DumpFontTexture->GetString();

		if ((pValue) && (*pValue != 0) && (*pValue != '0'))
		{
			string szFontName(pValue);
			string szFontFile(pValue);
			szFontFile += ".bmp";

			r_DumpFontTexture->Set("0"); // must be here, to avoid recursion
			CFFont *pFont = (CFFont *)GetFont(szFontName.c_str());

			if (pFont)
			{
				pFont->m_pFontTexture.WriteToFile(szFontFile.c_str());
			}

			m_pISystem->GetILog()->LogWithType(IMiniLog::eInputResponse,"Dumped '%s' texture to '%s'!", pValue, szFontFile.c_str());
		}
	}

	if (r_DumpFontNames)
	{
		if (r_DumpFontNames->GetIVal())
		{
			FontMapItor pItor;
			CFFont *pFont;

			m_pISystem->GetILog()->LogWithType(IMiniLog::eInputResponse,"Currently Loaded Fonts:");

			for (pItor=m_mapFonts.begin(); pItor != m_mapFonts.end(); ++pItor)
			{
				pFont = (CFFont *)pItor->second;

				m_pISystem->GetILog()->LogWithType(IMiniLog::eInputResponse,"  - %s", pFont->m_szName.c_str());
			}

			r_DumpFontNames->Set(0);
		}
	}

	FontMapItor itor;
	itor=m_mapFonts.find(CONST_TEMP_STRING(pszName));

	if(itor==m_mapFonts.end())
	{
		string sName=pszName;

		sName.MakeLower();

		itor = m_mapFonts.find(sName);

		if(itor!=m_mapFonts.end())
		{
			m_pISystem->GetILog()->Log("ERROR: We want to change GetFont() to be case sensitive but this fails: '%s' - change the call!",pszName);
			assert(0);
			return itor->second;
		}
		else return 0;
	}
	else return itor->second;
}

void CCryFont::GetMemoryUsage(class ICrySizer* pSizer)
{
	if(!pSizer->Add (*this))
		return;

	FontMapItor it;

	for(it=m_mapFonts.begin(); it!=m_mapFonts.end(); ++it)
	{
		pSizer->AddObject(&*it,sizeof(*it)+it->first.capacity()+1);
		it->second->GetMemoryUsage(pSizer);
	}
}

#include <CrtDebugStats.h>

