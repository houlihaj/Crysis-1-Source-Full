////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowInventoryNodes.cpp
//  Version:     v1.00
//  Created:     July 11th 2005 by Julien.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowBaseNode.h"
#include "CryAction.h"
#include "IActorSystem.h"
#include "ItemSystem.h"
#include "GameObjects/GameObject.h"


class CFlowNode_InventoryAddItem : public CFlowBaseNode
{
public:
	CFlowNode_InventoryAddItem( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "add", _HELP("Connect event here to add the item" )),
			InputPortConfig<string>( "item", _HELP("The item to add to the inventory" ), _HELP("Item"), _UICONFIG("enum_global_ref:item%s:ItemType")),
			InputPortConfig<string>( "ItemType", "", _HELP("Select from which items to choose"), 0, _UICONFIG("enum_string:All=,Givable=_givable,Selectable=_selectable")),
			InputPortConfig<bool> ("Unique", true, _HELP("If true, only adds the item if the inventory doesn't already contain such an item")),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig<bool>("out", _HELP("true if the item was actually added, false if the player already had the item" )),
			{0}
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
			IActor * pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
			if (!pActor)
				return;

			IEntity * pEntity = pActor->GetEntity();
			assert(pEntity);
			if (!pEntity)
				return;
      
      IEntitySystem* pEntSys = gEnv->pEntitySystem;

      IInventory *pInventory = pActor->GetInventory();
      if (!pInventory)
        return;

      const string& itemClass = GetPortString(pActInfo, 1);
        
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(itemClass.c_str());
      EntityId id = pInventory->GetItemByClass(pClass);

