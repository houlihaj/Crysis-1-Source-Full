/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script Binding for Inventory

-------------------------------------------------------------------------
History:
- 4:9:2005   15:29 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_INVENTORY_H__
#define __SCRIPTBIND_INVENTORY_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IScriptSystem.h>
#include <ScriptHelpers.h>


struct IItemSystem;
struct IGameFramework;
class CInventory;


class CScriptBind_Inventory :
	public CScriptableBase
{
public:
	CScriptBind_Inventory(ISystem *pSystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_Inventory();

	void Release() { delete this; };
	void AttachTo(CInventory *pItem);

	int AddItem(IFunctionHandler *pH, ScriptHandle id);
	int RemoveItem(IFunctionHandler *pH, ScriptHandle id);

	int Validate(IFunctionHandler *pH);
	int Destroy(IFunctionHandler *pH);
	int Clear(IFunctionHandler *pH);

	int Dump(IFunctionHandler *pH);

	int SetCapacity(IFunctionHandler *pH, int size);
	int GetCapacity(IFunctionHandler *pH);
	int GetCount(IFunctionHandler *pH);
	int GetCountOfClass(IFunctionHandler *pH, const char *className);

	int GetItem(IFunctionHandler *pH, int slotId);
	int GetItemByClass(IFunctionHandler *pH, const char *className);

	int FindItem(IFunctionHandler *pH, ScriptHandle itemId);

	int FindNext(IFunctionHandler *pH, const char *className, const char *category,	int firstSlot, bool wrap);
	int FindPrev(IFunctionHandler *pH, const char *className, const char *category, int firstSlot, bool wrap);

	int GetCurrentItemId(IFunctionHandler *pH);
	int GetCurrentItem(IFunctionHandler *pH);
	int SetCurrentItemId(IFunctionHandler *pH, ScriptHandle itemId);

	int SetLastItemId(IFunctionHandler *pH, ScriptHandle itemId);
	int GetLastItemId(IFunctionHandler *pH);
	int GetLastItem(IFunctionHandler *pH);

	int SetAmmoCount(IFunctionHandler *pH, const char *ammoName, int count);
	int GetAmmoCount(IFunctionHandler *pH, const char *ammoName);
	int SetAmmoCapacity(IFunctionHandler *pH, const char *ammoName, int max);
	int GetAmmoCapacity(IFunctionHandler *pH, const char *ammoName);
	int ResetAmmo(IFunctionHandler *pH);

	int GetInventoryTable(IFunctionHandler *pH);
private:
	void RegisterGlobals();
	void RegisterMethods();

	CInventory *GetInventory(IFunctionHandler *pH);

	ISystem					*m_pSystem;
	IEntitySystem		*m_pEntitySystem;
	IGameFramework	*m_pGameFramework;
};


#endif //__SCRIPTBIND_INVENTORY_H__