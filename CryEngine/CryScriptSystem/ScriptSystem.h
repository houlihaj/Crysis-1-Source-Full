// ScriptSystem.h: interface for the CScriptSystem class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCRIPTSYSTEM_H__8FCEA01B_BD85_4E4D_B54F_B09429A7CDFF__INCLUDED_)
#define AFX_SCRIPTSYSTEM_H__8FCEA01B_BD85_4E4D_B54F_B09429A7CDFF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#if !defined(__GNUC__)
#include "StdAfx.h"
#endif
#include <IConsole.h>
#include <IScriptSystem.h>
extern "C"{
#include <lua.h>
#include <lauxlib.h>
}

#include "StackGuard.h"
#include "ScriptBindings/ScriptBinding.h"
#include "ScriptTimerMgr.h"

class CLUADbg;

struct SLuaStackEntry
{
	int line;
	string source;
	string description;
};

// Returns literal representation of the type value
inline const char* ScriptAnyTypeToString( ScriptAnyType type )
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

// Returns literal representation of the type value
inline const char* ScriptVarTypeAsCStr(ScriptVarType t)
{
	switch (t)
	{
	case svtNull: return "Null";
	case svtBool: return "Boolean";
	case svtString: return "String";
	case svtNumber: return "Number";
	case svtFunction: return "Function";
	case svtObject: return "Table";
	case svtUserData: return "UserData";
	case svtPointer: return "Pointer";
	default: return "Unknown";
	}
}

typedef std::set<string> ScriptFileList;
typedef ScriptFileList::iterator ScriptFileListItor;

//////////////////////////////////////////////////////////////////////////
// forwarde declarations.
class CScriptSystem;
class CScriptTable;

#define SCRIPT_OBJECT_POOL_SIZE 15000

/*! IScriptSystem implementation
	@see IScriptSystem 
*/
class CScriptSystem : public IScriptSystem, public ISystemEventListener
{
public:
	//! constructor
	CScriptSystem();
	//! destructor
	virtual ~CScriptSystem();
	//!
	bool Init( ISystem *pSystem,bool bStdLibs,int nStackSize );

	void Update();
	void SetGCFrequency( const float fRate );
	//!
	void RegisterErrorHandler(void);

	//!
	bool _ExecuteFile(const char *sFileName,bool bRaiseError);
	//!

	// interface IScriptSystem -----------------------------------------------------------

	virtual bool ExecuteFile(const char *sFileName,bool bRaiseError,bool bForceReload);
	virtual bool ExecuteBuffer(const char *sBuffer, size_t nSize,const char *sBufferDescription);
	virtual void UnloadScript(const char *sFileName);
	virtual void UnloadScripts();
	virtual bool ReloadScript(const char *sFileName,bool bRaiseError);
	virtual bool ReloadScripts();
	virtual void DumpLoadedScripts();

	virtual IScriptTable* CreateTable( bool bEmpty=false );
	
	//////////////////////////////////////////////////////////////////////////
	// Begin Call.
	//////////////////////////////////////////////////////////////////////////
	virtual int BeginCall(HSCRIPTFUNCTION hFunc);
	virtual int BeginCall(const char *sFuncName);
	virtual int BeginCall(const char *sTableName,const char *sFuncName);
	virtual int BeginCall( IScriptTable *pTable, const char *sFuncName );
	
	//////////////////////////////////////////////////////////////////////////
	// End Call.
	//////////////////////////////////////////////////////////////////////////
	virtual bool EndCall();
	virtual bool EndCallAny( ScriptAnyValue &any );
	virtual bool EndCallAnyN( int n, ScriptAnyValue* anys );

	//////////////////////////////////////////////////////////////////////////
	// Get function pointer.
	//////////////////////////////////////////////////////////////////////////
	virtual HSCRIPTFUNCTION GetFunctionPtr(const char *sFuncName);
	virtual HSCRIPTFUNCTION GetFunctionPtr(const char *sTableName, const char *sFuncName);
	virtual void ReleaseFunc( HSCRIPTFUNCTION f );

	//////////////////////////////////////////////////////////////////////////
	// Push function param.
	//////////////////////////////////////////////////////////////////////////
	virtual void PushFuncParamAny( const ScriptAnyValue &any );

	//////////////////////////////////////////////////////////////////////////
	// Set global value.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetGlobalAny( const char *sKey,const ScriptAnyValue &any );
	virtual bool GetGlobalAny( const char *sKey,ScriptAnyValue &any );

