/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Inventory GameObject Extension

-------------------------------------------------------------------------
History:
- 29:8:2005   14:27 : Created by Márcio Martins

*************************************************************************/
#ifndef __INVENTORY_H__
#define __INVENTORY_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "CryAction.h"
#include "IGameObject.h"
#include "IItemSystem.h"
#include <IEntitySystem.h>


class CInventory :
	public CGameObjectExtensionHelper<CInventory, IInventory>
{
	typedef std::vector<EntityId>							TInventoryVector;
	typedef TInventoryVector::const_iterator	TInventoryCIt;
	typedef TInventoryVector::iterator				TInventoryIt;
	typedef std::map<IEntityClass*, int>			TAmmoMap;
public:
	CInventory();
	virtual ~CInventory();

	//IGameObjectExtension
	virtual bool Init( IGameObject * pGameObject );
	virtual void InitClient(int channelId) {};
	virtual void PostInit( IGameObject * pGameObject ) {};
	virtual void PostInitClient(int channelId) {};
	virtual void Release() { delete this; };
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) { return true; }
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update( SEntityUpdateContext& ctx, int ) {};
	virtual void PostUpdate(float frameTime ) {};
	virtual void PostRemoteSpawn() {};
	virtual void HandleEvent( const SGameObjectEvent& ) {};
	virtual void ProcessEvent( SEntityEvent& );
	virtual void SetChannelId(uint16 id) {};
	virtual void SetAuthority( bool auth ) {};
	//~IGameObjectExtension

	//IInventory
	virtual bool AddItem(EntityId id);
	virtual bool RemoveItem(EntityId id);
  virtual void RemoveAllItems();

	virtual int Validate();
	virtual void Destroy();
	virtual void Clear();

	virtual void Dump() const;

	virtual void SetCapacity(int size);
	virtual int GetCapacity() const;
	virtual int GetCount() const;
	virtual int GetCountOfClass(const char *className) const;
	virtual int GetCountOfCategory(const char *categoryName) const;
	virtual int GetCountOfUniqueId(uint8 uniqueId) const;
	
	virtual EntityId GetItem(int slotId) const;
	virtual EntityId GetItemByClass(IEntityClass *pClass, IItem *pIgnoreItem = NULL) const;

	virtual int FindItem(EntityId itemId) const;

	virtual int FindNext(IEntityClass *pClass, const char *category, int firstSlot, bool wrap) const;
	virtual int FindPrev(IEntityClass *pClass, const char *category, int firstSlot, bool wrap) const;

	virtual EntityId GetCurrentItem() const;
	virtual EntityId GetHolsteredItem() const;
	virtual void SetCurrentItem(EntityId itemId);

	virtual void SetLastItem(EntityId itemId);
	virtual EntityId GetLastItem() const;

	virtual void HolsterItem(bool holster);

	virtual void SerializeInventoryForLevelChange( TSerialize ser );
	virtual bool IsSerializingForLevelChange() const { return m_bSerializeLTL; }

	// these functions are not multiplayer safe..
	// any changes made using these functions will not be synchronized...
	virtual void SetAmmoCount(IEntityClass* pAmmoType, int count);
	virtual int GetAmmoCount(IEntityClass* pAmmoType) const;
	virtual void SetAmmoCapacity(IEntityClass* pAmmoType, int max);
	virtual int GetAmmoCapacity(IEntityClass* pAmmoType) const;

	virtual IEntityClass*GetAmmoTypeByIdx(int idx) const;
	virtual int GetAmmoTypeCount() const;

	virtual void ResetAmmo();

	virtual IActor *GetActor() { return m_pActor; };
	//~IInventory

	virtual void GetMemoryStatistics(ICrySizer * s) 
	{
		s->Add(*this);
		m_stats.GetMemoryStatistics(s);
		m_editorstats.GetMemoryStatistics(s);
	}

private:
	struct compare_slots
	{
		compare_slots()
		{
			m_pEntitySystem=gEnv->pEntitySystem;
			m_pItemSystem=CCryAction::GetCryAction()->GetIItemSystem();
		}
		bool operator() (const EntityId lhs, const EntityId rhs) const
		{
			const IEntity *pLeft = m_pEntitySystem->GetEntity(lhs);
			const IEntity *pRight = m_pEntitySystem->GetEntity(rhs);
			const uint lprio = pLeft?m_pItemSystem->GetItemPriority(pLeft->GetClass()->GetName()):0;
			const uint rprio = pRight?m_pItemSystem->GetItemPriority(pRight->GetClass()->GetName()):0;

			if (lprio != rprio)
				return lprio<rprio;
			else
				return lhs<rhs;
		}

		IEntitySystem* m_pEntitySystem;
		IItemSystem* m_pItemSystem;
	};

	struct SInventoryStats
	{
		SInventoryStats()
		: currentItemId(0),
			holsteredItemId(0),
			lastItemId(0)
		{};

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->AddContainer(ammo);
			s->AddContainer(ammoCapacity);
			s->AddContainer(slots);

			for (TAmmoMap::iterator iter = ammo.begin(); iter != ammo.end(); ++iter)
				s->Add(iter->first);
			for (TAmmoMap::iterator iter = ammoCapacity.begin(); iter != ammoCapacity.end(); ++iter)
				s->Add(iter->first);
		}

		TInventoryVector	slots;
		TAmmoMap					ammo;
		TAmmoMap					ammoCapacity;
		EntityId					currentItemId;
		EntityId					holsteredItemId;
		EntityId					lastItemId;
	};

	SInventoryStats		m_stats;
	SInventoryStats		m_editorstats;
	int								m_capacity;

	IGameFramework		*m_pGameFrameWork;
	IActor						*m_pActor;
	bool							m_bSerializeLTL;
};


#endif //__INVENTORY_H__