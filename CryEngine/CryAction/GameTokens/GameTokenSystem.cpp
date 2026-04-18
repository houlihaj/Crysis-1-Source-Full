////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   CGameTokenSystem.cpp
//  Version:     v1.00
//  Created:     20/10/2005 by Craig,Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include <CryAction.h>
#include "GameTokenSystem.h"
#include "GameToken.h"
#include "ILevelSystem.h"
#include "IRenderer.h"

#define SCRIPT_GAMETOKEN_ALWAYS_CREATE    // GameToken.SetValue from Script always creates the token
// #undef SCRIPT_GAMETOKEN_ALWAYS_CREATE     // GameToken.SetValue from Script warns if Token not found

#define DEBUG_GAME_TOKENS
#undef DEBUG_GAME_TOKENS

namespace
{
	// Returns literal representation of the type value
	const char* ScriptAnyTypeToString( ScriptAnyType type )
	{
		switch (type)
		{
		case ANY_ANY: return "Any";
		case ANY_TNIL: return "Null";
		case ANY_TBOOLEAN: return "Boolean";
		case ANY_TSTRING:  return "String";
		case ANY_TNUMBER:  return "Number";
		case ANY_TFUNCTION:  return "Function";
		case ANY_TTABLE: return "Table";
		case ANY_TUSERDATA: return "UserData";
		case ANY_THANDLE: return "Pointer";
		case ANY_TVECTOR: return "Vec3";
		default: return "Unknown";
		}
	}
};

int CGameTokenSystem::sShow = 0;
int CGameTokenSystem::sDebug = 0;

class CGameTokenScriptBind : public CScriptableBase
{
public:
	CGameTokenScriptBind( CGameTokenSystem *pTokenSystem )
	{
		m_pTokenSystem = pTokenSystem;

		Init( gEnv->pScriptSystem, GetISystem() );
		SetGlobalName("GameToken");

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CGameTokenScriptBind::

		SCRIPT_REG_FUNC(SetToken);
		SCRIPT_REG_TEMPLFUNC(GetToken, "sTokenName");
		SCRIPT_REG_FUNC(DumpAllTokens);
	}
	virtual ~CGameTokenScriptBind()
	{
	}

	void Release() { delete this; };

	int SetToken( IFunctionHandler *pH)
	{
		SCRIPT_CHECK_PARAMETERS(2);
		const char* tokenName = 0;
		if (pH->GetParams(tokenName) == false)
		{
			GameWarning("[GameToken.SetToken] Usage: GameToken.SetToken TokenName TokenValue]");
			return pH->EndFunction();
		}
		ScriptAnyValue val;
		TFlowInputData data;

		if (pH->GetParamAny( 2, val) == false)
		{
			GameWarning("[GameToken.SetToken(%s)] Usage: GameToken.SetToken TokenName TokenValue]", tokenName);
			return pH->EndFunction();
		}
		switch (val.type)
		{
		case ANY_TBOOLEAN:
			{ 
				bool v; 
				val.CopyTo(v); 	
				data.Set(v);
			}
			break;
		case ANY_TNUMBER:
			{ 
				float v; 
				val.CopyTo(v); 		
				data.Set(v);
			}
			break;
		case ANY_TSTRING:
			{ 
				const char* v;
				val.CopyTo(v); 		
				data.Set(string(v));
			}
			break;
		case ANY_TVECTOR:
			{ 
				Vec3 v; 
				val.CopyTo(v); 		
				data.Set(v);
			}
		case ANY_TTABLE:
			{
				float x,y,z;
				IScriptTable * pTable = val.table;
				assert (pTable != 0);
				if (pTable->GetValue( "x", x ) && pTable->GetValue( "y", y ) && pTable->GetValue( "z", z ))
				{
					data.Set(Vec3(x,y,z));
				}
				else
				{
					GameWarning("[GameToken.SetToken(%s)] Cannot convert parameter type '%s' to Vec3", tokenName, ScriptAnyTypeToString(val.type));
					return pH->EndFunction();
				}
			}
			break;
		case ANY_THANDLE:
			{
				ScriptHandle handle;
				val.CopyTo(handle);
				data.Set((EntityId)handle.n);
			}
			break;
		default:
			GameWarning("[GameToken.SetToken(%s)] Cannot convert parameter type '%s'", tokenName, ScriptAnyTypeToString(val.type));
			return pH->EndFunction();
			break; // dummy ;-)
		}
#ifdef SCRIPT_GAMETOKEN_ALWAYS_CREATE
		m_pTokenSystem->SetOrCreateToken(tokenName, data);
#else
		IGameToken *pToken = m_pTokenSystem->FindToken(tokenName);
		if (!pToken)
			GameWarning("[GameToken.SetToken] Cannot find token '%s'", tokenName);
		else
		{
			pToken->SetValue(data);
		}
#endif
		return pH->EndFunction();
	}

