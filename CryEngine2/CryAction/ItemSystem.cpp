/************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 29:9:2004   18:02 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ItemSystem.h"
#include <CryPath.h>
//#include "ScriptBind_ItemSystem.h"
#include "IActionMapManager.h"
#include "GameObjects/GameObject.h"
#include "CryAction.h"
#include "IGameObjectSystem.h"
#include "IActorSystem.h"

#include <IEntitySystem.h>
#include <ICryAnimation.h>
#include <IVehicleSystem.h>
#include "ItemParams.h"
#include "EquipmentManager.h"

ICVar *CItemSystem::m_pPrecache = 0;
ICVar *CItemSystem::m_pItemLimit = 0;


//------------------------------------------------------------------------
CItemSystem::CItemSystem(IGameFramework *pGameFramework, ISystem *pSystem)
: m_pGameFramework(pGameFramework),
	m_pSystem(pSystem),
	m_pEntitySystem(pSystem->GetIEntitySystem()),
	m_spawnCount(0),
	m_grenadeType("explosivegrenade"),
	m_precacheTime(0.0f),
	m_reloading(false),
	m_recursing(false)
{
	RegisterCVars();
	m_pEquipmentManager = new CEquipmentManager(this);
}

//------------------------------------------------------------------------
CItemSystem::~CItemSystem()
{
	SAFE_DELETE(m_pEquipmentManager);
	UnregisterCVars();

	ClearGeometryCache();
	ClearSoundCache();
}

//------------------------------------------------------------------------
void CItemSystem::OnLoadingStart(ILevelInfo *pLevelInfo)
{
	Reset();

	ClearGeometryCache();
	ClearSoundCache();

	if (gEnv->bMultiplayer)
		SetConfiguration("mp");
	else
		SetConfiguration("");
}

//------------------------------------------------------------------------
void CItemSystem::OnLoadingComplete(ILevel *pLevel)
{
	// marcio: precaching of items enabled by default for now
//	ICVar *sys_preload=m_pSystem->GetIConsole()->GetCVar("sys_preload");
//	if ((!sys_preload || sys_preload->GetIVal()) && m_pPrecache->GetIVal())
	{
		PrecacheLevel();
	}
}

//------------------------------------------------------------------------
void CItemSystem::RegisterItemClass(const char *name, IGameFramework::IItemCreator *pCreator)
{
	m_classes.insert(TItemClassMap::value_type(name, SItemClassDesc(pCreator)));
}

//------------------------------------------------------------------------
void CItemSystem::Update()
{
	ITimer *pTimer = m_pSystem->GetITimer();
	IActor *pActor = m_pGameFramework->GetClientActor();
	
	if (pActor)
	{
		IInventory* pInventory = pActor->GetInventory();
		if (pInventory)
		{
      IItem *pCurrentItem = GetItem(pInventory->GetCurrentItem());      
      if(pCurrentItem)
			{
				pCurrentItem->UpdateFPView(pTimer->GetFrameTime());
      }

			// marcok: argh ... we depend on Crysis game stuff here ... REMOVE
			IEntityClass* pOffHandClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("OffHand");
			IItem *pOffHandItem = GetItem(pInventory->GetItemByClass(pOffHandClass));

			// HAX: update offhand - shouldn't be here
			if(pOffHandItem)
				pOffHandItem->UpdateFPView(pTimer->GetFrameTime());
		}
	}
}

//------------------------------------------------------------------------
const IItemParamsNode *CItemSystem::GetItemParams(const char *itemName) const
{
	TItemParamsMap::const_iterator it = m_params.find(CONST_TEMP_STRING(itemName));
	if (it != m_params.end())
	{
		if (m_config.empty())
			return it->second.params;

		std::map<string, CItemParamsNode *>::const_iterator cit=it->second.configurations.find(m_config);
		if (cit != it->second.configurations.end())
			return cit->second;

		return it->second.params;
	}

	GameWarning("Trying to get xml description for item '%s'! Something very wrong has happened!", itemName);
	return 0;
}

//------------------------------------------------------------------------
uint8 CItemSystem::GetItemPriority(const char *itemName) const
{
	TItemParamsMap::const_iterator it = m_params.find(CONST_TEMP_STRING(itemName));
	if (it != m_params.end())
		return it->second.priority;

	GameWarning("Trying to get priority for item '%s'! Something very wrong has happened!", itemName);
	return 0;
}

//------------------------------------------------------------------------
const char *CItemSystem::GetItemCategory(const char *itemName) const
{
	TItemParamsMap::const_iterator it = m_params.find(CONST_TEMP_STRING(itemName));
	if (it != m_params.end())
		return it->second.category.c_str();

	GameWarning("Trying to get category for item '%s'! Something very wrong has happened!", itemName);
	return 0;
}

//-------------------------------------------------------------------------
uint8 CItemSystem::GetItemUniqueId(const char *itemName) const
{
	TItemParamsMap::const_iterator it = m_params.find(CONST_TEMP_STRING(itemName));
	if (it != m_params.end())
		return it->second.uniqueId;

	GameWarning("Trying to get uniqueId for item '%s'! Something very wrong has happened!", itemName);
	return 0;
}

//------------------------------------------------------------------------
bool CItemSystem::IsItemClass(const char *name) const
{
	TItemParamsMap::const_iterator it = m_params.find(CONST_TEMP_STRING(name));
	return (it != m_params.end());
}

//------------------------------------------------------------------------
void CItemSystem::RegisterForCollection(EntityId itemId)
{
#if _DEBUG
	IItem *pItem=GetItem(itemId);
	assert(pItem);

	if (pItem)
	{
		assert(!pItem->GetOwnerId());
	}
#endif

	CTimeValue now=gEnv->pTimer->GetFrameStartTime();

	TCollectionMap::iterator cit = m_collectionmap.find(itemId);
	if (cit == m_collectionmap.end())
		m_collectionmap.insert(TCollectionMap::value_type(itemId, now));
	else
		cit->second=now;

	if (gEnv->bServer && gEnv->bMultiplayer)
	{
		int limit=0;
		if (m_pItemLimit)
			limit=m_pItemLimit->GetIVal();

		if (limit>0)
		{
			int size=(int)m_collectionmap.size();
			if (size>limit)
			{
				EntityId itemId=0;
				for (TCollectionMap::const_iterator it=m_collectionmap.begin(); it!=m_collectionmap.end(); ++it)
				{
					if (it->second<now)
					{
						now=it->second;
						itemId=it->first;
					}
				}

				if (IItem *pItem=GetItem(itemId))
				{
					assert(!pItem->GetOwnerId());

				
					CryLogAlways("[game] Removing item %s due to i_lying_item_limit!", pItem->GetEntity()->GetName());
					UnregisterForCollection(itemId);
					gEnv->pEntitySystem->RemoveEntity(itemId);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CItemSystem::UnregisterForCollection(EntityId itemId)
{
	TCollectionMap::iterator cit = m_collectionmap.find(itemId);
	if (cit != m_collectionmap.end())
		m_collectionmap.erase(cit);
}

//------------------------------------------------------------------------
void CItemSystem::Reload()
{
	m_reloading=true;
	
	m_params.clear();

	for (TFolderList::iterator fit=m_folders.begin(); fit!=m_folders.end();++fit)
		Scan(fit->c_str());

	SEntityEvent event(ENTITY_EVENT_RESET);

	for (TItemMap::iterator it=m_items.begin(); it!=m_items.end();++it)
	{
		IEntity *pEntity = m_pEntitySystem->GetEntity(it->first);
		if (pEntity)
			pEntity->SendEvent(event);
	}

	m_reloading=false;
}

//------------------------------------------------------------------------
void CItemSystem::Reset()
{
	if(GetISystem()->IsSerializingFile() == 1)
	{
		TItemMap::iterator it = m_items.begin();
		IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
		for (; it != m_items.end(); ++it)
		{
			EntityId id = it->first;
			IEntity *pEntity = pEntitySystem->GetEntity(id);
			if(!pEntity)
			{
				m_items.erase(it);
				--it;
			}
		}
	}
}

//------------------------------------------------------------------------
void CItemSystem::Scan(const char *folderName)
{
	string folder = folderName;
	string search = folder;
	search += "/*.*";

	ICryPak *pPak = m_pSystem->GetIPak();

	_finddata_t fd;
	intptr_t handle = pPak->FindFirst(search.c_str(), &fd);

	if (!m_recursing)
		CryLog("Loading item XML definitions from '%s'!", folderName);

	if (handle > -1)
	{
		do
		{
			if (!strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
				continue;

			if (fd.attrib & _A_SUBDIR)
			{
				string subName = folder+"/"+fd.name;
				if (m_recursing)
					Scan(subName.c_str());
				else
				{
					m_recursing=true;
					Scan(subName.c_str());
					m_recursing=false;
				}
				continue;
			}

			if (stricmp(PathUtil::GetExt(fd.name), "xml"))
				continue;

			string xmlFile = folder + string("/") + string(fd.name);
			XmlNodeRef rootNode = m_pSystem->LoadXmlFile(xmlFile.c_str());

			if (!rootNode)
			{
				GameWarning("Invalid XML file '%s'! Skipping...", xmlFile.c_str());
				continue;
			}

			if (!ScanXML(rootNode, xmlFile.c_str()))
				continue;

		} while (pPak->FindNext(handle, &fd) >= 0);
	}

	if (!m_recursing)
		CryLog("Finished loading item XML definitions from '%s'!", folderName);

	if (!m_reloading && !m_recursing)
		m_folders.push_back(folderName);
}

//------------------------------------------------------------------------
bool CItemSystem::ScanXML(XmlNodeRef &root, const char *xmlFile)
{
	if (strcmpi(root->getTag(), "item"))
		return false;

	const char *name = root->getAttr("name");
	if (!name)
	{
		GameWarning("Missing item name in XML '%s'! Skipping...", xmlFile);
		return false;
	}

	const char *className = root->getAttr("class");
	
	if (!className)
	{
		GameWarning("Missing item class in XML '%s'! Skipping...", xmlFile);
		return false;
	}

	TItemClassMap::iterator it = m_classes.find(CONST_TEMP_STRING(className));
	if (it == m_classes.end())
	{
		GameWarning("Unknown item class '%s' specified in XML '%s'! Skipping...", className, xmlFile);
		return false;
	}

	const char *configName = root->getAttr("configuration");
	TItemParamsMap::iterator dit=m_params.find(CONST_TEMP_STRING(name));
	
	if (dit == m_params.end())
	{
		const char *scriptName = root->getAttr("script");
		IEntityClassRegistry::SEntityClassDesc classDesc;
		classDesc.sName = name;
		if (scriptName && scriptName[0])
			classDesc.sScriptFile = scriptName;
		else
		{
			classDesc.sScriptFile = DEFAULT_ITEM_SCRIPT;
			CreateItemTable(name);
		}

		int invisible=0;
		root->getAttr("invisible", invisible);

		classDesc.pUserProxyCreateFunc = (IEntityClass::UserProxyCreateFunc)it->second.pCreator;
		classDesc.flags |= invisible?ECLF_INVISIBLE:0;

		if (!m_reloading)
			CCryAction::GetCryAction()->GetIGameObjectSystem()->RegisterExtension( name, it->second.pCreator, &classDesc );

		std::pair<TItemParamsMap::iterator, bool> result = m_params.insert(TItemParamsMap::value_type(name, SItemParamsDesc()));
		dit = result.first;
	}

	SItemParamsDesc &desc = dit->second;

	if (!configName || !configName[0])
	{
		if (desc.params)
			desc.params->Release();

		desc.params = new CItemParamsNode();
		desc.params->ConvertFromXML(root);

		desc.priority = 0;
		int p=0;
		if (desc.params->GetAttribute("priority", p))
			desc.priority=(uint8)p;
		desc.category = desc.params->GetAttribute("category");
		desc.uniqueId = 0;
		int id=0;
		if (desc.params->GetAttribute("uniqueId", id))
			desc.uniqueId = (uint8)id;
	}
	else
	{
		CItemParamsNode *params = new CItemParamsNode();
		params->ConvertFromXML(root);
		desc.configurations.insert(std::make_pair<string, CItemParamsNode *>(configName, params));
	}

	return true;
}

//------------------------------------------------------------------------
void CItemSystem::AddItem(EntityId itemId, IItem *pItem)
{
	m_items.insert(TItemMap::value_type(itemId, pItem));
}

//------------------------------------------------------------------------
void CItemSystem::RemoveItem(EntityId itemId)
{
	TItemMap::iterator it = m_items.find(itemId);

	if (it != m_items.end())
	{
		m_items.erase(it);
	}

	TCollectionMap::iterator cit = m_collectionmap.find(itemId);
	if (cit != m_collectionmap.end())
	{
		m_collectionmap.erase(cit);
	}
}

//------------------------------------------------------------------------
IItem *CItemSystem::GetItem(EntityId itemId) const
{
	TItemMap::const_iterator it = m_items.find(itemId);

	if (it != m_items.end())
	{
		return it->second;
	}
	return 0;
}

//------------------------------------------------------------------------
EntityId CItemSystem::GiveItem(IActor *pActor, const char *item, bool sound, bool select, bool keepHistory)
{
	if (!gEnv->bServer)
	{
		GameWarning("Trying to spawn item of class '%s' on a process which is not server!", item);
		return 0;
	}

	if(gEnv->pSystem->IsSerializingFile())
		return 0;

	assert(item && pActor);
	static char itemName[65];
	_snprintf(itemName, 64, "%s%.03d", item, ++m_spawnCount);
  itemName[sizeof(itemName)-1] = '\0';
	SEntitySpawnParams params;
	params.sName = itemName;
	params.pClass = m_pEntitySystem->GetClassRegistry()->FindClass(item);
	params.nFlags |= ENTITY_FLAG_NO_PROXIMITY;
	if (!params.pClass)
	{
		GameWarning("Trying to spawn item of class '%s' which is unknown!", item);
		return 0;
	}

	if (IEntity *pItemEnt = m_pEntitySystem->SpawnEntity(params))
	{
		EntityId itemEntId = pItemEnt->GetId();
		if (IItem * pItem = GetItem(itemEntId))
		{
			// this may remove the entity
			pItem->PickUp(pActor->GetEntityId(), sound, select, keepHistory);

			//[kirill] make sure AI gets notified about new item
			if(IAIObject * pActorAI = pActor->GetEntity()->GetAI())
				gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 0, "OnUpdateItems", pActorAI);

			if ((pItemEnt = gEnv->pEntitySystem->GetEntity(itemEntId)) && !pItemEnt->IsGarbage())
			{
				// set properties table to null, since they'll not be used [timur's request]
				if (pItemEnt->GetScriptTable())
					pItemEnt->GetScriptTable()->SetToNull("Properties");

				return pItemEnt->GetId();
			}
		}
	}

	return 0;
}

//------------------------------------------------------------------------
void CItemSystem::SetActorItem(IActor *pActor, EntityId itemId, bool keepHistory)
{
	if(!pActor)
		return;
	IInventory *pInventory = pActor->GetInventory();
	if (!pInventory)
		return;

	EntityId currentItemId = pInventory->GetCurrentItem();

	if (currentItemId == itemId)
		return;

	if (currentItemId)
	{
		IItem *pCurrentItem = GetItem(currentItemId);
		if (pCurrentItem)
		{
			pInventory->SetCurrentItem(0);
			pCurrentItem->Select(false);
			if (keepHistory)
				pInventory->SetLastItem(currentItemId);			
		}
	}

	IItem *pItem = GetItem(itemId);

	TListenerVec::iterator iter = m_listeners.begin();
	while (iter != m_listeners.end())
	{
		(*iter)->OnSetActorItem(pActor, pItem);
		++iter;
	}

	if (!pItem)
		return;

	pInventory->SetCurrentItem(itemId);
	pItem->SetHand(IItem::eIH_Right);
	pItem->Select(true);

	CCryAction::GetCryAction()->GetIGameplayRecorder()->Event(pActor->GetEntity(), GameplayEvent(eGE_ItemSelected, 0, 0, (void *)itemId));
}

//------------------------------------------------------------------------
void CItemSystem::SetActorAccessory(IActor *pActor, EntityId itemId, bool keepHistory)
{
	if(!pActor) return;
	IInventory *pInventory = pActor->GetInventory();
	if (!pInventory) return;

	IItem *pItem = GetItem(itemId);

	TListenerVec::iterator iter = m_listeners.begin();
	while (iter != m_listeners.end())
	{
		(*iter)->OnSetActorItem(pActor, pItem);
		++iter;
	}
}

//------------------------------------------------------------------------

void CItemSystem::DropActorItem(IActor *pActor, EntityId itemId)
{
	if(!pActor) return;
	IInventory *pInventory = pActor->GetInventory();
	if (!pInventory) return;

	IItem *pItem = GetItem(itemId);
	if (!pItem) return;

	TListenerVec::iterator iter = m_listeners.begin();
	while (iter != m_listeners.end())
	{
		(*iter)->OnDropActorItem(pActor, pItem);
		++iter;
	}
}

//------------------------------------------------------------------------

void CItemSystem::DropActorAccessory(IActor *pActor, EntityId itemId)
{
	if(!pActor) return;
	IInventory *pInventory = pActor->GetInventory();
	if (!pInventory) return;

	IItem *pItem = GetItem(itemId);
	if (!pItem) return;

	TListenerVec::iterator iter = m_listeners.begin();
	while (iter != m_listeners.end())
	{
		(*iter)->OnDropActorItem(pActor, pItem);
		++iter;
	}
}

//------------------------------------------------------------------------
void CItemSystem::SetActorItem(IActor *pActor, const char *name, bool keepHistory)
{
	IInventory *pInventory = pActor->GetInventory();
	if (!pInventory)
		return;
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
	EntityId itemId = pInventory->GetItemByClass(pClass);
	if (!itemId)
		return;
	SetActorItem(pActor, itemId, keepHistory);
}

//------------------------------------------------------------------------
void CItemSystem::CacheGeometry(const IItemParamsNode *geometry)
{
	if (!geometry)
		return;

	int n = geometry->GetChildCount();
	if (n>0)
	{
		for (int i=0; i<n; i++)
		{
			const IItemParamsNode *slot = geometry->GetChild(i);
			const char *name = slot->GetAttribute("name");

			if (name && name[0])
				CacheObject(name);
		}
	}
}

//------------------------------------------------------------------------
void CItemSystem::CacheItemGeometry(const char *className)
{
	TItemParamsMap::iterator it=m_params.find(CONST_TEMP_STRING(className));
	if (it==m_params.end())
		return;

	if ((it->second.precacheFlags&SItemParamsDesc::ePreCached_Geometry) == 0)
	{
		if (const IItemParamsNode *root=GetItemParams(className))
		{
			const IItemParamsNode *geometry=root->GetChild("geometry");
			if (geometry)
				CacheGeometry(geometry);
		}

		it->second.precacheFlags|=SItemParamsDesc::ePreCached_Geometry;
	}
}

//------------------------------------------------------------------------
void CItemSystem::CacheItemSound(const char *className)
{
	TItemParamsMap::iterator it=m_params.find(CONST_TEMP_STRING(className));
	if (it==m_params.end())
		return;

	if ((it->second.precacheFlags&SItemParamsDesc::ePreCached_Sound) == 0)
	{
		if (const IItemParamsNode *root=GetItemParams(className))
		{
			const IItemParamsNode *actions=root->GetChild("actions");
			if (actions)
			{
				int n=actions->GetChildCount();
				for (int i=0;i<n;i++)
				{
					const IItemParamsNode *action=actions->GetChild(i);
					if (!stricmp(action->GetName(), "action"))
					{
						int na=action->GetChildCount();
						for (int a=0;a<na;a++)
						{
							const IItemParamsNode *sound=actions->GetChild(i);
							if (!stricmp(sound->GetName(), "sound"))
							{
								const char *soundName=sound->GetNameAttribute();

								gEnv->pSoundSystem->Precache(soundName, 0, FLAG_SOUND_PRECACHE_LOAD_GROUP);
							}
						}
					}
				}
				
			}
		}

		it->second.precacheFlags|=SItemParamsDesc::ePreCached_Sound;
	}
}

//------------------------------------------------------------------------
void CItemSystem::ClearSoundCache()
{
	for (TItemParamsMap::iterator it=m_params.begin(); it!=m_params.end(); ++it)
		it->second.precacheFlags&=~SItemParamsDesc::ePreCached_Sound;
}

//------------------------------------------------------------------------
ICharacterInstance *CItemSystem::GetCachedCharacter(const char *fileName)
{
	CryFixedStringT<256> name(fileName);
	name.replace('\\', '/');
	name.MakeLower();

	TCharacterCacheIt cit = m_characterCache.find(CONST_TEMP_STRING(name.c_str()));

	if (cit == m_characterCache.end())
		return 0;

	return cit->second;
}

//------------------------------------------------------------------------
IStatObj *CItemSystem::GetCachedObject(const char *fileName)
{
	CryFixedStringT<256> name = fileName;
	name.replace('\\', '/');
	name.MakeLower();

	TObjectCacheIt oit = m_objectCache.find(CONST_TEMP_STRING(name.c_str()));

	if (oit == m_objectCache.end())
		return 0;

	return oit->second;
}

//------------------------------------------------------------------------
void CItemSystem::CacheObject(const char *fileName)
{
	string name = PathUtil::ToUnixPath(string(fileName));

	TObjectCacheIt oit = m_objectCache.find(name);
	TCharacterCacheIt cit = m_characterCache.find(name);

	if ((oit != m_objectCache.end()) || (cit != m_characterCache.end()))
	{
		return;
	}

	string ext(PathUtil::GetExt(name));
	ext.MakeLower();

	if ((ext == "cdf") || (ext == "chr") || (ext == "cga"))
	{
		ICharacterInstance *pChar = GetISystem()->GetIAnimationSystem()->CreateInstance(fileName);
		if (pChar)
		{
			pChar->AddRef();
			m_characterCache.insert(TCharacterCache::value_type(name.MakeLower(), pChar));
		}
	}
	else
	{
		IStatObj *pStatObj = gEnv->p3DEngine->LoadStatObj(fileName);
		if (pStatObj)
		{
			pStatObj->AddRef();
			m_objectCache.insert(TObjectCache::value_type(name.MakeLower(), pStatObj));
		}
	}
}

//------------------------------------------------------------------------
void CItemSystem::ClearGeometryCache()
{
	for (TObjectCacheIt oit = m_objectCache.begin(); oit != m_objectCache.end(); oit++)
	{
		if (oit->second)
			oit->second->Release();
	}
	m_objectCache.clear();

	for (TCharacterCacheIt cit = m_characterCache.begin(); cit != m_characterCache.end(); cit++)
		cit->second->Release();
	m_characterCache.clear();

	for (TItemParamsMap::iterator it=m_params.begin(); it!=m_params.end(); ++it)
		it->second.precacheFlags&=~SItemParamsDesc::ePreCached_Geometry;
}

//------------------------------------------------------------------------
void CItemSystem::PrecacheLevel()
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );
	
	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;

	TItemMap::iterator it = m_items.begin();
	TItemMap::iterator end = m_items.end();
	
	for (; it!=end; ++it)
	{
		if(IEntity *pEntity = pEntitySystem->GetEntity(it->first))
		{
			CacheItemGeometry(pEntity->GetClass()->GetName());
			CacheItemSound(pEntity->GetClass()->GetName());
		}
	}
}

//------------------------------------------------------------------------
void CItemSystem::CreateItemTable(const char *name)
{
	IScriptSystem *pScriptSystem = gEnv->pScriptSystem;
	pScriptSystem->ExecuteFile(DEFAULT_ITEM_SCRIPT);
	pScriptSystem->BeginCall(DEFAULT_ITEM_CREATEFUNC);
	pScriptSystem->PushFuncParam(name);
	pScriptSystem->EndCall();
}

//------------------------------------------------------------------------
void CItemSystem::RegisterCVars()
{
	gEnv->pConsole->RegisterInt("i_noweaponlimit", 0, VF_CHEAT, "Player can carry all weapons he wants");
	
	gEnv->pConsole->AddCommand("i_giveitem", (ConsoleCommandFunc)GiveItemCmd, VF_CHEAT, "Gives specified item to the player!");
	gEnv->pConsole->AddCommand("i_giveallitems", (ConsoleCommandFunc)GiveAllItemsCmd, VF_CHEAT, "Gives all available items to the player!");
	gEnv->pConsole->AddCommand("i_givedebugitems", (ConsoleCommandFunc)GiveDebugItemsCmd, VF_CHEAT, "Gives special debug items to the player!");

	m_pPrecache = gEnv->pConsole->RegisterInt("i_precache", 1, VF_DUMPTODISK, "Enables precaching of items during level loading.");
	m_pItemLimit = gEnv->pConsole->RegisterInt("i_lying_item_limit", 64, 0, "Max number of items lying around in a level. Only works in multiplayer.");
}

//------------------------------------------------------------------------
void CItemSystem::UnregisterCVars()
{
	gEnv->pConsole->RemoveCommand("i_giveitem");
	gEnv->pConsole->RemoveCommand("i_giveallitems");
	gEnv->pConsole->RemoveCommand("i_givedebugitems");
	gEnv->pConsole->UnregisterVariable("i_noweaponlimit", true);
	gEnv->pConsole->UnregisterVariable("i_precache", true);
}

//------------------------------------------------------------------------
void CItemSystem::GiveItemCmd(IConsoleCmdArgs *args)
{
	if (args->GetArgCount()<2)
		return;

	IGameFramework *pGameFramework = gEnv->pGame->GetIGameFramework();
	IActorSystem	*pActorSystem = pGameFramework->GetIActorSystem();
	IItemSystem		*pItemSystem = pGameFramework->GetIItemSystem();

	const char *itemName = args->GetArg(1);
	const char *actorName = 0;

	if (args->GetArgCount()>2)
		actorName = args->GetArg(2);

	IActor *pActor = 0;

	if (actorName)
	{
		IEntity *pEntity = gEnv->pEntitySystem->FindEntityByName(actorName);
		if (pEntity)
		{
			pActor = pGameFramework->GetIActorSystem()->GetActor(pEntity->GetId());
		}
	}
	else
		pActor = pGameFramework->GetClientActor();

	if (!pActor)
		return;

	pItemSystem->GiveItem(pActor, itemName, true, true, true);
}

void CItemSystem::GiveItemsHelper(IConsoleCmdArgs *args, bool useGiveable, bool useDebug)
{
	if (args->GetArgCount()<1)
		return;

	IGameFramework *pGameFramework = gEnv->pGame->GetIGameFramework();
	IActorSystem	*pActorSystem = pGameFramework->GetIActorSystem();
	CItemSystem		*pItemSystem = static_cast<CItemSystem *>(pGameFramework->GetIItemSystem());

	const char *actorName = 0;

	if (args->GetArgCount()>1)
		actorName = args->GetArg(1);

	IActor *pActor = 0;

	if (actorName)
	{
		IEntity *pEntity = gEnv->pEntitySystem->FindEntityByName(actorName);
		if (pEntity)
		{
			pActor = pGameFramework->GetIActorSystem()->GetActor(pEntity->GetId());
		}
	}
	else
		pActor = pGameFramework->GetClientActor();

	if (!pActor)
		return;


	for (TItemParamsMap::iterator it = pItemSystem->m_params.begin(); it != pItemSystem->m_params.end(); it++)
	{
		const char* item = it->first.c_str();
		CItemParamsNode *params = it->second.params;
		if (!params)
			continue;
		
		const IItemParamsNode *itemParams = params->GetChild("params");
		
		bool give = false;
		bool debug = false;
		if (itemParams)
		{
			int n = itemParams->GetChildCount();
			for (int i=0; i<n; i++)
			{
				const IItemParamsNode *param = itemParams->GetChild(i);
				const char *name = param->GetAttribute("name");
				if (!stricmp(name?name:"", "giveable") && useGiveable)
				{
					int val = 0; param->GetAttribute("value", val);
					give = val != 0;
				}
				if (!stricmp(name?name:"", "debug") && useDebug)
				{
					int val = 0; param->GetAttribute("value", val);
					debug = val != 0;
				}
			}
		}

		if (give || debug)
			pItemSystem->GiveItem(pActor, it->first.c_str(), false);
	}
}


//------------------------------------------------------------------------
void CItemSystem::GiveAllItemsCmd(IConsoleCmdArgs *args)
{
	GiveItemsHelper(args, true, false);
}

//------------------------------------------------------------------------
void CItemSystem::GiveDebugItemsCmd(IConsoleCmdArgs *args)
{
	GiveItemsHelper(args, false, true);
}

//------------------------------------------------------------------------
void CItemSystem::RegisterListener(IItemSystemListener *pListener)
{
	stl::push_back_unique(m_listeners, pListener);
}

//------------------------------------------------------------------------
void CItemSystem::UnregisterListener(IItemSystemListener *pListener)
{
	stl::find_and_erase(m_listeners, pListener);
}

void CItemSystem::Serialize(TSerialize ser)
{
	if(ser.GetSerializationTarget() != eST_Network)
	{
		if(ser.IsReading())
			m_playerLevelToLevelSave = 0;

		bool hasInventoryNode = m_playerLevelToLevelSave?true:false;
		ser.Value("hasInventoryNode", hasInventoryNode);

		if(hasInventoryNode)
		{
			if(ser.IsWriting())
			{
				IXmlStringData* xmlData = m_playerLevelToLevelSave->getXMLData(50000);
				string data( xmlData->GetString() );
				ser.Value("LTLInventoryData", data);
				xmlData->Release();
			}
			else
			{
				string data;
				ser.Value("LTLInventoryData", data);

				m_playerLevelToLevelSave = gEnv->pSystem->LoadXmlFromString( data.c_str() );
			}
		}
	}
}

void CItemSystem::SerializePlayerLTLInfo(bool bReading)
{
	if(bReading && !m_playerLevelToLevelSave)
		return;

	if(gEnv->bMultiplayer)
		return;

	if(!bReading)
		m_playerLevelToLevelSave = gEnv->pSystem->CreateXmlNode( "Inventory" );

	IXmlSerializer *pSerializer = gEnv->pSystem->GetXmlUtils()->CreateXmlSerializer();
	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
	ISerialize *pSer = NULL; 
	if(!bReading)
		pSer = pSerializer->GetWriter(m_playerLevelToLevelSave);
	else
	{
		pSer = pSerializer->GetReader(m_playerLevelToLevelSave);
		m_pEquipmentManager->OnBeginGiveEquipmentPack();
	}
	TSerialize ser = TSerialize(pSer);
	if(pActor)
		pActor->SerializeLevelToLevel(ser);
	pSerializer->Release();

	if (bReading)
		m_pEquipmentManager->OnEndGiveEquipmentPack();
}

//------------------------------------------------------------------------
int CItemSystem::GetItemParamsCount() const
{
	return m_params.size();
}

//------------------------------------------------------------------------
const char* CItemSystem::GetItemParamName(int index) const
{
	// FIXME: maybe return an iterator class, so get rid of advance (it's a map, argh)
	assert (index >= 0 && index < m_params.size());
	TItemParamsMap::const_iterator iter = m_params.begin();
	std::advance(iter, index);
	return iter->first.c_str();
}

template <class T>
static int AddFactoryMapTo( const std::map<string, T>& m )
{
	int nSize = 0;
	if (!m.empty())
	{
		int nStringSize = 0;
		for (typename std::map<string, T>::const_iterator iter = m.begin(); iter != m.end(); ++iter)
		{
			nStringSize += iter->first.length();
		}
		nSize = m.size()*(sizeof(typename std::map<string, T>::value_type));
	}
	return nSize;
}
void CItemSystem::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_COMPONENT_NAME(s, "ItemSystem");

	int nSize = sizeof(*this);

	nSize += AddFactoryMapTo(m_objectCache);
	nSize += AddFactoryMapTo(m_characterCache);
	nSize += AddFactoryMapTo(m_classes);
	nSize += AddFactoryMapTo(m_params);

	s->AddContainer(m_items);
	s->AddContainer(m_listeners);
	s->AddContainer(m_folders);
	m_pEquipmentManager->GetMemoryStatistics(s);

	for (TItemParamsMap::iterator iter = m_params.begin(); iter != m_params.end(); ++iter)
	{
		nSize += iter->second.category.size();
		if (iter->second.params)
			nSize += iter->second.params->GetMemorySize();

		nSize += AddFactoryMapTo(iter->second.configurations);
	}
	nSize += m_grenadeType.size();

	s->AddObject(this,nSize);
}
