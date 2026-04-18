/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Script Bind for CActionMapManager
  
 -------------------------------------------------------------------------
  History:
  - 8:11:2004   16:48 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_ACTIONMAPMANAGER_H__
#define __SCRIPTBIND_ACTIONMAPMANAGER_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IScriptSystem.h>
#include <ScriptHelpers.h>


class CActionMapManager;
class CScriptBind_ActionMapManager :
	public CScriptableBase
{
public:
	CScriptBind_ActionMapManager(ISystem *pSystem, CActionMapManager *pActionMapManager);
	virtual ~CScriptBind_ActionMapManager();

	void Release() { delete this; };

	int EnableActionFilter(IFunctionHandler *pH, const char *name, bool enable);
	int EnableActionMap(IFunctionHandler *pH, const char *name, bool enable);
	int LoadFromXML(IFunctionHandler *pH, const char *name);

private:
	void RegisterGlobals();
	void RegisterMethods();

	ISystem						*m_pSystem;
	CActionMapManager *m_pManager;
};


#endif //__SCRIPTBIND_ACTIONMAPMANAGER_H__