      if (id == 0 || GetPortBool(pActInfo, 3) == false)
      {
        id = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GiveItem( pActor, itemClass.c_str(), false, true, true );
        ActivateOutput(pActInfo, 0, gEnv->pEntitySystem->GetEntity(id) != NULL);
      }
      else
      {
        // item already in inventory
        ActivateOutput(pActInfo, 0, false);
      }			
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNode_InventoryRemoveItem : public CFlowBaseNode
{
public:
	CFlowNode_InventoryRemoveItem( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Remove", _HELP("Connect event here to remove the item" )),
			InputPortConfig<string>( "item", _HELP("The item to add to the inventory" ), _HELP("Item"), _UICONFIG("enum_global_ref:item%s:ItemType")),
			InputPortConfig<string>( "ItemType", "", _HELP("Select from which items to choose"), 0, _UICONFIG("enum_string:All=,Givable=_givable,Selectable=_selectable")),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig<bool>("out", _HELP("true if the item was actually removed, false if the player did not have the item" )),
			{0}
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
			IGameFramework * pGameFramework = gEnv->pGame->GetIGameFramework();
			if (!pGameFramework)
				return;
			
      IActor * pActor = pGameFramework->GetClientActor();
			if (!pActor)
				return;
						
			IInventory *pInventory = pActor->GetInventory();
      if (!pInventory)
        return;

			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(GetPortString(pActInfo, 1));
      IItem* pItem = pGameFramework->GetIItemSystem()->GetItem(pInventory->GetItemByClass(pClass));
      if (pItem) 
      {
        pItem->Select(false);
        pGameFramework->GetIItemSystem()->SetActorItem(pActor, (EntityId)0, false);
      }

      bool res = pInventory->RemoveItem( pInventory->GetItemByClass(pClass) );
      ActivateOutput(pActInfo,0,res);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNode_InventoryRemoveAllItems : public CFlowBaseNode
{
public:
  CFlowNode_InventoryRemoveAllItems( SActivationInfo * pActInfo )
  {
  }

  void GetConfiguration( SFlowNodeConfig& config )
  {
    static const SInputPortConfig in_ports[] = 
    {
      InputPortConfig_Void( "Activate", _HELP("Connect event here to remove all items from inventory" )),        
      {0}
    };

    static const SOutputPortConfig out_ports[] = 
    {
      OutputPortConfig<bool>("Done", _HELP("True when done successfully")),
      {0}
    };

    config.pInputPorts = in_ports;   
    config.pOutputPorts = out_ports;   
		config.SetCategory(EFLN_APPROVED);
  }

  void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
  {
    if (event == eFE_Activate && IsPortActive(pActInfo,0))
    {
      IGameFramework* pGF = gEnv->pGame->GetIGameFramework();
      
      IActor * pActor = pGF->GetClientActor();
      if (!pActor)
        return;

			IInventory *pInventory = pActor->GetInventory();
      if (!pInventory)
        return;

      IItem* pItem = pGF->GetIItemSystem()->GetItem( pInventory->GetCurrentItem() );
      if (pItem) 
      {
        pItem->Select(false);
        pGF->GetIItemSystem()->SetActorItem(pActor, (EntityId)0, false);
      }
      
      //pInventory->RemoveAllItems();    
			pInventory->Destroy();
      CryLog("[CFlowNode_InventoryRemoveAllItems]: executed");

      ActivateOutput(pActInfo, 0, true);
    }
  }

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNode_InventoryHasItem : public CFlowBaseNode
{
public:
	CFlowNode_InventoryHasItem( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "check", _HELP("Connect event here to check the inventory for the item" ), _HELP("Check") ),
			InputPortConfig<string>( "item", _HELP("The item to add to the inventory" ), _HELP("Item"), _UICONFIG("enum_global_ref:item%s:ItemType")),
			InputPortConfig<string>( "ItemType", "", _HELP("Select from which items to choose"), 0, _UICONFIG("enum_string:All=,Givable=_givable,Selectable=_selectable")),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig<bool>("out", _HELP("True if the player has the item, false otherwise" ), _HELP("Out") ),
			OutputPortConfig_Void("False", _HELP("Triggered if player does not have the item" )),
			OutputPortConfig_Void("True",  _HELP("Triggered if player has the item" )),
			OutputPortConfig<EntityId>("ItemId", _HELP("Outputs the item's entity id" )),
			{0}
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
      IActor * pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
      if (!pActor)
        return;
      
			IInventory *pInventory = pActor->GetInventory();
      if (!pInventory)
        return;

			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(GetPortString(pActInfo,1));
      EntityId id = pInventory->GetItemByClass(pClass);
			
			ActivateOutput(pActInfo, 0, id !=0 ? true : false);
			ActivateOutput(pActInfo, id != 0 ? 2 : 1, true);
			ActivateOutput(pActInfo, 3, id);
			
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNode_InventoryHasAmmo : public CFlowBaseNode
{
public:
	CFlowNode_InventoryHasAmmo( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "check", _HELP("Connect event here to check the inventory for the ammo" ), _HELP("Check") ),
			InputPortConfig<string>( "AmmoType", _HELP("Select from which ammo to choose")),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig<bool>("out", _HELP("True if the player has the ammo, false otherwise" ), _HELP("Out") ),
			OutputPortConfig_Void("False", _HELP("Triggered if player does not have the ammo" )),
			OutputPortConfig_Void("True",  _HELP("Triggered if player has the ammo" )),
			OutputPortConfig<int>("Ammo count", _HELP("Outputs the current ammo count" )),
			{0}
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
			IActor * pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
			if (!pActor)
				return;

			IInventory *pInventory = pActor->GetInventory();
			if (!pInventory)
				return;

			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(GetPortString(pActInfo,1));
			int count = 0;
			if(pClass)
			 count = pInventory->GetAmmoCount(pClass);

			ActivateOutput(pActInfo, 0, count !=0 ? true : false);
			ActivateOutput(pActInfo, count != 0 ? 2 : 1, true);
			ActivateOutput(pActInfo, 3, count);

		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNode_InventorySelectItem : public CFlowBaseNode
{
public:
  CFlowNode_InventorySelectItem( SActivationInfo * pActInfo )
  {
  }

  void GetConfiguration( SFlowNodeConfig& config )
  {
    static const SInputPortConfig in_ports[] = 
    {
      InputPortConfig_Void( "Activate", _HELP("Select the item" )),
			InputPortConfig<string>( "Item", _HELP("Item to select, has to be in inventory" ), 0, _UICONFIG("enum_global:item_selectable")),
      {0}
    };

    static const SOutputPortConfig out_ports[] = 
    {
      OutputPortConfig<bool>("Done", _HELP("True when executed" )),
      {0}
    };

    config.pInputPorts = in_ports;
    config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
  }

  void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
  {
    if (event == eFE_Activate && IsPortActive(pActInfo,0))
    {
      IGameFramework* pGF = gEnv->pGame->GetIGameFramework();

      IActor * pActor = pGF->GetClientActor();
      if (!pActor)
        return;

			IInventory *pInventory = pActor->GetInventory();
      if (!pInventory)
        return;

      const string& item = GetPortString(pActInfo, 1);

			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(item);
      if (0 == pInventory->GetItemByClass(pClass))
      {
        ActivateOutput(pActInfo, 0, false);
      }
      else
      {
        pGF->GetIItemSystem()->SetActorItem(pActor, item, true);
        ActivateOutput(pActInfo, 0, true);
      }
    }
  }

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNode_InventoryHolsterItem : public CFlowBaseNode
{
public:
	CFlowNode_InventoryHolsterItem( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Holster", _HELP("Holster current item" )),
			InputPortConfig_Void( "UnHolster", _HELP("Unholster current item") ),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void("Done", _HELP("Done" )),
			{0}
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate)
		{
			IGameFramework* pGF = gEnv->pGame->GetIGameFramework();

			IActor * pActor = pGF->GetClientActor();
			if (!pActor)
				return;
			const bool bHolster = IsPortActive(pActInfo, 0);
			pActor->HolsterItem(bHolster);
			ActivateOutput(pActInfo, 0, true);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


class CFlowNode_InventoryItemSelected : public CFlowBaseNode, public IItemSystemListener
{
public:
	CFlowNode_InventoryItemSelected( SActivationInfo * pActInfo ) : m_actInfo(*pActInfo)
	{
		m_pGF = gEnv->pGame->GetIGameFramework();
	}

	~CFlowNode_InventoryItemSelected()
	{
		IItemSystem* pItemSys = m_pGF->GetIItemSystem();
		if (pItemSys)
			pItemSys->UnregisterListener(this);
	}

	IFlowNodePtr Clone(SActivationInfo *pActInfo)
	{
		return new CFlowNode_InventoryItemSelected(pActInfo);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig<bool>( "Active", true, _HELP("Set to active to get notified when a new item was selected" )),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig<string>  ("ItemClass", _HELP("Outputs selected item's class" )),
			OutputPortConfig<string>  ("ItemName", _HELP("Outputs selected item's name" )),
			OutputPortConfig<EntityId>("ItemId", _HELP("Outputs selected item's entity id" )),
			{0}
		};

		config.sDescription = _HELP("Tracks the currently selected item of the player. Use [Active] to enable");
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate || event == eFE_Initialize)
		{
			if (IsPortActive(pActInfo, 0))
			{
				bool active = GetPortBool(pActInfo, 0);
				if (active)
					m_pGF->GetIItemSystem()->RegisterListener(this);
				else
					m_pGF->GetIItemSystem()->UnregisterListener(this);
			}
		}
	}

	virtual void OnSetActorItem(IActor *pActor, IItem *pItem )
	{
		IActor * pClientActor = m_pGF->GetClientActor();
		if (!pClientActor || pClientActor != pActor)
			return;
		string name;
		if (pItem)
		{
			name = pItem->GetEntity()->GetClass()->GetName();
			ActivateOutput(&m_actInfo, 0, name);
			name = pItem->GetEntity()->GetName();
			ActivateOutput(&m_actInfo, 1, name);
			ActivateOutput(&m_actInfo, 2, pItem->GetEntityId());
		}
		else
		{
			ActivateOutput(&m_actInfo, 0, name);
			ActivateOutput(&m_actInfo, 1, name);
			ActivateOutput(&m_actInfo, 2, 0);
		}
	}

	virtual void OnDropActorItem(IActor *pActor, IItem *pItem )
	{
	}

	virtual void OnSetActorAccessory(IActor *pActor, IItem *pItem )
	{
	}

	virtual void OnDropActorAccessory(IActor *pActor, IItem *pItem )
	{

	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

protected:
	SActivationInfo m_actInfo;
	IGameFramework *m_pGF;
};


class CFlowNode_InventoryAddAmmo : public CFlowBaseNode
{
public:
	CFlowNode_InventoryAddAmmo( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Add", _HELP("Connect event here to add the ammo" )),
			InputPortConfig<string>( "Ammo", _HELP("The type of ammo to add to the inventory" ), _HELP("Ammo"), _UICONFIG("enum_global:ammos")),
			InputPortConfig<int>( "Amount", _HELP("The amount of ammo to add to the inventory" ), _HELP("Amount")),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig<bool>("Out", _HELP("true if the ammo was actually added, false otherwise" )),
			{0}
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
			IActor * pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
			if (!pActor)
				return;

			IEntity * pEntity = pActor->GetEntity();
			assert(pEntity);
			if (!pEntity)
				return;

			IEntitySystem* pEntSys = gEnv->pEntitySystem;

			IInventory *pInventory = pActor->GetInventory();
			if (pInventory)
			{
				const string& ammo = GetPortString(pActInfo, 1);
				IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammo.c_str());
				assert(pClass);
				int amount = GetPortInt(pActInfo, 2);
				if (amount)
				{
					pInventory->SetAmmoCount(pClass, pInventory->GetAmmoCount(pClass)+amount);
					ActivateOutput(pActInfo, 0, true);
				}
			}

			ActivateOutput(pActInfo, 0, false);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};



class CFlowNode_InventorySetAmmo : public CFlowBaseNode
{
public:
	CFlowNode_InventorySetAmmo( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Set", _HELP("Connect event here to set the ammo" )),
			InputPortConfig<string>( "Ammo", _HELP("The type of ammo to set on the inventory" ), _HELP("Ammo"), _UICONFIG("enum_global:ammos")),
			InputPortConfig<int>( "Amount", _HELP("The amount of ammo to set on the inventory" ), _HELP("Amount")),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig<bool>("Out", _HELP("true if the ammo was actually set, false otherwise" )),
			{0}
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
			IActor * pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
			if (!pActor)
				return;

			IEntity * pEntity = pActor->GetEntity();
			assert(pEntity);
			if (!pEntity)
				return;

			IEntitySystem* pEntSys = gEnv->pEntitySystem;

			IInventory *pInventory = pActor->GetInventory();
			if (pInventory)
			{
				const string& ammo = GetPortString(pActInfo, 1);
				IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammo.c_str());
				assert(pClass);
				int amount = GetPortInt(pActInfo, 2);
				if (amount)
				{
					pInventory->SetAmmoCount(pClass, amount);
					ActivateOutput(pActInfo, 0, true);
				}
			}

			ActivateOutput(pActInfo, 0, false);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


class CFlowNode_InventorySetAmmoEx : public CFlowBaseNode
{
public:  
  enum EInputs
  { 
    eIn_SetAmmo = 0,
    eIn_AmmoType,
    eIn_Amount,
  };

  enum EOutputs
  {
    eOut_Out = 0,
  };

  CFlowNode_InventorySetAmmoEx( SActivationInfo * pActInfo )
  {
  }

  IFlowNodePtr Clone( SActivationInfo *pActInfo )
  {
    return new CFlowNode_InventorySetAmmoEx( pActInfo );    
  };

  void GetConfiguration( SFlowNodeConfig& config )
  {
    static const SInputPortConfig in_ports[] = 
    {
      InputPortConfig_Void( "Set", _HELP("Connect event here to set the ammo" )),
      InputPortConfig<string>( "Ammo", _HELP("The type of ammo to set on the inventory" ), _HELP("Ammo"), _UICONFIG("enum_global:ammos")),
      InputPortConfig<int>( "Amount", _HELP("The amount of ammo to set on the inventory" ), _HELP("Amount")),
      {0}
    };

    static const SOutputPortConfig out_ports[] = 
    {
      OutputPortConfig<bool>("Out", _HELP("true if inventory was found and ammo was set, false otherwise" )),
      {0}
    };

    config.sDescription = _HELP("Extended version of SetAmmo, for use on arbitrary entities possessing an Inventory.");
    config.pInputPorts = in_ports;
    config.pOutputPorts = out_ports;
    config.nFlags |= EFLN_TARGET_ENTITY;
    config.SetCategory(EFLN_APPROVED);
  }

  void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
  {
    if (event == eFE_Activate && IsPortActive(pActInfo, eIn_SetAmmo))
    {
      if (!pActInfo->pEntity)
        return;

      bool res = false;
      
      if (CGameObject *pGameObject = (CGameObject *)pActInfo->pEntity->GetProxy(ENTITY_PROXY_USER))
      {
        if (IInventory *pInventory = static_cast<IInventory *>(pGameObject->QueryExtension("Inventory")))
        {   
          const string& ammo = GetPortString(pActInfo, eIn_AmmoType);
					IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammo.c_str());
					assert(pClass);
          int amount = GetPortInt(pActInfo, eIn_Amount);
            
          pInventory->SetAmmoCount(pClass, amount);
          res = true;          
        }
      }

      ActivateOutput(pActInfo, eOut_Out, res);
    }
  }

  virtual void GetMemoryStatistics(ICrySizer * s)
  {
    s->Add(*this);
  }
};


class CFlowNode_InventoryRemoveAllAmmo : public CFlowBaseNode
{
public:
	CFlowNode_InventoryRemoveAllAmmo( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Remove", _HELP("Connect event here to remove all the ammo" )),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig<bool>("Out", _HELP("true if the ammo was actually removed, false otherwise" )),
			{0}
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
			IActor * pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
			if (!pActor)
				return;

			IEntity * pEntity = pActor->GetEntity();
			assert(pEntity);
			if (!pEntity)
				return;

			IInventory *pInventory = pActor->GetInventory();
			if (pInventory)
			{
				pInventory->ResetAmmo();
				ActivateOutput(pActInfo, 0, true);
			}

			ActivateOutput(pActInfo, 0, false);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNode_AddEquipmentPack : public CFlowBaseNode
{
public:
	CFlowNode_AddEquipmentPack( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Trigger", _HELP("Trigger to give the pack" )),
			InputPortConfig<string>( "equip_Pack", _HELP("Name of the pack" )),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void("Done", _HELP("Triggered when done." )),
			{0}
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (pActInfo->pEntity == 0)
			return;

		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
			IActor * pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId());
			if (!pActor)
				return;

			const string& packName = GetPortString(pActInfo, 1);
			CCryAction::GetCryAction()->GetIItemSystem()->GetIEquipmentManager()->GiveEquipmentPack(pActor, packName.c_str(), true, true);
			ActivateOutput(pActInfo, 0, true);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNode_StorePlayerInventory : public CFlowBaseNode
{
public:
	CFlowNode_StorePlayerInventory( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Trigger", _HELP("Trigger to store Player's Inventory." )),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void("Done", _HELP("Triggered when done." )),
			{0}
		};

		// config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_ADVANCED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		//if (pActInfo->pEntity == 0)
		//	return;

		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
			IActor * pActor = CCryAction::GetCryAction()->GetClientActor();
			// IActor * pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId());
			if (!pActor)
				return;

			CItemSystem* pItemSystem = static_cast<CItemSystem*> (CCryAction::GetCryAction()->GetIItemSystem());
			pItemSystem->SerializePlayerLTLInfo(false);
			ActivateOutput(pActInfo, 0, true);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNode_RestorePlayerInventory : public CFlowBaseNode
{
public:
	CFlowNode_RestorePlayerInventory( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Trigger", _HELP("Trigger to restore Player's Inventory." )),
			{0}
		};

		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void("Done", _HELP("Triggered when done." )),
			{0}
		};

		// config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_ADVANCED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		//if (pActInfo->pEntity == 0)
		//	return;

		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
			IActor * pActor = CCryAction::GetCryAction()->GetClientActor();
			// IActor * pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId());
			if (!pActor)
				return;

			CItemSystem* pItemSystem = static_cast<CItemSystem*> (CCryAction::GetCryAction()->GetIItemSystem());
			pItemSystem->SerializePlayerLTLInfo(true);
			ActivateOutput(pActInfo, 0, true);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE_SINGLETON("Inventory:AddItem",		CFlowNode_InventoryAddItem);
REGISTER_FLOW_NODE_SINGLETON("Inventory:RemoveItem",CFlowNode_InventoryRemoveItem);
REGISTER_FLOW_NODE_SINGLETON("Inventory:RemoveAllItems",CFlowNode_InventoryRemoveAllItems);
REGISTER_FLOW_NODE_SINGLETON("Inventory:HasItem",		CFlowNode_InventoryHasItem);
REGISTER_FLOW_NODE_SINGLETON("Inventory:HasAmmo",  CFlowNode_InventoryHasAmmo);
REGISTER_FLOW_NODE_SINGLETON("Inventory:HolsterItem",		CFlowNode_InventoryHolsterItem);
REGISTER_FLOW_NODE_SINGLETON("Inventory:SelectItem",	CFlowNode_InventorySelectItem);
REGISTER_FLOW_NODE("Inventory:ItemSelected",	CFlowNode_InventoryItemSelected);
REGISTER_FLOW_NODE_SINGLETON("Inventory:AddAmmo",		CFlowNode_InventoryAddAmmo);
REGISTER_FLOW_NODE_SINGLETON("Inventory:SetAmmo",	CFlowNode_InventorySetAmmo);
REGISTER_FLOW_NODE("Inventory:SetAmmoEx",	CFlowNode_InventorySetAmmoEx);
REGISTER_FLOW_NODE_SINGLETON("Inventory:RemoveAllAmmo",	CFlowNode_InventoryRemoveAllAmmo);
REGISTER_FLOW_NODE_SINGLETON("Inventory:AddEquipPack",	CFlowNode_AddEquipmentPack);
REGISTER_FLOW_NODE_SINGLETON("Inventory:StorePlayerInventory",	CFlowNode_StorePlayerInventory);
REGISTER_FLOW_NODE_SINGLETON("Inventory:RestorePlayerInventory",	CFlowNode_RestorePlayerInventory);
