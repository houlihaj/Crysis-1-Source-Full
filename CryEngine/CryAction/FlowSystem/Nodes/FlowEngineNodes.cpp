#include "StdAfx.h"
#include "FlowBaseNode.h"

class CFlowNode_PortalSwitch : public CFlowBaseNode
{
public:
	CFlowNode_PortalSwitch( SActivationInfo * pActInfo )
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("Activate"),
			InputPortConfig<SFlowSystemVoid>("Deactivate"),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_SetEntityId:
			{
				if (!pActInfo->pEntity)
					return;
				// don't disable here, because it will trigger on QL as well
				//gEnv->p3DEngine->ActivatePortal( pActInfo->pEntity->GetWorldPos(), false, pActInfo->pEntity->GetName() );
			}
			break;

		case eFE_Initialize:
			{
				if (!pActInfo->pEntity)
					return;
				gEnv->p3DEngine->ActivatePortal( pActInfo->pEntity->GetWorldPos(), false, pActInfo->pEntity->GetName() );
			}
			break;
		case eFE_Activate:
			{
				if (!pActInfo->pEntity)
				{
					GameWarning("[flow] Trying to activate a portal at a non-existent entity");
					return;
				}
				bool doAnything = false;
				bool activate = false;
				if (IsPortActive(pActInfo, 0))
				{
					doAnything = true;
					activate = true;
				}
				else if (IsPortActive(pActInfo, 1))
				{
					doAnything = true;
					activate = false;
				}
				if (doAnything)
					gEnv->p3DEngine->ActivatePortal( pActInfo->pEntity->GetWorldPos(), activate, pActInfo->pEntity->GetName() );
			}
			break;
		}
	}
};

REGISTER_FLOW_NODE_SINGLETON("Engine:PortalSwitch", CFlowNode_PortalSwitch);
