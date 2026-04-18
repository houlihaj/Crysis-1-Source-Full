// ScriptSystem.cpp: implementation of the CScriptSystem class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#ifdef DEBUG_LUA_STATE
#include <IEntity.h>
#include <IEntitySystem.h>
#endif


#include <string.h>
#include <stdio.h>
#include "ScriptSystem.h"
#include "ScriptTable.h"
#include "StackGuard.h"
#include "BucketAllocator.h"

#include <ISystem.h> // For warning and errors.
// needed for crypak
#include <ICryPak.h>
#include <IConsole.h>
#include <ILog.h>
#include <IDataProbe.h>
#include <ITimer.h>
#include <IAISystem.h>
#include <MTPseudoRandom.h>

#include <ISerialize.h>

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lobject.h"
LUALIB_API void lua_bitlibopen (lua_State *L);
}

#include "LuaCryPakIO.h"

#ifdef WIN32
#include "LuaDebugger/LuaDbgInterface.h"
#include "LuaDebugger/LUADBG.h"
extern HANDLE gDLLHandle;
#endif

//////////////////////////////////////////////////////////////////////////
// Pools
//////////////////////////////////////////////////////////////////////////
CScriptTable_PoolAlloc *g_pScriptTablePoolAlloc = 0;

namespace
{
	void LuaDebuggerShow(IConsoleCmdArgs* /* pArgs */)
	{
		gEnv->pScriptSystem->ShowDebugger(NULL, 0, "");
	}
	void LuaDumpState(IConsoleCmdArgs* /* pArgs */)
	{
		((CScriptSystem*)gEnv->pScriptSystem)->DumpStateToFile("LuaState.txt");
	}

