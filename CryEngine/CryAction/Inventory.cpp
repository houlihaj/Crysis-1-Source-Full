/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 18:8:2005   17:27 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Inventory.h"
#include <IEntitySystem.h>
#include "IGameObject.h"
#include "CryAction.h"
#include "ScriptBind_Inventory.h"
#include "IActorSystem.h"

//------------------------------------------------------------------------
CInventory::CInventory()
: m_capacity(64),
	m_pActor(0)
{
	m_stats.slots.reserve(m_capacity);
	m_bSerializeLTL = false;
}

//------------------------------------------------------------------------
CInventory::~CInventory()
{
}

//------------------------------------------------------------------------
bool CInventory::Init( IGameObject * pGameObject )
{
	SetGameObject(pGameObject);
	// attach script bind
	CCryAction *pCryAction = static_cast<CCryAction *>(gEnv->pGame->GetIGameFramework());
	pCryAction->GetInventoryScriptBind()->AttachTo(this);

	m_pGameFrameWork = pCryAction;

	m_pActor = pCryAction->GetIActorSystem()->GetActor(pGameObject->GetEntityId());

	return true;
}

//------------------------------------------------------------------------
void CInventory::FullSerialize( TSerialize ser )
{
	ser.BeginGroup("InventoryItems");
	ser.Value("CurrentItem", m_stats.currentItemId);
	ser.Value("HolsterItem", m_stats.holsteredItemId);
	ser.Value("LastItem", m_stats.lastItemId);
	ser.Value("Capacity", m_capacity);
	int s = m_stats.slots.size();
	ser.Value("InventorySize", s);
	if(ser.IsReading())
		m_stats.slots.resize(s);

	ser.Value("Slots", m_stats.slots );

	ser.BeginGroup("Ammo");
	if(ser.IsReading())
	{
		m_stats.ammo.clear();
		m_stats.ammoCapacity.clear();
	}

	TAmmoMap::iterator it = m_stats.ammo.begin();
	int ammoAmount = m_stats.ammo.size();
	ser.Value("AmmoAmount", ammoAmount);
	for(int i = 0; i < ammoAmount; ++i, ++it)
	{
		string name;
		int amount = 0;
		if(ser.IsWriting())
		{
			assert(it->first);
			name = (it->first)?it->first->GetName():"";
			amount = it->second;
		}
		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.EndGroup();
		if(ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			assert(pClass);
			m_stats.ammo[pClass] = amount;
		}
	}

	it = m_stats.ammoCapacity.begin();
	int ammoCapacity = m_stats.ammoCapacity.size();
	ser.Value("AmmoCapacity", ammoCapacity);
	for(int i = 0; i < ammoCapacity; ++i, ++it)
	{
		string name;
		int amount = 0;
		if(ser.IsWriting())
		{
			name = it->first->GetName();
			amount = it->second;
		}
		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.EndGroup();
		if(ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			assert(pClass);
			m_stats.ammoCapacity[pClass] = amount;
		}
	}
	ser.EndGroup();

	ser.EndGroup();
}

