// ScriptObject.cpp: implementation of the CScriptTable class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ScriptTable.h"
#include "ScriptSystem.h"
#include "StackGuard.h"
#include "FunctionHandler.h"

#include <ISystem.h>

lua_State* CScriptTable::L = NULL;
CScriptSystem* CScriptTable::m_pSS = NULL;

#ifdef DEBUG_LUA_STATE
std::set<class CScriptTable*> gAllScriptTables;
#endif

//////////////////////////////////////////////////////////////////////////
IScriptSystem* CScriptTable::GetScriptSystem() const
{
	return m_pSS;
};

//////////////////////////////////////////////////////////////////////////
int CScriptTable::GetRef()
{
	return m_nRef;
}

//////////////////////////////////////////////////////////////////////////
void * CScriptTable::GetUserDataValue()
{
	PushRef();
	void * ptr = lua_touserdata(L, -1);
	lua_pop(L,1);
	return ptr;
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::PushRef()
{
	if (m_nRef != DELETED_REF && m_nRef != NULL_REF)
		lua_getref(L,m_nRef);
	else
	{
		lua_pushnil(L);
		if (m_nRef == DELETED_REF)
		{
				assert(0 && "Access to deleted script object" );
			CryError( "Access to deleted script object %x",(unsigned int)(UINT_PTR)this );
		}
		else
		{
			assert(0 && "Pushing Nil table reference");
			ScriptWarning( "Pushing Nil table reference" );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::PushRef( IScriptTable *pObj )
{
	int nRef = ((CScriptTable*)pObj)->m_nRef;
	if (nRef != DELETED_REF)
		lua_getref( L,nRef );
	else
		CryError( "Access to deleted script object" );
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::Attach()
{
	if (m_nRef != NULL_REF)
		lua_unref(L, m_nRef);
	m_nRef = lua_ref(L,1);

#ifdef DEBUG_LUA_STATE
	gAllScriptTables.insert(this);
#endif
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::AttachToObject( IScriptTable *so )
{
	PushRef(so);
	Attach();
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::DeleteThis()
{
	if (m_nRef == DELETED_REF)
	{
		assert(0);
		CryError( "Attempt to Release already released script table." );
	}

#ifdef DEBUG_LUA_STATE
	gAllScriptTables.erase(this);
#endif

	if (m_nRef != NULL_REF)
		lua_unref(L, m_nRef);

	m_nRef = DELETED_REF;
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::CreateNew()
{
	lua_newtable(L);
	Attach();
}

//////////////////////////////////////////////////////////////////////////
bool CScriptTable::BeginSetGetChain()
{
	PushRef();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::EndSetGetChain()
{
	if(lua_istable(L,-1))
		lua_pop(L,1);
	else
	{
		assert(0 && "Mismatch in Set/Get Chain" );
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::SetValueAny( const char *sKey,const ScriptAnyValue &any,bool bChain )
{
	CHECK_STACK(L);
	int top = lua_gettop(L);
	if (!bChain)
		PushRef();

	assert(sKey);
	size_t len = strlen(sKey);

	if (any.type == ANY_TVECTOR)
	{
		// Check if we can reuse Vec3 value already in the table.
		lua_pushlstring(L, sKey, len);
		lua_rawget(L, -2);
		int luatype = lua_type(L,-1);
		if (luatype == LUA_TTABLE)
		{
			lua_pushlstring(L,"x", 1);
			lua_rawget(L,-2);
			bool bXIsNumber = lua_isnumber(L,-1) != 0;
			lua_pop(L,1); // pop x value.
			if (bXIsNumber)
			{
				// Assume its a vector, just fill it with new vector values.
				lua_pushlstring(L,"x", 1);
				lua_pushnumber(L, any.vec3.x);
				lua_rawset(L,-3);
				lua_pushlstring(L,"y", 1);
				lua_pushnumber(L, any.vec3.y);
				lua_rawset(L,-3);
				lua_pushlstring(L,"z", 1);
				lua_pushnumber(L, any.vec3.z);
				lua_rawset(L,-3);

				lua_settop(L,top);
				return;
			}
		}
		lua_pop(L,1); // pop key value.
	}
	lua_pushlstring(L, sKey, len);
	m_pSS->PushAny(any);
	lua_rawset(L, -3);
	lua_settop(L,top);
}

//////////////////////////////////////////////////////////////////////////
bool CScriptTable::GetValueAny( const char *sKey,ScriptAnyValue &any,bool bChain )
{
	CHECK_STACK(L);
	int top = lua_gettop(L);
	if (!bChain)
		PushRef();
	bool res = false;
	lua_pushstring(L, sKey);
	lua_gettable(L, -2);
	res = m_pSS->PopAny(any);
	lua_settop(L,top);
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::SetAtAny( int nIndex,const ScriptAnyValue &any )
{
	CHECK_STACK(L);
	PushRef();
	m_pSS->PushAny(any);
	lua_rawseti(L,-2,nIndex);
	lua_pop(L, 1); // Pop table.
}

//////////////////////////////////////////////////////////////////////////
bool CScriptTable::GetAtAny( int nIndex,ScriptAnyValue &any )
{
	CHECK_STACK(L);
	bool res = false;
	PushRef();
	lua_rawgeti(L,-1,nIndex);
	res = m_pSS->PopAny(any);
	lua_pop(L, 1); // Pop table.

	return res;
}

//////////////////////////////////////////////////////////////////////////
int CScriptTable::Count()
{
	_GUARD_STACK(L);

	PushRef();
	int count = luaL_getn(L,-1);
	return count;
}

void CScriptTable::CloneTable_r( int srcTable,int trgTable )
{
	int top = lua_gettop(L);
	lua_pushnil(L);  // first key
	while (lua_next(L, srcTable) != 0) 
	{
		if (lua_type(L,-1) == LUA_TTABLE)
		{
			int srct = lua_gettop(L);

			lua_pushvalue(L, -2); // Push again index.
			lua_newtable(L);      // Make value.
			int trgt = lua_gettop(L);
			CloneTable_r( srct,trgt );
			lua_rawset(L, trgTable); // Set new table to trgtable.
		}
		else
		{
			// `key' is at index -2 and `value' at index -1
			lua_pushvalue(L, -2); // Push again index.
			lua_pushvalue(L, -2); // Push value.
			lua_rawset(L, trgTable);
		}
		lua_pop(L, 1); // pop value, leave index.
	}
	lua_settop(L, top); // Restore stack.
}

void CScriptTable::CloneTable( int srcTable,int trgTable )
{
	int top = lua_gettop(L);
	lua_pushnil(L);  // first key
	while (lua_next(L, srcTable) != 0) 
	{
		// `key' is at index -2 and `value' at index -1
		lua_pushvalue(L, -2); // Push again index.
		lua_pushvalue(L, -2); // Push value.
		lua_rawset(L, trgTable);
		lua_pop(L, 1); // pop value, leave index.
	}
	lua_settop(L, top); // Restore stack.
}

//////////////////////////////////////////////////////////////////////////
bool CScriptTable::Clone( IScriptTable *pSrcTable,bool bDeepCopy )
{	
	int top = lua_gettop(L);

	PushRef(pSrcTable);
	PushRef();

	int srcTable = top + 1;
	int trgTable = top + 2;

	if (bDeepCopy)
	{
		CloneTable_r( srcTable,trgTable );
	}
	else
	{
		CloneTable( srcTable,trgTable );
	}
	lua_settop(L, top); // Restore stack.

	return true;
}

void CScriptTable::Dump(IScriptTableDumpSink *p)
{
	if(!p)return;
	_GUARD_STACK(L);
	int top = lua_gettop(L);
	
	PushRef();
	
	int trgTable = top + 1;
	
	
	lua_pushnil(L);  // first key
	int reftop=lua_gettop(L);
	while (lua_next(L, trgTable) != 0) 
	{
		// `key' is at index -2 and `value' at index -1
		if (lua_type(L, -2) == LUA_TSTRING)
		{
			const char *sName = lua_tostring(L, -2); // again index
			switch (lua_type(L,-1))
			{
			case LUA_TNIL: p->OnElementFound(sName, svtNull); break;
			case LUA_TBOOLEAN: p->OnElementFound(sName, svtBool); break;
			case LUA_TLIGHTUSERDATA: p->OnElementFound(sName, svtPointer); break;
			case LUA_TNUMBER: p->OnElementFound(sName, svtNumber); break;
			case LUA_TSTRING: p->OnElementFound(sName, svtString); break;
			case LUA_TTABLE: p->OnElementFound(sName, svtObject); break;
			case LUA_TFUNCTION: p->OnElementFound(sName, svtFunction); break;
			case LUA_TUSERDATA: p->OnElementFound(sName, svtUserData); break;
			};
		}
		else
		{
			int nIdx = (int)lua_tonumber(L, - 2); // again index
			switch (lua_type(L,-1))
			{
			case LUA_TNIL: p->OnElementFound(nIdx, svtNull); break;
			case LUA_TBOOLEAN: p->OnElementFound(nIdx, svtBool); break;
			case LUA_TLIGHTUSERDATA: p->OnElementFound(nIdx, svtPointer); break;
			case LUA_TNUMBER: p->OnElementFound(nIdx, svtNumber); break;
			case LUA_TSTRING: p->OnElementFound(nIdx, svtString); break;
			case LUA_TTABLE: p->OnElementFound(nIdx, svtObject); break;
			case LUA_TFUNCTION: p->OnElementFound(nIdx, svtFunction); break;
			case LUA_TUSERDATA: p->OnElementFound(nIdx, svtUserData); break;
			};
		}
		lua_settop(L, reftop); // pop value, leave index.
	}
	//lua_settop(L, top); // Restore stack.
	//END_CHECK_STACK
}

ScriptVarType CScriptTable::GetValueType( const char *sKey )
{
	CHECK_STACK(L);
	ScriptVarType type=svtNull;
	
	PushRef();
	lua_pushstring(L, sKey);
	lua_gettable(L, -2);
	int luatype = lua_type(L,-1);
	switch (luatype)
	{
	case LUA_TNIL: type = svtNull; break;
	case LUA_TBOOLEAN: type = svtBool; break;
	case LUA_TNUMBER: type = svtNumber; break;
	case LUA_TSTRING: type = svtString; break;
	case LUA_TFUNCTION: type = svtFunction; break;
	case LUA_TLIGHTUSERDATA: type = svtPointer; break;
	case LUA_TTABLE: type = svtObject; break;
	}
	lua_pop(L, 2); // Pop value and table.
	return type;
}

ScriptVarType CScriptTable::GetAtType(int nIdx)
{
	_GUARD_STACK(L);
	ScriptVarType svtRetVal=svtNull;
	PushRef();
	if (luaL_getn(L, -1) < nIdx)
	{
		//lua_pop(L, 1);
		return svtNull;
	}
	
	lua_rawgeti(L, -1, nIdx);

	if (lua_isnil(L, -1))
	{
		svtRetVal=svtNull;
	}
	else if (lua_isfunction(L, -1))
	{
		svtRetVal=svtFunction;
	}
	else if (lua_isnumber(L, -1))
	{
		svtRetVal=svtNumber;
	}
	else if (lua_isstring(L, -1))
	{
		svtRetVal=svtString;
	}
	else if (lua_istable(L, -1))
	{
		svtRetVal=svtObject;
	}

	//lua_pop(L, 2);
	//END_CHECK_STACK
	return svtRetVal;
}

//////////////////////////////////////////////////////////////////////////
IScriptTable::Iterator CScriptTable::BeginIteration()
{
	Iterator iter;
	iter.nInternal = lua_gettop(L)+1;
	iter.nKey = -1;
	iter.sKey = NULL;

	PushRef();
	lua_pushnil(L);
	return iter;
}

//////////////////////////////////////////////////////////////////////////
bool CScriptTable::MoveNext( Iterator &iter )
{
	if (!iter.nInternal)
		return false;
	int nTop = iter.nInternal-1;

  //leave only the index into the stack
	while((lua_gettop(L)-(nTop+1))>1)
	{
		lua_pop(L,1);
	}
	bool bResult = lua_next(L, nTop+1) != 0;
	if (bResult)
	{
		iter.value.Clear();
		bResult = m_pSS->PopAny( iter.value );
		// Get current key.
		if (lua_type(L,-1) == LUA_TSTRING)
		{
			// String key.
			iter.sKey = (const char*)lua_tostring(L,-1);
			iter.nKey = -1;
		}
		else if(lua_type(L,-1) == LUA_TNUMBER)
		{
			// Number key.
			iter.sKey = NULL;
			iter.nKey = (int)lua_tonumber(L, -1);
		}
		else
		{
			iter.sKey = 0;
			iter.nKey = -1;
		}
	}
	if (!bResult)
	{
		EndIteration(iter);
	}
	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::EndIteration( const Iterator &iter )
{
	if (iter.nInternal)
	{
		lua_settop(L,iter.nInternal-1);
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::Clear()
{
	_GUARD_STACK(L);
	int top = lua_gettop(L);
	
	PushRef();
	int trgTable = top + 1;
		
	lua_pushnil(L);  // first key
	while (lua_next(L, trgTable) != 0) 
	{
			lua_pop(L, 1); // pop value, leave index.
			lua_pushvalue(L, -1); // Push again index.
			lua_pushnil(L);
			lua_rawset(L,trgTable);
	}
//	lua_settop(L, top); // Restore stack.
//	END_CHECK_STACK
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::SetMetatable( IScriptTable *pMetatable )
{
	CHECK_STACK(L);
	//////////////////////////////////////////////////////////////////////////
	// Set metatable for this script object.
	//////////////////////////////////////////////////////////////////////////
	PushRef(); // -2
	PushRef(pMetatable); // -1
	lua_setmetatable(L,-2);
	lua_pop(L,1); // pop table
}

//////////////////////////////////////////////////////////////////////////
void CScriptTable::Delegate( IScriptTable *pMetatable )
{
	if (!pMetatable)
		return;

	CHECK_STACK(L);

	PushRef(pMetatable);
	lua_pushstring(L,"__index"); // push key.	
	PushRef(pMetatable);
	lua_rawset(L,-3); // sets metatable.__index = metatable
	lua_pop(L,1); // pop metatable from stack.

	SetMetatable( pMetatable );
}

//////////////////////////////////////////////////////////////////////////
int CScriptTable::StdCFunction( lua_State *L )
{
	unsigned char *pBuffer = (unsigned char*)lua_touserdata(L,lua_upvalueindex(1));
	// Not very safe cast, but we sure what is in the upvalue 1 should be.
	FunctionFunctor *pFunction = (FunctionFunctor*)pBuffer;
	int8 nParamIdOffset = *(int8*)(pBuffer+sizeof(FunctionFunctor));
	const char *sFuncName = (const char*)(pBuffer+sizeof(FunctionFunctor)+1);
	CFunctionHandler fh(m_pSS,L,sFuncName,nParamIdOffset);
	// Call functor.
	int nRet = (*pFunction)( &fh ); // int CallbackFunction( IFunctionHandler *pHandler )
	return nRet;
}

//////////////////////////////////////////////////////////////////////////
int CScriptTable::StdCUserDataFunction( lua_State *L )
{
	unsigned char *pBuffer = (unsigned char*)lua_touserdata(L,lua_upvalueindex(1));
	// Not very safe cast, but we sure what is in the upvalue 1 should be.
	UserDataFunction *pFunction = (UserDataFunction*)pBuffer;
	pBuffer += sizeof(UserDataFunction);
	int nSize = *((int*)pBuffer); // first 4 bytes are size of user data.
	pBuffer += sizeof(int);
	int8 nParamIdOffset = *(int8*)(pBuffer+nSize);
	const char *sFuncName = (const char*)(pBuffer+nSize+1);
	CFunctionHandler fh(m_pSS,L,sFuncName,nParamIdOffset);
	// Call functor.
	int nRet = (*pFunction)( &fh,pBuffer,nSize ); // int CallbackFunction( IFunctionHandler *pHandler,pBuffer,nSize )
	return nRet;
}

//////////////////////////////////////////////////////////////////////////
bool CScriptTable::AddFunction( const SUserFunctionDesc &fd )
{
	CHECK_STACK(L);

	assert( fd.pFunctor || fd.pUserDataFunc != 0 );

	// Make function signature.
	char sFuncSignature[256];
	if (fd.sGlobalName[0] != 0)
		sprintf( sFuncSignature,"%s.%s(%s)",fd.sGlobalName,fd.sFunctionName,fd.sFunctionParams );
	else
		sprintf( sFuncSignature,"%s.%s(%s)",fd.sGlobalName,fd.sFunctionName,fd.sFunctionParams );

	PushRef();
	lua_pushstring(L, fd.sFunctionName);

	int8 nParamIdOffset = fd.nParamIdOffset;
	if (fd.pFunctor)
	{
		int nDataSize = sizeof(fd.pFunctor) + strlen(sFuncSignature) + 1 + 1;

		// Store functor in first upvalue.
		unsigned char *pBuffer = (unsigned char*)lua_newuserdata(L, nDataSize);
		memcpy( pBuffer, &fd.pFunctor, sizeof(fd.pFunctor));
		memcpy( pBuffer+sizeof(fd.pFunctor), &nParamIdOffset, 1 );
		memcpy( pBuffer+sizeof(fd.pFunctor)+1, sFuncSignature, strlen(sFuncSignature)+1 );
		lua_pushcclosure( L,StdCFunction,1 );
	}
	else 
	{
		assert( fd.pDataBuffer != NULL && fd.nDataSize > 0 );
		UserDataFunction function = fd.pUserDataFunc;
		int nSize = fd.nDataSize;
		int nTotalSize = sizeof(function) + sizeof(int) + nSize + strlen(sFuncSignature) + 1 + 1;
		// Store functor in first upvalue.
		unsigned char *pBuffer = (unsigned char*)lua_newuserdata(L, nTotalSize );
		memcpy( pBuffer, &function, sizeof(function) );
		memcpy( pBuffer+sizeof(function), &nSize, sizeof(nSize) );
		memcpy( pBuffer+sizeof(function)+sizeof(nSize), fd.pDataBuffer, nSize );
		memcpy( pBuffer+sizeof(function)+sizeof(nSize)+nSize, &nParamIdOffset, 1 );
		memcpy( pBuffer+sizeof(function)+sizeof(nSize)+nSize+1, sFuncSignature, strlen(sFuncSignature)+1 );
		lua_pushcclosure( L,StdCUserDataFunction,1 );
	}

	lua_rawset(L, -3);
	lua_pop(L,1); // pop table.
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CScriptTable::GetValueRecursive( const char *szPath, IScriptTable *pObj )
{
	assert(pObj);
	_GUARD_STACK(L);

	const char *pSrc=szPath;

	IScriptTable *pCurrent=this;

	pObj->Clone(this);

	while(*pSrc)
	{
		char szInterm[256],*pDst=szInterm;

		while(*pSrc)
			*pDst++=*pSrc++;

		*pDst=0;								// zero termination

		if(!pCurrent->GetValue(szInterm,pObj))
			return false;

		pCurrent=pObj;
	}

	return true;
}
