
#include "StdAfx.h"
#include <ISystem.h>
#include "FlowBaseNode.h"


class CComputeLighting_Node : public CFlowBaseNode
{

public:
	CComputeLighting_Node( SActivationInfo * pActInfo )
	{
	};
	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>( "Position",_HELP("Position to compute lighting at") ),
			InputPortConfig<float>( "Radius", 1.0f, _HELP("Radius around the position") ),
			InputPortConfig<bool>( "Accuracy",_HELP("TRUE for accurate, FALSE for fast") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("Light",_HELP("Light output")),
			{0}
		};
		config.sDescription = _HELP( "Compute the amount of light at a given point" );
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		switch (event)
		{
			case eFE_Activate:
			{
				float r = gEnv->p3DEngine->GetLightAmountInRange(
					GetPortVec3(pActInfo, 0),
					GetPortFloat(pActInfo, 1),
					GetPortBool(pActInfo, 2));
					ActivateOutput(pActInfo, 0, r);
				break;
			}

			case eFE_Initialize:
			{
				ActivateOutput(pActInfo, 0, 0.0f);
			}

			case eFE_Update:
			{
				break;
			}
		};
	};
};

REGISTER_FLOW_NODE_SINGLETON( "Environment:ComputeLighting",CComputeLighting_Node );
