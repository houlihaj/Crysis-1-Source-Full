/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a seat action to control vehicle weapons

-------------------------------------------------------------------------
History:
- 06:12:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "GameObjects/GameObject.h"
#include "IGameObject.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehicleSeatActionWeapons.h"

#include "IActorSystem.h"
#include "IItemSystem.h"
#include "IItem.h"
#include "IWeapon.h"



//------------------------------------------------------------------------
CVehicleSeatActionWeapons::CVehicleSeatActionWeapons()
: m_passengerId(0),
	m_isUsingShootingByUpdate(false),
	m_isShootingToCrosshair(true),
	m_fireTarget(0,0,0)
{
}

//------------------------------------------------------------------------
CVehicleSeatActionWeapons::~CVehicleSeatActionWeapons()
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
    if (IWeapon* pWeapon = GetIWeapon(*ite))
    {
      pWeapon->SetFiringLocator(NULL);
      pWeapon->RemoveEventListener(this);
    }

    if (static_cast<CVehicle*>(m_pVehicle)->SpawnAndDeleteEntities())
      pEntitySystem->RemoveEntity(ite->weaponEntityId, true);

		(*ite).weaponEntityId = 0;
	}

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
}

//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::Init(IVehicle* pVehicle, TVehicleSeatId seatId, const SmartScriptTable &table)
{
	m_pVehicle = pVehicle;
	m_seatId = seatId;

	IEntity* pVehicleEntity = pVehicle->GetEntity();

	SmartScriptTable weaponsParamsTable;
	if (!table->GetValue("Weapons", weaponsParamsTable))
		return false;

	SmartScriptTable weaponsArrayTable;
	if (!weaponsParamsTable->GetValue("Weapons", weaponsArrayTable))
		return false;

	IScriptTable::Iterator weaponIte = weaponsArrayTable->BeginIteration();
	m_weapons.reserve(weaponsArrayTable->Count());

	int i = 0;

	while (weaponsArrayTable->MoveNext(weaponIte))
	{
		if (weaponIte.value.GetVarType() == svtObject)
		{
			SmartScriptTable weaponTable;
			weaponIte.value.CopyTo(weaponTable);

			char* pClassName = NULL;
			if (weaponTable->GetValue("class", pClassName) && pClassName[0])
			{
				IVehiclePart* pVehiclePart = NULL;

				char* pPartName = NULL;

				if (weaponTable->GetValue("part", pPartName))
					pVehiclePart = m_pVehicle->GetPart(pPartName);

        SVehicleWeapon vehicleWeapon;        
				IEntity* pWeaponEntity = 0;        
        SEntitySpawnParams spawnParams;
        
        spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pClassName);					
        if (!spawnParams.pClass)
        {
          CryLog("[CVehicleSeatActionWeapons]: EntityClass %s not found!", pClassName);
          return false;
        }
				
        bool isServer=gEnv->bServer;
				if (isServer && static_cast<CVehicle*>(m_pVehicle)->SpawnAndDeleteEntities())        
				{
					char pWeaponName[512];
					_snprintf(pWeaponName, 512, "%s_Weapon_%s_%s_%d", pVehicle->GetEntity()->GetName(), pClassName, pPartName, ++i);
          pWeaponName[sizeof(pWeaponName)-1] = '\0';

					spawnParams.sName = pWeaponName;					
          spawnParams.vPosition.zero();
					spawnParams.nFlags = ENTITY_FLAG_NEVER_NETWORK_STATIC;
          
					pWeaponEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams);

					if (pWeaponEntity)
					{
						CVehicle *pCVehicle=static_cast<CVehicle *>(pVehicle);
						assert(pCVehicle);

						// this is effectively creating a chain of weapons' parents
						// so that all weapons will be created on clients before the vehicle itself
						if (EntityId lastId=pCVehicle->GetLastCreatedWeaponId())
						{
							IGameObject *pGameObject=CCryAction::GetCryAction()->GetGameObject(pWeaponEntity->GetId());
							if (pGameObject)
								pGameObject->SetNetworkParent(lastId);
						}

						pCVehicle->SetLastCreatedWeaponId(pWeaponEntity->GetId());

						// note: we are setting the weapon as the network parent, in this case,
						// because the vehicle initialization needs the weapon to be present at this point.
						// see ClSetupWeapons and CVehicle::PostInit
						m_pVehicle->GetGameObject()->SetNetworkParent(pWeaponEntity->GetId());
					}
				}

				vehicleWeapon.weaponEntityId = pWeaponEntity?pWeaponEntity->GetId():0;				
				vehicleWeapon.pPart = pVehiclePart;                
        weaponTable->GetValue("inheritVelocity", vehicleWeapon.inheritVelocity);

        if (pWeaponEntity)        
          SetupWeapon(vehicleWeapon);        

				m_weapons.push_back(vehicleWeapon);
				THelperVector& helpers = m_weapons.back().helpers;

				SmartScriptTable helpersTable;
				if (weaponTable->GetValue("Helpers", helpersTable))
				{
					helpers.reserve(helpersTable->Count());

					IScriptTable::Iterator tableIte = helpersTable->BeginIteration();

					while (helpersTable->MoveNext(tableIte))
					{
						if (tableIte.value.type == ANY_TSTRING)
						{
							char* pHelperName = NULL;

							if (tableIte.value.CopyTo(pHelperName))
								if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(pHelperName))
									helpers.push_back(pHelper);
						}
					}

					helpersTable->EndIteration(tableIte);
				}
        
        SmartScriptTable actionsTable;
        if (weaponTable->GetValue("Actions", actionsTable))
        {
          TWeaponActionMap& actions = m_weapons.back().actions;          
          IScriptTable::Iterator it = actionsTable->BeginIteration();

          while (actionsTable->MoveNext(it))
          {            
            char* name = 0;            
            IScriptTable* actionTable = it.value.table;

            if (actionTable->GetValue("name", name) && name && *name)
            {
              SWeaponAction action;
              
              SmartScriptTable animsTable;
              if (actionTable->GetValue("Animations", animsTable))
              {
                for (int j=1; j<=animsTable->Count(); ++j)
                {
                  SmartScriptTable animTable;
                  if (animsTable->GetAt(j, animTable))
                  { 
                    char* animation = 0;
                    if (animTable->GetValue("name", animation) && animation && *animation)
                      action.animations.push_back(animation);
                  }
                }
              }              
                      
                            
              actions.insert(TWeaponActionMap::value_type(name, action));
            }            
          }
          actionsTable->EndIteration(it);
        }

        if (pWeaponEntity)
          UpdateWeaponTM(vehicleWeapon);
			}
		}
	}

	weaponsArrayTable->EndIteration(weaponIte);

	if (m_weapons.size() == 0)
		return false;
	else if (m_weapons.size() > 1)
		m_isUsingShootingByUpdate = true;

	m_weaponIndex = 0;
	m_isShooting = false;
	m_lastActionActivationMode = 0;

	if (!weaponsParamsTable->GetValue("isSecondary", m_isSecondary))
		m_isSecondary = false;

	if (weaponsParamsTable->GetValue("disablesShootToCrosshair", m_isShootingToCrosshair))
		m_isShootingToCrosshair = !m_isShootingToCrosshair;
	else
		m_isShootingToCrosshair = true;

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_PassengerUpdate);

	 return true;
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::SetupWeapon(SVehicleWeapon& weapon)
{
  assert(weapon.weaponEntityId);

  if (weapon.weaponEntityId == 0)
    return;
    
  IItem* pItem = GetIItem(weapon); 
  IVehiclePart* pVehiclePart = weapon.pPart;

  if (pItem)
  {
    EntityId mountId = pVehiclePart ? pVehiclePart->GetEntity()->GetId() : m_pVehicle->GetEntityId();

    static Vec3 offset(ZERO); 
    static Ang3 angles(ZERO);
    pItem->MountAtEntity(mountId, offset, angles);

    if (IWeapon* pWeapon = pItem->GetIWeapon())
      pWeapon->SetHostId(m_pVehicle->GetEntityId());
  }
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::Reset()
{
	m_weaponIndex = 0;
	m_isShooting = false;
	m_lastActionActivationMode = 0;
	m_fireTarget.Set(0,0,0);
  
  for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
  { 
    UpdateWeaponTM(*ite);
    
    IEntity* pEntity = GetEntity(*ite);
    
    if (pEntity && pEntity->IsHidden() && !m_pVehicle->GetEntity()->IsHidden())    
      pEntity->Hide(false);
  }
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::StartUsing(EntityId passengerId)
{
	if (!passengerId)
		return;

	if (m_passengerId && m_passengerId != passengerId)
  {
	  assert(!"error: another passenger is already linked to this weapon seat action");
  }

	m_passengerId = passengerId;  
  
  if (!gEnv->pSystem->IsSerializingFile())
    m_fireTarget.zero();

	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
    if (IItem* pItem = GetIItem(*ite))
    {	
      pItem->StartUse(passengerId);

      if (IWeapon* pWeapon = pItem->GetIWeapon())              
      {
        pWeapon->SetFiringLocator(this);      
        pWeapon->AddEventListener(this, __FUNCTION__);

        if (pWeapon->OutOfAmmo(false))
          pWeapon->Reload(false);
      }
    }
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::StopUsing()
{
	if (!m_passengerId)
		return;

	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		SVehicleWeapon& vehicleWeapon = *ite;
  
    if (IItem* pItem = GetIItem(vehicleWeapon))
    {
      if (IWeapon* pWeapon = pItem->GetIWeapon())
      {
        pWeapon->StopFire();
        pWeapon->RemoveEventListener(this);
      }

      pItem->StopUse(m_passengerId);	
    }
	}

  m_passengerId = 0;
  m_fireTarget.zero();
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	if ((actionId == eVAI_Attack1 && !m_isSecondary) || (actionId == eVAI_Attack2 && m_isSecondary))
	{
		if (activationMode == eAAM_OnPress)
			StartFire();
		else if (activationMode == eAAM_OnRelease)
			StopFire();
	}
  else if (actionId == eVAI_FireMode && !m_isSecondary && activationMode == eAAM_OnPress)
  {
    if (IWeapon* pWeapon = GetIWeapon(GetCurrentWeapon()))
      pWeapon->ChangeFireMode();
  }
  else if (actionId == eVAI_Attack2 && !m_isSecondary)
  { 
    if (IWeapon* pWeapon = GetIWeapon(GetCurrentWeapon()))
    {
      if (IFireMode* pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()))
      {
        if (activationMode == eAAM_OnPress && !pFireMode->IsReloading())
        { 
          if (pFireMode->AllowZoom() && pWeapon->CanZoom())          
            pWeapon->StartZoom(m_passengerId,1);          
          else
            pFireMode->Cancel();
        }        
      }      
    }    
  }
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::Update(float frameTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);
  
  bool distant = m_pVehicle->GetGameObject()->IsProbablyDistant();
  bool visible = m_pVehicle->GetGameObject()->IsProbablyVisible();
  bool updateTM = !gEnv->bClient || visible || !distant; 
    
	for (TVehicleWeaponVector::iterator it = m_weapons.begin(); it != m_weapons.end(); ++it)
	{ 
    if (updateTM && (m_passengerId || it->helpers.empty()))
    {
		  // update weapon positions
		  UpdateWeaponTM(*it);
    }

    if (m_passengerId && gEnv->bClient)    
    {
      if (IItem* pItem = GetIItem(*it))
        pItem->UpdateFPView(frameTime);
    }		
	}

  if (!m_passengerId)
    return;

	if (m_isUsingShootingByUpdate)
	{
		if (m_lastActionActivationMode == eAAM_OnPress || m_lastActionActivationMode == eAAM_OnRelease)
		{
			if (m_isShooting == false && m_lastActionActivationMode == eAAM_OnPress)
			{
				m_isShooting = true;
				m_lastActionActivationMode = 0;
			}
			else if (m_isShooting == true &&  m_lastActionActivationMode == eAAM_OnRelease)
			{
				m_isShooting = false;
				m_lastActionActivationMode = 0;
			}
		}

		if (m_isShooting)
		{
			SVehicleWeapon& vehicleWeapon = GetVehicleWeapon();

			if (IWeapon* pWeapon = GetIWeapon(vehicleWeapon))
			{
				pWeapon->StartFire();
				pWeapon->StopFire();
			}
		}
	}
}

//------------------------------------------------------------------------
SVehicleWeapon* CVehicleSeatActionWeapons::GetVehicleWeapon(EntityId weaponId)
{
  // todo: all these iterations should be removed
  for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
  {
    if (ite->weaponEntityId == weaponId)
    {
      return &(*ite);
    }
  }
  
  return NULL;
}

//------------------------------------------------------------------------
SVehicleWeapon& CVehicleSeatActionWeapons::GetVehicleWeapon()
{
	if (m_weapons.size() > 1)
		return m_weapons[m_weaponIndex];
	else
		return m_weapons[0];
}

//------------------------------------------------------------------------
SVehicleWeapon& CVehicleSeatActionWeapons::GetWeaponInfo(int weaponIndex)
{
	if (weaponIndex < m_weapons.size())
		return m_weapons[weaponIndex];
	else
		return m_weapons[0];
}

//------------------------------------------------------------------------
IItem* CVehicleSeatActionWeapons::GetIItem(const SVehicleWeapon& vehicleWeapon)
{	
  return CCryAction::GetCryAction()->GetIItemSystem()->GetItem(vehicleWeapon.weaponEntityId);
}

//------------------------------------------------------------------------
IWeapon* CVehicleSeatActionWeapons::GetIWeapon(const SVehicleWeapon& vehicleWeapon)
{ 
  IItem* pItem = GetIItem(vehicleWeapon);

  if (pItem)
    return pItem->GetIWeapon();

  return NULL;
}

//------------------------------------------------------------------------
IEntity* CVehicleSeatActionWeapons::GetEntity(const SVehicleWeapon& weapon)
{
  return gEnv->pEntitySystem->GetEntity(weapon.weaponEntityId);
}

//------------------------------------------------------------------------
IVehicleHelper* CVehicleSeatActionWeapons::GetHelper(SVehicleWeapon& vehicleWeapon)
{
	THelperVector& helpers = vehicleWeapon.helpers;
	
	if (!helpers.empty())
	{ 
    IWeapon* pWeapon = GetIWeapon(vehicleWeapon);    
    if (pWeapon)
    {
      IFireMode* pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
      if (pFireMode)
      {
        int index = pFireMode->GetCurrentBarrel();
        if (index < helpers.size())
          return helpers[index];
      }      
    }

    return helpers[0];
	}
	
	return NULL;
}


//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::Serialize(TSerialize ser, unsigned aspects)
{
  if (ser.GetSerializationTarget() != eST_Network)
  {
    char buffer[16];
    int n = m_weapons.size();

    for (int i=0; i<n; ++i)
    {  
      _snprintf(buffer, 16, "weapon_%d", i);
      buffer[sizeof(buffer)-1] = '\0';
      ser.Value(buffer, m_weapons[i].weaponEntityId, 'eid');            
    }
		ser.Value("m_fireTarget", m_fireTarget);      
  }
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::PostSerialize()
{ 
  for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
  { 
    if (IItem* pItem = GetIItem(*ite))
    {	
      if (IWeapon* pWeapon = pItem->GetIWeapon())      
      {
        pWeapon->SetFiringLocator(this);      
        pWeapon->AddEventListener(this, __FUNCTION__);        
        
        if (m_passengerId)
          pItem->StartUse(m_passengerId);

				IEntity *pEntity=GetEntity(*ite);
				if (pEntity)
				{
					bool shouldbe=m_pVehicle->GetEntity()->IsHidden();

					if (pEntity->IsHidden() != shouldbe)
					{
						pEntity->Hide(shouldbe);
					}
				}
      }
    }
  }       
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::StartFire()
{	
  if (!m_passengerId)
		return;

	if (!m_isUsingShootingByUpdate)
	{
		SVehicleWeapon& vehicleWeapon = GetWeaponInfo(0);
		IWeapon* pWeapon = GetIWeapon(vehicleWeapon);
    
    if (pWeapon)
    {
			THelperVector& helpers = vehicleWeapon.helpers;

		  UpdateWeaponTM(vehicleWeapon);

		  pWeapon->StartFire();
		  m_isShooting = true;
    }
    else
    {
      GameWarning("%s: Weapon not available (EntId %i)", m_pVehicle->GetEntity()->GetName(), vehicleWeapon.weaponEntityId);
    }
	}
	else
	{
		// the update function will do the remaining of the job
		m_lastActionActivationMode = eAAM_OnPress;
	}	
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::StopFire()
{
	if (!m_isUsingShootingByUpdate)
	{
		SVehicleWeapon& vehicleWeapon = GetWeaponInfo(0);
		IWeapon* pWeapon = GetIWeapon(vehicleWeapon);
    
    if (pWeapon)
		  pWeapon->StopFire();

		m_isShooting = false;
	}
	else
	{
		// the update function will do the remaining of the job
		m_lastActionActivationMode = eAAM_OnRelease;
	}	
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::OnOutOfAmmo(IWeapon *pWeapon, IEntityClass* pAmmoType)
{
  pWeapon->Reload(false);
}

//------------------------------------------------------------------------
unsigned int CVehicleSeatActionWeapons::GetWeaponCount() 
{ 
	return m_weapons.size(); 
}

//------------------------------------------------------------------------
EntityId CVehicleSeatActionWeapons::GetWeaponEntityId(unsigned int index)
{
	if (index >= 0 && index < m_weapons.size())
	{
		SVehicleWeapon& vehicleWeapon = m_weapons[index];
		return vehicleWeapon.weaponEntityId;
	}
	
	return 0;
}

//------------------------------------------------------------------------
IItem* CVehicleSeatActionWeapons::GetIItem(unsigned int index)
{
  if (index >= 0 && index < m_weapons.size())
  {
    return GetIItem(m_weapons[index]);
  }

  return 0;
}

//------------------------------------------------------------------------
IWeapon* CVehicleSeatActionWeapons::GetIWeapon(unsigned int index)
{
  if (index >= 0 && index < m_weapons.size())
  {
    return GetIWeapon(m_weapons[index]);    
  }

  return 0;
}


//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::SetWeaponEntityId(unsigned int index, EntityId weaponId)
{
	IItem* pItem = CCryAction::GetCryAction()->GetIItemSystem()->GetItem(weaponId); 
	if (!pItem)
		return;

	SVehicleWeapon &weaponInfo = GetWeaponInfo(index);
	weaponInfo.weaponEntityId = weaponId;

	IVehiclePart* pVehiclePart = weaponInfo.pPart;

	EntityId mountId = pVehiclePart ? pVehiclePart->GetEntity()->GetId() : m_pVehicle->GetEntityId();

	static Vec3 offset(ZERO); 
	static Ang3 angles(ZERO);
	pItem->MountAtEntity(mountId, offset, angles);

	if (IWeapon* pWeapon = pItem->GetIWeapon())
		pWeapon->SetHostId(m_pVehicle->GetEntityId());

  UpdateWeaponTM(weaponInfo);
}


//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::UpdateWeaponTM(SVehicleWeapon& weapon)
{
  FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

  if (IEntity* pEntityWeapon = GetEntity(weapon))
  {
    if (!weapon.helpers.empty())
    { 
      // weapon is always kept at first helper
      const Matrix34& worldTM = weapon.helpers[0]->GetWorldTM();
	    pEntityWeapon->SetWorldTM(worldTM);
      
    }
    else if (weapon.pPart)
    {
      const Matrix34& worldTM = weapon.pPart->GetWorldTM();
      pEntityWeapon->SetWorldTM(worldTM);      
    }
    
    if (VehicleCVars().v_draw_tm)    
      VehicleUtils::DrawTM(pEntityWeapon->GetWorldTM());
  }
}

//------------------------------------------------------------------------
int CVehicleSeatActionWeapons::GetSkipEntities(IPhysicalEntity** pSkipEnts, int nMaxSkip)
{
	int nSkip = 0;

	// skip vehicle and all child entities
	IEntity* pVehicleEntity = m_pVehicle->GetEntity();

	if (nSkip < nMaxSkip)
		pSkipEnts[nSkip++] = pVehicleEntity->GetPhysics();

	int count = pVehicleEntity->GetChildCount();      

	for (int c=0; c<count&&nSkip<nMaxSkip; ++c)
		if (IPhysicalEntity* pPhysics = pVehicleEntity->GetChild(c)->GetPhysics())
			pSkipEnts[nSkip++] = pPhysics;

	return nSkip;
}

//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hitPos)
{
	if (m_fireTarget.IsZero())
		return false;

	Vec3 pos;
	if (!GetFiringPos(weaponId, pFireMode, pos))
		return false;

	IPhysicalEntity* pSkipEntities[10];
	int nSkip = GetSkipEntities(pSkipEntities, 10);

	Vec3 dir = m_fireTarget - pos;
	static ray_hit hit;	
	if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir, ent_all,
		rwi_stop_at_pierceable|rwi_ignore_back_faces, &hit, 1, pSkipEntities, nSkip))
	{
		hitPos = hit.pt;
	}
	else
		hitPos = m_fireTarget;

	return true;
}


//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos)
{
  FUNCTION_PROFILER( gEnv->pSystem, PROFILE_GAME );
	
  for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		SVehicleWeapon& weapon = *ite;
		
		if (weapon.weaponEntityId == weaponId)
		{
      if (IEntity* pEntityWeapon = GetEntity(weapon))
      {
        if (!weapon.helpers.empty())
        { 
          if (IVehicleHelper* pHelper = GetHelper(weapon))
            pos = pHelper->GetWorldTM().GetTranslation();
          else
            pos = pEntityWeapon->GetWorldTM().GetTranslation();
        } 
        else 
        {        
          IWeapon* pWeapon = GetIWeapon(weapon);
          IFireMode* pFireMode = pWeapon ? pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()) : 0;

          if (pFireMode && pFireMode->HasFireHelper())
          {
            // weapon helper
            pos = pFireMode->GetFireHelperPos();
          } 
          else
          {
            // fallback, entity pos
            pos = pEntityWeapon->GetWorldTM().GetTranslation();
          }
        }

        return true;      
      }
      break;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{ 
  FUNCTION_PROFILER( gEnv->pSystem, PROFILE_GAME );

  IActor* pActor = GetUserActor();

  // for player, use direction to probable hit
  if (pActor && pActor->IsPlayer() && m_isShootingToCrosshair) 
  { 
    IVehicleSeat* pSeat = m_pVehicle->GetSeatById(m_seatId);
    IVehicleView* pView = pSeat ? pSeat->GetView(pSeat->GetCurrentView()) : 0;
    if (pView && pView->ShootToCrosshair())
    {
      if (!probableHit.IsZero())
      { 
        dir = (probableHit-firingPos).normalize(); 
        return true;
      }
		}

    {
      // fallback: fill in view direction, but return false to indicate 
      // it's not the precise firing direction 
      dir = pActor->GetViewRotation().GetColumn1();
      return false;
    }        
  }  

  // for AI, always return weapon/helper direction
	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		SVehicleWeapon& weapon = *ite;

		if (weapon.weaponEntityId == weaponId)
		{
		  if (IEntity* pWeaponEntity = GetEntity(weapon))
		  {
				dir = probableHit - firingPos;
				dir.NormalizeSafe();
				return true;
		  }
		  break;
		}
	}

	return false;

}
//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{ 
  FUNCTION_PROFILER( gEnv->pSystem, PROFILE_GAME );

  IActor* pActor = GetUserActor();

  // for player, use direction to probable hit
  if (pActor && pActor->IsPlayer() && m_isShootingToCrosshair) 
  { 
    IVehicleSeat* pSeat = m_pVehicle->GetSeatById(m_seatId);
    IVehicleView* pView = pSeat ? pSeat->GetView(pSeat->GetCurrentView()) : 0;
    if (pView && pView->ShootToCrosshair())
    {
      if (!probableHit.IsZero())
      { 
        dir = (probableHit-firingPos).normalize(); 
        return true;
      }
		}

    {
      // fallback: fill in view direction, but return false to indicate 
      // it's not the precise firing direction 
      dir = pActor->GetViewRotation().GetColumn1();
      return false;
    }        
  }  

  // for AI, always return weapon/helper direction
  // for AI, always return weapon/helper direction
	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		SVehicleWeapon& weapon = *ite;

		if (weapon.weaponEntityId == weaponId)
		{
			if (IEntity* pWeaponEntity = GetEntity(weapon))
			{
				if (!weapon.helpers.empty())
				{ 
					if (IVehicleHelper* pHelper = GetHelper(weapon))
						dir = pHelper->GetWorldTM().GetColumn1();
					else
						dir = pWeaponEntity->GetWorldTM().GetColumn1();
				}
				else 
				{           
					IWeapon* pWeapon = GetIWeapon(weapon);
					IFireMode* pFireMode = pWeapon ? pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()) : 0;
					// [MR] dont use helper direction until its fixed
					//if (pFireMode && pFireMode->HasFireHelper()) 
					if (0)
						dir = pFireMode->GetFireHelperDir();            
					else
						dir = pWeaponEntity->GetWorldTM().GetColumn1();
				} 
				return true;
			}
			break;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir)
{
  SVehicleWeapon* pVehicleWeapon = GetVehicleWeapon(weaponId);
  
  if (pVehicleWeapon && !pVehicleWeapon->inheritVelocity)
    return false;

  const SVehicleStatus& status = m_pVehicle->GetStatus();
  vel.zero();
  
  if (status.speed > 0.01f)
  {
    float dot = status.vel * firingDir;
    if (dot > 0.f)
      vel = status.vel.GetNormalized() * dot;              
  }
    
  return true;  
}


//------------------------------------------------------------------------
IActor* CVehicleSeatActionWeapons::GetUserActor()
{
  IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();

  return pActorSystem->GetActor(m_passengerId);
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
  if (event == eVE_Destroyed)
  {
		if (m_isShooting)
		{
			StopFire();
		}

    // set AI state    
    IAISystem* pAISystem = gEnv->pAISystem;

    for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
    {
      if (IEntity* pEntity = GetEntity(*ite))
      {
        if (pAISystem) pAISystem->SetSmartObjectState(pEntity, "Busy");
        pEntity->Hide(true);
      }
    }    
  }
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel)
{
  for (TVehicleWeaponVector::const_iterator it=m_weapons.begin(); it!=m_weapons.end(); ++it)
  {
    const SVehicleWeapon& vehicleWeapon = *it;
    if (GetIWeapon(vehicleWeapon) == pWeapon)
    {
      TWeaponActionMap::const_iterator itAction = vehicleWeapon.actions.find(CONST_TEMP_STRING("fire"));
      if (itAction != vehicleWeapon.actions.end())
      {
        const SWeaponAction& action = itAction->second;        
        IFireMode* pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
        
        if (pFireMode && pFireMode->GetCurrentBarrel() < action.animations.size())
        { 
          IVehicleAnimation* pAnim = m_pVehicle->GetAnimation(action.animations[pFireMode->GetCurrentBarrel()].c_str());
          if (pAnim)
          {
            pAnim->StartAnimation();
          }
        }
      }
      break;
    }
  }

	if (m_weapons.size() > 1)
	{
		m_weaponIndex++;
		if (m_weaponIndex >= m_weapons.size())
			m_weaponIndex  = 0;
	}	

	m_pVehicle->GetGameObject()->Pulse('bang');
}

//------------------------------------------------------------------------
Vec3 CVehicleSeatActionWeapons::GetAverageFiringPos()
{
  FUNCTION_PROFILER( gEnv->pSystem, PROFILE_GAME );
	
  AABB bounds;
	bounds.Reset();
	
	TVehicleWeaponVector::iterator weaponIte = m_weapons.begin();
	TVehicleWeaponVector::iterator weaponIteEnd = m_weapons.end();

	for (; weaponIte != weaponIteEnd; ++weaponIte)
	{
		SVehicleWeapon& weapon = *weaponIte;
		THelperVector& helpers = weapon.helpers;

		THelperVector::iterator helperIte = weapon.helpers.begin();
		THelperVector::iterator helperIteEnd = weapon.helpers.end();
		for (; helperIte != helperIteEnd; ++helperIte)
		{
			IVehicleHelper* pHelper = *helperIte;
			bounds.Add(pHelper->GetVehicleTM().GetTranslation());
		}
	}

	return bounds.GetCenter();
}

void CVehicleSeatActionWeapons::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_weapons);
	for (size_t i=0; i<m_weapons.size(); i++)
		m_weapons[i].GetMemoryStatistics(s);
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionWeapons)
