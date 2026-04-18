
#include "StdAfx.h"
#include <ISystem.h>
#include <Cry_Math.h>
#include "FlowBaseNode.h"

class CRotateEntityTo_Node : public CFlowBaseNode
{
	CTimeValue m_startTime;
	CTimeValue m_endTime;
	Quat m_targetQuat;
	Quat m_sourceQuat;

public:
	CRotateEntityTo_Node( SActivationInfo * pActInfo )
	{
		m_targetQuat.SetIdentity();
		m_sourceQuat.SetIdentity();
	};

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CRotateEntityTo_Node(pActInfo);
	}

	void Serialize(SActivationInfo *pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("m_startTime", m_startTime);
		ser.Value("m_endTime", m_endTime);
		ser.Value("m_sourceQuat", m_sourceQuat);
		ser.Value("m_targetQuat", m_targetQuat);
		ser.EndGroup();
	}

	enum EInPorts
	{
		IN_DEST = 0,
		IN_DYN_DEST,
		IN_DURATION,
		IN_START,
		IN_STOP
	};
	enum EOutPorts
	{
		OUT_CURRENT = 0,
		OUT_CURRENT_RAD,
		OUT_DONE
	};

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>( "Destination",_HELP("Destination") ),
			InputPortConfig<bool>( "DynamicUpdate", true, _HELP("Dynamic update of Destination [follow-during-movement]"), _HELP("DynamicUpdate") ),
			InputPortConfig<float>( "Time",_HELP("Duration in seconds") ),
			InputPortConfig_Void( "Start", _HELP("Trigger this port to start the movement") ),
			InputPortConfig_Void( "Stop",  _HELP("Trigger this port to stop the movement") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("Current",_HELP("Current Rotation in Degrees")),
			OutputPortConfig<Vec3>("CurrentRad",_HELP("Current Rotation in Radian")),
			OutputPortConfig_Void("DoneTrigger", _HELP("Triggered when destination rotation is reached or rotation stopped"), 	_HELP("Done")),
			{0}
		};
		config.sDescription = _HELP( "Rotate an entity during a defined period of time" );
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void DegToQuat(const Vec3& rotation, Quat& destQuat)
	{
		Ang3 temp;
		// snap the angles 
		temp.x = Snap_s360(rotation.x);
		temp.y = Snap_s360(rotation.y);
		temp.z = Snap_s360(rotation.z);
		const Ang3 angRad = DEG2RAD(temp);
		destQuat.SetRotationXYZ(angRad);
	}

	void Interpol(const float fTime, SActivationInfo* pActInfo)
	{
		Quat theQuat;
		theQuat.SetSlerp(m_sourceQuat, m_targetQuat, fTime);
		if (pActInfo->pEntity)
			pActInfo->pEntity->SetRotation(theQuat);
		Ang3 angles = Ang3(theQuat);
		Ang3 anglesDeg = RAD2DEG(angles);
		ActivateOutput(pActInfo, OUT_CURRENT_RAD, Vec3(angles));
		ActivateOutput(pActInfo, OUT_CURRENT, Vec3(anglesDeg));
	}

	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		switch (event)
		{
			case eFE_Activate:
			{
				if (IsPortActive(pActInfo, IN_STOP)) // Stop
				{
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
					ActivateOutput(pActInfo, OUT_DONE, true);
				}
				if (IsPortActive(pActInfo, IN_DEST) && GetPortBool(pActInfo, IN_DYN_DEST) == true)
				{
					const Vec3& destRotDeg = GetPortVec3(pActInfo,	IN_DEST);
					DegToQuat(destRotDeg, m_targetQuat);
				}
				if (IsPortActive(pActInfo,IN_START)) {
					m_startTime = gEnv->pTimer->GetFrameStartTime();
					m_endTime = m_startTime + GetPortFloat(pActInfo, IN_DURATION);

					const Vec3& destRotDeg = GetPortVec3(pActInfo,	IN_DEST);
					DegToQuat(destRotDeg, m_targetQuat);

					IEntity *pEnt = pActInfo->pEntity;
					if(pEnt)
						m_sourceQuat = pEnt->GetRotation();
					else
						m_sourceQuat = m_targetQuat;

					Interpol(0.0f, pActInfo);
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true	);
				}
				break;
			}

			case eFE_Initialize:
			{
				pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
				break;
			}

			case eFE_Update:
			{
				CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();
				const float fDuration = (m_endTime - m_startTime).GetSeconds();
				float fPosition;
				if (fDuration <= 0.0)
					fPosition = 1.0;
				else
				{
					fPosition = (curTime - m_startTime).GetSeconds() / fDuration;
					fPosition = CLAMP(fPosition, 0.0f, 1.0f);
				}
				Interpol(fPosition, pActInfo);
				if (curTime >= m_endTime)
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					ActivateOutput(pActInfo, OUT_DONE, true);
				}
				break;
			}
		};
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE( "Movement:RotateEntityTo", CRotateEntityTo_Node );
