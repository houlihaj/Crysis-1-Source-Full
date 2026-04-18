/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Manages items.
  
 -------------------------------------------------------------------------
  History:
  - 29:9:2004   18:01 : Created by Márcio Martins

*************************************************************************/
#ifndef __ITEMSYSTEM_H__
#define __ITEMSYSTEM_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameFramework.h"
#include "IItemSystem.h"
#include "ILevelSystem.h"
#include "ItemParams.h"
#include "EquipmentManager.h"

#define DEFAULT_ITEM_SCRIPT			"Scripts/Entities/Items/Item.lua"
#define DEFAULT_ITEM_CREATEFUNC	"CreateItemTable"

class CItemSystem :
	public ILevelSystemListener,
	public IItemSystem
{
public:
	CItemSystem(IGameFramework *pGameFramework, ISystem *pSystem);
	virtual ~CItemSystem();

	void Release() { delete this; };
	void Update();

	// IItemSystem
	virtual void Reload();
	virtual void Reset();
	virtual void Scan(const char *folderName);
	virtual IItemParamsNode *CreateParams() { return new CItemParamsNode; };
	virtual const IItemParamsNode *GetItemParams(const char *itemName) const;
	virtual int GetItemParamsCount() const;
	virtual const char* GetItemParamName(int index) const;
	virtual uint8 GetItemPriority(const char *item) const;
	virtual const char *GetItemCategory(const char *item) const;
	virtual uint8   GetItemUniqueId(const char *item) const;

	virtual bool IsItemClass(const char *name) const;

	virtual void RegisterForCollection(EntityId itemId);
	virtual void UnregisterForCollection(EntityId itemId);

	virtual void AddItem(EntityId itemId, IItem *pItem);
	virtual void RemoveItem(EntityId itemId);
	virtual IItem *GetItem(EntityId itemId) const;

	virtual ICharacterInstance *GetCachedCharacter(const char *fileName);
	virtual IStatObj *GetCachedObject(const char *fileName);
	virtual void CacheObject(const char *fileName);
	virtual void CacheGeometry(const IItemParamsNode *geometry);
	virtual void CacheItemGeometry(const char *className);
	virtual void ClearGeometryCache();

	virtual void CacheItemSound(const char *className);
	virtual void ClearSoundCache();

	virtual EntityId GiveItem(IActor *pActor, const char *item, bool sound, bool select=true, bool keepHistory=true);
	virtual void SetActorItem(IActor *pActor, EntityId itemId, bool keepHistory);
	virtual void SetActorItem(IActor *pActor, const char *name, bool keepHistory);
	virtual void SetActorAccessory(IActor *pActor, EntityId itemId, bool keepHistory);
	virtual void DropActorItem(IActor *pActor, EntityId itemId);
	virtual void DropActorAccessory(IActor *pActor, EntityId itemId);

	virtual void SetConfiguration(const char *name) { m_config=name; };
	virtual const char *GetConfiguration() const { return m_config.c_str(); };

	virtual void RegisterListener(IItemSystemListener *pListener);
	virtual void UnregisterListener(IItemSystemListener *pListener);

	virtual void Serialize(TSerialize ser);
	virtual void SerializePlayerLTLInfo(bool bReading);

	virtual IEquipmentManager* GetIEquipmentManager() { return m_pEquipmentManager; }
	// ~IItemSystem

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char *levelName) {};
	virtual void OnLoadingStart(ILevelInfo *pLevel);
	virtual void OnLoadingComplete(ILevel *pLevel);
	virtual void OnLoadingError(ILevelInfo *pLevel, const char *error) {};
	virtual void OnLoadingProgress(ILevelInfo *pLevel, int progressAmount) {};
	//~ILevelSystemListener
	
	bool ScanXML(XmlNodeRef &root, const char *xmlFile);

	void RegisterItemClass(const char *name, IGameFramework::IItemCreator *pCreator);
	void PrecacheLevel();
	void PrecacheSound();

	void GetMemoryStatistics(ICrySizer * s);

private:
	void CreateItemTable(const char *name);
	void RegisterCVars();
	void UnregisterCVars();
	static void GiveItemCmd(IConsoleCmdArgs *args);
	static void GiveItemsHelper(IConsoleCmdArgs *args, bool useGiveable, bool useDebug);
	static void GiveAllItemsCmd(IConsoleCmdArgs *args);
	static void GiveDebugItemsCmd(IConsoleCmdArgs *args);
	static ICVar *m_pPrecache;
	static ICVar *m_pItemLimit;

	struct SSpawnUserData
	{
		SSpawnUserData( const char * cls, uint16 channel ) : className(cls), channelId(channel) {}
		const char * className;
		uint16 channelId;
	};

	typedef std::map<string, IStatObj *>						TObjectCache;
	typedef TObjectCache::iterator									TObjectCacheIt;
	
	typedef std::map<string, ICharacterInstance *>	TCharacterCache;
	typedef TCharacterCache::iterator								TCharacterCacheIt;

	typedef struct SItemParamsDesc
	{
		SItemParamsDesc(): precacheFlags(0), params(0) {};
		~SItemParamsDesc()
		{
			if (params)
				params->Release(); 

			if (!configurations.empty())
				for (std::map<string, CItemParamsNode *>::iterator it=configurations.begin(); it!=configurations.end();++it)
					it->second->Release();
			configurations.clear();
		};

		CItemParamsNode *params;
		uint8						priority;
		string					category;
		uint8             uniqueId;

		std::map<string, CItemParamsNode *> configurations;

		enum PrecacheFlags
		{
			ePreCached_Sound		= 1 << 0,
			ePreCached_Geometry	= 1 << 1
		};

		uint8 precacheFlags;
	} SItemParamsDesc;

	typedef struct SItemClassDesc
	{
		SItemClassDesc(): pCreator(0) {};
		SItemClassDesc(IGameObjectExtensionCreatorBase *pCrtr): pCreator(pCrtr) {};

		IGameObjectExtensionCreatorBase *pCreator;
	} SItemClassDesc;

	typedef std::map<string, SItemClassDesc>				TItemClassMap;
	typedef std::map<string, SItemParamsDesc>				TItemParamsMap;

	typedef std::map<EntityId, IItem *>							TItemMap;
	typedef std::vector<IItemSystemListener*>       TListenerVec;

	typedef std::vector<string>											TFolderList;

	typedef std::map<EntityId, CTimeValue>					TCollectionMap;

	CTimeValue				m_precacheTime;

	TObjectCache			m_objectCache;
	TCharacterCache		m_characterCache;

	ISystem						*m_pSystem;
	IGameFramework		*m_pGameFramework;
	IEntitySystem			*m_pEntitySystem;

	XmlNodeRef				m_playerLevelToLevelSave;

	TItemClassMap			m_classes;
	TItemParamsMap		m_params;
	TItemMap					m_items;
	TListenerVec      m_listeners;
	uint							m_spawnCount;

	TCollectionMap		m_collectionmap;

	TFolderList				m_folders;
	bool							m_reloading;
	bool							m_recursing;

	string						m_config;

	CEquipmentManager* m_pEquipmentManager;

	const string			m_grenadeType;
};

#endif //__ITEMSYSTEM_H__
