////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   ScriptBind_Script.cpp
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ScriptBind_Script.h"
#include "../ScriptTimerMgr.h"
#include "../ScriptSystem.h"
#include <ILog.h>
#include <IEntitySystem.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CScriptBind_Script::CScriptBind_Script(IScriptSystem *pScriptSystem, ISystem *pSystem)
{
	CScriptableBase::Init(pScriptSystem,pSystem);
	SetGlobalName( "Script" );

#undef SCRIPT_REG_CLASSNAME 
#define SCRIPT_REG_CLASSNAME &CScriptBind_Script::

	SCRIPT_REG_FUNC(ReloadScripts);
	SCRIPT_REG_FUNC(ReloadScript);
	SCRIPT_REG_TEMPLFUNC(ReloadEntityScript, "className");
	SCRIPT_REG_FUNC(LoadScript);
	SCRIPT_REG_FUNC(UnloadScript);
	SCRIPT_REG_FUNC(DumpLoadedScripts);
	SCRIPT_REG_FUNC(Debug);
	SCRIPT_REG_FUNC(DebugFull);
	SCRIPT_REG_TEMPLFUNC(SetTimer,"nMilliseconds,Function");
	SCRIPT_REG_TEMPLFUNC(SetTimerForFunction,"nMilliseconds,Function");
	SCRIPT_REG_TEMPLFUNC(KillTimer,"nTimerId");
}

CScriptBind_Script::~CScriptBind_Script()
{

}

void CScriptBind_Script::Debug_Full_recursive( IScriptTable *pCurrent, string &sPath, std::set<const void *> &setVisited )
{
	/*  deactivate because it can be used to hack

	assert(pCurrent);

	pCurrent->BeginIteration();

	while(pCurrent->MoveNext())
	{
		char *szKeyName;

		if(!pCurrent->GetCurrentKey(szKeyName))
			szKeyName="NO";

		ScriptVarType type=pCurrent->GetCurrentType();

		if(type==svtObject)		// svtNull,svtFunction,svtString,svtNumber,svtUserData,svtObject
		{
			const void *pVis;
			
			pCurrent->GetCurrentPtr(pVis);

			gEnv->pLog->Log("  table '%s/%s'",sPath.c_str(),szKeyName);				

			if(setVisited.count(pVis)!=0)
			{
				gEnv->pLog->Log("    .. already processed ..");				
				continue;
			}

			setVisited.insert(pVis);

			{
				IScriptTable *pNewObject = m_pSS->CreateEmptyObject();

				pCurrent->GetCurrent(pNewObject);

#if defined(LINUX)
				string s = sPath+string("/")+szKeyName;
				Debug_Full_recursive(pNewObject,s,setVisited);
#else
				Debug_Full_recursive(pNewObject,sPath+string("/")+szKeyName,setVisited);
#endif
				pNewObject->Release();
			}
		}
		else if(type==svtFunction)		// svtNull,svtFunction,svtString,svtNumber,svtUserData,svtObject
		{
			unsigned int *pCode=0;
			int iSize=0;
			
			if(pCurrent->GetCurrentFuncData(pCode,iSize))
			{
				// lua function
				if(pCode) 
					gEnv->pLog->Log("         lua function '%s' size=%d",szKeyName,(uint32)iSize);				
				 else
					gEnv->pLog->Log("         cpp function '%s'",szKeyName);				
			}
		}
		else													// svtNull,svtFunction,svtString,svtNumber,svtUserData,svtObject
		{
			gEnv->pLog->Log("         data '%s'",szKeyName);				
		}
	}
	
	pCurrent->EndIteration();
	*/
}


