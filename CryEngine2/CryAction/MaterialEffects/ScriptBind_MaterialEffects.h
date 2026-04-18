////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2007.
// -------------------------------------------------------------------------
//  File name:   ScriptBind_MaterialEffects.cpp
//  Version:     v1.00
//  Created:     03/22/2007 by MichaelR
//  Compilers:   Visual Studio.NET
//  Description: MaterialEffects ScriptBind
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __SCRIPTBIND_MaterialEffects_H__
#define __SCRIPTBIND_MaterialEffects_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include <IScriptSystem.h>
#include <ScriptHelpers.h>

class CMaterialEffects;

class CScriptBind_MaterialEffects : public CScriptableBase
{
public:
	CScriptBind_MaterialEffects(ISystem* pSystem, CMaterialEffects* pDS);
	virtual ~CScriptBind_MaterialEffects();

	void Release() { delete this; };

private:
	void RegisterGlobals();
	void RegisterMethods();

  int GetEffectId(IFunctionHandler* pH, const char* customName, int surfaceIndex2);
  int ExecuteEffect(IFunctionHandler* pH, int effectId, SmartScriptTable paramsTable);

private:
	ISystem*       m_pSystem;
	IScriptSystem* m_pSS;	
	CMaterialEffects* m_pMFX;
};

#endif //__SCRIPTBIND_MaterialEffects_H__
