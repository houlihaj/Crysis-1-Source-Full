 ////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   EntitiesInRange.cpp
//  Version:     v1.00
//  Created:     Oct 24th 2005 by AlexL
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowBaseNode.h"

class CFlowNode_EntitiesInRange : public CFlowBaseNode
{
public:
	CFlowNode_EntitiesInRange( SActivationInfo * pActInfo )
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void ("Trigger", _HELP("Trigger this port to check if entities are in range")),
			InputPortConfig<EntityId> ("Entity1", _HELP("Entity 1")),
			InputPortConfig<EntityId> ("Entity2", _HELP("Entity 2")),
			InputPortConfig<float> ("Range", 1.0f, _HELP("Range to check")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<bool>("InRange", _HELP("True if Entities are in Range")),
			OutputPortConfig_Void ("False", _HELP("Triggered if Entities are not in Range")),
			OutputPortConfig_Void ("True", _HELP("Triggered if Entities are in Range")),
			{0}
		};    
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Check if two entities are in range, activated by Trigger");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0)) {
				IEntitySystem* pESys = gEnv->pEntitySystem;
				IEntity *pEnt1 = pESys->GetEntity(GetPortEntityId(pActInfo, 1));
				IEntity *pEnt2 = pESys->GetEntity(GetPortEntityId(pActInfo, 2));
				if (pEnt1 == NULL) {
					GameWarning("[flow] Entity::EntitiesInRange - Invalid Entity 1!");
					break;
				}
				if (pEnt2 == NULL) {
					GameWarning("[flow] Entity::EntitiesInRange - Invalid Entity 2!");
					break;
				}

				const float range = GetPortFloat(pActInfo, 3);
				const bool inRange = pEnt1->GetPos().GetSquaredDistance(pEnt2->GetPos()) <= range*range;
				ActivateOutput(pActInfo, 0, inRange);
				ActivateOutput(pActInfo, 1 + (inRange ? 1 : 0), true);
			}
			break;
		}
	}
};


REGISTER_FLOW_NODE_SINGLETON("Entity:EntitiesInRange", CFlowNode_EntitiesInRange);