int CScriptBind_Script::Debug_Buckets_recursive( IScriptTable *pCurrent, string &sPath, std::set<const void *> &setVisited, const int dwMinBucket )
{
	/*  deactivate because it can be used to hack
	assert(pCurrent);

	uint32 dwTableElementCount=0;

	pCurrent->BeginIteration();

	while(pCurrent->MoveNext())
	{
		char *szKeyName;

		dwTableElementCount++;

		if(!pCurrent->GetCurrentKey(szKeyName))
			szKeyName="NO";

		ScriptVarType type=pCurrent->GetCurrentType();

		if(type==svtObject)		// svtNull,svtFunction,svtString,svtNumber,svtUserData,svtObject
		{
			const void *pVis;
			
			pCurrent->GetCurrentPtr(pVis);

			if(setVisited.count(pVis)!=0)
				continue;

			setVisited.insert(pVis);

			{
				IScriptTable *pNewObject = m_pSS->CreateEmptyObject();

				pCurrent->GetCurrent(pNewObject);

#if defined(LINUX)
				string s = sPath+string("/")+szKeyName;
				uint32 dwSubTableCount = Debug_Buckets_recursive(pNewObject,s,setVisited,dwMinBucket);
#else
				uint32 dwSubTableCount = Debug_Buckets_recursive(pNewObject,sPath+string("/")+szKeyName,setVisited,dwMinBucket);
#endif
				pNewObject->Release();

				if(dwSubTableCount>=dwMinBucket)
				{
					gEnv->pLog->Log("  %8d '%s/%s'\n",dwSubTableCount,sPath.c_str(),szKeyName);				
				}
				else dwTableElementCount+=dwSubTableCount;
			}
		}
/*		else if(type==svtFunction)		// svtNull,svtFunction,svtString,svtNumber,svtUserData,svtObject
		{
			unsigned int *pCode=0;
			int iSize=0;
			
			if(pCurrent->GetCurrentFuncData(pCode,iSize))
			if(pCode)
			{
				// lua function

				char str[256];

				sprintf(str,"%s lua  '%s' size=%d\n",sPath.c_str(),szKeyName,(uint32)iSize);

				OutputDebugString(str);
			}
		}
*/
	/*  deactivate because it can be used to hack
	}
	
	pCurrent->EndIteration();

	return dwTableElementCount;
	*/
	return 0;
}


void CScriptBind_Script::Debug_Elements( IScriptTable *pCurrent, string &sPath, std::set<const void *> &setVisited )
{
	/*  deactivate because it can be used to hack
	assert(pCurrent);

	pCurrent->BeginIteration();

	while(pCurrent->MoveNext())
	{
		char *szKeyName;

		if(!pCurrent->GetCurrentKey(szKeyName))
			szKeyName="NO";

		ScriptVarType type=pCurrent->GetCurrentType();

		if(type==svtObject)		// svtNull,svtFunction,svtString,svtNumber,svtUserData,svtObject
		{
			const void *pVis;
			
			pCurrent->GetCurrentPtr(pVis);

			if(setVisited.count(pVis)!=0)
				continue;

			setVisited.insert(pVis);

			{
				IScriptTable *pNewObject = m_pSS->CreateEmptyObject();

				pCurrent->GetCurrent(pNewObject);

#if defined(LINUX)
				string s = sPath+string("/")+szKeyName;
				uint32 dwSubTableCount = Debug_Buckets_recursive(pNewObject,s,setVisited,0xffffffff);
#else
				uint32 dwSubTableCount = Debug_Buckets_recursive(pNewObject,sPath+string("/")+szKeyName,setVisited,0xffffffff);
#endif

				pNewObject->Release();

				gEnv->pLog->Log("  %8d '%s' '%s'\n",dwSubTableCount,sPath.c_str(),szKeyName);				
			}
		}
	}
	
	pCurrent->EndIteration();
	*/
}



int CScriptBind_Script::Debug(IFunctionHandler *pH)
{
/*  deactivate because it can be used to hack

	SCRIPT_CHECK_PARAMETERS(0);

	IScriptTable *pGlobals=m_pSS->GetGlobalObject();


	{
		std::set<const void *> setVisited;
	
		gEnv->pLog->Log("CScriptBind_Script::Debug globals recursive minbuckets");

		uint32 dwCount=Debug_Buckets_recursive(pGlobals,string(""),setVisited,50);

		gEnv->pLog->Log("  %8d '' ''\n",dwCount);			
	}

	{
		std::set<const void *> setVisited;

		gEnv->pLog->Log("CScriptBind_Script::Debug globals");
		Debug_Elements(pGlobals,string(""),setVisited);
	}

	pGlobals->Release();
*/
	return pH->EndFunction();
}


int CScriptBind_Script::DebugFull(IFunctionHandler *pH)
{
/* deactivate because it can be used to hack
	SCRIPT_CHECK_PARAMETERS(0);

	IScriptTable *pGlobals=m_pSS->GetGlobalObject();

	{
		std::set<const void *> setVisited;
	
		gEnv->pLog->Log("CScriptBind_Script::Debug full");

		Debug_Full_recursive(pGlobals,string(""),setVisited);
	}

	pGlobals->Release();
*/

	return pH->EndFunction();
}

/*!reload all previosly loaded scripts
*/
int CScriptBind_Script::ReloadScripts(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	m_pSS->ReloadScripts();
	return pH->EndFunction();
}

