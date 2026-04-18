/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Vehicle System

-------------------------------------------------------------------------
History:
- 05:10:2004   11:55 : Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include <set>
#include <IScriptSystem.h>
#include <ICryPak.h>
#include "IGameObjectSystem.h"
#include "Serialization/XMLScriptLoader.h"

#include "CryAction.h"
#include "VehicleSystem.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehiclePartEntity.h"
#include "VehicleSystem/VehiclePartDetachedEntity.h"
#include "VehicleSystem/VehicleDamagesTemplateRegistry.h"
#include "VehicleSystem/VehicleCVars.h"


const char* gScriptPath = "Scripts/Entities/Vehicles/Implementations/";
const char* gXmlPath    = "Scripts/Entities/Vehicles/Implementations/Xml/";


//------------------------------------------------------------------------
CVehicleSystem::CVehicleSystem(ISystem *pSystem, IEntitySystem *pEntitySystem)
: m_pDamagesTemplateRegistry(NULL),
	m_pVehicleClient(NULL),
	m_pInitializingSeat(NULL)
{
  m_pDamagesTemplateRegistry = new CVehicleDamagesTemplateRegistry;
  m_pCVars = new CVehicleCVars;  
}

//------------------------------------------------------------------------
CVehicleSystem::~CVehicleSystem()
{
	std::for_each(m_iteratorPool.begin(), m_iteratorPool.end(), stl::container_object_deleter());

  SAFE_RELEASE(m_pDamagesTemplateRegistry);
  SAFE_DELETE(m_pCVars);
}

//------------------------------------------------------------------------
bool CVehicleSystem::Init()
{
	m_nextObjectId = InvalidVehicleObjectId + 1;
  
  LoadDamageTemplates();
  LoadDefaultLights();
  	
	return true;
}

//------------------------------------------------------------------------
void CVehicleSystem::LoadDamageTemplates()
{
  m_pDamagesTemplateRegistry->Init("Scripts/Entities/Vehicles/DamagesTemplates/def_vehicledamages.xml", 
    "Scripts/Entities/Vehicles/DamagesTemplates/");
}

//------------------------------------------------------------------------
void CVehicleSystem::LoadDefaultLights()
{
  SmartScriptTable lightDefaults = XmlScriptLoad("Scripts/Entities/Vehicles/Lights/def_lights.xml", "Scripts/Entities/Vehicles/Lights/DefaultVehicleLights.xml");
  if (lightDefaults.GetPtr())
    lightDefaults->GetValue("Lights", m_lightDefaults);
}

//------------------------------------------------------------------------
void CVehicleSystem::ReloadSystem()
{
  LoadDamageTemplates();
  LoadDefaultLights();
}

//------------------------------------------------------------------------
IVehicle* CVehicleSystem::CreateVehicle(uint16 channelId, const char *name, const char *vehicleClass, const Vec3 &pos, const Quat &rot, const Vec3 &scale, EntityId id)
{
	// get the entity class
	IEntityClass *pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(vehicleClass);

	if (!pEntityClass)
	{
		assert(pEntityClass);

		return 0;
	}

	// if a channel is specified and already has a vehicle,
	// use that entity id

	if (channelId)
	{
		IVehicle *pExistingProxy = GetVehicleByChannelId(channelId);

		if (pExistingProxy)
		{
			id = pExistingProxy->GetEntityId();
		}
	}

	SSpawnUserData userData( vehicleClass, channelId );
	SEntitySpawnParams params;
	params.id = id;
	params.pUserData = (void *)&userData;
	params.sName = name;
	params.vPosition = pos;
	params.qRotation = rot;
	params.vScale = scale;
	params.pClass = pEntityClass;

	IEntity *pEntity = gEnv->pEntitySystem->SpawnEntity(params, true);
	assert(pEntity);

	if (!pEntity)
	{
		return 0;
	}

	return GetVehicle(pEntity->GetId());
}

//------------------------------------------------------------------------
IVehicle* CVehicleSystem::GetVehicle(EntityId entityId)
{
	TVehicleMap::iterator it = m_vehicles.find(entityId);

	if (it != m_vehicles.end())
	{
		return it->second;
	}

	return 0;
}

