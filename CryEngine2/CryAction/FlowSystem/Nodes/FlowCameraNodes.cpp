/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2005.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Camera Nodes
  
 -------------------------------------------------------------------------
  History:
  - 30:6:2005   15:53 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "FlowBaseNode.h"

class CFlowNode_Camera : public CFlowBaseNode
{
public:
	CFlowNode_Camera( SActivationInfo * pActInfo )
	{
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>( "pos",	"Input camera position." ),
			InputPortConfig<Vec3>( "dir",	"Input camera direction." ),
			InputPortConfig<float>( "roll", "Input camera roll." ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("pos", "Current camera position."),
			OutputPortConfig<Vec3>("dir", "Current camera direction."),
			OutputPortConfig<float>("roll", "Current camera roll."),
			{0}
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		CCamera camera(gEnv->pSystem->GetViewCamera());
		Vec3	pos(camera.GetPosition());
		Vec3	dir(camera.GetViewdir());
		float	roll(camera.GetAngles().y);

		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, 0))
					pos = GetPortVec3(pActInfo, 0);
				if (IsPortActive(pActInfo, 1))
					dir = GetPortVec3(pActInfo, 1);
				if (IsPortActive(pActInfo, 2))
					roll = GetPortFloat(pActInfo, 2);

				if (dir.len2() > 0.0f)
				{
					camera.SetMatrix(CCamera::CreateOrientationYPR(CCamera::CreateAnglesYPR(dir, roll)));
					camera.SetPosition(pos);
					gEnv->pSystem->SetViewCamera(camera);
				}
			}
			break;
		case eFE_Update:
			ActivateOutput(pActInfo, 0, pos);
			ActivateOutput(pActInfo, 1, dir);
			ActivateOutput(pActInfo, 2, roll);
			break;
		}
	}
};


class CFlowNode_CameraYPR : public CFlowBaseNode
{
public:
	CFlowNode_CameraYPR( SActivationInfo * pActInfo ) 
	{
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>( "yaw",	"Input camera yaw." ),
			InputPortConfig<float>( "pitch",	"Input camera pitch." ),
			InputPortConfig<float>( "roll", "Input camera roll." ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("pos", "Current camera yaw."),
			OutputPortConfig<float>("dir", "Current camera pitch."),
			OutputPortConfig<float>("roll", "Current camera roll."),
			{0}
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		CCamera camera(gEnv->pSystem->GetViewCamera());
		Ang3 angles(camera.GetAngles());
		float	yaw(angles.x);
		float pitch(angles.y);
		float	roll(angles.z);

		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, 0))
					yaw = GetPortFloat(pActInfo, 0);
				if (IsPortActive(pActInfo, 1))
					pitch = GetPortFloat(pActInfo, 1);
				if (IsPortActive(pActInfo, 2))
					roll = GetPortFloat(pActInfo, 2);

				Vec3 pos(camera.GetPosition());
				camera.SetMatrix(CCamera::CreateOrientationYPR(Ang3(yaw, pitch, roll)));
				camera.SetPosition(pos);
				gEnv->pSystem->SetViewCamera(camera);
			}
			break;
		case eFE_Update:
			ActivateOutput(pActInfo, 0, yaw);
			ActivateOutput(pActInfo, 1, pitch);
			ActivateOutput(pActInfo, 2, roll);
			break;
		}
	}
};

class CFlowNode_CameraViewShake : public CFlowBaseNode
{
public:
	CFlowNode_CameraViewShake( SActivationInfo * pActInfo )
	{
	};

	enum Restriction
	{
		ER_None = 0,
		ER_NoVehicle,
		ER_InVehicle,
	};
	
	enum Inputs
	{
		EIP_Trigger = 0,
		EIP_Restriction,
		EIP_ViewType,
		EIP_GroundOnly,
		EIP_Angle,
		EIP_Shift,
		EIP_Duration,
		EIP_Frequency,
		EIP_Randomness,
		// EIP_Flip,
	};

