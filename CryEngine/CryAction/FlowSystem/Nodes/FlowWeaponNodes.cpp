////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowWeaponNodes.cpp
//  Version:     v1.00
//  Created:     July 7th 2005 by Julien.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowBaseNode.h"
#include "CryAction.h"
#include "IItemSystem.h"
#include <IEntitySystem.h>
#include "IWeapon.h"
#include "IActorSystem.h"


namespace
{
	IItem* GetItem(EntityId entityId)
	{
		IItemSystem* pItemSystem = CCryAction::GetCryAction()->GetIItemSystem();    
		return pItemSystem->GetItem( entityId );
	}

	IWeapon* GetWeapon(EntityId entityId)
	{
		IItem* pItem = GetItem(entityId);    
		return pItem ? pItem->GetIWeapon() : NULL;
	} 

	struct SWeaponListener : public IWeaponEventListener
	{
		// IWeaponEventListener
		virtual void OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel){}
		virtual void OnStartFire(IWeapon *pWeapon, EntityId shooterId){}
		virtual void OnStopFire(IWeapon *pWeapon, EntityId shooterId){}
		virtual void OnStartReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType){}
		virtual void OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType){}
		virtual void OnOutOfAmmo(IWeapon *pWeapon, IEntityClass* pAmmoType){}
		virtual void OnReadyToFire(IWeapon *pWeapon){}   
		virtual void OnPickedUp(IWeapon *pWeapon, EntityId actorId, bool destroyed){}
		virtual void OnDropped(IWeapon *pWeapon, EntityId actorId){}
		virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId){}
		virtual void OnStartTargetting(IWeapon *pWeapon){}
		virtual void OnStopTargetting(IWeapon *pWeapon){}
		virtual void OnSelected(IWeapon *pWeapon, bool select) {}
		// ~IWeaponEventListener

		virtual void AddListener(EntityId weaponId, IWeaponEventListener* pListener)
		{
			if (!(weaponId && pListener))
				return;

			if (IWeapon* pWeapon = GetWeapon(weaponId))      
				pWeapon->AddEventListener(pListener, __FUNCTION__);
		}

		virtual void RemoveListener(EntityId weaponId, IWeaponEventListener* pListener)
		{
			if (!(weaponId && pListener))
				return;

			if (IWeapon* pWeapon = GetWeapon(weaponId))      
				pWeapon->RemoveEventListener(pListener);
		}
	};
}

