// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__6E7C4805_F57C_4D50_81A7_7745B3776538__INCLUDED_)
#define AFX_STDAFX_H__6E7C4805_F57C_4D50_81A7_7745B3776538__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#pragma warning(disable:4786) // identifier was truncated to '255' characters in the debug information

#include <CryModuleDefs.h>
#define eCryModule eCryM_Font

#define CRYFONT_EXPORTS

#include <platform.h>

#ifdef WIN32
// Insert your headers here
#undef GetCharWidth
#undef GetCharHeight
#endif

// TODO: reference additional headers your program requires here
// Safe memory freeing
#ifndef SAFE_DELETE
#define SAFE_DELETE(p)			{ if(p) { delete (p);		(p)=NULL; } }
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p)	{ if(p) { delete[] (p);		(p)=NULL; } }
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)			{ if(p) { (p)->Release();	(p)=NULL; } }
#endif

//STL headers
#include <vector>
#include <list>
#include <map>	
#include <algorithm>

#ifdef GetCharWidth
#undef GetCharWidth
#undef GetCharHeight
#endif // GetCharWidth


#include <Cry_Math.h>
#include <IFont.h>

//! Include main interfaces.
#include <IProcess.h>
//#include <ITimer.h>
#include <ISystem.h>
#include <ILog.h>
//#include <CryPhysics.h>
#include <IConsole.h>
#include <IRenderer.h>
#include <ICryPak.h>
#include <CrySizer.h>


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__6E7C4805_F57C_4D50_81A7_7745B3776538__INCLUDED_)
