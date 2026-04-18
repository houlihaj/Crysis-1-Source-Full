#include "StdAfx.h"
#include "FlowBaseNode.h"

#include <IEntitySystem.h>
#include <IAISystem.h>
#include <IAgent.h>

class CFlowNode_Dynamics : public CFlowBaseNode
{
public:
	CFlowNode_Dynamics( SActivationInfo * pActInfo ) 
	{
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("v", _HELP("Velocity of entity")),
			OutputPortConfig<Vec3>("a", _HELP("Acceleration of entity")),
			OutputPortConfig<Vec3>("w", _HELP("Angular velocity of entity")),
			OutputPortConfig<Vec3>("wa", _HELP("Angular acceleration of entity")),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = 0;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Dynamic physical state of an entity");
		config.SetCategory(EFLN_APPROVED); // POLICY CHANGE: Maybe an Enable/Disable Port
	}


	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
			break;
		case eFE_Update:
			{
				IEntity * pEntity = pActInfo->pEntity;
				if (pEntity)
				{
					IPhysicalEntity * pPhysEntity = pEntity->GetPhysics();
					if (pPhysEntity)
					{
						pe_status_dynamics dyn;
						pPhysEntity->GetStatus( &dyn );
						ActivateOutput(pActInfo, 0, dyn.v);
						ActivateOutput(pActInfo, 1, dyn.a);
						ActivateOutput(pActInfo, 2, dyn.w);
						ActivateOutput(pActInfo, 3, dyn.wa);
					}
				}
			}
		}
	}
};

class CFlowNode_ActionImpulse : public CFlowBaseNode
{
public:
	CFlowNode_ActionImpulse( SActivationInfo * pActInfo ) {}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "activate", _HELP("Trigger the impulse") ),
			InputPortConfig<Vec3>( "impulse", Vec3(0,0,0), _HELP("Impulse vector (in world coordinate space)") ),
			InputPortConfig<Vec3>( "angImpulse", Vec3(0,0,0), _HELP("The angular impulse (in world coordinate space)") ),
			InputPortConfig<Vec3>( "Point", Vec3(0,0,0), _HELP("Point of application (optional - in world coordinate space)") ),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.sDescription = _HELP("Applies an impulse on an entity");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, 0))
		{
			pe_action_impulse action;
			action.impulse = GetPortVec3( pActInfo, 1 );

			Vec3 temp = GetPortVec3( pActInfo, 2 );
			if ( !temp.IsZero() )
				action.angImpulse = temp;

			temp = GetPortVec3( pActInfo, 3 );
			if ( !temp.IsZero() )
				action.point = temp;

			IEntity * pEntity = pActInfo->pEntity;
			if (pEntity)
			{
				IPhysicalEntity * pPhysEntity = pEntity->GetPhysics();
				if (pPhysEntity)
					pPhysEntity->Action( &action );
			}
		}
	}
};

class CFlowNode_Raycast : public CFlowBaseNode
{
public:
	CFlowNode_Raycast( SActivationInfo * pActInfo ) {}