class CFlowNode_AutoSightWeapon : public CFlowBaseNode
{
public:
	CFlowNode_AutoSightWeapon( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig<Vec3>( "enemy", _HELP("Connect the enemy position vector to shoot here" )),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		/*if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
		IScriptSystem * pSS = gEnv->pScriptSystem;
		SmartScriptTable table;
		pSS->GetGlobalValue("Weapon", table);
		Script::CallMethod( table, "StartFire");
		}
		else */
		if (event == eFE_Initialize)
		{
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
		}
		else if (event == eFE_Update)
		{
			IEntity * pEntity = pActInfo->pEntity;
			if (pEntity)
			{
				Vec3 dir = GetPortVec3(pActInfo, 0) - pEntity->GetWorldTM().GetTranslation();

				dir.normalize();

				Vec3 up(0,0,1);
				Vec3 right(dir^up);

				if (right.len2() < 0.01f)
				{
					right = dir.GetOrthogonal();
				}

				Matrix34 tm(pEntity->GetWorldTM());

				tm.SetColumn(1, dir);
				tm.SetColumn(0, right.normalize());
				tm.SetColumn(2, right^dir);

				pEntity->SetWorldTM(tm);
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

private:	
};


class CFlowNode_WeaponListener : public CFlowBaseNode, public SWeaponListener
{
public:

	enum EInputs
	{ 
		IN_ENABLE = 0,
		IN_DISABLE,
		IN_WEAPONID,   
		IN_WEAPONCLASS,    
		IN_AMMO,    
	};

	enum EOutputs
	{
		OUT_ONSHOOT = 0,
		OUT_AMMOLEFT,
		OUT_ONMELEE,
		OUT_ONDROPPED,
	};

	CFlowNode_WeaponListener( SActivationInfo * pActInfo )
	{
		m_active = false;
		m_ammo = 0;    
		m_weapId = 0;
	}

	~CFlowNode_WeaponListener()
	{
		RemoveListener(m_weapId, this);    
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		IFlowNode* pNode = new CFlowNode_WeaponListener(pActInfo);
		return pNode;
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("active", m_active);
		ser.Value("ammo", m_ammo);
		ser.Value("weaponId", m_weapId);

		if (ser.IsReading())
		{
			if (m_active && m_weapId != 0)
			{
				IItemSystem* pItemSys = CCryAction::GetCryAction()->GetIItemSystem();

				IItem* pItem = pItemSys->GetItem(m_weapId);

				if (!pItem || !pItem->GetIWeapon())
				{
					GameWarning("[flow] CFlowNode_WeaponListener: Serialize no item/weapon.");
					return;
				}
				IWeapon* pWeapon = pItem->GetIWeapon();
				// set weapon listener
				pWeapon->AddEventListener(this, "CFlowNode_WeaponListener");
				// CryLog("[flow] CFlowNode_WeaponListener::Serialize() successfully created on '%s'", pItem->GetEntity()->GetName());
			}
			else
			{
				Reset();
			}
		}
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig inputs[] = 
		{ 
			InputPortConfig_Void( "Enable",  _HELP("Enable Listener")),
			InputPortConfig_Void( "Disable", _HELP("Disable Listener")),
			InputPortConfig<EntityId>( "WeaponId", _HELP("Listener will be set on this weapon if active")),
			InputPortConfig<string>( "WeaponClass", _HELP("Use this if you want to specify the players weapon by name"), 0, _UICONFIG("enum_global:weapon")),
			InputPortConfig<int> ( "Ammo", _HELP("Number of times the listener can be triggered. 0 means infinite"), _HELP("ShootCount") ),
			{0}
		};
		static const SOutputPortConfig outputs[] = 
		{
			OutputPortConfig_Void("OnShoot", _HELP("OnShoot")),
			OutputPortConfig<int>("AmmoLeft", _HELP("Shoots left"), _HELP("ShootsLeft")),
			OutputPortConfig_Void("OnMelee", _HELP("Triggered on melee attack")),
			OutputPortConfig<string>("OnDropped", _HELP("Triggered when weapon was dropped.")),
			{0}
		};  
		config.sDescription = "Listens on [WeaponId] (or players [WeaponClass], or as fallback current players weapon) and triggers OnShoot when shot.";
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_APPROVED);
	}


	void Reset()
	{
		if (m_weapId != 0)
		{
			RemoveListener(m_weapId, this);
			m_weapId = 0;
		}
		m_active = false;
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{ 
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				Reset();
			}
			break;

		case eFE_Activate:
			{ 
				IItemSystem* pItemSys = CCryAction::GetCryAction()->GetIItemSystem();

				// create listener
				if (IsPortActive(pActInfo, IN_DISABLE))
				{ 
					Reset();
				}
				if (IsPortActive(pActInfo, IN_ENABLE))
				{ 
					Reset();
					IItem* pItem = 0;

					EntityId weaponId = GetPortEntityId(pActInfo, IN_WEAPONID);                    
					if (weaponId != 0)
					{
						pItem = pItemSys->GetItem(weaponId);
					}
					else
					{            
						IActor* pActor = CCryAction::GetCryAction()->GetClientActor();
						if (!pActor) 
							return;

						IInventory *pInventory = pActor->GetInventory();
						if (!pInventory)
							return;

						const string& weaponClass = GetPortString(pActInfo, IN_WEAPONCLASS);
						if (!weaponClass.empty())
						{
							// get actor weapon by class
							IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(weaponClass);
							pItem = pItemSys->GetItem( pInventory->GetItemByClass(pClass) );
						}
						else
						{
							// get current actor weapon
							pItem = pItemSys->GetItem(pInventory->GetCurrentItem());
						}
					}          

					if (!pItem || !pItem->GetIWeapon())
					{
						GameWarning("[flow] CFlowNode_WeaponListener: no item/weapon.");
						return;
					}

					m_weapId = pItem->GetEntity()->GetId();
					IWeapon* pWeapon = pItem->GetIWeapon();

					// set initial ammo
					m_ammo = GetPortInt(pActInfo, IN_AMMO);
					if (m_ammo == 0)
						m_ammo = -1; // 0 input means infinite

					// set weapon listener                            
					pWeapon->AddEventListener(this, "CFlowNode_WeaponListener");

					m_active = true;

					//CryLog("WeaponListener successfully created on %s", pItem->GetEntity()->GetName());
				}           
				break;
			}
		}
	}

	virtual void OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel)
	{
		if (m_active)
		{
			if (m_ammo != 0)
			{
				ActivateOutput(&m_actInfo, OUT_ONSHOOT, true);
				if (m_ammo != -1)
					--m_ammo;
			}
			ActivateOutput(&m_actInfo, OUT_AMMOLEFT, m_ammo);
		}
	}