//------------------------------------------------------------------------
void CInventory::SerializeInventoryForLevelChange( TSerialize ser )
{
	IActor *pActor = GetActor();
	if(!pActor)
		return;

	m_bSerializeLTL = true;

	if(ser.IsReading())
	{
		for(int r = 0; r < m_stats.slots.size(); ++r)
		{
			IItem *pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_stats.slots[r]);
			if(pItem)
			{
				pItem->Drop();
				pItem->GetEntity()->SetFlags(pItem->GetEntity()->GetFlags()|ENTITY_FLAG_UPDATE_HIDDEN);	
				pItem->GetEntity()->Hide(true);
				gEnv->pEntitySystem->RemoveEntity(m_stats.slots[r]);
			}
		}
	}

	int numItems = 0;

	std::vector<EntityId> dualWieldMasters;
	string currentItemNameOnReading;

	if(ser.IsReading())
	{
		m_stats.slots.clear();
		m_stats.currentItemId = 0;
		m_stats.holsteredItemId = 0;
		m_stats.lastItemId = 0;
		
		string itemName;
		ser.Value("numOfItems", numItems);
		for(int i = 0; i < numItems; ++i)
		{
			ser.BeginGroup("Items");
			bool nextItemExists = false;
			ser.Value("nextItemExists", nextItemExists);
			if(nextItemExists)
			{
				ser.Value("ItemName", itemName);
				EntityId id = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GiveItem(pActor, itemName.c_str(), false, false, false);
				IItem *pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(id);
				if(pItem)
				{
					bool dualWieldMaster = pItem->IsDualWieldMaster();
					ser.Value("DualWieldMaster", dualWieldMaster);
					if(dualWieldMaster && ser.IsReading())
						dualWieldMasters.push_back(pItem->GetEntityId());
					//actual serialization
					pItem->SerializeLTL(ser);
				}
				else
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Couldn't spawn inventory item %s, got id %i.", itemName.c_str(), id);
			}

			ser.EndGroup();
		}
		ser.Value("CurrentItemName", itemName);
		if(stricmp(itemName.c_str(), "none"))
		{
			currentItemNameOnReading = itemName;
			IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(itemName.c_str());
			if(pClass)
			{
				if(IItem* pItem = m_pGameFrameWork->GetIItemSystem()->GetItem(GetItemByClass(pClass)))
				{
					if(pActor->GetCurrentItem() && pActor->GetCurrentItem() != pItem)
						pActor->GetCurrentItem()->Select(false);
					pItem->Select(true);
					m_stats.currentItemId = pItem->GetEntityId();
				}
				else
					m_stats.currentItemId = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GiveItem(pActor, itemName.c_str(), false, true, false);
			}
		}
	}
	else
	{
		numItems  = m_stats.slots.size();
		ser.Value("numOfItems", numItems);
		
		for(int i = 0; i < numItems; ++i)
		{
			ser.BeginGroup("Items");
			IItem *pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_stats.slots[i]);
			bool nextItem = true;
			if(pItem)
			{
				ser.Value("nextItemExists", nextItem);
				ser.Value("ItemName", pItem->GetEntity()->GetClass()->GetName());
				bool dualWieldMaster = pItem->IsDualWieldMaster();
				ser.Value("DualWieldMaster", dualWieldMaster);
				pItem->SerializeLTL(ser);
			}
			else
			{
				nextItem = false;
				ser.Value("nextItemExists", nextItem);
			}
			ser.EndGroup();
		}

		IItem *pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_stats.currentItemId);
		if(pItem && !pItem->IsMounted())
		{
			ser.Value("CurrentItemName", pItem->GetEntity()->GetClass()->GetName());
		}
		else
		{
			string name("none");
			ser.Value("CurrentItemName", name);
		}
	}

	//**************************************DUAL WIELD

	if(ser.IsReading())
	{
		for(int i = 0; i < dualWieldMasters.size(); ++i)
		{
			IItem *pMaster = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(dualWieldMasters[i]);
			assert(FindItem(dualWieldMasters[i]) != -1);
			if(pMaster)
			{
				IItem *pSlave = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(
					GetItemByClass(pMaster->GetEntity()->GetClass(), pMaster));
				if(pSlave)
				{
					//pick up selects slaves already, but doesn't serialize the attachments ...
					pMaster->ResetDualWield();
					pSlave->ResetDualWield();
					//also can't trust pickup to select right master/slave on quickload
					pSlave->SetDualWieldMaster(pMaster->GetEntityId());
					pMaster->SetDualWieldSlave(pSlave->GetEntityId());
					pSlave->RemoveAllAccessories();
					pMaster->SetDualSlaveAccessory(true);
					if(currentItemNameOnReading.size() && !stricmp(currentItemNameOnReading.c_str(), pMaster->GetEntity()->GetClass()->GetName()))
					{
						pMaster->Select(false);
						pMaster->Select(true);
					}
				}
			}
		}
	}

	//**************************************AMMO

	ser.BeginGroup("Ammo");
	if(ser.IsReading())
	{
		m_stats.ammo.clear();
		m_stats.ammoCapacity.clear();
	}

	TAmmoMap::iterator it = m_stats.ammo.begin();
	int ammoAmount = m_stats.ammo.size();
	ser.Value("AmmoAmount", ammoAmount);
	for(int i = 0; i < ammoAmount; ++i, ++it)
	{
		string name;
		int amount = 0;
		if(ser.IsWriting())
		{
			assert(it->first);
			name = (it->first)?it->first->GetName():"";
			amount = it->second;
		}
		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.EndGroup();
		if(ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			assert(pClass);
			m_stats.ammo[pClass] = amount;
		}
	}

	it = m_stats.ammoCapacity.begin();
	int ammoCapacity = m_stats.ammoCapacity.size();
	ser.Value("AmmoCapacity", ammoCapacity);
	for(int i = 0; i < ammoCapacity; ++i, ++it)
	{
		string name;
		int amount = 0;
		if(ser.IsWriting())
		{
			name = it->first->GetName();
			amount = it->second;
		}
		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.EndGroup();
		if(ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			assert(pClass);
			m_stats.ammoCapacity[pClass] = amount;
		}
	}
	ser.EndGroup();

	//*******************************fixup

	if(ser.IsReading() && GetCurrentItem())
	{
		if(IItem *pItem = m_pGameFrameWork->GetIItemSystem()->GetItem(GetCurrentItem()))
		{
			if(EntityId ownerId = pItem->GetOwnerId())
			{
				if(pItem->GetIWeapon() && pItem->GetEntityId() != GetHolsteredItem())
				{
					if(IActor *pOwner = m_pGameFrameWork->GetIActorSystem()->GetActor(ownerId))
					{
						ActionId next("nextItem");
						ActionId prev("prevItem");
						pOwner->GetGameObject()->OnAction(next, 1, 1);
						pOwner->GetGameObject()->OnAction(prev, 1, 1);
					}
				}
			}
		}
	}

	m_bSerializeLTL = false;
}

