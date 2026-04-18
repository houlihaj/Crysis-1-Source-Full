/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 4:9:2005   15:32 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_Inventory.h"
#include "Inventory.h"
#include "IGameObject.h"


//------------------------------------------------------------------------
CScriptBind_Inventory::CScriptBind_Inventory(ISystem *pSystem, IGameFramework *pGameFramework)
: m_pSystem(pSystem),
	m_pEntitySystem(pSystem->GetIEntitySystem()),
	m_pGameFramework(pGameFramework)
{
	Init(m_pSystem->GetIScriptSystem(), m_pSystem, 1);

	RegisterMethods();
	RegisterGlobals();
}

//------------------------------------------------------------------------
CScriptBind_Inventory::~CScriptBind_Inventory()
{
}

//------------------------------------------------------------------------
void CScriptBind_Inventory::AttachTo(CInventory *pInventory)
{
	IScriptTable *pScriptTable = pInventory->GetEntity()->GetScriptTable();

	if (pScriptTable)
	{
		SmartScriptTable thisTable(m_pSS);

		thisTable->SetValue("__this", ScriptHandle(pInventory));
		thisTable->Delegate(GetMethodsTable());

		pScriptTable->SetValue("inventory", thisTable);
	}
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetInventoryTable(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	//SmartScriptTable inventory = pInventory->GetInventoryTable();
	SmartScriptTable inventory(gEnv->pScriptSystem->CreateTable());
	int curitem = 1;
	for (int i = 0; i < pInventory->GetCount(); i++)
	{

		IEntity *cur = gEnv->pEntitySystem->GetEntity(pInventory->GetItem(i));
		if (cur)
		{
			inventory->SetAt(curitem, ScriptHandle(cur->GetId()));
			curitem++;
			// check for children
			for (int j = 0; j < cur->GetChildCount(); j++)
			{
				IEntity *curChild = cur->GetChild(j);
				inventory->SetAt(curitem, ScriptHandle(curChild->GetId()));
				curitem++;
			}
		}
	}
	
	return pH->EndFunction(inventory);
}

//------------------------------------------------------------------------
void CScriptBind_Inventory::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptBind_Inventory::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Inventory::
	SCRIPT_REG_TEMPLFUNC(AddItem, "id");
	SCRIPT_REG_TEMPLFUNC(RemoveItem, "id");

	SCRIPT_REG_TEMPLFUNC(Validate, "");
	SCRIPT_REG_TEMPLFUNC(Destroy, "");
	SCRIPT_REG_TEMPLFUNC(Clear, "");

	SCRIPT_REG_TEMPLFUNC(Dump, "");

	SCRIPT_REG_TEMPLFUNC(SetCapacity, "size");
	SCRIPT_REG_TEMPLFUNC(GetCapacity, "");
	SCRIPT_REG_TEMPLFUNC(GetCount, "");
	SCRIPT_REG_TEMPLFUNC(GetCountOfClass, "className");

	SCRIPT_REG_TEMPLFUNC(GetItem, "slotId");
	SCRIPT_REG_TEMPLFUNC(GetItemByClass, "className");

	SCRIPT_REG_TEMPLFUNC(FindItem, "itemId");

	SCRIPT_REG_TEMPLFUNC(FindNext, "className, category, firstSlot, wrap");
	SCRIPT_REG_TEMPLFUNC(FindPrev, "className, category, firstSlot, wrap");

	SCRIPT_REG_TEMPLFUNC(GetCurrentItemId, "");
	SCRIPT_REG_TEMPLFUNC(GetCurrentItem, "");
	SCRIPT_REG_TEMPLFUNC(SetCurrentItemId, "itemId");

	SCRIPT_REG_TEMPLFUNC(SetLastItemId, "itemId");
	SCRIPT_REG_TEMPLFUNC(GetLastItemId, "");
	SCRIPT_REG_TEMPLFUNC(GetLastItem, "");

	SCRIPT_REG_TEMPLFUNC(SetAmmoCount, "ammoName, count");
	SCRIPT_REG_TEMPLFUNC(GetAmmoCount, "ammoName");
	SCRIPT_REG_TEMPLFUNC(SetAmmoCapacity, "ammoName, max");
	SCRIPT_REG_TEMPLFUNC(GetAmmoCapacity, "ammoName");
	SCRIPT_REG_TEMPLFUNC(ResetAmmo, "");

	SCRIPT_REG_TEMPLFUNC(GetInventoryTable, "");
}

