/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 30:9:2004   14:21 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_ITEMSYSTEM_H__
#define __SCRIPTBIND_ITEMSYSTEM_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IScriptSystem.h>
#include <ScriptHelpers.h>

// <title ItemSystem>
// Syntax: ItemSystem
class CItemSystem;
class CEquipmentManager;

class CScriptBind_ItemSystem :
	public CScriptableBase
{
public:
	CScriptBind_ItemSystem(ISystem *pSystem, CItemSystem *pItemSystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_ItemSystem();

	void Release() { delete this; };

	int Reset(IFunctionHandler *pH);

	int GiveItem(IFunctionHandler *pH, const char *itemName);
	int GiveItemPack(IFunctionHandler *pH, ScriptHandle actorId, const char *packName);
	int GetPackPrimaryItem(IFunctionHandler *pH, const char *packName);
  int GetPackNumItems(IFunctionHandler *pH, const char* packName);
	int GetPackItemByIndex(IFunctionHandler *pH, const char *packName, int index);
	int SetActorItem(IFunctionHandler *pH, ScriptHandle actorId, ScriptHandle itemId, bool keepHistory);
	int SetActorItemByName(IFunctionHandler *pH, ScriptHandle actorId, const char *name, bool keepHistory);
	int SerializePlayerLTLInfo(IFunctionHandler *pH, bool reading);

private:
	void RegisterGlobals();
	void RegisterMethods();

	CItemSystem			  *m_pItemSystem;
	IGameFramework	  *m_pGameFramework;
  CEquipmentManager *m_pEquipmentManager;
};


#endif //__SCRIPTBIND_ITEMSYSTEM_H__