//------------------------------------------------------------------------
void CInventory::ProcessEvent(SEntityEvent& event)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	if (event.event==ENTITY_EVENT_RESET)
	{
		if (gEnv->pSystem->IsEditor())
		{
			if (event.nParam[0]) // entering game mode in editor
				m_editorstats=m_stats;
			else
			{// leaving game mode
				if (m_stats.currentItemId != m_editorstats.currentItemId)
					m_pGameFrameWork->GetIItemSystem()->SetActorItem(m_pActor, m_editorstats.currentItemId, false);
				m_stats=m_editorstats;
			}
		}
	}
}

//------------------------------------------------------------------------
bool CInventory::AddItem(EntityId id)
{
	if (FindItem(id)>-1)
		return true;

	if (m_stats.slots.size() >= m_capacity)
		return false;

	m_stats.slots.push_back(id);
	std::sort(m_stats.slots.begin(), m_stats.slots.end(), compare_slots());
	return true;
}

//------------------------------------------------------------------------
bool CInventory::RemoveItem(EntityId id)
{
	TInventoryIt it = std::find(m_stats.slots.begin(), m_stats.slots.end(), id);
	if (it == m_stats.slots.end())
		return false;

	m_stats.slots.erase(it);
	return true;
}

//------------------------------------------------------------------------
void CInventory::RemoveAllItems()
{
  this->Clear();
}

//------------------------------------------------------------------------
int CInventory::Validate()
{
	int count = 0;
	bool done = false;
	while(!done)
	{
		IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
		for (TInventoryIt it = m_stats.slots.begin(); it != m_stats.slots.end(); ++it)
		{
			IEntity *pEntity = pEntitySystem->GetEntity(*it);
			if (!pEntity)
			{
				++count;
				m_stats.slots.erase(it);
				break;
			}
			done = true;
		}
	}

	return count;
}

//------------------------------------------------------------------------
void CInventory::Destroy()
{
	if(!GetISystem()->IsSerializingFile())
	{
		IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
		for (TInventoryIt it = m_stats.slots.begin(); it != m_stats.slots.end(); ++it)
			pEntitySystem->RemoveEntity(*it);
	}

	Clear();
}

//------------------------------------------------------------------------
void CInventory::Clear()
{
	m_stats.slots.resize(0);
	m_stats.ammo.clear();

	m_stats.currentItemId = 0;
	m_stats.holsteredItemId = 0;
	m_stats.lastItemId = 0;
}

//------------------------------------------------------------------------
void CInventory::Dump() const
{
	struct Dumper
	{
		Dumper(EntityId entityId, const char *desc)
		{
			IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
			IEntity *pEntity = pEntitySystem->GetEntity(entityId);
			if (pEntity)
				CryLogAlways(">> [%s] $3%s $5%s", pEntity->GetName(), pEntity->GetClass()->GetName(), desc?desc:"");
		}
	};

	int count = GetCount();
	CryLogAlways("-- $3%s$1's Inventory: %d Items --", GetEntity()->GetName(), count);

	if (count)
	{
		for (TInventoryCIt it = m_stats.slots.begin(); it != m_stats.slots.end(); ++it)
			Dumper dump(*it, 0);
	}

	CryLogAlways(">> --");

	Dumper current(m_stats.currentItemId, "Current");
	Dumper last(m_stats.lastItemId, "Last");
	Dumper holstered(m_stats.holsteredItemId, "Holstered");

	CryLogAlways("-- $3%s$1's Inventory: %d Ammo Types --", GetEntity()->GetName(), m_stats.ammo.size());

	if (!m_stats.ammo.empty())
	{
		for (TAmmoMap::const_iterator ait = m_stats.ammo.begin(); ait!=m_stats.ammo.end(); ++ait)
			CryLogAlways(">> [%s] $3%d$1/$3%d", ait->first->GetName(), ait->second, GetAmmoCapacity(ait->first));
	}
}

