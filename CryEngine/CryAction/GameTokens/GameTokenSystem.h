////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   GameTokenSystem.h
//  Version:     v1.00
//  Created:     20/10/2005 by Craig,Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _GameTokenSystem_h_
#define _GameTokenSystem_h_
#pragma once

#include "IGameTokens.h"
#include "StlUtils.h"

class CGameToken;

//////////////////////////////////////////////////////////////////////////
// Game Token Manager implementation class.
//////////////////////////////////////////////////////////////////////////
class CGameTokenSystem : public IGameTokenSystem
{
public:
	CGameTokenSystem();
	~CGameTokenSystem();

	//////////////////////////////////////////////////////////////////////////
	// IGameTokenSystem
	//////////////////////////////////////////////////////////////////////////
	virtual IGameToken* SetOrCreateToken( const char *sTokenName,const TFlowInputData &defaultValue );
	virtual void DeleteToken( IGameToken* pToken );
	virtual IGameToken* FindToken( const char *sTokenName );
	virtual void RenameToken( IGameToken *pToken,const char *sNewName );

	virtual void RegisterListener( const char *sGameToken,IGameTokenEventListener *pListener,bool bForceCreate,bool bImmediateCallback );
	virtual void UnregisterListener( const char *sGameToken,IGameTokenEventListener *pListener );

	virtual void RegisterListener( IGameTokenEventListener *pListener ) { stl::push_back_unique(m_listeners,pListener); };
	virtual void UnregisterListener( IGameTokenEventListener *pListener ) { stl::find_and_erase(m_listeners,pListener); };

	virtual void Reset();
	virtual void Serialize( TSerialize ser );

	virtual void DebugDraw();

 	virtual void LoadLibs( const char *sFileSpec );
	virtual void RemoveLibrary( const char *sPrefix); 

	virtual void GetMemoryStatistics(ICrySizer * s);
	//////////////////////////////////////////////////////////////////////////

	CGameToken* GetToken( const char *sTokenName );
	void Notify( EGameTokenEvent event,CGameToken *pToken );
	void DumpAllTokens();

	static int sDebug;

private:
	bool _InternalLoadLibrary( const char *filename, const char* tag );

	// Use Hash map for speed.
#ifdef USE_HASH_MAP
	typedef stl::hash_map<const char*,CGameToken*,stl::hash_stricmp<const char*> > GameTokensMap;
#else
	typedef std::map<const char*,CGameToken*,stl::less_stricmp<const char*> > GameTokensMap;
#endif

	typedef std::vector<IGameTokenEventListener*> Listeners;
	Listeners m_listeners;

	// Loaded libraries.
	std::vector<string> m_libraries;

	GameTokensMap m_gameTokensMap;
	class CGameTokenScriptBind* m_pScriptBind;

	static int sShow;
};

#endif // _GameTokenSystem_h_
