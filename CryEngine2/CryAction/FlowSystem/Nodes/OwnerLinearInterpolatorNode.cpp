
#include "StdAfx.h"
#include <ISystem.h>
#include "FlowBaseNode.h"


class CMoveEntityTo : public CFlowBaseNode
{
	Vec3 m_position;
	Vec3 m_destination;
	float m_lastTime;
	float m_topSpeed;
	float m_easeDistance;
	bool m_bActive;

public:
	CMoveEntityTo( SActivationInfo * pActInfo ) 
		:	m_position(ZERO), 
		m_destination(ZERO),
		m_lastTime(0.0f),
		m_topSpeed(0.0f),
		m_easeDistance(0.0f),
		m_bActive(false)
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CMoveEntityTo(pActInfo);
	}

	virtual void Serialize(SActivationInfo *pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("m_position", m_position);
		ser.Value("m_destination", m_destination);
		ser.Value("m_lastTime", m_lastTime);
		ser.Value("m_topSpeed", m_topSpeed);
		ser.Value("m_easeDistance", m_easeDistance);
		ser.Value("m_bActive", m_bActive);
		// FIXME: regular update is controlled by FlowGraph, too 
		if (ser.IsReading()) {
			if (m_bActive) pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
		}
		ser.EndGroup();
	}

	enum EInPorts
	{
		IN_DEST = 0,
		IN_DYN_DEST,
		IN_SPEED,
		IN_EASE,
		IN_START,
		IN_STOP
	};
	enum EOutPorts
	{
		OUT_CURRENT = 0,
		OUT_DONE_TRIGGER
	};

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>( "Destination",_HELP("Destination") ),
			InputPortConfig<bool>( "DynamicUpdate", true, _HELP("Dynamic update of Destination [follow-during-movement]"), _HELP("DynamicUpdate") ),
			InputPortConfig<float>( "Speed",_HELP("speed (m/sec)") ),
			InputPortConfig<float>( "EaseDistance",_HELP("distance at which the speed is changed (in meters)") ),
			InputPortConfig_Void( "Start", _HELP("Trigger this port to start the movement") ),
			InputPortConfig_Void( "Stop",  _HELP("Trigger this port to stop the movement") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("Current",_HELP("Current position")),
			OutputPortConfig_Void("DoneTrigger", _HELP("Triggered when destination is reached or movement stopped"), 	_HELP("Done")),
			{0}
		};
		config.sDescription = _HELP( "Move an entity to a destination position at a defined speed" );
		config.nFlags |= EFLN_TARGET_ENTITY;
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
				// update destination if dynamic update is enabled. otherwise destination is read on Start only
				if (IsPortActive(pActInfo, IN_DEST) && GetPortBool(pActInfo, IN_DYN_DEST) == true)
				{
					m_destination = GetPortVec3(pActInfo, IN_DEST);
				}
				if (IsPortActive(pActInfo, IN_SPEED))
				{
					m_topSpeed = GetPortFloat(pActInfo, IN_SPEED);
					if(m_topSpeed < 0.0f) m_topSpeed = 0.0f;
				}
				if (IsPortActive(pActInfo, IN_EASE))
				{
					m_easeDistance = GetPortFloat(pActInfo, IN_EASE);
					if(m_easeDistance < 0.0f) m_easeDistance = 0.0f;
				}
				if (IsPortActive(pActInfo, IN_START))
				{
					// start interpolation
					IEntity *pEnt = pActInfo->pEntity;
					if(pEnt) m_position = pEnt->GetPos();
					m_lastTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
					m_destination = GetPortVec3(pActInfo, IN_DEST);
					m_topSpeed = GetPortFloat(pActInfo, IN_SPEED);
					m_easeDistance = GetPortFloat(pActInfo, IN_EASE);
					if(m_topSpeed < 0.0f) m_topSpeed = 0.0f;
					if(m_easeDistance < 0.0f) m_easeDistance = 0.0f;

					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
					m_bActive = true;
				}
				if (IsPortActive(pActInfo, IN_STOP)) // Stop
				{
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
					ActivateOutput(pActInfo, OUT_DONE_TRIGGER, true); // Done Trigger
					m_bActive = false;
				}
				break;
			}

			case eFE_Initialize:
			{
				IEntity *pEnt = pActInfo->pEntity;
				if(pEnt)
				{
					m_position = pEnt->GetPos();
					m_destination = m_position;
					m_topSpeed = 0.0f;
				}

				m_lastTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
				ActivateOutput(pActInfo, OUT_CURRENT, m_position);
				pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
				m_bActive = false;
				break;
			}

			case eFE_Update:
			{
				if (!m_bActive) break;
				float time = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
				float timeDifference = time - m_lastTime; m_lastTime = time;

				// let's compute the movement vector now
				Vec3 oldPosition = m_position;
				if(m_position.IsEquivalent(m_destination))
				{
					m_position = m_destination;
					oldPosition = m_destination; // so, velocity will be 0.0
					ActivateOutput(pActInfo, OUT_DONE_TRIGGER, true);
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
					m_bActive = false;
				}
				else
				{
					Vec3 direction = m_destination - m_position;
					float distance = direction.GetLength();
					Vec3 directionAndSpeed = direction.normalized();
					if(distance < m_easeDistance)	// takes care of m_easeDistance being 0
					{
						directionAndSpeed *= distance / m_easeDistance;
					}
					directionAndSpeed *= (m_topSpeed * timeDifference) / 1000.0f;

					if(direction.GetLength() < directionAndSpeed.GetLength())
						m_position = m_destination;
					else
						m_position += directionAndSpeed;

				}
				ActivateOutput(pActInfo, OUT_CURRENT, m_position);
				IEntity *pEnt = pActInfo->pEntity;
				if (pEnt)
				{
					pEnt->SetPos(m_position);
					IPhysicalEntity * pPhysEnt = pEnt->GetPhysics();
					if (pPhysEnt)
					{
						pe_action_set_velocity setVel;
						float rTimeStep = timeDifference>1E-5f ? 1000.0f / timeDifference : 0.0f; // timeDifference is in millisecs
						setVel.v = (m_position - oldPosition) * rTimeStep;  
						pPhysEnt->Action( &setVel );
					}
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

REGISTER_FLOW_NODE( "Movement:MoveEntityTo", CMoveEntityTo );