//------------------------------------------------------------------------
void CInventory::SetCapacity(int size)
{
	m_capacity = size;
	m_stats.slots.reserve(size);
}

//------------------------------------------------------------------------
int CInventory::GetCapacity() const
{
	return m_capacity;
}

//------------------------------------------------------------------------
int CInventory::GetCount() const
{
	return m_stats.slots.size();
}

//------------------------------------------------------------------------
int CInventory::GetCountOfClass(const char *className) const
{
	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
	
	int count = 0;
	for (TInventoryCIt it = m_stats.slots.begin(); it != m_stats.slots.end(); ++it)
	{
		IEntity *pEntity = pEntitySystem->GetEntity(*it);
		if (pEntity)
			if (!strcmp(pEntity->GetClass()->GetName(), className))
				++count;
	}

	return count;
}
//------------------------------------------------------------------------
int CInventory::GetCountOfCategory(const char *categoryName) const
{
	IItemSystem *pItemSystem = gEnv->pGame->GetIGameFramework()->GetIItemSystem();

	int count = 0;
	TInventoryCIt end = m_stats.slots.end();
	for (TInventoryCIt it = m_stats.slots.begin(); it != end; ++it)
	{
		IItem *pItem = pItemSystem->GetItem(*it);
		if (pItem)
		{
			const char *cat = pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
			if (!strcmp(cat, categoryName))
				count++;
		}

	}

	return count;
}

//------------------------------------------------------------------------
int CInventory::GetCountOfUniqueId(uint8 uniqueId) const
{
	//Skip uniqueId 0
	if(!uniqueId)
		return 0;

	IItemSystem *pItemSystem = gEnv->pGame->GetIGameFramework()->GetIItemSystem();

	int count = 0;
	TInventoryCIt end = m_stats.slots.end();
	for (TInventoryCIt it = m_stats.slots.begin(); it != end; ++it)
	{
		IItem *pItem = pItemSystem->GetItem(*it);
		if (pItem)
		{
			uint8 id = pItemSystem->GetItemUniqueId(pItem->GetEntity()->GetClass()->GetName());
			if (id==uniqueId)
				count++;
		}

	}

	return count;
}

//------------------------------------------------------------------------
EntityId CInventory::GetItem(int slotId) const
{
	if (slotId<0 || slotId>=m_stats.slots.size())
		return 0;
	return m_stats.slots[slotId];
}
//------------------------------------------------------------------------
EntityId CInventory::GetItemByClass(IEntityClass* pClass, IItem *pIgnoreItem) const
{
	if (!pClass)
		return 0;

	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;

	TInventoryCIt end = m_stats.slots.end();
	for (TInventoryCIt it = m_stats.slots.begin(); it != end; ++it)
	{
		if (IEntity *pEntity = pEntitySystem->GetEntity(*it))
			if (pEntity->GetClass() == pClass)
				if(!pIgnoreItem || pIgnoreItem->GetEntity() != pEntity)
					return *it;
	}

	return 0;
}

//------------------------------------------------------------------------
int CInventory::FindItem(EntityId itemId) const
{
	for (int i=0; i<m_stats.slots.size(); i++)
	{
		if (m_stats.slots[i]==itemId)
			return i;
	}
	return -1;
}

//------------------------------------------------------------------------
int CInventory::FindNext(IEntityClass *pClass,	const char *category, int firstSlot, bool wrap) const
{
	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
	for (int i = (firstSlot > -1)?firstSlot+1:0; i < m_stats.slots.size(); i++)
	{
		IEntity *pEntity = pEntitySystem->GetEntity(m_stats.slots[i]);
		bool ok=true;
		if (pEntity->GetClass() != pClass)
			ok=false;
		if (ok && category && category[0] && strcmp(m_pGameFrameWork->GetIItemSystem()->GetItemCategory(pEntity->GetClass()->GetName()), category))
			ok=false;

		if (ok)
			return i;
	}

	if (wrap && firstSlot > 0)
	{
		for (int i = 0; i < firstSlot; i++)
		{
			IEntity *pEntity = pEntitySystem->GetEntity(m_stats.slots[i]);
			bool ok=true;
			if (pEntity->GetClass() != pClass)
				ok=false;
			if (ok && category && category[0] && strcmp(m_pGameFrameWork->GetIItemSystem()->GetItemCategory(pEntity->GetClass()->GetName()), category))
				ok=false;

			if (ok)
				return i;
		}
	}

	return -1;
}