/*!reload a specified script. If the script wasn't loaded at least once before the function will fail
	@param sFileName path of the script that has to be reloaded
*/
int CScriptBind_Script::ReloadScript(IFunctionHandler *pH)
{
	const char *sFileName;
	if (!pH->GetParams(sFileName))
		return pH->EndFunction();
		
	m_pSS->ExecuteFile(sFileName,true,false);
	//m_pSS->ReloadScript(sFileName);
	return pH->EndFunction();
}

int CScriptBind_Script::ReloadEntityScript(IFunctionHandler *pH, const char *className)
{
	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;

	IEntityClass *pClass = pEntitySystem->GetClassRegistry()->FindClass(className);

	if (pClass)
	{
		pClass->LoadScript(false);
	}

	return pH->EndFunction();
}

/*!load a specified script
	@param sFileName path of the script that has to be loaded
*/
int CScriptBind_Script::LoadScript(IFunctionHandler *pH)
{
	bool bReload = false;
	bool bRaiseError = true;

	if (pH->GetParamCount() >= 3)
	{
		pH->GetParam(3, bRaiseError);
	}
	if (pH->GetParamCount() >= 2)
	{
		pH->GetParam(2, bReload);
	}
	
	const char *sScriptFile;
	pH->GetParam(1,sScriptFile);

	if (m_pSS->ExecuteFile(sScriptFile, bRaiseError, bReload))
		return pH->EndFunction(1);
	else
		return pH->EndFunction();
}

/*!unload script from the "loaded scripts map" so if this script is loaded again
	the Script system will reloadit. this function doesn't
	involve the LUA VM so the resources allocated by the script will not be released
	unloading the script
	@param sFileName path of the script that has to be loaded
*/
int CScriptBind_Script::UnloadScript(IFunctionHandler *pH)
{
	const char *sScriptFile;
	if (!pH->GetParams(sScriptFile))
		return pH->EndFunction();

	m_pSS->UnloadScript(sScriptFile);
	return pH->EndFunction();
}

/*!Dump all loaded scripts path calling IScriptSystemSink::OnLoadedScriptDump
	@see IScriptSystemSink
*/
int CScriptBind_Script::DumpLoadedScripts(IFunctionHandler *pH)
{
	m_pSS->DumpLoadedScripts();
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Script::SetTimer(IFunctionHandler *pH,int nMilliseconds,HSCRIPTFUNCTION hFunc )
{
	SmartScriptTable pUserData;
	bool bUpdateDuringPause = false;
	if (pH->GetParamCount() > 2)
	{
		pH->GetParam(3,pUserData);
	}
	if (pH->GetParamCount() > 3)
	{
		pH->GetParam(4,bUpdateDuringPause);
	}
	ScriptTimer timer;
	timer.bUpdateDuringPause = bUpdateDuringPause;
	timer.sFuncName[0] = 0;
	timer.pScriptFunction = hFunc;
	timer.pUserData = pUserData;
	timer.nMillis = nMilliseconds;

	int nTimerId = ((CScriptSystem*)m_pSS)->GetScriptTimerMgr()->AddTimer( timer );
	ScriptHandle timerHandle;
	timerHandle.n = nTimerId;

	return pH->EndFunction(timerHandle);
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Script::SetTimerForFunction(IFunctionHandler *pH,int nMilliseconds,const char *sFunctionName )
{
	SmartScriptTable pUserData;
	bool bUpdateDuringPause = false;
	if (pH->GetParamCount() > 2)
	{
		pH->GetParam(3,pUserData);
	}
	if (pH->GetParamCount() > 3)
	{
		pH->GetParam(4,bUpdateDuringPause);
	}
	ScriptTimer timer;
	timer.bUpdateDuringPause = bUpdateDuringPause;
	strncpy( timer.sFuncName,sFunctionName,sizeof(timer.sFuncName)-1 );
	timer.pScriptFunction = 0;
	timer.pUserData = pUserData;
	timer.nMillis = nMilliseconds;

	int nTimerId = ((CScriptSystem*)m_pSS)->GetScriptTimerMgr()->AddTimer( timer );
	ScriptHandle timerHandle;
	timerHandle.n = nTimerId;

	return pH->EndFunction(timerHandle);
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Script::KillTimer(IFunctionHandler *pH,ScriptHandle nTimerId )
{
	int nid = nTimerId.n;
	((CScriptSystem*)m_pSS)->GetScriptTimerMgr()->RemoveTimer(nid);
	return pH->EndFunction();
}
