////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   SurfaceManager.h
//  Version:     v1.00
//  Created:     29/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SurfaceManager_h__
#define __SurfaceManager_h__
#pragma once

#include <I3DEngine.h>

#define MAX_SURFACE_ID 255

//////////////////////////////////////////////////////////////////////////
// SurfaceManager is implementing ISurfaceManager interface.
// Register and manages all known to game surface typs.
//////////////////////////////////////////////////////////////////////////
class CSurfaceTypeManager : public ISurfaceTypeManager, public Cry3DEngineBase
{
public:
	CSurfaceTypeManager( ISystem *pSystem );
	virtual ~CSurfaceTypeManager();

	void LoadSurfaceTypes();

	virtual ISurfaceType* GetSurfaceTypeByName( const char *sName,const char *sWhy=NULL,bool warn=true );
	virtual ISurfaceType* GetSurfaceType( int nSurfaceId,const char *sWhy=NULL );
	virtual ISurfaceTypeEnumerator* GetEnumerator();

	bool RegisterSurfaceType( ISurfaceType *pSurfaceType,bool bDefault=false );
	void UnregisterSurfaceType( ISurfaceType *pSurfaceType );

	ISurfaceType* GetSurfaceTypeFast( int nSurfaceId,const char *sWhy=NULL );

	void RemoveAll();

private:
	ISystem *m_pSystem;
	int m_lastSurfaceId;

	class CMaterialSurfaceType* m_pDefaultSurfaceType;

	struct SSurfaceRecord
	{
		bool bLoaded;
		ISurfaceType *pSurfaceType;
	};

	SSurfaceRecord* m_idToSurface[MAX_SURFACE_ID+1];

	typedef std::map<string,SSurfaceRecord*> NameToSurfaceMap;
	NameToSurfaceMap m_nameToSurface;
};

#endif //__SurfaceManager_h__