#include "StdAfx.h"
#include "FlowBaseNode.h"
#include "CryAction.h"
#include "IActorSystem.h"
#include "GameObjects/GameObject.h"
#include "IGameRulesSystem.h"
#include <IInterestSystem.h>

inline IActor* GetAIActor( IFlowNode::SActivationInfo *pActInfo )
{
	if (!pActInfo->pEntity)
		return 0;
	return CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId());
}

class CFlowPlayer : public CFlowBaseNode
{
public:
	CFlowPlayer( SActivationInfo * pActInfo )
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<EntityId>("entityId", _HELP("Player entity id")),
			{0}
		};
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Outputs the local players entity id - NOT USABLE FOR MULTIPLAYER");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
		case eFE_Activate:
			{
				IActor * pActor = CCryAction::GetCryAction()->GetClientActor();
				if (pActor)
					ActivateOutput(pActInfo, 0, pActor->GetEntityId());
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowDamageActor : public CFlowBaseNode
{
public:
	CFlowDamageActor( SActivationInfo * pActInfo )
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void   ("Trigger", _HELP("Trigger this port to actually damage the actor")),
			InputPortConfig<int>   ("damage", 0, _HELP("Amount of damage to exert when [Trigger] is activated"), _HELP("Damage")),
			InputPortConfig<Vec3>  ("Position", Vec3(ZERO), _HELP("Position of damage")),
			{0}
		};
		config.pInputPorts = inputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Damages attached entity by [Damage] when [Trigger] is activated");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				IActor * pActor = GetAIActor(pActInfo);
				if (pActor)
				{ 
					if (IEntity* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRulesEntity())
					{
						if (IScriptTable* pGameRulesScript = pGameRules->GetScriptTable())
						{
							IScriptSystem* pSS = pGameRulesScript->GetScriptSystem();
							if (pGameRulesScript->GetValueType("CreateHit") == svtFunction &&
								  pSS->BeginCall(pGameRulesScript, "CreateHit"))
							{
								pSS->PushFuncParam(pGameRulesScript);
								pSS->PushFuncParam(ScriptHandle(pActor->GetEntityId())); // target
								pSS->PushFuncParam(ScriptHandle(pActor->GetEntityId())); // shooter
								pSS->PushFuncParam(ScriptHandle(0)); // weapon
								pSS->PushFuncParam(GetPortInt(pActInfo, 1)); // damage
								pSS->PushFuncParam(0);  // radius
								pSS->PushFuncParam(""); // material
								pSS->PushFuncParam(0); // partID
								pSS->PushFuncParam("FG");  // type
								pSS->PushFuncParam(GetPortVec3(pActInfo, 2)); // pos
								pSS->PushFuncParam(FORWARD_DIRECTION);  // dir
								pSS->PushFuncParam(FORWARD_DIRECTION); // normal
								pSS->EndCall();
							}
						}
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


class CFlowActorGrabObject : public CFlowBaseNode
{
public:
  CFlowActorGrabObject( SActivationInfo * pActInfo )
  {
  }

  virtual void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = {      
      InputPortConfig<EntityId>("objectId", _HELP("Entity to grab")),
      InputPortConfig_Void("grab", _HELP("Pulse this to grab object.")),
      InputPortConfig_Void("drop", _HELP("Pulse this to drop currently grabbed object.")),
      InputPortConfig<bool>("throw", _HELP("If true, object is thrown forward.")),
      {0}
    };
    static const SOutputPortConfig outputs[] = {
      OutputPortConfig<bool>("success", _HELP("true if object was grabbed/dropped successfully")),
      OutputPortConfig<EntityId>("grabbedObjId", _HELP("Currently grabbed object, or 0")),
      {0}
    };    
    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.nFlags |= EFLN_TARGET_ENTITY;
    config.sDescription = _HELP("Target entity will try to grab the input object, respectively drop/throw its currently grabbed object.");
		config.SetCategory(EFLN_APPROVED);
  }

  virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
  {
    switch (event)
    {    
    case eFE_Activate:
      {
        IActor* pActor = GetAIActor(pActInfo);
        if (!pActor)
          break;
        
        IEntity* pEnt = pActor->GetEntity();
        if (!pEnt)
          break;
        
        HSCRIPTFUNCTION func;        
        int ret = 0;
        IScriptTable* pTable = pEnt->GetScriptTable();
        if (!pTable)
          break;

        if (IsPortActive(pActInfo, 1) && pTable->GetValue("GrabObject", func))
        {                   
          IEntity* pObj = gEnv->pEntitySystem->GetEntity( GetPortEntityId(pActInfo, 0) );
          if (pObj)
          {
            IScriptTable* pObjTable = pObj->GetScriptTable();
            Script::CallReturn(gEnv->pScriptSystem, func, pTable, pObjTable, ret);
          }
          ActivateOutput(pActInfo, 0, ret);
        }  
        else if (IsPortActive(pActInfo, 2) && pTable->GetValue("DropObject", func))
        {
          bool bThrow = GetPortBool(pActInfo, 3);
          Script::CallReturn(gEnv->pScriptSystem, func, pTable, bThrow, ret);
          ActivateOutput(pActInfo, 0, ret);
        }
        
        if (pTable->GetValue("GetGrabbedObject", func))
        {          
          ScriptHandle sH(0);
          Script::CallReturn(gEnv->pScriptSystem, func, pTable, sH);
          ActivateOutput(pActInfo, 1, EntityId(sH.n));
        }

        break;
      }
    }
  }

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


class CFlowActorGetHealth : public CFlowBaseNode
{
public:
	CFlowActorGetHealth( SActivationInfo * pActInfo )
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger", _HELP("Trigger this port to get the current health")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<int>("Health", _HELP("Current health of entity")),
			{0}
		};    
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Get health of an entity (actor)");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				IActor* pActor = GetAIActor(pActInfo);
				if(pActor) {
					ActivateOutput(pActInfo, 0, pActor->GetHealth());
				} else {
					GameWarning("CFlowActorGetHealth - No Entity or Entity not an Actor!");
				}
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


class CFlowActorSetHealth : public CFlowBaseNode
{
public:
	CFlowActorSetHealth( SActivationInfo * pActInfo )
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger", _HELP("Trigger this port to set health")),
			InputPortConfig<int>("Value", _HELP("Health value to be set on entity when Trigger is activated")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<int>("Health", _HELP("Current health of entity (activated when Trigger is activated)")),
			{0}
		};    
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Set health of an entity (actor)");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0)) {
				IActor* pActor = GetAIActor(pActInfo);
				if(pActor) {
					pActor->SetHealth( GetPortInt(pActInfo, 1) );
					ActivateOutput(pActInfo, 0, pActor->GetHealth()); // use pActor->GetHealth (might have been clamped to maxhealth]
				} else {
					GameWarning("CFlowActorSetHealth - No Entity or Entity not an actor!");
				}
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


class CFlowActorCheckHealth : public CFlowBaseNode
{
public:
	CFlowActorCheckHealth( SActivationInfo * pActInfo )
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger", _HELP("Trigger this port to check health")),
			InputPortConfig<int>("MinHealth", 0, _HELP("Lower limit of range")),
			InputPortConfig<int>("MaxHealth", 100, _HELP("Upper limit of range")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<bool>("InRange", _HELP("True if Health is in range, False otherwise")),
			{0}
		};    
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Check if health of entity (actor) is in range [MinHealth, MaxHealth]");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0)) {
				IActor* pActor = GetAIActor(pActInfo);
				if(pActor) {
					int health = pActor->GetHealth();
					int minH = GetPortInt(pActInfo, 1);
					int maxH = GetPortInt(pActInfo, 2);
					ActivateOutput(pActInfo, 0, minH <= health && health <= maxH);
				} else {
					CryLogAlways("CFlowActorCheckHealth - No Entity or Entity not an Actor!");
				}
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowGameObjectEvent : public CFlowBaseNode
{
public:
	CFlowGameObjectEvent( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Trigger", _HELP("Trigger this port to send the event" )),
			InputPortConfig<string>( "EventName", _HELP("Name of event to send" )),
			InputPortConfig<string>( "EventParam", _HELP("Parameter of the event [event-specific]" )),			
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.sDescription = _HELP("Broadcast a game object event or send to a specific entity. EventParam is an event specific string");
		config.SetCategory(EFLN_ADVANCED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event && IsPortActive(pActInfo,0))
		{
			const string& eventName = GetPortString(pActInfo, 1);
			uint32 eventId = CCryAction::GetCryAction()->GetIGameObjectSystem()->GetEventID(eventName.c_str());
			SGameObjectEvent evt(eventId, eGOEF_ToAll, IGameObjectSystem::InvalidExtensionID, (void*) (GetPortString(pActInfo, 2).c_str()));
			if (pActInfo->pEntity == 0)
			{
				// broadcast to all gameobject events
				CCryAction::GetCryAction()->GetIGameObjectSystem()->BroadcastEvent(evt);
			}
			else
			{
				// send to a specific entity only
				if (CGameObject * pGameObject = (CGameObject*)pActInfo->pEntity->GetProxy(ENTITY_PROXY_USER))
					pGameObject->SendEvent(evt);
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Register an entity with the interest system
class CFlowNode_EntityInterest : public CFlowBaseNode
{
public:
	enum EInputs
	{
		IN_REGISTER_INTERESTING,
		IN_REGISTER_USER,
		IN_INTEREST_LEVEL
	};

	CFlowNode_EntityInterest( SActivationInfo * pActInfo )
	{
	}

	//void Serialize(SActivationInfo *pActInfo, TSerialize ser)
	//{
	//}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		// declare input ports
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "registerInteresting", _HELP("Register entity as interesting")),
			InputPortConfig_Void( "registerUser", _HELP("Register an actor to use the system")),
			InputPortConfig<float>( "interestLevel",	5.0f, _HELP("Base interest level of object or minimum interest level of actor" )),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.sDescription = _HELP("Registers an entity with the interest system");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		IEntity *pEnt = pActInfo->pEntity;
		if(eFE_Activate == event && pEnt)
		{
			ICentralInterestManager * pCIM = gEnv->pAISystem->GetCentralInterestManager();

			if (IsPortActive(pActInfo,IN_REGISTER_INTERESTING))
			{
				pCIM->RegisterInterestingEntity(pEnt, GetPortFloat(pActInfo,IN_INTEREST_LEVEL));
			}
			else if (IsPortActive(pActInfo,IN_REGISTER_USER))
			{
				pCIM->RegisterInterestedAIActor(pEnt, 0.0f);
			}

		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s) 
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE_SINGLETON("Game:LocalPlayer", CFlowPlayer);
REGISTER_FLOW_NODE_SINGLETON("Game:DamageActor", CFlowDamageActor);
REGISTER_FLOW_NODE_SINGLETON("Game:ActorGrabObject", CFlowActorGrabObject);
REGISTER_FLOW_NODE_SINGLETON("Game:ActorGetHealth", CFlowActorGetHealth);
REGISTER_FLOW_NODE_SINGLETON("Game:ActorSetHealth", CFlowActorSetHealth);
REGISTER_FLOW_NODE_SINGLETON("Game:ActorCheckHealth", CFlowActorCheckHealth);
REGISTER_FLOW_NODE_SINGLETON("Game:GameObjectEvent", CFlowGameObjectEvent);
REGISTER_FLOW_NODE( "Game:EntityInterest",CFlowNode_EntityInterest);