	int GetToken( IFunctionHandler *pH,const char *sTokenName )
	{
		IGameToken *pToken = m_pTokenSystem->FindToken(sTokenName);
		if (!pToken)
		{
			GameWarning("[GameToken.GetToken] Cannot find token '%s'", sTokenName);
			return pH->EndFunction();
		}

		return pH->EndFunction( pToken->GetValueAsString() );
	}

	int DumpAllTokens( IFunctionHandler* pH)
	{
		m_pTokenSystem->DumpAllTokens();
		return pH->EndFunction();
	}

private:
	CGameTokenSystem *m_pTokenSystem;
};

//////////////////////////////////////////////////////////////////////////
CGameTokenSystem::CGameTokenSystem()
{
	CGameToken::g_pGameTokenSystem = this;
	m_pScriptBind = new CGameTokenScriptBind(this);


	IConsole *pConsole = gEnv->pConsole;
	pConsole->Register("gt_show", &CGameTokenSystem::sShow, 0, 0, "Show Game Tokens with values started from specified parameter");
	pConsole->Register("gt_debug", &CGameTokenSystem::sDebug, 0, 0, "Debug Game Tokens");
}

//////////////////////////////////////////////////////////////////////////
CGameTokenSystem::~CGameTokenSystem()
{
	if (m_pScriptBind)
		delete m_pScriptBind;

	IConsole *pConsole = gEnv->pConsole;
	pConsole->UnregisterVariable("gt_show", true);
	pConsole->UnregisterVariable("gt_debug", true);
}