	virtual IScriptTable * CreateUserData(void * ptr, size_t size);
	virtual void ForceGarbageCollection();
	virtual int GetCGCount();
	virtual void SetGCThreshhold(int nKb);
	virtual void Release();
	virtual void ShowDebugger(const char *pszSourceFile, int iLine, const char *pszReason);
	virtual HBREAKPOINT AddBreakPoint(const char *sFile,int nLineNumber);
	virtual IScriptTable *GetLocalVariables(int nLevel = 0);
	virtual IScriptTable *GetCallsStack() { return 0; };
	
	virtual void DebugContinue(){}
	virtual void DebugStepNext(){}
	virtual void DebugStepInto(){}
	virtual void DebugDisable(){}

	virtual BreakState GetBreakState(){return bsNoBreak;}
	virtual void GetMemoryStatistics(ICrySizer *pSizer);
	virtual void GetScriptHash( const char *sPath, const char *szKey, unsigned int &dwHash );
	virtual void PostInit();
	virtual void RaiseError( const char *format,... ) PRINTF_PARAMS(2, 3);
	virtual void LoadScriptedSurfaceTypes( const char *sFolder,bool bReload );
	virtual void SerializeTimers( struct ISerialize *pSer );
	virtual void ResetTimers( );

	virtual int GetStackSize();
	virtual uint32 GetScriptAllocSize();

	//////////////////////////////////////////////////////////////////////////
	// Emulates weak references.
	//////////////////////////////////////////////////////////////////////////
	// Make a new weak reference.
	static int  weak_ref( lua_State *L );
	// Removes a weak reference.
	static void weak_unref( lua_State *L,int ref );
	// Push a weak reference on lua stack.
	static void weak_getref( lua_State *L,int ref );
	//////////////////////////////////////////////////////////////////////////

	void PushAny( const ScriptAnyValue &var );
	bool PopAny( ScriptAnyValue &var );
	// Convert top stack item to Any.
	bool ToAny( ScriptAnyValue &var,int index );
	void PushVec3( const Vec3 &vec );
	bool ToVec3( Vec3 &vec,int index );
	// Push table reference
	void PushTable( IScriptTable *pTable );
	// Attach reference at the top of the stack to the specified table pointer.
	void AttachTable( IScriptTable *pTable );
	bool GetRecursiveAny( IScriptTable *pTable,const char *sKey,ScriptAnyValue &any );

	//lua_State* GetLuaState() const { return L; }
	void LogStackTrace();

	CScriptTimerMgr* GetScriptTimerMgr() { return m_pScriptTimerMgr; };

	void GetCallStack( std::vector<SLuaStackEntry> &callstack );
	bool IsCallStackEmpty(void);
	void DumpStateToFile( const char *filename );

	//////////////////////////////////////////////////////////////////////////
	// ISystemEventListener
	//////////////////////////////////////////////////////////////////////////
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam );
	//////////////////////////////////////////////////////////////////////////

private: // ---------------------------------------------------------------------
	//!
	bool EndCallN( int nReturns );

	static int ErrorHandler(lua_State *L);

	// Create default metatables.
	void CreateMetatables();

	// Now private, doesn't need to be updated by main thread
	void EnableDebugger( ELuaDebugMode eDebugMode );

	static void DebugModeChange(ICVar *cvar);

	//!
	//static int GCTagHandler(lua_State *L);

//	void GetScriptHashFunction( IScriptTable &Current, unsigned int &dwHash);
	
	// ----------------------------------------------------------------------------
private:
	static CScriptSystem *s_mpScriptSystem;
	lua_State *L;
	ICVar *m_cvar_script_debugger; // Stores current debugging mode
	int m_nTempArg;
	int m_nTempTop;

	IScriptTable* m_pUserDataMetatable;
	static CScriptTable* m_pWeakRefsTable;

	HSCRIPTFUNCTION m_pErrorHandlerFunc;

	ScriptFileList m_dqLoadedFiles;

	CScriptBindings m_stdScriptBinds;
	ISystem *m_pSystem;

	float						m_fGCFreq;				//!< relative time in seconds
	float						m_lastGCTime;			//!< absolute time in seconds
	int							m_nLastGCCount;		//!<
	int             m_forceReloadCount;

	CScriptTimerMgr* m_pScriptTimerMgr;

public: // -----------------------------------------------------------------------

	string m_sLastBreakSource;		//!
	int    m_nLastBreakLine;		//!
	CLUADbg *m_pLuaDebugger;
};


#endif // !defined(AFX_SCRIPTSYSTEM_H__8FCEA01B_BD85_4E4D_B54F_B09429A7CDFF__INCLUDED_)