	virtual void OnDropped(IWeapon *pWeapon, EntityId actorId)
	{
		if (m_active)
			ActivateOutput(&m_actInfo, OUT_ONDROPPED, true);
	}

	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId)
	{
		if (m_active)
			ActivateOutput(&m_actInfo, OUT_ONMELEE, true);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

private:  
	SActivationInfo m_actInfo;
	EntityId m_weapId;  	
	int m_ammo;  
	bool m_active;
};

class CFlowNode_WeaponAmmo : public CFlowBaseNode
{
public:
	CFlowNode_WeaponAmmo( SActivationInfo * pActInfo )
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		IFlowNode* pNode = new CFlowNode_WeaponAmmo(pActInfo);
		return pNode;
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig inputs[] = 
		{ 
			InputPortConfig_Void   ( "Trigger", _HELP("Trigger this port to give/take ammo and clips")),
			InputPortConfig<string>( "Weapon", _HELP("Weapon name"), 0, _UICONFIG("enum_global:weapon") ),
			InputPortConfig<string>( "AmmoType", _HELP("Ammo name"), 0, _UICONFIG("enum_global_ref:ammo_%s:weapon") ),
			InputPortConfig<int>   ( "AmmoCount", _HELP("Ammo amount to give/take")),
			{0}
		};
		static const SOutputPortConfig outputs[] = 
		{
			{0}
		};  
		config.sDescription = "Give or Take ammo to/from local player. Weapon and AmmoType must match [e.g. \"SOCOM\" and \"bullet\"]";
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{ 
		switch (event)
		{
		case (eFE_Activate):
			{
				if (!IsPortActive(pActInfo, 0))
					return;

				IItemSystem* pItemSys = CCryAction::GetCryAction()->GetIItemSystem();

				// get actor
				IActor* pActor = CCryAction::GetCryAction()->GetClientActor();
				if (!pActor) 
					return;

				IInventory *pInventory = pActor->GetInventory();
				if (!pInventory)
					return;

				IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(GetPortString(pActInfo,1));
				IItem* pItem = pItemSys->GetItem(pInventory->GetItemByClass(pClass));
				if (!pItem || !pItem->GetIWeapon())
				{
					GameWarning("[flow] CFlowNode_WeaponAmmo: No item/weapon %s!", GetPortString(pActInfo,1).c_str());
					return;
				}
				IWeapon *pWeapon = pItem->GetIWeapon();
				const string& ammoType = GetPortString(pActInfo,2);
				IEntityClass* pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoType.c_str());
				assert(pAmmoClass);
				pWeapon->SetAmmoCount(pAmmoClass, max(0, pWeapon->GetAmmoCount(pAmmoClass) + GetPortInt(pActInfo,3)));
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


class CFlowNode_FireWeapon : public CFlowBaseNode, public SWeaponListener
{
public:
	enum EInputs
	{ 
		IN_AMMO = 0,
		IN_TARGETID,
		IN_TARGETPOS,   
		IN_ALIGNTOTARGET, 
		IN_AUTORELOAD,
		IN_STARTFIRE,
		IN_STOPFIRE    
	};

	CFlowNode_FireWeapon( SActivationInfo * pActInfo )
	{     
		m_weapId = 0;
		m_isFiring = false;
		m_needsReload = false;
		m_reloaded = false;

	}
	~CFlowNode_FireWeapon()
	{ 
		RemoveListener(m_weapId, this);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		// FIXME:!!! No proper serialization !!! Need to register after QL
		ser.Value("weaponId", m_weapId);
		ser.Value("firing", m_isFiring);
		ser.Value("needsReload", m_needsReload);
		ser.Value("reloaded", m_reloaded);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		IFlowNode* pNode = new CFlowNode_FireWeapon(pActInfo);
		return pNode;
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig inputs[] = 
		{ 
			InputPortConfig<int>(      "Ammo",          _HELP("Sets Weapon ammo")),
			InputPortConfig<EntityId>( "TargetId",      _HELP("Target entity")),
			InputPortConfig<Vec3>(     "TargetPos",     _HELP("Alternatively, Target position")),            
			InputPortConfig<bool>(     "AlignToTarget", _HELP("If true, weapon entity will be aligned to target direction. Using this can have side effects. Do not use on self-aligning weapons.")),
			InputPortConfig<bool>(     "AutoReload",    _HELP("Auto-reload")),
			InputPortConfig_Void (     "StartFire",     _HELP("Trigger this to start fire.")),
			InputPortConfig_Void (     "StopFire",      _HELP("Trigger this to stop fire.")),      
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<bool>("FireStarted", _HELP("True if StartFire command was successful")),
			{0}
		};  

		config.sDescription = _HELP("Fires a weapon and sets a target entity or a target position.");
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs; 
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);    
	}

	IWeapon* GetWeapon(SActivationInfo* pActInfo)
	{ 
		if (pActInfo->pEntity)
			return ::GetWeapon(pActInfo->pEntity->GetId());

		return NULL;
	}

	EntityId GetUserId(SActivationInfo *pActInfo)
	{
		if (pActInfo->pEntity)
			return pActInfo->pEntity->GetId();
		else
			return 0;
	}

	Vec3 GetTargetPos(SActivationInfo* pActInfo)
	{
		EntityId targetId = GetPortEntityId(pActInfo, IN_TARGETID);

		if (targetId)
		{
			IEntity* pTarget = gEnv->pEntitySystem->GetEntity(targetId);
			if (pTarget)
			{
				AABB box;
				pTarget->GetWorldBounds(box);
				return box.GetCenter();
			}        
		}
		return GetPortVec3(pActInfo, IN_TARGETPOS);
	}

	void SetDestination(IWeapon* pWeapon, SActivationInfo* pActInfo)
	{
		EntityId targetId = GetPortEntityId(pActInfo, IN_TARGETID);

		if (targetId)
		{
			pWeapon->SetDestinationEntity(targetId);
		}
		else
		{      
			pWeapon->SetDestination( GetPortVec3(pActInfo, IN_TARGETPOS) );
		}
	}

	void UpdateWeaponTM(SActivationInfo* pActInfo)
	{
		// update entity rotation           
		IEntity* pEntity = pActInfo->pEntity;
    Vec3 dir = GetTargetPos(pActInfo) - pEntity->GetWorldPos();
    dir.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);

    if (IEntity* pParent = pEntity->GetParent())
    {
      dir = pParent->GetWorldRotation().GetInverted() * dir;      
    }

    Matrix33 rotation = Matrix33::CreateRotationVDir(dir);
    pEntity->SetRotation(Quat(rotation));    
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{     
		IWeapon* pWeapon = GetWeapon(pActInfo);

		if (!pWeapon) 
			return;

		if (event == eFE_Activate)
		{ 
			// start/stop fire
			if (IsPortActive(pActInfo, IN_STOPFIRE))
			{
				pWeapon->StopFire();
				m_isFiring = false;
				pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
			}
			if (IsPortActive(pActInfo, IN_STARTFIRE))
			{
				if (GetPortBool(pActInfo, IN_ALIGNTOTARGET))        
					UpdateWeaponTM(pActInfo);                  

				SetDestination(pWeapon, pActInfo);

				pWeapon->StartFire();
				m_isFiring = true;
				pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );

				ActivateOutput(pActInfo, 0, true);        
			}        
		}     
		else if (event == eFE_Initialize)
		{
			m_isFiring = false;    
			m_reloaded = false;
			m_needsReload = false;
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );

			pWeapon->StopFire();

			// set ammo
			int ammo = GetPortInt(pActInfo, IN_AMMO);
			if (ammo > 0)
			{        
				IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

				if (pFireMode)
					pWeapon->SetAmmoCount(pFireMode->GetAmmoType(), ammo);
			}  

			if (pActInfo->pEntity->GetId() != m_weapId)
				RemoveListener(m_weapId, this);

			m_weapId = pActInfo->pEntity->GetId();
			pWeapon->AddEventListener(this, __FUNCTION__);
		}
		else if (event == eFE_Update)
		{ 
			if (m_isFiring) 
			{ 
				// update weapon destination
				SetDestination(pWeapon, pActInfo);

				if (GetPortBool(pActInfo, IN_ALIGNTOTARGET))        
					UpdateWeaponTM(pActInfo);                  

				if (m_needsReload && GetPortBool(pActInfo, IN_AUTORELOAD))
				{
					pWeapon->Reload(false);
					m_needsReload = false;
				}
				else if (m_reloaded)
				{
					pWeapon->StartFire();
					m_reloaded = false;
				}
			}
		}
	}

	virtual void OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType)
	{
		m_reloaded = true;
	}

	virtual void OnOutOfAmmo(IWeapon *pWeapon, IEntityClass* pAmmoType)
	{ 
		m_needsReload = true;
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

private:        
	EntityId m_weapId;
	bool m_isFiring;  
	bool m_needsReload;
	bool m_reloaded;
};

REGISTER_FLOW_NODE("Weapon:AutoSightWeapon",CFlowNode_AutoSightWeapon);
REGISTER_FLOW_NODE("Weapon:FireWeapon",CFlowNode_FireWeapon);
REGISTER_FLOW_NODE("Weapon:WeaponListener",CFlowNode_WeaponListener);
REGISTER_FLOW_NODE("Weapon:ChangeAmmo",CFlowNode_WeaponAmmo);
