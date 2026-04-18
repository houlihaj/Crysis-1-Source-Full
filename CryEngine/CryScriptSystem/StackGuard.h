#ifndef _STACKGUARD_H_
#define _STACKGUARD_H_

struct _StackGuard
{
	_StackGuard(lua_State *p)
	{
		m_pLS=p;
		m_nTop=lua_gettop(m_pLS);
	}
	~_StackGuard()
	{
		lua_settop(m_pLS,m_nTop);
	}
private:
	int m_nTop;
	lua_State *m_pLS;
};

#define _GUARD_STACK(ls)  _StackGuard __guard__(ls);

//////////////////////////////////////////////////////////////////////////
// Stack validator.
//////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG
struct SLuaStackValidator
{
	const char *text;
	lua_State *L;
	int top;
	SLuaStackValidator( lua_State *pL,const char *sText )
	{
		text = sText;
		L = pL;
		top = lua_gettop(L);
	}
	~SLuaStackValidator()
	{
		if (top != lua_gettop(L))
		{
			assert( 0 && "Lua Stack Validation Failed" );
			lua_settop(L,top);
		}
	}
};
#define CHECK_STACK(L) SLuaStackValidator __stackCheck((L),__FUNCTION__);
#else //_DEBUG
#define CHECK_STACK(L)
#endif //_DEBUG

#endif _STACKGUARD_H_