//------------------------------------------------------------------------
CInventory *CScriptBind_Inventory::GetInventory(IFunctionHandler *pH)
{
	return (CInventory *)pH->GetThis();
}


//------------------------------------------------------------------------
int CScriptBind_Inventory::AddItem(IFunctionHandler *pH, ScriptHandle id)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	return pH->EndFunction(pInventory->AddItem((EntityId)id.n));
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::RemoveItem(IFunctionHandler *pH, ScriptHandle id)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	return pH->EndFunction(pInventory->RemoveItem((EntityId)id.n));
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::Validate(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	pInventory->Validate();
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::Destroy(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	pInventory->Destroy();
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::Clear(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	pInventory->Clear();
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::Dump(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	pInventory->Dump();
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::SetCapacity(IFunctionHandler *pH, int size)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	pInventory->SetCapacity(size);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetCapacity(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	return pH->EndFunction(pInventory->GetCapacity());
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetCount(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	return pH->EndFunction(pInventory->GetCount());
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetCountOfClass(IFunctionHandler *pH, const char *className)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	return pH->EndFunction(pInventory->GetCountOfClass(className));
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetItem(IFunctionHandler *pH, int slotId)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	EntityId itemId = pInventory->GetItem(slotId);
	if (itemId)
		return pH->EndFunction(ScriptHandle(itemId));

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetItemByClass(IFunctionHandler *pH, const char *className)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
	EntityId itemId = pInventory->GetItemByClass(pClass);
	if (itemId)
		return pH->EndFunction(ScriptHandle(itemId));

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::FindItem(IFunctionHandler *pH, ScriptHandle itemId)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	int slotId = pInventory->FindItem(itemId.n);
	if (slotId > -1)
		return pH->EndFunction(slotId);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::FindNext(IFunctionHandler *pH, const char *className, const char *category, int firstSlot, bool wrap)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
	int slotId = pInventory->FindNext(pClass, category, firstSlot, wrap);
	if (slotId > -1)
		return pH->EndFunction(slotId);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::FindPrev(IFunctionHandler *pH, const char *className, const char *category, int firstSlot, bool wrap)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
	int slotId = pInventory->FindPrev(pClass, category, firstSlot, wrap);
	if (slotId > -1)
		return pH->EndFunction(slotId);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetCurrentItemId(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	EntityId currentItemId = pInventory->GetCurrentItem();
	if (currentItemId)
		return pH->EndFunction(ScriptHandle(currentItemId));
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetCurrentItem(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	EntityId currentItemId = pInventory->GetCurrentItem();
	if (currentItemId)
	{
		IEntity *pEntity = m_pEntitySystem->GetEntity(currentItemId);
		if (pEntity)
			return pH->EndFunction(pEntity->GetScriptTable());
	}
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::SetCurrentItemId(IFunctionHandler *pH, ScriptHandle itemId)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	pInventory->SetCurrentItem(itemId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::SetLastItemId(IFunctionHandler *pH, ScriptHandle itemId)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	pInventory->SetLastItem(itemId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetLastItemId(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	EntityId lastItemId = pInventory->GetLastItem();
	if (lastItemId)
		return pH->EndFunction(ScriptHandle(lastItemId));
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetLastItem(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	EntityId lastItemId = pInventory->GetLastItem();
	if (lastItemId)
	{
		IEntity *pEntity = m_pEntitySystem->GetEntity(lastItemId);
		if (pEntity)
      return pH->EndFunction(pEntity->GetScriptTable());
	}
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::SetAmmoCount(IFunctionHandler *pH, const char *ammoName, int count)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoName);
	assert(pClass);
	pInventory->SetAmmoCount(pClass, count);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetAmmoCount(IFunctionHandler *pH, const char *ammoName)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoName);
	assert(pClass);
	return pH->EndFunction(pInventory->GetAmmoCount(pClass));
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::SetAmmoCapacity(IFunctionHandler *pH, const char *ammoName, int max)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoName);
	assert(pClass);
	pInventory->SetAmmoCapacity(pClass, max);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::GetAmmoCapacity(IFunctionHandler *pH, const char *ammoName)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoName);
	assert(pClass);
	return pH->EndFunction(pInventory->GetAmmoCapacity(pClass));
}

//------------------------------------------------------------------------
int CScriptBind_Inventory::ResetAmmo(IFunctionHandler *pH)
{
	CInventory *pInventory = GetInventory(pH);
	assert(pInventory);

	pInventory->ResetAmmo();

	return pH->EndFunction();
}