//////////////////////////////////////////////////////////////////////////
IGameToken* CGameTokenSystem::SetOrCreateToken( const char *sTokenName,const TFlowInputData &defaultValue )
{
	assert(sTokenName);
	if (*sTokenName == 0) // empty string
	{
		GameWarning( _HELP("Creating game token with empty name") );
		return 0;
	}

#ifdef DEBUG_GAME_TOKENS
	GameTokensMap::iterator iter (m_gameTokensMap.begin());
	CryLogAlways("GT 0x%p: DEBUG looking for token '%s'", this, sTokenName);
	while (iter != m_gameTokensMap.end())
	{
		CryLogAlways("GT 0x%p: Token key='%s' name='%s' Val='%s'", this, (*iter).first, (*iter).second->GetName(), (*iter).second->GetValueAsString());
		++iter;
	}
#endif

	// Check if token already exist, if it is return existing token.
	CGameToken *pToken = stl::find_in_map(m_gameTokensMap,sTokenName,NULL);
	if (pToken)
	{
		pToken->SetValue(defaultValue);
		// Should we also output a warning here?
		// GameWarning( "Game Token 0x%p %s already exist. New value %s", pToken, sTokenName, pToken->GetValueAsString());
		return pToken;
	}
	pToken = new CGameToken;
	pToken->m_name = sTokenName;
	pToken->m_value = defaultValue;
	m_gameTokensMap[pToken->m_name] = pToken;

	if (sDebug)
		GameWarning("GameTokenSystemNew::SetOrCreateToken: New Token %p '%s' val=%s",
			pToken, sTokenName, pToken->GetValueAsString());

	return pToken;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::DeleteToken( IGameToken* pToken )
{
	assert(pToken);
	GameTokensMap::iterator it = m_gameTokensMap.find( ((CGameToken*)pToken)->m_name );
	if (it != m_gameTokensMap.end())
	{
#ifdef DEBUG_GAME_TOKENS
		CryLogAlways("GameTokenSystemNew::DeleteToken: About to delete Token 0x%p '%s' val=%s", pToken, pToken->GetName(), pToken->GetValueAsString());
#endif

		m_gameTokensMap.erase(it);
		delete (CGameToken*)pToken;
	}
}

//////////////////////////////////////////////////////////////////////////
IGameToken* CGameTokenSystem::FindToken( const char *sTokenName )
{
	return stl::find_in_map(m_gameTokensMap,sTokenName,NULL);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::RenameToken( IGameToken *pToken,const char *sNewName )
{
	assert(pToken);
	CGameToken *pCToken = (CGameToken*)pToken;
	GameTokensMap::iterator it = m_gameTokensMap.find( pCToken->m_name );
	if (it != m_gameTokensMap.end())
		m_gameTokensMap.erase(it);
#ifdef DEBUG_GAME_TOKENS
	CryLogAlways("GameTokenSystemNew::RenameToken: 0x%p '%s' -> '%s'", pCToken, pCToken->m_name, sNewName);
#endif
	pCToken->m_name = sNewName;
	m_gameTokensMap[pCToken->m_name] = pCToken;
}

//////////////////////////////////////////////////////////////////////////
CGameToken* CGameTokenSystem::GetToken( const char *sTokenName )
{
	// Load library if not found.
	CGameToken *pToken = stl::find_in_map(m_gameTokensMap,sTokenName,NULL);
	if (!pToken)
	{
		//@TODO: Load game token lib here.
	}
	return pToken;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::RegisterListener( const char *sGameToken,IGameTokenEventListener *pListener,bool bForceCreate,bool bImmediateCallback )
{
	CGameToken *pToken = GetToken(sGameToken);

	if(!pToken && bForceCreate)
	{
		pToken = new CGameToken;
		pToken->m_name = sGameToken;
		pToken->m_value = TFlowInputData(0.0f);
		m_gameTokensMap[pToken->m_name] = pToken;
	}

	if(pToken)
	{
		pToken->AddListener( pListener );

		if(bImmediateCallback)
		{
			pListener->OnGameTokenEvent(EGAMETOKEN_EVENT_CHANGE,pToken);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::UnregisterListener( const char *sGameToken,IGameTokenEventListener *pListener )
{
	CGameToken *pToken = GetToken(sGameToken);
	if (pToken)
	{
		pToken->RemoveListener( pListener );
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::Notify( EGameTokenEvent event,CGameToken *pToken )
{
	for (Listeners::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		(*it)->OnGameTokenEvent(event,pToken);
	}
	if (pToken)
		pToken->Notify(event);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::Serialize( TSerialize ser )
{
	// when writing game tokens we store the per-level tokens (Level.xxx) in a group
	// with the real level name
	// FIXME: for levels in subfolders of Level/, e.g. !Code/player_zoo
	//        Editor returns "player_zoo" whereas Game returns "!code/player_zoo"
	const char *pLevelName = CCryAction::GetCryAction()->GetLevelName();
	assert (pLevelName != 0);
	if (pLevelName == 0)
		pLevelName = "UnknownLevel";

	TFlowInputData data;
	std::vector<CGameToken*> levelTokens;
	const string levelPrefix ("Level.");
	const bool bSaving = ser.IsWriting();

	if (bSaving)
	{
		// Saving.
		int nTokens = 0;
		if (!m_gameTokensMap.empty())
		{
			ser.BeginGroup("GlobalTokens");
			GameTokensMap::iterator iter = m_gameTokensMap.begin();
			GameTokensMap::iterator iterEnd = m_gameTokensMap.end();
			while (iter != iterEnd)
			{
				CGameToken *pToken = iter->second;
				// if (bWriting && !(pToken->m_nFlags&(EGAME_TOKEN_FROMSAVEGAME|EGAME_TOKEN_MODIFIED)))
				//	continue;

				const bool bIsLevelToken (pToken->m_name.find(levelPrefix) == 0); // beginsWith
				if (bIsLevelToken)
					levelTokens.push_back(pToken);
				else
				{
					nTokens++;
					ser.BeginGroup("Token");
					ser.Value( "name",pToken->m_name );
					pToken->m_value.Serialize(ser);
					ser.EndGroup();
				}
				++iter;
			}
			ser.Value( "count",nTokens );
			ser.EndGroup();
		}
		
		nTokens = (int)levelTokens.size();
		ser.BeginGroup( "LevelTokens" );
		ser.Value( "count",nTokens );
		if (!levelTokens.empty())
		{
			std::vector<CGameToken*>::iterator iter = levelTokens.begin();
			std::vector<CGameToken*>::iterator iterEnd = levelTokens.end();
			while (iter != iterEnd)
			{
				CGameToken *pToken = *iter;
				{
					ser.BeginGroup("Token");
					ser.Value( "name",pToken->m_name );
					pToken->m_value.Serialize(ser);
					ser.EndGroup();
				}
				++iter;
			}
		}	
		ser.EndGroup();
	}
	else
	{
		// Loading.
		for (int pass = 0; pass < 2; pass++)
		{
			if (pass == 0)
				ser.BeginGroup("GlobalTokens");
			else
				ser.BeginGroup("LevelTokens");

			string tokenName;
			int nTokens = 0;
			ser.Value( "count",nTokens );
			for (int i = 0; i < nTokens; i++)
			{
				ser.BeginGroup("Token");
				ser.Value( "name",tokenName );
				data.Serialize(ser);
				CGameToken *pToken = (CGameToken*)FindToken(tokenName);
				if (pToken)
				{
					pToken->m_value = data;
					pToken->m_nFlags |= EGAME_TOKEN_FROMSAVEGAME;
					// we have to mark the token as modified
					// this ensures that when setting the same value again after the serialize
					// it doesn't re-trigger. See the check in CGameToken::SetValue
					// for the EGAME_TOKEN_MODIFIED flag (which always triggers, also if
					// value hasn't changed). Maybe remove the flag completely.
					pToken->m_nFlags |= EGAME_TOKEN_MODIFIED; 
				}
				else
				{
					// Create token.
					CGameToken *pToken = (CGameToken*)SetOrCreateToken( tokenName,data );
					if (pToken)
						pToken->m_nFlags |= EGAME_TOKEN_FROMSAVEGAME;
				}
				ser.EndGroup();
			}
			ser.EndGroup();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::LoadLibs( const char *sFileSpec )
{
	ICryPak *pPak = gEnv->pCryPak;
	_finddata_t fd;
	string dir = PathUtil::GetPath(sFileSpec);
	intptr_t handle = pPak->FindFirst( sFileSpec,&fd );
	if (handle != -1)
	{
		int res = 0;
		do {
			_InternalLoadLibrary( PathUtil::Make(dir,fd.name), "GameTokensLibrary" );
			res = pPak->FindNext( handle,&fd );
		} 
		while (res >= 0);
		pPak->FindClose(handle);
	}
}

#if 0 // temp leave in
bool CGameTokenSystem::LoadLevelLibrary(const char *sFileSpec, bool bEraseLevelTokens)
{
	if (bEraseLevelTokens)
		RemoveLevelTokens();

	return _InternalLoadLibrary(sFileSpec, "GameTokensLevelLibrary");
}
#endif

void CGameTokenSystem::RemoveLibrary(const char* sPrefix)
{
	string prefixDot (sPrefix);
	prefixDot+=".";

	GameTokensMap::iterator iter = m_gameTokensMap.begin();
	while (iter != m_gameTokensMap.end())
	{
		bool isLibToken (iter->second->m_name.find(prefixDot) == 0);
		GameTokensMap::iterator next = iter;
		++next;
		if (isLibToken)
			m_gameTokensMap.erase(iter);
		iter = next;
	}
	stl::find_and_erase(m_libraries, sPrefix);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::Reset()
{
	GameTokensMap::iterator iter = m_gameTokensMap.begin();
	while (iter != m_gameTokensMap.end())
	{
		SAFE_DELETE((*iter).second);
		++iter;
	}
	m_gameTokensMap.clear();
	m_listeners.resize(0);
	m_libraries.resize(0);
}

//////////////////////////////////////////////////////////////////////////
bool CGameTokenSystem::_InternalLoadLibrary( const char *filename, const char* tag )
{
	XmlNodeRef root = GetISystem()->LoadXmlFile( filename );
	if (!root)
	{
		GameWarning( _HELP("Unable to load game token database: %s"), filename );
		return false;
	}

	if (0 != strcmp( tag, root->getTag() ))
	{
		GameWarning( _HELP("Not a game tokens library : %s"), filename );
		return false;
	}

	// GameTokens are (currently) not saved with their full path
	// we expand it here to LibName.TokenName
	string libName;
	{
		const char *sLibName = root->getAttr("Name");
		if (sLibName == 0) {
			GameWarning( "GameTokensLibrary::LoadLibrary: Unable to find LibName in file '%s'", filename);
			return false;
		}
		libName = sLibName;
	}

	// check if already loaded
	if (stl::find(m_libraries, libName)) return true;

	// remember
	m_libraries.push_back(libName);

	libName+= ".";

	int numChildren = root->getChildCount();
	for (int i=0; i<numChildren; i++)
	{
		XmlNodeRef node = root->getChild(i);

		const char *sName = node->getAttr("Name");
		const char *sType = node->getAttr("Type");
		const char *sValue = node->getAttr("Value");

		EFlowDataTypes tokenType = eFDT_Any;
		if (0 == strcmp(sType,"Int"))
			tokenType = eFDT_Int;
		else if (0 == strcmp(sType,"Float"))
			tokenType = eFDT_Float;
		else if (0 == strcmp(sType,"EntityId"))
			tokenType = eFDT_EntityId;
		else if (0 == strcmp(sType,"Vec3"))
			tokenType = eFDT_Vec3;
		else if (0 == strcmp(sType,"String"))
			tokenType = eFDT_String;
		else if (0 == strcmp(sType,"Bool"))
			tokenType = eFDT_Bool;

		if (tokenType == eFDT_Any)
		{
			GameWarning(_HELP("Unknown game token type %s in token %s (%s:%d)"),sType,sName,node->getTag(),node->getLine());
			continue;
		}

		TFlowInputData initialValue = TFlowInputData(string(sValue));

		string fullName (libName);
		fullName+=sName;

		// Make a token.
		SetOrCreateToken( fullName,initialValue );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenSystem::DumpAllTokens()
{
	GameTokensMap::iterator iter (m_gameTokensMap.begin());
	int i = 0;
	while (iter != m_gameTokensMap.end())
	{
		CryLogAlways("#%04d [%s]=[%s]", i, (*iter).second->GetName(), (*iter).second->GetValueAsString());
		++iter;
		++i;
	}
}

void CGameTokenSystem::DebugDraw()
{
	if(CGameTokenSystem::sShow>0)
	{
		int nStart = CGameTokenSystem::sShow;
		if(nStart == 1)
			nStart = 0;
		IRenderer * pRenderer = GetISystem()->GetIRenderer();
		if(pRenderer)
		{
			float color[]={1, 1, 1, 1};
			char str[256];
			GameTokensMap::iterator iter (m_gameTokensMap.begin());
			int i = 0;
			while (iter != m_gameTokensMap.end())
			{
				if(nStart<=i)
				{
					sprintf(str, "#%04d %s = %s", i, (*iter).second->GetName(), (*iter).second->GetValueAsString());
					pRenderer->Draw2dLabel(20,20+14*(i - CGameTokenSystem::sShow+1), 1.5f, color, false, str);
				}
				++iter;
				++i;
				if(nStart+50 < i)
					break;
			}
		}
	}
}

void CGameTokenSystem::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "GameTokenSystem");
	s->Add(*this);
	s->AddContainer(m_listeners);
	s->AddContainer(m_gameTokensMap);

	for (GameTokensMap::iterator iter = m_gameTokensMap.begin(); iter != m_gameTokensMap.end(); ++iter)
	{
		iter->second->GetMemoryStatistics(s);
	}
}