	enum ViewType
	{
		VT_FirstPerson = 0,
		VT_Current = 1
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void  ( "Trigger", _HELP("Trigger to start shaking") ),
			InputPortConfig<int>  ( "Restrict", ER_None, _HELP("Restriction"), 0, _UICONFIG("enum_int:None=0,NoVehicle=1,InVehicle=2")),
			InputPortConfig<int>  ( "View", VT_FirstPerson, _HELP("Which view to use. FirstPerson or Current (might be Trackview)."), 0, _UICONFIG("enum_int:FirstPerson=0,Current=1")),
			InputPortConfig<bool> ( "GroundOnly", false, _HELP("Apply shake only when the player is standing on the ground") ),
			InputPortConfig<Vec3> ( "Angle",	Vec3(ZERO), _HELP("Shake Angles") ),
			InputPortConfig<Vec3> ( "Shift",	Vec3(ZERO), _HELP("Shake shifting") ),
			InputPortConfig<float>( "Duration", 1.0f, _HELP("Duration")),
			InputPortConfig<float>( "Frequency", 10.0f, _HELP("Frequency. Can be changed dynamically.")),
			InputPortConfig<float>( "Randomness", 0.1f, _HELP("Randomness")),
			// InputPortConfig<bool> ( "Flip", true, _HELP("Flip")),
			{0}
		};
		config.sDescription = _HELP("Camera View Shake node");
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate)
			return;
		const bool bTriggered = IsPortActive(pActInfo, EIP_Trigger);
		const bool bFreqTriggered = IsPortActive(pActInfo, EIP_Frequency);
		if (bTriggered == false && bFreqTriggered == false)
			return;

		IGameFramework* pGF = gEnv->pGame->GetIGameFramework();
		IView* pView = 0;
		IView* pActiveView = pGF->GetIViewSystem()->GetActiveView();
		int viewType = GetPortInt(pActInfo, EIP_ViewType);
		if (viewType == VT_FirstPerson) // use player's view
		{
			IActor* pActor = pGF->GetClientActor();
			if (pActor == 0)
				return;

			const int restrict = GetPortInt(pActInfo, EIP_Restriction);
			if (restrict != ER_None)
			{
				IVehicle* pVehicle = pActor->GetLinkedVehicle();
				if (restrict == ER_InVehicle && pVehicle == 0)
					return;
				if (restrict == ER_NoVehicle && pVehicle != 0 /* && pVehicle->GetSeatForPassenger(entityId) != 0 */)
					return;
			}

			EntityId entityId = pActor->GetEntityId();
			pView = pGF->GetIViewSystem()->GetViewByEntityId(entityId);
		}
		else // active view 
		{
			pView = pActiveView;
		}
		if (pView == 0 || pView != pActiveView)
			return;

		static const int FLOWGRAPH_SHAKE_ID = 24;
		const bool  bGroundOnly = GetPortBool(pActInfo, EIP_GroundOnly);
		const Ang3  angles = Ang3(DEG2RAD(GetPortVec3(pActInfo, EIP_Angle)));
		const Vec3  shift = GetPortVec3(pActInfo, EIP_Shift);
		const float duration = GetPortFloat(pActInfo, EIP_Duration);
		float freq = GetPortFloat(pActInfo, EIP_Frequency);
		if (iszero(freq) == false)
			freq = 1.0f / freq;
		const float rand = GetPortFloat(pActInfo, EIP_Randomness);
		static const bool bFlip = true; // GetPortBool(pActInfo, EIP_Flip);
		const bool bUpdateOnly = !bTriggered && bFreqTriggered; // it's an update if and only if Frequency has been changed
		pView->SetViewShake(angles, shift, duration, freq, rand, FLOWGRAPH_SHAKE_ID, bFlip, bUpdateOnly, bGroundOnly);
	}
};

REGISTER_FLOW_NODE_SINGLETON( "Camera:Camera",CFlowNode_Camera );
REGISTER_FLOW_NODE_SINGLETON( "Camera:CameraYPR",CFlowNode_CameraYPR );
REGISTER_FLOW_NODE_SINGLETON( "Camera:ViewShake",CFlowNode_CameraViewShake );