	enum EInPorts
	{
		GO = 0,
		DIR,
		MAXLENGTH,
		POS,
	};
	enum EOutPorts
	{
		NOHIT = 0,
		HIT,
		DIROUT,
		DISTANCE,
		HITPOINT,
		NORMAL,
		HIT_ENTITY,
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>( "go", SFlowSystemVoid(), _HELP("Perform Raycast") ),
			InputPortConfig<Vec3>( "direction", Vec3(0,1,0), _HELP("Direction of Raycast") ),
			InputPortConfig<float>( "maxLength", 10.0f, _HELP("Maximum length of Raycast") ),
			InputPortConfig<Vec3>( "position", Vec3(0,0,0), _HELP("Ray start position, relative to entity") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<SFlowSystemVoid>( "nohit", _HELP("Triggered if NO object was hit by raycast") ),
			OutputPortConfig<SFlowSystemVoid>( "hit", _HELP("Triggered if an object was hit by raycast") ),
			OutputPortConfig<Vec3>( "direction", _HELP("Actual direction of the cast ray (transformed by entity rotation")),
			OutputPortConfig<float>( "distance", _HELP("Distance to object hit") ),
			OutputPortConfig<Vec3>( "hitpoint", _HELP("Position the ray hit") ),
			OutputPortConfig<Vec3>( "normal", _HELP("Normal of the surface at the hitpoint") ),
			OutputPortConfig<EntityId> ( "entity", _HELP("Entity which was hit")), 
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, GO))
		{
			IEntity * pEntity = pActInfo->pEntity;
			if (pEntity)
			{
				ray_hit hit;
				IPhysicalEntity *pSkip = pEntity->GetPhysics();
				Vec3 direction = pEntity->GetWorldTM().TransformVector( GetPortVec3(pActInfo, DIR).GetNormalized() );
				IPhysicalWorld * pWorld = gEnv->pPhysicalWorld;
				int numHits = pWorld->RayWorldIntersection( 
					pEntity->GetPos() + GetPortVec3(pActInfo, POS),
					direction * GetPortFloat(pActInfo, MAXLENGTH),
					ent_all,
					rwi_stop_at_pierceable|rwi_colltype_any,
					&hit, 1, 
					&pSkip, 1 );
					
				if (numHits)
				{
					pEntity = (IEntity*)hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);    
					ActivateOutput( pActInfo, HIT,(bool)true );
					ActivateOutput( pActInfo, DIROUT, direction );
					ActivateOutput( pActInfo, DISTANCE, hit.dist );
					ActivateOutput( pActInfo, HITPOINT, hit.pt );
					ActivateOutput( pActInfo, NORMAL, hit.n );
					ActivateOutput( pActInfo, HIT_ENTITY, pEntity ? pEntity->GetId() : 0);
				}
				else
					ActivateOutput( pActInfo, NOHIT, false);
			}
		}
	}
};

class CFlowNode_RaycastCamera : public CFlowBaseNode
{
public:
	CFlowNode_RaycastCamera( SActivationInfo * pActInfo ) {}

