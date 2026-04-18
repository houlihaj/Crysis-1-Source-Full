//////////////////////////////////////////////////////////////////////
//
//  CryFont Source Code
//
//  File: CryFont.h
//  Description: Main interface to the DLL.
//
//  History:
//  - August 17, 2001: Created by Alberto Demichelis
//
//////////////////////////////////////////////////////////////////////

#ifndef CRYFONT_CRYFONT_H
#define CRYFONT_CRYFONT_H

#include <map>

typedef std::map<string, IFFont*> FontMap;
typedef FontMap::iterator FontMapItor;

//////////////////////////////////////////////////////////////////////////////////////////////

class CCryFont : public ICryFont
{
public:
	// constructor
	CCryFont(ISystem *pSystem);
	// destructor
	virtual ~CCryFont();

	// interface ICryFont --------------------------------------
	
	virtual void Release();
	virtual struct IFFont *NewFont(const char *pszName);
	virtual struct IFFont *GetFont(const char *pszName);
	virtual void GetMemoryUsage(class ICrySizer* pSizer);

	void UnregisterFont(const char* pszName);
private: // ---------------------------------------------------

	FontMap				m_mapFonts;						//
  ISystem *			m_pISystem;		        // system interface

	friend class CFFont;
};

// USE_NULLFONT should be defined for all platforms running as a pure
// dedicated server.
#if !defined(PS3) || defined(DEDICATED_SERVER)
#define USE_NULLFONT 1
#endif

#endif // CRYFONT_CRYFONT_H