//------------------------------------------------------------------------
IVehicle* CVehicleSystem::GetVehicleByChannelId(uint16 channelId)
{
	for (TVehicleMap::iterator it = m_vehicles.begin(); it != m_vehicles.end(); it++)
	{
		if (it->second->GetChannelId() == channelId)
		{
			return it->second;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
bool CVehicleSystem::IsVehicleClass(const char *name) const
{
	TVehicleClassMap::const_iterator it = m_classes.find( CONST_TEMP_STRING(name) );
	return (it != m_classes.end());
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleClass(const char *name, IGameFramework::IVehicleCreator * pCreator, bool isAI)
{
	IEntityClassRegistry::SEntityClassDesc vehicleClass;

	// Allow the name to contain relative path, but use only the name part as class name.
	string	className(PathUtil::GetFile(name));
	vehicleClass.sName = className.c_str();
	vehicleClass.sScriptFile = "Scripts/Entities/Vehicles/VehiclePool.lua";

	CCryAction::GetCryAction()->GetIGameObjectSystem()->RegisterExtension( name, pCreator, &vehicleClass );

	m_classes.insert( TVehicleClassMap::value_type(name, pCreator) );
}

//------------------------------------------------------------------------
void CVehicleSystem::AddVehicle(EntityId entityId, IVehicle *pProxy)
{
	m_vehicles.insert(TVehicleMap::value_type(entityId, pProxy));
}

//------------------------------------------------------------------------
void CVehicleSystem::RemoveVehicle(EntityId entityId)
{
	TVehicleMap::iterator it = m_vehicles.find(entityId);

	if (it != m_vehicles.end())
	{
		m_vehicles.erase(it);
	}
}
	

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleMovement(const char *name, IVehicleMovement *(*func)(), bool isAI)
{
	m_movementClasses.insert(TVehicleMovementClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleView(const char *name, IVehicleView *(*func)(), bool isAI)
{
	m_viewClasses.insert(TVehicleViewClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehiclePart(const char *name, IVehiclePart *(*func)(), bool isAI)
{
	m_partClasses.insert(TVehiclePartClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleDamageBehavior(const char *name, IVehicleDamageBehavior *(*func)(), bool isAI)
{
	m_damageBehaviorClasses.insert(TVehicleDamageBehaviorClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleSeatAction(const char* name, IVehicleSeatAction *(*func)(), bool isAI)
{
	m_seatActionClasses.insert(TVehicleSeatActionClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleAction(const char* name, IVehicleAction *(*func)(), bool isAI)
{
	m_actionClasses.insert(TVehicleActionClassMap::value_type(name, func));
}

//------------------------------------------------------------------------
IVehicleMovement* CVehicleSystem::CreateVehicleMovement(const string& name)
{
	TVehicleMovementClassMap::iterator ite = m_movementClasses.find(name);

	if (ite == m_movementClasses.end())
	{
		GameWarning("Unknown Vehicle movement: <%s>", name.c_str());
		return NULL;
	}

	return (*ite->second)();
}

//------------------------------------------------------------------------
IVehicleView* CVehicleSystem::CreateVehicleView(const string& name)
{
	TVehicleViewClassMap::iterator ite = m_viewClasses.find(name);

	if (ite == m_viewClasses.end())
	{
		GameWarning("Unknown Vehicle view: <%s>", name.c_str());
		return NULL;
	}

	return (*ite->second)();
}

//------------------------------------------------------------------------
IVehiclePart* CVehicleSystem::CreateVehiclePart(const string& name)
{
	TVehiclePartClassMap::iterator ite = m_partClasses.find(name);

	if (ite == m_partClasses.end())
	{
		GameWarning("Unknown Vehicle part: <%s>", name.c_str());
		return NULL;
	}

	return (*ite->second)();
}

//------------------------------------------------------------------------
IVehicleDamageBehavior* CVehicleSystem::CreateVehicleDamageBehavior(const string& name)
{
	TVehicleDamageBehaviorClassMap::iterator ite = m_damageBehaviorClasses.find(name);

	if (ite == m_damageBehaviorClasses.end())
	{
		GameWarning("Unknown Vehicle damage behavior: <%s>", name.c_str());
		return NULL;
	}

	return (*ite->second)();
}

//------------------------------------------------------------------------
IVehicleSeatAction* CVehicleSystem::CreateVehicleSeatAction(const string& name)
{
	TVehicleSeatActionClassMap::iterator ite = m_seatActionClasses.find(name);

	if (ite == m_seatActionClasses.end())
	{
		GameWarning("Unknown Vehicle seat action: <%s>", name.c_str());
		return NULL;
	}

	return (*ite->second)();
}

//------------------------------------------------------------------------
IVehicleAction* CVehicleSystem::CreateVehicleAction(const string& name)
{
	TVehicleActionClassMap::iterator ite = m_actionClasses.find(name);

	if (ite == m_actionClasses.end())
	{
		GameWarning("Unknown Vehicle action: <%s>", name.c_str());
		return NULL;
	}

	return (*ite->second)();
}

//------------------------------------------------------------------------
void CVehicleSystem::GetVehicleImplementations(SVehicleImpls& impls)
{  
	ICryPak *pack = gEnv->pCryPak;  
	_finddata_t fd;
	int res;
	intptr_t handle;  
	std::set<string> setVehicles;
  
	string mask(gXmlPath + string("*.xml"));

	if ((handle = pack->FindFirst( mask.c_str(), &fd )) != -1)
	{ 
		do
		{ 
			//PathUtil::RemoveExtension(vehicleName);      
			if (XmlNodeRef root = GetISystem()->LoadXmlFile( gXmlPath + string(fd.name)))
			{
				const char* name = root->getAttr("name");
				if (name[0])
				{ 
          bool show = true;
          root->getAttr("show", show);
          if (show || VehicleCVars().v_show_all)
          {
					  //CryLog("GetVehicleImpls: adding <%s>", name);          
					  std::pair<std::set<string>::iterator, bool> result = setVehicles.insert(string(name));
					  if (result.second)
						  impls.Add(name);
					  else
						  CryLog("Vehicle <%s> already registered (duplicate vehicle .xml files?)", name);
          }
				}
				else 
					CryLog("VehicleSystem: %s is missing 'name' attribute, skipping", fd.name );
			}
			res = pack->FindNext( handle,&fd );  
		} 
		while (res >= 0);
		pack->FindClose(handle);    
	}  
}

//------------------------------------------------------------------------
bool CVehicleSystem::GetOptionalScript(const char* vehicleName, char* buf, size_t len)
{	
	_finddata_t fd;
	intptr_t handle;    
	
  _snprintf(buf, len, "%s%s.lua", gScriptPath, vehicleName);
  buf[len-1] = 0;

	if ((handle = gEnv->pCryPak->FindFirst(buf, &fd)) != -1)
	{         
		gEnv->pCryPak->FindClose(handle);

    return true;
	}

  return false;
}

//------------------------------------------------------------------------
void CVehicleSystem::RegisterObject(IVehicleObject* pVehicleObject, const string& name)
{
	m_objectIds.insert(TVehicleObjectIdMap::value_type(name, pVehicleObject->GetId()));
}

//------------------------------------------------------------------------
TVehicleObjectId CVehicleSystem::GetObjectId(const string& className)
{
	return InvalidVehicleObjectId;
}

//------------------------------------------------------------------------
void CVehicleSystem::Reset()
{	
	if(GetISystem()->IsSerializingFile() == 1)
	{
		TVehicleMap::iterator it = m_vehicles.begin();
		IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
		for (; it != m_vehicles.end(); ++it)
		{
			EntityId id = it->first;
			IEntity *pEntity = pEntitySystem->GetEntity(id);
			if(!pEntity)
			{
				m_vehicles.erase(it);
				--it;
			}
		}
	}
}

//------------------------------------------------------------------------
IVehicleIteratorPtr CVehicleSystem::CreateVehicleIterator()
{
	if (m_iteratorPool.empty())
	{
		return new CVehicleIterator(this);
	}
	else
	{
		CVehicleIterator* pIter = m_iteratorPool.back();
		m_iteratorPool.pop_back();
		new (pIter) CVehicleIterator(this);
		return pIter;
	}
}



//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicleClient(IVehicleClient* pVehicleClient)
{
	m_pVehicleClient = pVehicleClient;
}

//------------------------------------------------------------------------
bool CVehicleSystem::GetVehicleLightDefaults(const char* type, SmartScriptTable& table)
{
  bool res = false;

  if (m_lightDefaults.GetPtr())    
  {
    IScriptTable::Iterator it = m_lightDefaults->BeginIteration();
    while (m_lightDefaults->MoveNext(it))
    {
      if (it.value.type == ANY_TTABLE)
      {
        const char* t = "";
        if (it.value.table->GetValue("type", t) && 0==strcmp(t, type))
        {
          table = it.value.table;
          res = true;
          break;
        }        
      }
    }
    m_lightDefaults->EndIteration(it);
  }

  return res;
}

template <class T>
static void AddFactoryMapTo( const std::map<string, T>& m, ICrySizer * s )
{
	s->AddContainer(m);
	for (typename std::map<string, T>::const_iterator iter = m.begin(); iter != m.end(); ++iter)
	{
		s->Add(iter->first);
	}
}

void CVehicleSystem::GetMemoryStatistics(ICrySizer * s)
{
	s->AddContainer(m_vehicles);
	AddFactoryMapTo(m_classes, s);
	AddFactoryMapTo(m_movementClasses, s);
	AddFactoryMapTo(m_viewClasses, s);
	AddFactoryMapTo(m_partClasses, s);
	AddFactoryMapTo(m_seatActionClasses, s);
	AddFactoryMapTo(m_actionClasses, s);
	AddFactoryMapTo(m_damageBehaviorClasses, s);
	AddFactoryMapTo(m_objectIds, s);
	s->AddContainer(m_iteratorPool);
	for (size_t i=0; i<m_iteratorPool.size(); i++)
		s->Add(*m_iteratorPool[i]);
	// TODO: VehicleDamagesTemplateRegistry
}