	enum EInPorts
	{
		GO = 0,
		POS,
		MAXLENGTH,
	};
	enum EOutPorts
	{
		NOHIT = 0,
		HIT,
		DIROUT,
		DISTANCE,
		HITPOINT,
		NORMAL,
		HIT_ENTITY,
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>( "go", SFlowSystemVoid(), _HELP("Perform Raycast") ),
			InputPortConfig<Vec3>( "offset", Vec3(0,0,0), _HELP("Ray start position, relative to camera") ),
			InputPortConfig<float>( "maxLength", 10.0f, _HELP("Maximum length of Raycast") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<SFlowSystemVoid>( "nohit", _HELP("Triggered if NO object was hit by raycast") ),
			OutputPortConfig<SFlowSystemVoid>( "hit", _HELP("Triggered if an object was hit by raycast") ),
			OutputPortConfig<Vec3>( "direction", _HELP("Actual direction of the cast ray (transformed by entity rotation")),
			OutputPortConfig<float>( "distance", _HELP("Distance to object hit") ),
			OutputPortConfig<Vec3>( "hitpoint", _HELP("Position the ray hit") ),
			OutputPortConfig<Vec3>( "normal", _HELP("Normal of the surface at the hitpoint") ),
			OutputPortConfig<EntityId> ( "entity", _HELP("Entity which was hit")), 
			{0}
		};
		config.sDescription = _HELP("Perform a raycast relative to the camera");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, GO))
		{
			IEntity * pEntity = pActInfo->pEntity;
			// if (pEntity)
			{
				ray_hit hit;
				CCamera& cam = GetISystem()->GetViewCamera();
				Vec3 pos = cam.GetPosition()+cam.GetViewdir();
				Vec3 direction = cam.GetViewdir();
				IPhysicalWorld * pWorld = gEnv->pPhysicalWorld;
				IPhysicalEntity *pSkip = 0; // pEntity->GetPhysics();
				int numHits = pWorld->RayWorldIntersection( 
				pos + GetPortVec3(pActInfo, POS),
				direction * GetPortFloat(pActInfo, MAXLENGTH),
				ent_all,
				rwi_stop_at_pierceable|rwi_colltype_any,
				&hit, 1
				/* ,&pSkip, 1 */  );
				if (numHits)
				{
					pEntity = (IEntity*)hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);    
					ActivateOutput( pActInfo, HIT,(bool)true );
					ActivateOutput( pActInfo, DIROUT, direction );
					ActivateOutput( pActInfo, DISTANCE, hit.dist );
					ActivateOutput( pActInfo, HITPOINT, hit.pt );
					ActivateOutput( pActInfo, NORMAL, hit.n );
					ActivateOutput( pActInfo, HIT_ENTITY, pEntity ? pEntity->GetId() : 0);
				}
				else
					ActivateOutput( pActInfo, NOHIT, false );
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// Enable/Disable AI for an entity/
//////////////////////////////////////////////////////////////////////////
class CFlowNode_PhysicsEnable : public CFlowBaseNode
{
public:
	enum EInputs {
		IN_ENABLE,
		IN_DISABLE,
		IN_ENABLE_AI,
		IN_DISABLE_AI,
	};
	CFlowNode_PhysicsEnable( SActivationInfo * pActInfo ) {};

	/*
	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return this;
	}
	*/

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "Enable",_HELP("Enable Physics for target entity") ),
			InputPortConfig_Void( "Disable",_HELP("Disable Physics for target entity") ),
			InputPortConfig_Void( "AI_Enable",_HELP("Enable AI for target entity") ),
			InputPortConfig_Void( "AI_Disable",_HELP("Disable AI for target entity") ),
			{0}
		};
		config.sDescription = _HELP( "Enables/Disables Physics" );
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_ADVANCED);
	}
	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (!pActInfo->pEntity)
					return;

				if(IsPortActive(pActInfo, IN_ENABLE))
				{
					IEntityPhysicalProxy *pPhysicalProxy = (IEntityPhysicalProxy*)pActInfo->pEntity->GetProxy(ENTITY_PROXY_PHYSICS);
					if (pPhysicalProxy)
						pPhysicalProxy->EnablePhysics(true);
				}
				if(IsPortActive(pActInfo, IN_DISABLE))
				{
					IEntityPhysicalProxy *pPhysicalProxy = (IEntityPhysicalProxy*)pActInfo->pEntity->GetProxy(ENTITY_PROXY_PHYSICS);
					if (pPhysicalProxy)
						pPhysicalProxy->EnablePhysics(false);
				}
	
				if(IsPortActive(pActInfo, IN_ENABLE_AI))
				{
					if (pActInfo->pEntity->GetAI())
						pActInfo->pEntity->GetAI()->Event(AIEVENT_ENABLE,0);
				}
				if(IsPortActive(pActInfo, IN_DISABLE_AI))
				{
					if (pActInfo->pEntity->GetAI())
						pActInfo->pEntity->GetAI()->Event(AIEVENT_DISABLE,0);
				}
				break;
			}

		case eFE_Initialize:
			break;
		};
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE_SINGLETON( "Physics:Dynamics", CFlowNode_Dynamics );
REGISTER_FLOW_NODE_SINGLETON( "Physics:ActionImpulse", CFlowNode_ActionImpulse );
REGISTER_FLOW_NODE_SINGLETON( "Physics:RayCast", CFlowNode_Raycast );
REGISTER_FLOW_NODE_SINGLETON( "Physics:RayCastCamera", CFlowNode_RaycastCamera );
REGISTER_FLOW_NODE_SINGLETON( "Physics:PhysicsEnable", CFlowNode_PhysicsEnable );