//------------------------------------------------------------------------
int CInventory::FindPrev(IEntityClass *pClass, const char *category, int firstSlot, bool wrap) const
{
	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
	for (int i = (firstSlot > -1)?firstSlot+1:0; i < m_stats.slots.size(); i++)
	{
		IEntity *pEntity = pEntitySystem->GetEntity(m_stats.slots[firstSlot-i]);
		bool ok=true;
		if (pEntity->GetClass() != pClass)
			ok=false;
		if (ok && category && category[0] && strcmp(m_pGameFrameWork->GetIItemSystem()->GetItemCategory(pEntity->GetClass()->GetName()), category))
			ok=false;

		if (ok)
			return i;
	}

	if (wrap && firstSlot > 0)
	{
		int count = GetCount();
		for (int i = 0; i < firstSlot; i++)
		{
			IEntity *pEntity = pEntitySystem->GetEntity(m_stats.slots[count-i+firstSlot]);
			bool ok=true;
			if (pEntity->GetClass() != pClass)
				ok=false;
			if (ok && category && category[0] && strcmp(m_pGameFrameWork->GetIItemSystem()->GetItemCategory(pEntity->GetClass()->GetName()), category))
				ok=false;

			if (ok)
				return i;
		}
	}

	return -1;
}

//------------------------------------------------------------------------
EntityId CInventory::GetHolsteredItem() const
{
	return m_stats.holsteredItemId;
}

//------------------------------------------------------------------------
EntityId CInventory::GetCurrentItem() const
{
	return m_stats.currentItemId;
}

//------------------------------------------------------------------------
void CInventory::SetCurrentItem(EntityId itemId)
{
	m_stats.currentItemId = itemId;
}

//------------------------------------------------------------------------
void CInventory::SetLastItem(EntityId itemId)
{
	m_stats.lastItemId = itemId;
}

//------------------------------------------------------------------------
EntityId CInventory::GetLastItem() const
{
	if (FindItem(m_stats.lastItemId)>-1)
		return m_stats.lastItemId;

	return 0;
}

//------------------------------------------------------------------------
void CInventory::HolsterItem(bool holster)
{
	//CryLogAlways("%s::HolsterItem(%s)", GetEntity()->GetName(), holster?"true":"false");

	if (!holster)
	{
		if (m_stats.holsteredItemId)
			m_pGameFrameWork->GetIItemSystem()->SetActorItem(GetActor(), m_stats.holsteredItemId, false);
		m_stats.holsteredItemId = 0;
	}
	else if (m_stats.currentItemId && (!m_stats.holsteredItemId || m_stats.holsteredItemId == m_stats.currentItemId))
	{
		m_stats.holsteredItemId = m_stats.currentItemId;
		m_pGameFrameWork->GetIItemSystem()->SetActorItem(GetActor(), (EntityId)0, false);
	}
}

//------------------------------------------------------------------------
void CInventory::SetAmmoCount(IEntityClass* pAmmoType, int count)
{
	int capacity=GetAmmoCapacity(pAmmoType);
	assert(pAmmoType);
	if(pAmmoType)
	{
		if (capacity>0)
			m_stats.ammo[pAmmoType]=MIN(count, capacity);
		else
			m_stats.ammo[pAmmoType]=count;
	}
}

//------------------------------------------------------------------------
int CInventory::GetAmmoCount(IEntityClass* pAmmoType) const
{
	TAmmoMap::const_iterator it=m_stats.ammo.find(pAmmoType);
	if (it!=m_stats.ammo.end())
		return it->second;

	return 0;
}

//------------------------------------------------------------------------
void CInventory::SetAmmoCapacity(IEntityClass* pAmmoType, int max)
{
	if(pAmmoType)
		m_stats.ammoCapacity[pAmmoType]=max;
}

//------------------------------------------------------------------------
int CInventory::GetAmmoCapacity(IEntityClass* pAmmoType) const
{
	TAmmoMap::const_iterator it=m_stats.ammoCapacity.find(pAmmoType);
	if (it!=m_stats.ammoCapacity.end())
		return it->second;

	return 0;
}

//------------------------------------------------------------------------
IEntityClass*CInventory::GetAmmoTypeByIdx(int idx) const
{
	if (idx>=0 && !m_stats.ammo.empty() && idx<(int)m_stats.ammo.size())
	{
		TAmmoMap::const_iterator it=m_stats.ammo.begin();
		std::advance(it, idx);

		return it->first;
	}

	return 0;
}

//------------------------------------------------------------------------
int CInventory::GetAmmoTypeCount() const
{
	return (int)m_stats.ammo.size();
}


//------------------------------------------------------------------------
void CInventory::ResetAmmo()
{
	m_stats.ammo.clear();
}