	inline CScriptTable* AllocTable() { return new CScriptTable; }
	inline void FreeTable( CScriptTable *pTable ) { /*delete pTable;*/ }
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
int g_dumpStackOnAlloc = 0;
int g_nPrecaution = 0; // will cause delayed crash, will make engine extremelly unstable.

// Global Lua debugger pointer (if initialized)
CLUADbg *g_pLuaDebugger = 0;

CScriptTable* CScriptSystem::m_pWeakRefsTable = NULL;

// Script memory allocator.
//PageBucketAllocatorForLua gLuaAlloc;

#include "CryMemoryAllocator.h"

//template<>
#ifdef _DEBUG
class lua_allocator : public node_alloc<eCryDefaultMalloc, false, 524288>
#else //_DEBUG
class lua_allocator : public node_alloc<eCryMallocCryFreeAll, false, 524288>
#endif //_DEBUG
{
public:
	static void *re_alloc(void *ptr, size_t osize, size_t nsize)
	{

		if (NULL == ptr)
		{
			if (nsize)
			{
				m_iAllocated += nsize;
				return alloc(nsize);
			}

			return 0;
		}
		/*
		else if (osize > _MAX_BYTES && nsize > _MAX_BYTES)
		{
			m_iAllocated += nsize;
			m_iAllocated -= osize;
			void *nptr = 0;
			nptr = (char*)alloc(nsize);// + g_nPrecaution;
			memcpy(nptr, ptr, nsize>osize ? osize : nsize);
			dealloc(ptr, osize);
			return nptr;

			// realloc 
			//return realloc(ptr,nsize); 
		}*/
		else 
		{
			void *nptr = 0;
			if (nsize)
			{
				nptr = (char*)alloc(nsize) + g_nPrecaution;
				memcpy(nptr, ptr, nsize>osize ? osize : nsize);
				m_iAllocated += nsize;
			}
			dealloc(ptr, osize);
			m_iAllocated -= osize;
			return nptr;
		}
	}

	static	size_t get_alloc_size() { return m_iAllocated; }

private:
	static size_t m_iAllocated;
};
size_t lua_allocator::m_iAllocated = 0;

static lua_allocator gLuaAlloc;

CScriptSystem* CScriptSystem::s_mpScriptSystem = NULL;

extern CMTRand_int32 g_random_generator;

//////////////////////////////////////////////////////////////////////
extern "C"
{
	int vl_initvectorlib(lua_State *L);

	static lua_State* g_LStack = 0;

	//////////////////////////////////////////////////////////////////////////
	void DumpCallStack( lua_State *L )
	{
		lua_Debug ar;

		memset(&ar,0,sizeof(lua_Debug));

		//////////////////////////////////////////////////////////////////////////
		// Print callstack.
		//////////////////////////////////////////////////////////////////////////
		int level = 0;
		while (lua_getstack(L, level++, &ar))
		{
			const char *slevel = "";
			if (level == 1)
				slevel = "  ";
			int nRes = lua_getinfo(L, "lnS", &ar);
			if (ar.name)
				CryLog( "$6%s    > %s, (%s: %d)",slevel,ar.name,ar.short_src ,ar.currentline );
			else
				CryLog( "$6%s    > (null) (%s: %d)",slevel,ar.short_src ,ar.currentline );
		}
	}

	void *custom_lua_alloc (void *ud, void *ptr, size_t osize, size_t nsize)
	{
		if (g_dumpStackOnAlloc)
			DumpCallStack(g_LStack);

	#if !defined(NOT_USE_CRY_MEMORY_MANAGER) || defined(PS3)
		return gLuaAlloc.re_alloc(ptr,osize,nsize);
	#endif
		(void)ud;
		(void)osize;
		if (nsize == 0) {
			free(ptr);
			return NULL;
		}
		else 
			return realloc(ptr, nsize);
	}
	static int cutsom_lua_panic (lua_State *L)
	{
		ScriptWarning( "PANIC: unprotected error during Lua-API call\n" );
		DumpCallStack( L );
		return 0;
	}
	
	// Random function used by lua.
	float script_frand0_1()
	{
		return g_random_generator.GenerateFloat();
	}
}

//////////////////////////////////////////////////////////////////////////
// For debugger.
//////////////////////////////////////////////////////////////////////////
IScriptTable *CScriptSystem::GetLocalVariables( int nLevel )
{
	lua_Debug ar;
	nLevel=0;
	const char *name;
	lua_newtable(L);
	int nTable=lua_ref(L,1);
	lua_getref(L,nTable);
	
	IScriptTable *pObj = CreateTable(true);
	pObj->AddRef();
	AttachTable(pObj);

	while(lua_getstack(L, nLevel, &ar) != 0)
	{
		//return 0; /* failure: no such level in the stack */

		//create a table and fill it with the local variables (name,value)

		int i = 1;
		while ((name = lua_getlocal(L, &ar, i)) != NULL) 
		{
			lua_getref(L,nTable);
			lua_pushstring(L,name);
			//::OutputDebugString(name);
			//::OutputDebugString("\n");
			lua_pushvalue(L,-3);
			lua_rawset(L,-3);
			//pop table and value
			lua_pop(L,2);
			i++;
		}
		nLevel++;
	}

	return pObj;
}

#define LEVELS1	12	/* size of the first part of the stack */
#define LEVELS2	10	/* size of the second part of the stack */

void CScriptSystem::GetCallStack( std::vector<SLuaStackEntry> &callstack )
{
	callstack.clear();

	int level = 0; 
	int firstpart = 1;  /* still before eventual `...' */
	lua_Debug ar;
	int index=0;
	while (lua_getstack(L, level++, &ar)) {

		char buff[512];  /* enough to fit following `sprintf's */
		if (level == 2)
		{
			//luaL_addstring(&b, ("stack traceback:\n"));
		}
		else if (level > LEVELS1 && firstpart)
		{
			/* no more than `LEVELS2' more levels? */
			if (!lua_getstack(L, level+LEVELS2, &ar))
				level--;  /* keep going */
			else {
				//		luaL_addstring(&b, ("       ...\n"));  /* too many levels */
				while (lua_getstack(L, level+LEVELS2, &ar))  /* find last levels */
					level++;
			}
			firstpart = 0;
			continue;
		}

		sprintf(buff, ("%4d:  "), level-1);

		lua_getinfo(L, ("Snl"), &ar);
		switch (*ar.namewhat)
		{
		case 'l':
			sprintf(buff, "function[local] `%.50s'", ar.name);
			break;
		case 'g':
			sprintf(buff, "function[global] `%.50s'", ar.name);
			break;
		case 'f':  /* field */
			sprintf(buff, "field `%.50s'", ar.name);
			break;
		case 'm':  /* field */
			sprintf(buff, "method `%.50s'", ar.name);
			break;
		case 't':  /* tag method */
			sprintf(buff, "`%.50s' tag method", ar.name);
			break;
		default: 
			strcpy(buff,"");
		}

		SLuaStackEntry se;
		se.description = buff;
		se.line = ar.currentline;
		se.source = ar.source;

		callstack.push_back(se);
	}
}

bool CScriptSystem::IsCallStackEmpty(void)
{
	lua_Debug ar;
	return (lua_getstack(L, 0, &ar) == 0);
}


int listvars(lua_State *L, int level) 
{
	char sTemp[1000];
	lua_Debug ar;
	int i = 1;
	const char *name;
	if (lua_getstack(L, level, &ar) == 0)
		return 0; /* failure: no such level in the stack */
	while ((name = lua_getlocal(L, &ar, i)) != NULL) 
	{
		sprintf(sTemp, "%s =", name);
		OutputDebugString(sTemp);


		if (lua_isnumber(L, i))
		{
			int n = (int)lua_tonumber(L, i);
			itoa(n, sTemp, 10);
			OutputDebugString(sTemp);
		}
		else if (lua_isstring(L, i))
		{
			OutputDebugString(lua_tostring(L, i));
		}else
			if (lua_isnil(L, i))
			{
				OutputDebugString("nil");
			}
			else
			{
				OutputDebugString("<<unknown>>");
			}
			OutputDebugString("\n");
			i++;
			lua_pop(L, 1); /* remove variable value */		
	}
	/*lua_getglobals(L);
	i = 1;
	while ((name = lua_getlocal(L, &ar, i)) != NULL) 
	{
	sprintf(sTemp, "%s =", name);
	OutputDebugString(sTemp);


	if (lua_isnumber(L, i))
	{
	int n = (int)lua_tonumber(L, i);
	itoa(n, sTemp, 10);
	OutputDebugString(sTemp);
	}
	else if (lua_isstring(L, i))
	{
	OutputDebugString(lua_tostring(L, i));
	}else
	if (lua_isnil(L, i))
	{
	OutputDebugString("nil");
	}
	else
	{
	OutputDebugString("<<unknown>>");
	}
	OutputDebugString("\n");
	i++;
	lua_pop(L, 1); 
	}*/

	return 1;
}

static void LuaDebugHook( lua_State *L,lua_Debug *ar )
{
#ifdef WIN32

	if (g_pLuaDebugger)
		g_pLuaDebugger->OnDebugHook( L,ar );
#endif
}

/*
static void callhook(lua_State *L, lua_Debug *ar)
{
	CScriptSystem *pSS = (CScriptSystem*)gEnv->pScriptSystem;
	if(pSS->m_bsBreakState!=bsStepInto)return;
	lua_getinfo(L, "Sl", ar);
	ScriptDebugInfo sdi;
	pSS->m_sLastBreakSource = sdi.sSourceName = ar->source;
	pSS->m_nLastBreakLine =	sdi.nCurrentLine = ar->currentline;
	
	pSS->ShowDebugger( sdi.sSourceName,sdi.nCurrentLine, "Breakpoint Hit");
}

static void linehook(lua_State *L, lua_Debug *ar)
{
	CScriptSystem *pSS=(CScriptSystem*)gEnv->pScriptSystem;

	if(pSS->m_bsBreakState!=bsNoBreak)
	{
		switch(pSS->m_bsBreakState)
		{
		case bsContinue:
			if(pSS->m_BreakPoint.nLine==ar->currentline)
			{
				lua_getinfo(L, "Sl", ar);
				if(ar->source)
				{
					if(stricmp(ar->source,pSS->m_BreakPoint.sSourceFile.c_str())==0)
						break;
				}
			}
			return;
		case bsStepNext:
		case bsStepInto:
			if(pSS->m_BreakPoint.nLine!=ar->currentline)
			{
				lua_getinfo(L, "Sl", ar);
				if((stricmp(pSS->m_sLastBreakSource.c_str(),ar->source)==0)){
					break;
				}
			}
			return;

			/*lua_getinfo(L, "S", ar);
			if(ar->source)
			{
			if(pSS->m_BreakPoint.nLine!=ar->currentline && (stricmp(pSS->m_sLastBreakSource.c_str(),ar->source)!=0))
			break;
			}
			return;*//*
		default:
			return;
		};
		ScriptDebugInfo sdi;
		pSS->m_sLastBreakSource = sdi.sSourceName = ar->source;
		pSS->m_nLastBreakLine =	sdi.nCurrentLine = ar->currentline;
		pSS->ShowDebugger( sdi.sSourceName,sdi.nCurrentLine, "Breakpoint Hit");
	}

}
*/

//////////////////////////////////////////////////////////////////////////
const char* FormatPath( const char* sPath )
{
	static char sLowerName[_MAX_PATH];
	strcpy(sLowerName, sPath );
	int i = 0;
	while (sLowerName[i] != 0)
	{
		if (sLowerName[i] == '\\')
			sLowerName[i] = '/';
		i++;
	}
	return sLowerName;
}

/* For LuaJIT
static void l_message (const char *pname, const char *msg) {
	if (pname) fprintf(stderr, "%s: ", pname);
	fprintf(stderr, "%s\n", msg);
	fflush(stderr);
}

static const char *progname = "CryScriptSystem";

static int report (lua_State *L, int status) {
	if (status && !lua_isnil(L, -1)) {
		const char *msg = lua_tostring(L, -1);
		if (msg == NULL) msg = "(error object is not a string)";
		l_message(progname, msg);
		lua_pop(L, 1);
	}
	return status;
}

// ---- start of LuaJIT extensions
static int loadjitmodule (lua_State *L, const char *notfound)
{
	lua_getglobal(L, "require");
	lua_pushliteral(L, "jit.");
	lua_pushvalue(L, -3);
	lua_concat(L, 2);
	if (lua_pcall(L, 1, 1, 0)) {
		const char *msg = lua_tostring(L, -1);
		if (msg && !strncmp(msg, "module ", 7)) {
			l_message(progname, notfound);
			return 1;
		}
		else
			return report(L, 1);
	}
	lua_getfield(L, -1, "start");
	lua_remove(L, -2);  // drop module table 
	return 0;
}

// start optimizer 
static int dojitopt (lua_State *L, const char *opt) {
	lua_pushliteral(L, "opt");
	if (loadjitmodule(L, "LuaJIT optimizer module not installed"))
		return 1;
	lua_remove(L, -2);  // drop module name
	if (*opt) lua_pushstring(L, opt);
	return report(L, lua_pcall(L, *opt ? 1 : 0, 0, 0));
}

// JIT engine control command: try jit library first or load add-on module
static int dojitcmd (lua_State *L, const char *cmd) {
	const char *val = strchr(cmd, '=');
	lua_pushlstring(L, cmd, val ? val - cmd : strlen(cmd));
	lua_getglobal(L, "jit");  // get jit.* table
	lua_pushvalue(L, -2);
	lua_gettable(L, -2);  // lookup library function
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);  // drop non-function and jit.* table, keep module name
		if (loadjitmodule(L, "unknown luaJIT command"))
			return 1;
	}
	else {
		lua_remove(L, -2);  // drop jit.* table
	}
	lua_remove(L, -2);  // drop module name
	if (val) lua_pushstring(L, val+1);
	return report(L, lua_pcall(L, val ? 1 : 0, 0, 0));
}
*/

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
CScriptSystem::CScriptSystem()
{
	L = NULL;
	m_pLuaDebugger = NULL;
	m_lastGCTime = 0;
	m_fGCFreq = 10;
	m_nLastGCCount = 0;
	m_pErrorHandlerFunc = NULL;
	m_pScriptTimerMgr = 0;
	m_forceReloadCount = 0;
	s_mpScriptSystem = this; // Should really be checking here...
	g_pScriptTablePoolAlloc = new CScriptTable_PoolAlloc;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
CScriptSystem::~CScriptSystem()
{
	m_pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	delete m_pScriptTimerMgr;

#ifdef WIN32
	if (m_pLuaDebugger)
	{
		delete m_pLuaDebugger;
		m_pLuaDebugger = NULL;
		g_pLuaDebugger = NULL;
	}
#endif

	m_stdScriptBinds.Done();

	SAFE_RELEASE(m_pWeakRefsTable);

	/*	while (!m_stkPooledWeakObjects.empty())
	{
	delete m_stkPooledWeakObjects.top();
	m_stkPooledWeakObjects.pop();
	}

	while (!m_stkPooledWeakObjectsEmpty.empty())
	{
	delete m_stkPooledWeakObjectsEmpty.top();
	m_stkPooledWeakObjectsEmpty.pop();
	}
	*/
	if (L)
	{
		lua_close(L); 

		L = NULL;
	}

	delete g_pScriptTablePoolAlloc;
}

//////////////////////////////////////////////////////////////////////
bool CScriptSystem::Init( ISystem *pSystem,bool bStdLibs,int nStackSize )
{
	m_pSystem = pSystem;
	m_pScriptTimerMgr = new CScriptTimerMgr( this );

	m_pSystem->GetISystemEventDispatcher()->RegisterListener(this);

	//L = lua_open();
	L = lua_newstate( custom_lua_alloc,NULL );
	lua_atpanic(L, &cutsom_lua_panic);
	
	g_LStack = L;
	CScriptTable::L = L; // Set lua state for script table class.
	CScriptTable::m_pSS = this;

	if (bStdLibs)
	{
		luaL_openlibs(L);
	}
	lua_bitlibopen(L);
	vl_initvectorlib(L);

	// For LuaJIT
	//dojitopt(L,"2");
	//dojitcmd(L,"trace");
	//dojitcmd(L,"dumphints=ljit_dumphints.txt");
	//dojitcmd(L,"dump=ljit_dump.txt");


	CreateMetatables();

	m_pWeakRefsTable = (CScriptTable*)CreateTable();
	m_pWeakRefsTable->AddRef();

	m_stdScriptBinds.Init( GetISystem(),this );

	// Set global time variable into the script.
	SetGlobalValue( "_time", 0 );
	SetGlobalValue( "_frametime",0 );
	SetGlobalValue( "_aitick",0 );
	m_nLastGCCount = GetCGCount();

	// Make the error handler available to LUA
	RegisterErrorHandler();

	// Register the command for showing the lua debugger
	IConsole *pConsole = gEnv->pConsole;
	pConsole->AddCommand( "lua_debugger_show", LuaDebuggerShow );
	pConsole->AddCommand( "lua_dump_state", LuaDumpState );

	// Publish the debugging mode as console variable
	m_cvar_script_debugger = pConsole->RegisterInt("lua_debugger",0,VF_CHEAT,
		"Enables the script debugger.\n"
		"1 to trigger on breakpoints and errors\n"
		"2 to only trigger on errors\n"
		"Usage: lua_debugger [0/1/2]\n",
		CScriptSystem::DebugModeChange);

	// Publish the debugging mode as console variable
	pConsole->RegisterInt( "lua_StopOnError",0,VF_CHEAT,"Stops on error.\n" );

	// Ensure the debugger is in the correct mode
	EnableDebugger( (ELuaDebugMode) m_cvar_script_debugger->GetIVal() );

	//DumpStateToFile( "LuaState.txt" );


	//////////////////////////////////////////////////////////////////////////
	// Execute common lua file.
	//////////////////////////////////////////////////////////////////////////
	ExecuteFile("scripts/common.lua",true,false);

	//initvectortag(L);
	return L?true:false;
}

void CScriptSystem::DebugModeChange(ICVar *cvar) {
	// Update the mode of the debugger
	if (s_mpScriptSystem)
		s_mpScriptSystem->EnableDebugger( (ELuaDebugMode) cvar->GetIVal() );
}

void CScriptSystem::PostInit()
{
	//////////////////////////////////////////////////////////////////////////
	// Register console vars.
	//////////////////////////////////////////////////////////////////////////
	if (gEnv->pConsole)
	{
		gEnv->pConsole->Register( "lua_stackonmalloc",&g_dumpStackOnAlloc,0 );
	}
}

// Make a new weak reference.
int  CScriptSystem::weak_ref( lua_State *L )
{
	lua_getref(L, m_pWeakRefsTable->GetRef());
	return luaL_ref( L,-1 );
}

// Removes a weak reference.
void CScriptSystem::weak_unref( lua_State *L,int ref )
{
	lua_getref(L, m_pWeakRefsTable->GetRef());
	luaL_unref(L, -1,ref );
}

// Push a weak reference on Lua stack.
void CScriptSystem::weak_getref( lua_State *L,int ref )
{
	lua_getref(L, m_pWeakRefsTable->GetRef());
	lua_rawgeti(L, -1, ref );
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::EnableDebugger( ELuaDebugMode eDebugMode )
	{
#ifdef WIN32
	// Create the debugger if need be
	if ( eDebugMode != eLDM_NoDebug && !m_pLuaDebugger )
		{
			m_pLuaDebugger = new CLUADbg(this);
			g_pLuaDebugger = m_pLuaDebugger;
		}

	// Set hooks
	if (eDebugMode == eLDM_FullDebug)
		// Enable
		lua_sethook(L, LuaDebugHook, LUA_MASKCALL|LUA_MASKLINE|LUA_MASKRET,0);
	else
		// Disable
		lua_sethook(L, LuaDebugHook,0,0 );

	// Error handler takes care of itself by checking the cvar
#endif // WIN32
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::LogStackTrace()
{
	::DumpCallStack(L);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
int CScriptSystem::ErrorHandler(lua_State *L)
{
	// Handle error
	const char *sFuncName = NULL;
	const char *sSourceFile = NULL;
	lua_Debug ar;
	int nRes;
	CScriptSystem *pThis = (CScriptSystem *)gEnv->pScriptSystem;

	memset(&ar,0,sizeof(lua_Debug));

	const char *sErr = lua_tostring(L, 1);

	if (sErr)
	{
		ScriptWarning( "[Lua Error] %s",sErr );
	}

	//////////////////////////////////////////////////////////////////////////
	// Print error callstack.
	//////////////////////////////////////////////////////////////////////////
	int level = 1;
	while (lua_getstack(L, level++, &ar))
	{
		nRes = lua_getinfo(L, "lnS", &ar);
		if (ar.name)
			CryLog( "$6    > %s, (%s: %d)",ar.name,ar.short_src ,ar.currentline );
		else
			CryLog( "$6    > (null) (%s: %d)",ar.short_src ,ar.currentline );
	}

	if (sErr)
	{
		ICVar *lua_StopOnError = gEnv->pConsole->GetCVar("lua_StopOnError");
		if (lua_StopOnError && lua_StopOnError->GetIVal() != 0)
		{
			ScriptWarning( "![Lua Error] %s",sErr );
		}
	}

	// Check debugging mode
	ELuaDebugMode eMode = eLDM_NoDebug;
	ICVar *pCVar = gEnv->pConsole->GetCVar("lua_debugger");
	if (pCVar) 
		eMode = (ELuaDebugMode) pCVar->GetIVal();

	//////////////////////////////////////////////////////////////////////////
	// Display debugger
	//////////////////////////////////////////////////////////////////////////
	if (eMode != eLDM_NoDebug)
	{
		int level = 1;
		if (lua_getstack(L, level++, &ar))
		{
			if (lua_getinfo(L, "lnS", &ar))
			{
				pThis->ShowDebugger( ar.source,ar.currentline,sErr );
			}
		}
	}
	return 0;
}

void CScriptSystem::RegisterErrorHandler()
{
	// Legacy approach
	/*
	if(bDebugger)
	{
		//lua_register(L, LUA_ERRORMESSAGE, CScriptSystem::ErrorHandler );
		//lua_setglobal(L, LUA_ERRORMESSAGE);
	}
	else
	{
		//lua_register(L, LUA_ERRORMESSAGE, CScriptSystem::ErrorHandler );
		
		//lua_newuserdatabox(L, this);
		//lua_pushcclosure(L, CScriptSystem::ErrorHandler, 1);
		//lua_setglobal(L, LUA_ALERT);
		//lua_pushcclosure(L, errorfb, 0);
		//lua_setglobal(L, LUA_ERRORMESSAGE);
	}
		*/

	// Register global error handler.
	// This just makes it available - when we call LUA, we insert it so it will be called
	if (!m_pErrorHandlerFunc)
	{
	lua_pushcfunction(L,CScriptSystem::ErrorHandler);
	m_pErrorHandlerFunc = (HSCRIPTFUNCTION)(INT_PTR)lua_ref(L,1);
}
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::CreateMetatables()
{
	m_pUserDataMetatable = CreateTable();
	m_pUserDataMetatable->AddRef();

	/*
	m_pUserDataMetatable->AddFunction( )

	// Make Garbage collection for user data metatable.
	lua_newuserdatabox(L, this);
	lua_pushcclosure(L, CScriptSystem::GCTagHandler, 1);
	lua_settagmethod(L, m_nGCTag, "gc");
	*/
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
IScriptTable* CScriptSystem::CreateTable( bool bEmpty )
{
	CScriptTable *pObj = AllocTable();
	if (!bEmpty)
	{
		pObj->CreateNew();
	}
	return pObj;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::_ExecuteFile(const char *sFileName, bool bRaiseError)
{
	FILE *pFile = NULL;

	//#ifdef USE_CRYPAK
	ICryPak *pPak=gEnv->pCryPak;
	pFile = pPak->FOpen(sFileName, "rb");
	//#else
	//	pFile = fxopen(sFileName, "rb"); 
	//#endif

	if (!pFile)
	{
		if (bRaiseError)
			ScriptWarning( "[Lua Error] Failed to load script file %s",sFileName );

		return false;
	}

	int nSize=0;

	//#ifdef USE_CRYPAK
	pPak->FSeek(pFile, 0, SEEK_END); 
	nSize = pPak->FTell(pFile); 
	pPak->FSeek(pFile, 0, SEEK_SET); 
	if (nSize==0)
	{
		pPak->FClose(pFile); 
		return (false);
	}	
	/*
	#else
	fseek(pFile, 0, SEEK_END);		
	nSize = ftell(pFile);		
	fseek(pFile, 0, SEEK_SET);
	if (nSize == 0)
	{
	fclose(pFile);
	return false;
	}		
	#endif
	*/

	char *pBuffer;	
	pBuffer = new char[nSize];

	//#ifdef USE_CRYPAK
	if (pPak->FRead(pBuffer, nSize, pFile) == 0) 
	{		
		delete [] pBuffer;
		pPak->FClose(pFile); 
		return false;
	}
	pPak->FClose(pFile); 
	/*
	#else
	if (fread(pBuffer, nSize, 1, pFile) == 0)
	{		
	fclose(pFile);
	return false;
	}

	fclose(pFile);
	#endif		
	*/

	/*
	char script_decr_key[32] = "3CE2698701289029F3926DF0191189A";
	//////////////////////////////////////////////////////////////////////////
	// Check if it is encrypted script.
	//////////////////////////////////////////////////////////////////////////
	string CRY_ENCRYPT_FILE_HEADER = string("")+"c"+"r"+"y"+"e"+"h"+"d"+"r";
	if (nSize > (int)CRY_ENCRYPT_FILE_HEADER.size())
	{
		if (memcmp(pBuffer,CRY_ENCRYPT_FILE_HEADER.c_str(),CRY_ENCRYPT_FILE_HEADER.size()) == 0)
		{
			// Decrypt this buffer.
			GetISystem()->GetIDataProbe()->AESDecryptBuffer( pBuffer,nSize,pBuffer,nSize,script_decr_key );
		}
	}
	*/

	//////////////////////////////////////////////////////////////////////////
	//if (m_bDebug)
	//{
		//OnLoadSource(sFileName,(unsigned char*) pBuffer, nSize);
	//}

	//int nRes = lua_dofile(L, sFileName);

	

	// Translate pakalias filenames
	char szTranslatedBuff[_MAX_PATH + 1];
	const char * szTranslated = gEnv->pCryPak->AdjustFileName(sFileName, szTranslatedBuff,ICryPak::FLAGS_NO_FULL_PATH);

	char szFileName[_MAX_PATH + 1];
	szFileName[0] = '@';
	strcpy(&szFileName[1], szTranslated);

	// Finally, make sure the slashes are "lua format" - could be more efficient!
	std::replace( szFileName,szFileName+strlen(szFileName),'\\','/');

	bool bResult = ExecuteBuffer( pBuffer,nSize,szFileName );
	delete [] pBuffer;

	return bResult;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::ExecuteFile(const char *sFileName, bool bRaiseError,bool bForceReload)
{
	if (strlen(sFileName) <= 0)
		return false;

	if (bForceReload)
		m_forceReloadCount++;

	char sTemp[_MAX_PATH];
	strcpy( sTemp,FormatPath(sFileName) );
	//ScriptFileListItor itor = std::find(m_dqLoadedFiles.begin(), m_dqLoadedFiles.end(), sTemp.c_str());
	ScriptFileListItor itor = m_dqLoadedFiles.find(CONST_TEMP_STRING(sTemp));
	if (itor == m_dqLoadedFiles.end() || bForceReload || m_forceReloadCount > 0)
	{
		if (!_ExecuteFile(sTemp, bRaiseError))
		{
			if (itor != m_dqLoadedFiles.end())
				m_dqLoadedFiles.erase(sTemp);
			if (bForceReload)
				m_forceReloadCount--;
			return false;
		}
		if(itor == m_dqLoadedFiles.end())
			m_dqLoadedFiles.insert(sTemp);
	}
	if (bForceReload)
		m_forceReloadCount--;
	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::UnloadScript(const char *sFileName)
{
	if (strlen(sFileName) <= 0)
		return;

	const char *sTemp = FormatPath(sFileName);
	//ScriptFileListItor itor = std::find(m_dqLoadedFiles.begin(), m_dqLoadedFiles.end(), sTemp.c_str());
	ScriptFileListItor itor = m_dqLoadedFiles.find(CONST_TEMP_STRING(sTemp));
	if (itor != m_dqLoadedFiles.end())
	{
		m_dqLoadedFiles.erase(itor);
	}
}
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::UnloadScripts()
{
	m_dqLoadedFiles.clear();
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::ReloadScripts()
{
	ScriptFileListItor itor;
	itor = m_dqLoadedFiles.begin();
	while (itor != m_dqLoadedFiles.end())
	{
		ReloadScript(itor->c_str(), true);
		++itor;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::ReloadScript(const char *sFileName, bool bRaiseError)
{
	return ExecuteFile( sFileName,bRaiseError,false );
}

void CScriptSystem::DumpLoadedScripts()
{
	ScriptFileListItor itor;
	itor = m_dqLoadedFiles.begin();
	while (itor != m_dqLoadedFiles.end())
	{
		CryLogAlways( itor->c_str() );
		++itor;
	}
}

/*
void CScriptSystem::GetScriptHashFunction( IScriptTable &Current, unsigned int &dwHash)
{
unsigned int *pCode=0;
int iSize=0;

if(pCurrent.GetCurrentFuncData(pCode,iSize))						// function ?
{
if(pCode) 																						// lua function ?
GetScriptHashFunction(pCode,iSize,dwHash);
}
}
*/


void CScriptSystem::GetScriptHash( const char *sPath, const char *szKey, unsigned int &dwHash )
{
	//	IScriptTable *pCurrent;



	//	GetGlobalValue(szKey,pCurrent);

	//	if(!pCurrent)		// is not a table
	//	{
	//	}
	/*
	else if(lua_isfunction(L, -1))
	{
	GetScriptHashFunction(*pCurrent,dwHash);

	return;
	}
	else
	{
	lua_pop(L, 1);
	return;
	}
	*/
	/*
	pCurrent->BeginIteration();

	while(pCurrent->MoveNext())
	{
	char *szKeyName;

	if(!pCurrent->GetCurrentKey(szKeyName))
	szKeyName="NO";

	ScriptVarType type=pCurrent->GetCurrentType();

	if(type==svtObject)		// svtNull,svtFunction,svtString,svtNumber,svtUserData,svtObject
	{
	void *pVis;

	pCurrent->GetCurrentPtr(pVis);

	gEnv->pLog->Log("  table '%s/%s'",sPath.c_str(),szKeyName);				

	if(setVisited.count(pVis)!=0)
	{
	gEnv->pLog->Log("    .. already processed ..");				
	continue;
	}

	setVisited.insert(pVis);

	{
	IScriptTable *pNewObject = m_pScriptSystem->CreateEmptyObject();

	pCurrent->GetCurrent(pNewObject);

	Debug_Full_recursive(pNewObject,sPath+string("/")+szKeyName,setVisited);

	pNewObject->Release();
	}
	}
	else if(type==svtFunction)		// svtNull,svtFunction,svtString,svtNumber,svtUserData,svtObject
	{
	GetScriptHashFunction(*pCurrent,dwHash);
	}
	}

	pCurrent->EndIteration();
	*/
}



//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::ExecuteBuffer(const char *sBuffer, size_t nSize,const char *sBufferDescription)
{
	int status = luaL_loadbuffer(L,sBuffer,nSize,sBufferDescription);
	if (status == 0) {  // parse OK?
		int base = lua_gettop(L);  // function index.
		lua_getref(L,(int)(INT_PTR)m_pErrorHandlerFunc);
		lua_insert(L, base);  // put it under chunk and args
		status = lua_pcall(L, 0, LUA_MULTRET, base);  // call main
		lua_remove(L, base);  // remove error handler function.
	}
	if (status != 0)
	{
		const char *sErr = lua_tostring(L, -2);
		if (sBufferDescription && strlen(sBufferDescription) != 0)
			GetISystem()->Warning( VALIDATOR_MODULE_SCRIPTSYSTEM,VALIDATOR_WARNING,VALIDATOR_FLAG_FILE|VALIDATOR_FLAG_SCRIPT,sBufferDescription,
			"[Lua Error] Failed to execute file %s: %s", sBufferDescription, sErr );
		else
			ScriptWarning( "[Lua Error] Error executing lua %s",sErr );
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::Release()
{
	delete this;
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
int CScriptSystem::BeginCall( HSCRIPTFUNCTION hFunc )
{
;	assert( hFunc != 0 );
	if (!hFunc)
		return 0;

	lua_getref(L,(int)(INT_PTR)hFunc);
	if(!lua_isfunction(L,-1))
	{
#if defined(__GNUC__)
		ScriptWarning( "[CScriptSystem::BeginCall] Function Ptr:%d not found",(int)(INT_PTR)hFunc );
#else
		ScriptWarning( "[CScriptSystem::BeginCall] Function Ptr:%d not found",hFunc );
#endif
		m_nTempArg = -1;
		return 0;
	}
	m_nTempArg = 0;

	return 1;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
int CScriptSystem::BeginCall(const char *sTableName, const char *sFuncName)
{
	lua_getglobal(L, sTableName);

	if(!lua_istable(L,-1))
	{
		ScriptWarning("[CScriptSystem::BeginCall] Tried to call %s:%s(), Table %s not found (check for syntax errors or if the file wasn't loaded)",sTableName,sFuncName,sTableName); 
		m_nTempArg = -1;
		lua_pop(L,1);
		return 0;
	}

	lua_pushstring(L, sFuncName);
	lua_gettable(L, - 2);
	lua_remove(L, - 2); // Remove table global.
	m_nTempArg = 0;

	if(!lua_isfunction(L,-1))
	{
		ScriptWarning("[CScriptSystem::BeginCall] Function %s:%s not found(check for syntax errors or if the file wasn't loaded)",sTableName,sFuncName);
		m_nTempArg = -1;
		return 0;
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////
int CScriptSystem::BeginCall( IScriptTable *pTable,const char *sFuncName )
{
	PushTable(pTable);

	lua_pushstring(L, sFuncName);
	lua_gettable(L, - 2);
	lua_remove(L, - 2); // Remove table global.
	m_nTempArg = 0;

	if(!lua_isfunction(L,-1))
	{
		ScriptWarning("[CScriptSystem::BeginCall] Function %s not found in the table",sFuncName);
		m_nTempArg = -1;
		return 0;
	}

	return 1;
}


//////////////////////////////////////////////////////////////////////////
int CScriptSystem::BeginCall(const char *sFuncName)
{
	lua_getglobal(L, sFuncName);
	m_nTempArg = 0;

	if(!lua_isfunction(L,-1))
	{
		ScriptWarning("[CScriptSystem::BeginCall] Function %s not found(check for syntax errors or if the file wasn't loaded)",sFuncName);
		m_nTempArg = -1;

		return 0;
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
HSCRIPTFUNCTION CScriptSystem::GetFunctionPtr(const char *sFuncName)
{
	_GUARD_STACK(L);
	HSCRIPTFUNCTION func;
	lua_getglobal(L, sFuncName);
	if (lua_isnil(L, -1) ||(!lua_isfunction(L, -1)))
	{
		lua_pop(L, 1);
		return NULL;
	}
	func = (HSCRIPTFUNCTION)(INT_PTR)lua_ref(L,1);

	return func;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
HSCRIPTFUNCTION CScriptSystem::GetFunctionPtr(const char *sTableName, const char *sFuncName)
{
	_GUARD_STACK(L);
	HSCRIPTFUNCTION func;
	lua_getglobal(L, sTableName);
	if(!lua_istable(L,-1))
	{
		lua_pop(L,1);
		return 0;
	}
	lua_pushstring(L, sFuncName);
	lua_gettable(L, - 2);
	lua_remove(L, - 2); // Remove table global.
	if (lua_isnil(L, -1) ||(!lua_isfunction(L, -1)))
	{
		lua_pop(L, 1);
		return FALSE;
	}
	func = (HSCRIPTFUNCTION)(INT_PTR)lua_ref(L,1);
	return func;
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::PushAny( const ScriptAnyValue &var )
{
	switch (var.type)
	{
	case ANY_ANY:
	case ANY_TNIL:
		lua_pushnil(L);
		break;;
	case ANY_TBOOLEAN:
		lua_pushboolean(L,var.b);
		break;;
	case ANY_THANDLE:
		lua_pushlightuserdata(L,const_cast<void*>(var.ptr));
		break;
	case ANY_TNUMBER:
		lua_pushnumber(L,var.number);
		break;
	case ANY_TSTRING:
		lua_pushstring(L,var.str);
		break;
	case ANY_TTABLE:
		if (var.table)
			PushTable(var.table);
		else
			lua_pushnil(L);
		break;
	case ANY_TFUNCTION:
		lua_getref(L,(int)(INT_PTR)var.function);
		break;
	case ANY_TUSERDATA:
		lua_getref(L,var.ud.nRef);
		break;
	case ANY_TVECTOR:
		PushVec3(Vec3(var.vec3.x,var.vec3.y,var.vec3.z));
		break;
	default:
		// Must handle everything.
		assert(0);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::ToAny( ScriptAnyValue &var,int index )
{
	if (!lua_gettop(L))
		return false;

	CHECK_STACK(L);

	if (var.type == ANY_ANY)
	{
		switch (lua_type(L,index))
		{
		case LUA_TNIL:
			var.type = ANY_TNIL;
			break;
		case LUA_TBOOLEAN:
			var.b = lua_toboolean(L, index) != 0;
			var.type = ANY_TBOOLEAN;
			break;
		case LUA_TLIGHTUSERDATA:
			var.ptr = lua_topointer(L, index);
			var.type = ANY_THANDLE;
			break;
		case LUA_TNUMBER:
			var.number = (float)lua_tonumber(L, index);
			var.type = ANY_TNUMBER;
			break;
		case LUA_TSTRING:
			var.str = lua_tostring(L, index);
			var.type = ANY_TSTRING;
			break;
		case LUA_TTABLE:
		case LUA_TUSERDATA:
			if (!var.table)
			{
				var.table = AllocTable();
				var.table->AddRef();
			}
			lua_pushvalue(L,index);
			AttachTable(var.table);
			var.type = ANY_TTABLE;
			break;
		case LUA_TFUNCTION:
			{
				var.type = ANY_TFUNCTION;
				// Make reference to function.
				lua_pushvalue(L,index);
				var.function = (HSCRIPTFUNCTION)(INT_PTR)lua_ref(L,1);
			}
			break;
		case LUA_TTHREAD:
		default:
			return false;
		}
		return true;
	}
	else
	{
		bool res = false;
		switch (lua_type(L,index))
		{
		case LUA_TNIL:
			if (var.type == ANY_TNIL)
				res = true;
			break;
		case LUA_TBOOLEAN:
			if (var.type == ANY_TBOOLEAN)
			{
				var.b = lua_toboolean(L, index) != 0;
				res = true;
			}
			break;
		case LUA_TLIGHTUSERDATA:
			if (var.type == ANY_THANDLE)
			{
				var.ptr = lua_topointer(L, index);
				res = true;
			}
			break;
		case LUA_TNUMBER:
			if (var.type == ANY_TNUMBER)
			{
				var.number = (float)lua_tonumber(L, index);
				res = true;
			}
			else if (var.type == ANY_TBOOLEAN)
			{
				var.b = lua_tonumber(L, index) != 0;
				res = true;
			}
			break;
		case LUA_TSTRING:
			if (var.type == ANY_TSTRING)
			{
				var.str = lua_tostring(L, index);
				res = true;
			}
			break;
		case LUA_TTABLE:
			if (var.type == ANY_TTABLE)
			{
				if (!var.table)
				{
					var.table = AllocTable();
					var.table->AddRef();
				}
				lua_pushvalue(L,index);
				AttachTable(var.table);
				res = true;
			}
			else if (var.type == ANY_TVECTOR)
			{
				Vec3 v(0,0,0);
				if (res = ToVec3(v,index))
				{
					var.vec3.x = v.x;
					var.vec3.y = v.y;
					var.vec3.z = v.z;
				}
			}
			break;
		case LUA_TUSERDATA:
			if (var.type == ANY_TTABLE)
			{
				if (!var.table)
				{
					var.table = AllocTable();
					var.table->AddRef();
				}
				lua_pushvalue(L,index);
				AttachTable(var.table);
				res = true;
			}
			break;
		case LUA_TFUNCTION:
			if (var.type == ANY_TFUNCTION)
			{
				// Make reference to function.
				lua_pushvalue(L,index);
				var.function = (HSCRIPTFUNCTION)(INT_PTR)lua_ref(L,1);
				res = true;
			}
			break;
		case LUA_TTHREAD:
			break;
		}
		return res;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::PopAny( ScriptAnyValue &var )
{
	bool res = ToAny(var,-1);
	lua_pop(L,1);
	return res;
}

/*
#include <signal.h> 
static void cry_lstop (lua_State *l, lua_Debug *ar) {
	(void)ar;  // unused arg. 
	lua_sethook(l, NULL, 0, 0);
	luaL_error(l, "interrupted!");
} 
static void cry_laction (int i)
{
	CScriptSystem *pThis = (CScriptSystem *)gEnv->pScriptSystem;
	lua_State *L = pThis->GetLuaState();
	signal(i, SIG_DFL); // if another SIGINT happens before lstop, terminate process (default action)
	lua_sethook(L, cry_lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}
*/

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CScriptSystem::EndCallN( int nReturns )
{
	if (m_nTempArg < 0)
		return false;

	int base = lua_gettop(L) - m_nTempArg;  // function index.
	lua_getref(L,(int)(INT_PTR)m_pErrorHandlerFunc);
	lua_insert(L, base);  // put it under chunk and args

	//signal(SIGINT, cry_laction);
	int status = lua_pcall(L, m_nTempArg, nReturns, base);
	//signal(SIGINT, SIG_DFL);
	lua_remove(L, base);  // remove error handler function.

	return status == 0;
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::EndCall()
{
	return EndCallN(0);
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::EndCallAny( ScriptAnyValue &any )
{
	//CHECK_STACK(L);
	if (!EndCallN(1))
		return false;
	return PopAny(any);
}

//////////////////////////////////////////////////////////////////////
bool CScriptSystem::EndCallAnyN( int n, ScriptAnyValue* anys )
{
	if (!EndCallN(n))
		return false;
	for (int i=0; i<n; i++)
	{
		if (!PopAny( anys[i] ))
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
void CScriptSystem::PushFuncParamAny( const ScriptAnyValue &any )
{
	if(m_nTempArg==-1)
		return;
	PushAny(any);
	m_nTempArg++;
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::SetGlobalAny( const char *sKey,const ScriptAnyValue &any )
{
	CHECK_STACK(L);
	PushAny(any);
	lua_setglobal(L, sKey);
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::GetRecursiveAny( IScriptTable *pTable,const char *sKey,ScriptAnyValue &any )
{
	char key1[256];
	char key2[256];
	strcpy(key1,sKey);
	key2[0] = 0;

	const char *sep = strchr(sKey,'.');
	if (sep)
	{
		key1[sep-sKey] = 0;
		strcpy(key2,sep+1);
	}

	ScriptAnyValue localAny;
	if (!pTable->GetValueAny(key1,localAny))
		return false;

	if (localAny.type == ANY_TFUNCTION && NULL == sep)
	{
		any = localAny;
		return true;
	}
	else if (localAny.type == ANY_TTABLE && NULL != sep)
	{
		return GetRecursiveAny( localAny.table,key2,any );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::GetGlobalAny( const char *sKey,ScriptAnyValue &any )
{
	CHECK_STACK(L);
	const char *sep = strchr(sKey,'.');
	if (sep)
	{
		ScriptAnyValue globalAny;
		char key1[256];
		strcpy(key1,sKey);
		key1[sep-sKey] = 0;
		GetGlobalAny(key1,globalAny);
		if (globalAny.type == ANY_TTABLE)
		{
			return GetRecursiveAny(globalAny.table,sep+1,any );
		}
		return false;
	}
	
	lua_getglobal(L, sKey);
	if (!PopAny(any))
	{
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::ForceGarbageCollection()
{
	// Do a full garbage collection cycle.
	lua_gc(L,LUA_GCCOLLECT,0);

	/*char sTemp[200];
	lua_StateStats lss;
	lua_getstatestats(L,&lss);
	sprintf(sTemp,"protos=%d closures=%d tables=%d udata=%d strings=%d\n",lss.nProto,lss.nClosure,lss.nHash,lss.nUdata,lss.nString);
	OutputDebugString("BEFORE GC STATS :");
	OutputDebugString(sTemp);*/

	//lua_setgcthreshold(L, 0);

	/*lua_getstatestats(L,&lss);
	sprintf(sTemp,"protos=%d closures=%d tables=%d udata=%d strings=%d\n",lss.nProto,lss.nClosure,lss.nHash,lss.nUdata,lss.nString);
	OutputDebugString("AFTER GC STATS :");
	OutputDebugString(sTemp);*/

}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
int CScriptSystem::GetCGCount()
{
	return lua_getgccount(L);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CScriptSystem::SetGCThreshhold(int nKb)
{
	//lua_setgcthreshold(L, nKb);
}

/*
int CScriptSystem::GCTagHandler(lua_State *L)
{
	CScriptSystem *pThis = (CScriptSystem*)gEnv->pScriptSystem;
	ScriptUserData *udc = (ScriptUserData*)lua_touserdata(L,1);
	if (pThis && pThis->m_pSink && udc)
	{
		pThis->m_pSink->OnCollectUserData(udc->nVal,udc->nCookie);
		weak_unref( L,udc->nRef );
		udc->nVal = udc->nCookie = 0xDEADBEEF;
	}
	return 0;
}
*/

IScriptTable * CScriptSystem::CreateUserData(void * ptr, size_t size)	//AMD Port
{
	void * nptr = lua_newuserdata(L,size);
	memcpy( nptr, ptr, size );
	CScriptTable * pNewTbl = new CScriptTable();
	pNewTbl->Attach();

	return pNewTbl;
}

//////////////////////////////////////////////////////////////////////
HBREAKPOINT CScriptSystem::AddBreakPoint(const char *sFile,int nLineNumber)
{
#ifdef WIN32
	if (m_pLuaDebugger)
		m_pLuaDebugger->AddBreakPoint(sFile,nLineNumber);
#endif
	return 0;
}

void CScriptSystem::ReleaseFunc(HSCRIPTFUNCTION f)
{
	if(f)
	{
		lua_unref(L,(int)(INT_PTR)f);
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::PushTable( IScriptTable *pTable )
{
	((CScriptTable*)pTable)->PushRef();
};

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::AttachTable( IScriptTable *pTable )
{
	((CScriptTable*)pTable)->Attach();
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::PushVec3( const Vec3 &vec )
{
	lua_newtable(L);
	lua_pushlstring(L,"x", 1);
	lua_pushnumber(L,vec.x);
	lua_rawset(L,-3);
	lua_pushlstring(L,"y", 1);
	lua_pushnumber(L,vec.y);
	lua_rawset(L,-3);
	lua_pushlstring(L,"z", 1);
	lua_pushnumber(L,vec.z);
	lua_rawset(L,-3);
}

//////////////////////////////////////////////////////////////////////////
bool CScriptSystem::ToVec3( Vec3 &vec,int tableIndex )
{
	CHECK_STACK(L);

	if (tableIndex < 0)
	{
		tableIndex = lua_gettop(L)+tableIndex+1;
	}

	if (lua_type(L,tableIndex) != LUA_TTABLE)
	{
		return false;
	}

	//int num = luaL_getn(L,index);
	//if (num != 3)
		//return false;
	
	float x,y,z;
	lua_pushlstring(L,"x", 1);
	lua_rawget(L,tableIndex);
	if (!lua_isnumber(L,-1))
	{
		lua_pop(L,1); // pop x value.
		//////////////////////////////////////////////////////////////////////////
		// Try an indexed table.
		lua_rawgeti(L,tableIndex,1);
		if (!lua_isnumber(L,-1))
		{
			lua_pop(L,1); // pop value.
			return false;
		}
		x = lua_tonumber(L,-1);
		lua_rawgeti(L,tableIndex,2);
		if (!lua_isnumber(L,-1))
		{
			lua_pop(L,2); // pop value.
			return false;
		}
		y = lua_tonumber(L,-1);
		lua_rawgeti(L,tableIndex,3);
		if (!lua_isnumber(L,-1))
		{
			lua_pop(L,3); // pop value.
			return false;
		}
		z = lua_tonumber(L,-1);
		lua_pop(L,3); // pop value.

		vec.x = x;
		vec.y = y;
		vec.z = z;
		return true;
		//////////////////////////////////////////////////////////////////////////
	}
	x = lua_tonumber(L,-1);
	lua_pop(L,1); // pop value.

	lua_pushlstring(L,"y", 1);
	lua_rawget(L,tableIndex);
	if (!lua_isnumber(L,-1))
	{
		lua_pop(L,1); // pop table.
		return false;
	}
	y = lua_tonumber(L,-1);
	lua_pop(L,1); // pop value.

	lua_pushlstring(L,"z", 1);
	lua_rawget(L,tableIndex);
	if (!lua_isnumber(L,-1))
	{
		lua_pop(L,1); // pop table.
		return false;
	}
	z = lua_tonumber(L,-1);
	lua_pop(L,1); // pop value.

	vec.x = x;
	vec.y = y;
	vec.z = z;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::ShowDebugger(const char *pszSourceFile, int iLine, const char *pszReason)
{
#ifdef WIN32
	// Create the debugger if need be
	if (!m_pLuaDebugger)
		EnableDebugger( eLDM_OnlyErrors );

	if (m_pLuaDebugger)
	{
		m_pLuaDebugger->InvokeDebugger(pszSourceFile, iLine, pszReason);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CScriptSystem::Update( )
{
	FUNCTION_PROFILER( m_pSystem, PROFILE_SCRIPT );

	//CryGetScr
	//L->l_G->totalbytes = 
	ITimer *pTimer = m_pSystem->GetITimer();
	CTimeValue nCurTime = pTimer->GetFrameStartTime();

	// Enable debugger if needed.
	ELuaDebugMode debugMode = (ELuaDebugMode) m_cvar_script_debugger->GetIVal();
	EnableDebugger(debugMode);
	
	// Might need to check for new lua code needing hooks

	float currTime=pTimer->GetCurrTime();
	float frameTime=pTimer->GetFrameTime();

	IScriptSystem *pScriptSystem = m_pSystem->GetIScriptSystem();

	// Set global time variable into the script.
	pScriptSystem->SetGlobalValue( "_time", currTime );
	pScriptSystem->SetGlobalValue( "_frametime",frameTime );

	{
		int aiTicks = 0;

		IAISystem *pAISystem=m_pSystem->GetAISystem();

		if(pAISystem)
			aiTicks = pAISystem->GetAITickCount();
		pScriptSystem->SetGlobalValue( "_aitick",aiTicks );
	}

	//TRACE("GC DELTA %d",m_pScriptSystem->GetCGCount()-nStartGC);
	//int nStartGC = pScriptSystem->GetCGCount();

	bool bKickIn=false;															// Invoke Gargabe Collector

	if(currTime-m_lastGCTime>m_fGCFreq)	 // g_GC_Frequence->GetIVal())
		bKickIn=true;

	int nGCCount=pScriptSystem->GetCGCount();

	bool bNoLuaGC = false;

	if(nGCCount-m_nLastGCCount>2000 && !bNoLuaGC)		//
		bKickIn=true;

	if(bKickIn)
	{
		FRAME_PROFILER( "Lua GC",m_pSystem,PROFILE_SCRIPT );

		//CryLog( "Lua GC=%d",GetCGCount() );

		//float fTimeBefore=pTimer->GetAsyncCurTime()*1000;
		
		
		//pScriptSystem->ForceGarbageCollection();

		m_nLastGCCount=pScriptSystem->GetCGCount();
		m_lastGCTime = currTime;
		//float fTimeAfter=pTimer->GetAsyncCurTime()*1000;
		//CryLog("--[after coll]GC DELTA %d ",pScriptSystem->GetCGCount()-nGCCount);
		//TRACE("--[after coll]GC DELTA %d [time =%f]",m_pScriptSystem->GetCGCount()-nStartGC,fTimeAfter-fTimeBefore);
	}

	m_pScriptTimerMgr->Update( nCurTime.GetMilliSecondsAsInt64() );
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::SetGCFrequency( const float fRate )
{
	if (fRate >= 0)
		m_fGCFreq = fRate;
	else if (g_nPrecaution == 0)
	{
		g_nPrecaution = 1 + (rand()%3); // lets get nasty.
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::RaiseError( const char *format,... )
{
	va_list	arglist;
	char		sBuf[2048];
	lua_Debug ar;
	int nCurrentLine = 0;
	const char *sFuncName = "undefined";
	const char *sSourceFile = "undefined";

	va_start(arglist, format);
	vsprintf_s(sBuf, format, arglist);
	va_end(arglist);	

	ScriptWarning( "[Lua Error] %s",sBuf );

	// If in debug mode, try to enable debugger.
	if ( (ELuaDebugMode) m_cvar_script_debugger->GetIVal() != eLDM_NoDebug )
	{
		if (lua_getstack(L, 1, &ar))
		{
			if (lua_getinfo(L, "lnS", &ar))
			{
				ShowDebugger( sSourceFile,nCurrentLine,sBuf );
			}
		}
	}
	else
	{
		LogStackTrace();
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::LoadScriptedSurfaceTypes( const char *sFolder,bool bReload )
{
	m_stdScriptBinds.LoadScriptedSurfaceTypes( sFolder,bReload );
}

//////////////////////////////////////////////////////////////////////////
int CScriptSystem::GetStackSize()
{
	return lua_gettop(L);
}

//////////////////////////////////////////////////////////////////////////
uint32 CScriptSystem::GetScriptAllocSize()
{
	//return gLuaAlloc.get_alloc_size();
	return gLuaAlloc.get_alloc_size();
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::GetMemoryStatistics(ICrySizer *pSizer)
{
	{
		SIZER_COMPONENT_NAME(pSizer,"Self");

		m_stdScriptBinds.GetMemoryStatistics(pSizer);
		
		int nSize = 0;
		for (ScriptFileList::iterator it = m_dqLoadedFiles.begin(); it != m_dqLoadedFiles.end(); it++)
		{
			nSize += (*it).size();
		}
		pSizer->AddObject( this,sizeof(*this)+nSize );

		m_pScriptTimerMgr->GetMemoryStatistics( pSizer );
	}
#ifndef _LIB // Only when compiling as dynamic library
	{
		SIZER_COMPONENT_NAME(pSizer,"Strings");
		pSizer->AddObject( (this+1),string::_usedMemory(0) );
	}
#endif
	{
		SIZER_COMPONENT_NAME(pSizer,"Lua");
		pSizer->AddObject( &gLuaAlloc,gLuaAlloc.get_alloc_size() );
	}
	{
		SIZER_COMPONENT_NAME(pSizer,"Script Tables");
		pSizer->AddObject( g_pScriptTablePoolAlloc,g_pScriptTablePoolAlloc->GetTotalAllocatedMemory() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
{
	switch (event)
	{
		case ESYSTEM_EVENT_LEVEL_RELOAD:
			{
				ForceGarbageCollection();
			}
			break;
	}
}

//////////////////////////////////////////////////////////////////////////
struct IRecursiveLuaDump
{
	virtual void OnElement( int nLevel,const char *sKey,int nKey,ScriptAnyValue &value ) = 0;
	virtual void OnBeginTable( int nLevel,const char *sKey,int nKey ) = 0;
	virtual void OnEndTable( int nLevel ) = 0;
};

struct SRecursiveLuaDumpToFile : public IRecursiveLuaDump
{
	FILE *file;
	char sLevelOffset[1024];
	char sKeyStr[32];
	int nSize;

	const char *GetOffsetStr( int nLevel )
	{
		if (nLevel > sizeof(sLevelOffset)-1)
			nLevel = sizeof(sLevelOffset)-1;
		memset(sLevelOffset,'\t',nLevel);
		sLevelOffset[nLevel] = 0;
		return sLevelOffset;
	}
	const char *GetKeyStr( const char *sKey,int nKey )
	{
		if (sKey)
			return sKey;
		sprintf( sKeyStr,"[%02d]",nKey );
		return sKeyStr;
	}
	SRecursiveLuaDumpToFile( const char *filename )
	{
		file = fxopen(filename,"wt");
		nSize = 0;
	}
	~SRecursiveLuaDumpToFile()
	{
		if (file)
			fclose(file);
	}
	virtual void OnElement( int nLevel,const char *sKey,int nKey,ScriptAnyValue &value )
	{
		nSize += sizeof(Node);
		if (sKey)
			nSize += strlen(sKey)+1;
		else
			nSize += sizeof(int);
		switch (value.type)
		{
		case ANY_TBOOLEAN:
			if (value.b)
				fprintf( file,"[%6d] %s %s=true\n",nSize,GetOffsetStr(nLevel),GetKeyStr(sKey,nKey) );
			else
				fprintf( file,"[%6d] %s %s=false\n",nSize,GetOffsetStr(nLevel),GetKeyStr(sKey,nKey) );
			break;
		case ANY_THANDLE:
			fprintf( file,"[%6d] %s %s=%X\n",nSize,GetOffsetStr(nLevel),GetKeyStr(sKey,nKey),(uintptr_t)value.ptr );
			break;
		case ANY_TNUMBER:
			fprintf( file,"[%6d] %s %s=%g\n",nSize,GetOffsetStr(nLevel),GetKeyStr(sKey,nKey),value.number );
			break;
		case ANY_TSTRING:
			fprintf( file,"[%6d] %s %s=%s\n",nSize,GetOffsetStr(nLevel),GetKeyStr(sKey,nKey),value.str );
			nSize += strlen(value.str)+1;
			break;
		//case ANY_TTABLE:
		case ANY_TFUNCTION:
			fprintf( file,"[%6d] %s %s()\n",nSize,GetOffsetStr(nLevel),GetKeyStr(sKey,nKey) );
			break;
		case ANY_TUSERDATA:
			fprintf( file,"[%6d] %s [userdata] %s\n",nSize,GetOffsetStr(nLevel),GetKeyStr(sKey,nKey) );
			break;
		case ANY_TVECTOR:
			fprintf( file,"[%6d] %s %s=%g,%g,%g\n",nSize,GetOffsetStr(nLevel),GetKeyStr(sKey,nKey),value.vec3.x,value.vec3.y,value.vec3.z );
			nSize += sizeof(Vec3);
			break;
		}
	}
	virtual void OnBeginTable( int nLevel,const char *sKey,int nKey )
	{
		nSize += sizeof(Node);
		nSize += sizeof(Table);
		fprintf( file,"[%6d] %s %s = {\n",nSize,GetOffsetStr(nLevel),GetKeyStr(sKey,nKey) );
	}
	virtual void OnEndTable( int nLevel )
	{
		fprintf( file,"[%6d] %s }\n",nSize,GetOffsetStr(nLevel) );
	}
};

//////////////////////////////////////////////////////////////////////////
static void RecursiveTableDump( CScriptSystem *pSS,lua_State* L,int idx,int nLevel,IRecursiveLuaDump *sink,std::set<void*> &tables )
{
	const char *sKey = 0;
	int nKey = 0;

	CHECK_STACK(L);

	void *pTable = (void*)lua_topointer(L,idx);
	if (tables.find(pTable) != tables.end())
	{
		// This table was already dumped.
		return;
	}
	tables.insert(pTable);

	lua_pushnil(L);
	while(lua_next(L,idx)!=0)
	{
		// `key' is at index -2 and `value' at index -1
		if (lua_type(L, -2) == LUA_TSTRING)
			sKey = lua_tostring(L,-2);
		else
		{
			sKey = 0;
			nKey = (int)lua_tonumber(L,-2); // key index.
		}
		int type = lua_type(L,-1);
		switch(type)
		{
		case LUA_TNIL:
			break;
		case LUA_TTABLE:
			{
				if (!(sKey && nLevel == 0 && strcmp(sKey,"_G")==0))
				{
					sink->OnBeginTable(nLevel,sKey,nKey);
					RecursiveTableDump(pSS,L,lua_gettop(L),nLevel+1,sink,tables);
					sink->OnEndTable(nLevel);
				}
			}
			break;
		default:
			{
				ScriptAnyValue any;
				pSS->ToAny( any,-1 );
				sink->OnElement( nLevel,sKey,nKey,any );
			}
			break;
		}
		lua_pop(L,1);
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::DumpStateToFile( const char *filename )
{
	CHECK_STACK(L);
	SRecursiveLuaDumpToFile sink(filename);
	if (sink.file)
	{
		std::set<void*> tables;
		RecursiveTableDump( this,L,LUA_GLOBALSINDEX,0,&sink,tables );

#ifdef DEBUG_LUA_STATE
		{
			CHECK_STACK(L);
			for (std::set<CScriptTable*>::iterator it = gAllScriptTables.begin(); it != gAllScriptTables.end(); ++it)
			{
				CScriptTable *pTable = *it;

				ScriptHandle handle;
				if (pTable->GetValue( "id",handle ) && gEnv->pEntitySystem)
				{
					EntityId id = handle.n;
					IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id);
					char str[256];
					sprintf( str,"*Entity: %s",pEntity->GetEntityTextDescription() );
					sink.OnBeginTable(0,str,0);
				}
				else
					sink.OnBeginTable(0,"*Unknown Table",0);

				pTable->PushRef();
				RecursiveTableDump( this,L,lua_gettop(L),1,&sink,tables );
				lua_pop(L,1);
				sink.OnEndTable(0);
			}
		}
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::SerializeTimers( ISerialize *pSer )
{
	TSerialize ser(pSer);
	m_pScriptTimerMgr->Serialize( ser );
}

//////////////////////////////////////////////////////////////////////////
void CScriptSystem::ResetTimers( )
{
	m_pScriptTimerMgr->Reset( );
}
