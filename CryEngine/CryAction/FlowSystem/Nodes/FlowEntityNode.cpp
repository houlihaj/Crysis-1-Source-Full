////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowEntityNode.h
//  Version:     v1.00
//  Created:     23/5/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowEntityNode.h"

#include <IActorSystem.h>
#include "CryAction.h"


//////////////////////////////////////////////////////////////////////////
CFlowEntityClass::CFlowEntityClass( IEntityClass *pEntityClass )
{
	m_nRefCount = 0;
	//m_classname = pEntityClass->GetName();
	m_pEntityClass = pEntityClass;
}

//////////////////////////////////////////////////////////////////////////
CFlowEntityClass::~CFlowEntityClass()
{
	for (int i = 0; i < m_inputs.size(); i++)
	{
		FreeStr( m_inputs[i].name );
	//	FreeStr( m_inputs[i].description );
	}
	for (int i = 0; i < m_outputs.size(); i++)
	{
		FreeStr( m_outputs[i].name );
	//	FreeStr( m_outputs[i].description );
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityClass::FreeStr( const char *src )
{
	if (src)
		delete []src;
}

//////////////////////////////////////////////////////////////////////////
const char* CFlowEntityClass::CopyStr( const char *src )
{
	char *dst = 0;
	if (src)
	{
		int len = strlen(src);
		dst = new char[len+1];
		strcpy(dst,src);
	}
	return dst;
}

//////////////////////////////////////////////////////////////////////////
void MakeHumanName(SInputPortConfig& config)
{
	char* name = strchr((char*)config.name, '_');
	if (name != 0)
		config.humanName = name+1;
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityClass::GetInputsOutputs( IEntityClass *pEntityClass )
{
	int nEvents = pEntityClass->GetEventCount();
	for (int i = 0; i < nEvents; i++)
	{
		IEntityClass::SEventInfo event = pEntityClass->GetEventInfo(i);
		if (event.bOutput)
		{
			// Output
			switch (event.type)
			{
				case IEntityClass::EVT_BOOL:
					m_outputs.push_back( OutputPortConfig<bool>(CopyStr(event.name)) );
					break;
				case IEntityClass::EVT_INT:
					m_outputs.push_back( OutputPortConfig<int>(CopyStr(event.name)) );
					break;
				case IEntityClass::EVT_FLOAT:
					m_outputs.push_back( OutputPortConfig<float>(CopyStr(event.name)) );
					break;
				case IEntityClass::EVT_VECTOR:
					m_outputs.push_back( OutputPortConfig<Vec3>(CopyStr(event.name)) );
					break;
				case IEntityClass::EVT_ENTITY:
					m_outputs.push_back( OutputPortConfig<EntityId>(CopyStr(event.name)) );
					break;
				case IEntityClass::EVT_STRING:
					m_outputs.push_back( OutputPortConfig<string>(CopyStr(event.name)) );
					break;
			}
		}
		else
		{
			// Input
			switch (event.type)
			{
			case IEntityClass::EVT_BOOL:
				m_inputs.push_back( InputPortConfig<bool>(CopyStr(event.name)) );
				MakeHumanName(m_inputs.back());
				break;
			case IEntityClass::EVT_INT:
				m_inputs.push_back( InputPortConfig<int>(CopyStr(event.name)) );
				MakeHumanName(m_inputs.back());
				break;
			case IEntityClass::EVT_FLOAT:
				m_inputs.push_back( InputPortConfig<float>(CopyStr(event.name)) );
				MakeHumanName(m_inputs.back());
				break;
			case IEntityClass::EVT_VECTOR:
				m_inputs.push_back( InputPortConfig<Vec3>(CopyStr(event.name)) );
				MakeHumanName(m_inputs.back());
				break;
			case IEntityClass::EVT_ENTITY:
				m_inputs.push_back( InputPortConfig<EntityId>(CopyStr(event.name)) );
				MakeHumanName(m_inputs.back());
				break;
			case IEntityClass::EVT_STRING:
				m_inputs.push_back( InputPortConfig<string>(CopyStr(event.name)) );
				MakeHumanName(m_inputs.back());
				break;
			}
		}
	}
	m_outputs.push_back( OutputPortConfig<bool>(0,0) );
	m_inputs.push_back( InputPortConfig<bool>(0,false) );
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityClass::GetConfiguration( SFlowNodeConfig& config )
{
	if (m_inputs.empty() && m_outputs.empty())
		GetInputsOutputs( m_pEntityClass );

	static const SInputPortConfig in_config[] = {
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		{0}
	};

	config.nFlags |= EFLN_TARGET_ENTITY|EFLN_HIDE_UI;
	config.SetCategory(EFLN_APPROVED); // all Entity flownodes are approved!

	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;

	if (!m_inputs.empty())
		config.pInputPorts = &m_inputs[0];

	if (!m_outputs.empty())
		config.pOutputPorts = &m_outputs[0];

}

//////////////////////////////////////////////////////////////////////////
IFlowNodePtr CFlowEntityClass::Create( IFlowNode::SActivationInfo* pActInfo )
{
	return new CFlowEntityNode( this,pActInfo );
}

//////////////////////////////////////////////////////////////////////////
// CFlowEntityNode implementation.
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CFlowEntityNode::CFlowEntityNode( CFlowEntityClass *pClass,SActivationInfo * pActInfo )
{
	m_event = ENTITY_EVENT_SCRIPT_EVENT;
	m_pClass = pClass;
	//pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
//	m_entityId = (EntityId)pActInfo->m_pUserData;

	m_nodeID = pActInfo->myID;
	m_pGraph = pActInfo->pGraph;
	m_lastInitializeFrameId = -1;
}

IFlowNodePtr CFlowEntityNode::Clone( SActivationInfo *pActInfo )
{
	IFlowNode *pNode = new CFlowEntityNode( m_pClass,pActInfo );
	return pNode;
};

//////////////////////////////////////////////////////////////////////////
void CFlowEntityNode::GetConfiguration( SFlowNodeConfig& config )
{
	m_pClass->GetConfiguration( config );
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityNode::ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	CFlowEntityNodeBase::ProcessEvent( event, pActInfo );

	switch (event)
	{
	case eFE_Activate:
		{
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_entityId);
			if (!pEntity)
				return;

			IEntityScriptProxy* pScriptProxy = (IEntityScriptProxy*)pEntity->GetProxy(ENTITY_PROXY_SCRIPT);
			if (!pScriptProxy)
				return;

			// Check active ports, and send event to entity.
			for (int i = 0; i < m_pClass->m_inputs.size()-1; i++)
			{
				if (IsPortActive(pActInfo,i))
				{
					EFlowDataTypes type = GetPortType(pActInfo,i);
					switch (type)
					{
						case eFDT_Int:
							pScriptProxy->CallEvent( m_pClass->m_inputs[i].name,(float)GetPortInt(pActInfo,i) );
							break;
						case eFDT_Float:
							pScriptProxy->CallEvent( m_pClass->m_inputs[i].name,GetPortFloat(pActInfo,i) );
							break;
						case eFDT_Bool:
							pScriptProxy->CallEvent( m_pClass->m_inputs[i].name,GetPortBool(pActInfo,i) );
							break;
						case eFDT_Vec3:
							pScriptProxy->CallEvent( m_pClass->m_inputs[i].name,GetPortVec3(pActInfo,i) );
							break;
						case eFDT_EntityId:
							pScriptProxy->CallEvent( m_pClass->m_inputs[i].name,GetPortEntityId(pActInfo,i) );
							break;
						case eFDT_String:
							pScriptProxy->CallEvent( m_pClass->m_inputs[i].name,GetPortString(pActInfo,i).c_str() );
							break;
					}
				}
			}
		}
		break;
	case eFE_Initialize:
		const int frameId = gEnv->pRenderer->GetFrameID(false);
		if (frameId != m_lastInitializeFrameId)
		{
			m_lastInitializeFrameId = frameId;
			for (size_t i=0; i<m_pClass->GetNumOutputs(); i++)
			{
				ActivateOutput( pActInfo, i, SFlowSystemVoid() );
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFlowEntityNode::SerializeXML( SActivationInfo *, const XmlNodeRef& root, bool reading )
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityNode::Serialize( SActivationInfo *, TSerialize ser )
{
	if (ser.IsReading())
		m_lastInitializeFrameId = -1;
}

//////////////////////////////////////////////////////////////////////////
void CFlowEntityNode::OnEntityEvent( IEntity *pEntity,SEntityEvent &event )
{
	if ( !m_pGraph->IsEnabled() || m_pGraph->IsSuspended() || !m_pGraph->IsActive() )
		return;

	switch (event.event)
	{
	case ENTITY_EVENT_DONE:
		//gEnv->pLog->Log(">>>>>> Entity event done: owner %d requester %d", m_pGraph->GetEntityId(m_nodeID), pEntity->GetId());

		if (gEnv->pSystem->IsSerializingFile() && pEntity && pEntity->GetId() == m_pGraph->GetEntityId(m_nodeID))
		m_pGraph->SetEntityId(m_nodeID, 0);
		break;

	case ENTITY_EVENT_SCRIPT_EVENT:
		{
			const char *sEvent = (const char*)event.nParam[0];
			// Find output port.
			for (int i = 0; i < m_pClass->m_outputs.size(); i++)
			{
				if (!m_pClass->m_outputs[i].name)
					break;
				if (strcmp(sEvent,m_pClass->m_outputs[i].name) == 0)
				{
					SFlowAddress addr( m_nodeID, i, true );
					switch (event.nParam[1])
					{
					case IEntityClass::EVT_INT:
						m_pGraph->ActivatePort( addr,*(int*)event.nParam[2] );
						break;
					case IEntityClass::EVT_FLOAT:
						m_pGraph->ActivatePort( addr,*(float*)event.nParam[2] );
						break;
					case IEntityClass::EVT_BOOL:
						m_pGraph->ActivatePort( addr,*(bool*)event.nParam[2] );
						break;
					case IEntityClass::EVT_VECTOR:
						m_pGraph->ActivatePort( addr,*(Vec3*)event.nParam[2] );
						break;
					case IEntityClass::EVT_ENTITY:
						m_pGraph->ActivatePort( addr,*(EntityId*)event.nParam[2] );
						break;
					case IEntityClass::EVT_STRING:
						{
							const char *str = (const char*)event.nParam[2];
							m_pGraph->ActivatePort( addr,string(str) );
						}
						break;
					}
					//m_pGraph->ActivatePort( addr,(bool)true );
					break;
				}
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// Flow node for entity position.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityPos : public CFlowEntityNodeBase
{
public:
	enum EInputs
	{
		IN_POS,
		IN_ROTATE,
		IN_SCALE,
	};
	enum EOutputs
	{
		OUT_POS,
		OUT_ROTATE,
		OUT_SCALE,
		OUT_YDIR,
		OUT_XDIR,
		OUT_ZDIR,
	};
	CFlowNode_EntityPos( SActivationInfo *pActInfo ) : CFlowEntityNodeBase()
	{
		m_event = ENTITY_EVENT_XFORM;
		m_nodeID = pActInfo->myID;
		m_pGraph = pActInfo->pGraph;
		m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
	}

	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo ) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntityPos(pActInfo); };
	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>( "pos",_HELP("Entity position vector") ),
			InputPortConfig<Vec3>( "rotate",_HELP("Entity rotation angle in degrees") ),
			InputPortConfig<Vec3>( "scale",_HELP("Entity scale vector") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>( "pos",_HELP("Entity position vector") ),
			OutputPortConfig<Vec3>( "rotate",_HELP("Entity rotation angle in degrees") ),
			OutputPortConfig<Vec3>( "scale",_HELP("Entity scale vector") ),
			OutputPortConfig<Vec3>( "fwdDir",_HELP("Entity direction vector - Y axis") ),
			OutputPortConfig<Vec3>( "rightDir",_HELP("Entity direction vector - X axis") ),
			OutputPortConfig<Vec3>( "upDir",_HELP("Entity direction vector - Z axis") ),
			{0}
		};
		config.sDescription = _HELP("Entity Position/Rotation/Scale");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		CFlowEntityNodeBase::ProcessEvent( event, pActInfo );

		IEntity *pEntity = GetEntity();
		if (!pEntity)
			return;

		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo,IN_POS))
				{
					const Vec3 *v = pActInfo->pInputPorts[IN_POS].GetPtr<Vec3>();
					Matrix34 tm = pEntity->GetWorldTM();
					tm.SetTranslation(*v);
					pEntity->SetWorldTM(tm);
				}
				if (IsPortActive(pActInfo,IN_ROTATE))
				{
					const Vec3 *v = pActInfo->pInputPorts[IN_ROTATE].GetPtr<Vec3>();
					Matrix34 tm = Matrix33( Quat::CreateRotationXYZ( Ang3(DEG2RAD(*v)) ) );
					tm.SetTranslation( pEntity->GetWorldPos() );
					pEntity->SetWorldTM(tm);
				}
				if (IsPortActive(pActInfo,IN_SCALE))
				{
					const Vec3 *v = pActInfo->pInputPorts[IN_SCALE].GetPtr<Vec3>();
					Vec3 scale = *v;
					if (scale.x == 0) scale.x = 1.0f;
					if (scale.y == 0) scale.y = 1.0f;
					if (scale.z == 0) scale.z = 1.0f;
					pEntity->SetScale( scale );
				}
			}
			break;
		case eFE_Initialize:
		case eFE_SetEntityId:
			{
				ActivateOutput(pActInfo, OUT_POS, pEntity->GetWorldPos());
				ActivateOutput(pActInfo, OUT_ROTATE, Vec3(RAD2DEG(Ang3(pEntity->GetWorldAngles()))));
				ActivateOutput(pActInfo, OUT_SCALE, pEntity->GetScale());

				const Matrix34& mat = pEntity->GetWorldTM();
				ActivateOutput( pActInfo, OUT_YDIR, mat.GetColumn1()); // forward -> mat.TransformVector(Vec3(0,1,0)) );
				ActivateOutput( pActInfo, OUT_XDIR, mat.GetColumn0()); // right   -> mat.TransformVector(Vec3(1,0,0)) );
				ActivateOutput( pActInfo, OUT_ZDIR, mat.GetColumn2()); // up      -> mat.TransformVector(Vec3(0,0,1)) );
			}
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// IEntityEventListener
	virtual void OnEntityEvent( IEntity *pEntity,SEntityEvent &event )
	{
		if ( !m_pGraph->IsEnabled() || m_pGraph->IsSuspended() || !m_pGraph->IsActive() )
			return;

		switch (event.event)
		{
		case ENTITY_EVENT_XFORM:
			{
				IEntity *pEntity = GetEntity();
				if (!pEntity)
					return;
				
				bool bAny = (event.nParam[0] & (ENTITY_XFORM_POS|ENTITY_XFORM_ROT|ENTITY_XFORM_SCL)) != 0;
				if (!bAny || (event.nParam[0] & ENTITY_XFORM_POS))
				{
					SFlowAddress addr( m_nodeID, OUT_POS, true );
					m_pGraph->ActivatePort( addr, Vec3(pEntity->GetWorldPos()) );
				}
				if (!bAny || (event.nParam[0] & ENTITY_XFORM_ROT))
				{
					SFlowAddress addr( m_nodeID, OUT_ROTATE, true );
					m_pGraph->ActivatePort( addr, Vec3(RAD2DEG(Ang3(pEntity->GetWorldAngles()))) );
					// need to update fwd/right/up only when rotation changed
					const Matrix34& mat = pEntity->GetWorldTM();
					addr.port = OUT_YDIR;
					m_pGraph->ActivatePort( addr, mat.GetColumn1());
					addr.port = OUT_XDIR;
					m_pGraph->ActivatePort( addr, mat.GetColumn0());
					addr.port = OUT_ZDIR;
					m_pGraph->ActivatePort( addr, mat.GetColumn2());
				}
				if (!bAny || (event.nParam[0] & ENTITY_XFORM_SCL))
				{
					SFlowAddress addr( m_nodeID, OUT_SCALE, true );
					m_pGraph->ActivatePort( addr, Vec3(pEntity->GetScale()) );
				}
			}
			break;
		}
	};
	//////////////////////////////////////////////////////////////////////////

protected:
	IFlowGraph *m_pGraph;
	TFlowNodeId m_nodeID;
};

//////////////////////////////////////////////////////////////////////////
// Flow node for entity position.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityGetPos : public CFlowBaseNode
{
public:
	enum EInputs
	{
		IN_GET,
	};
	enum EOutputs
	{
		OUT_POS = 0,
		OUT_ROTATE,
		OUT_SCALE,
		OUT_YDIR,
		OUT_XDIR,
		OUT_ZDIR,
	};

	CFlowNode_EntityGetPos( SActivationInfo *pActInfo ) : CFlowBaseNode()
	{
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "Get",_HELP("Trigger to get current values") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>( "Pos",_HELP("Entity position vector") ),
			OutputPortConfig<Vec3>( "Rotate",_HELP("Entity rotation angle in degrees") ),
			OutputPortConfig<Vec3>( "Scale",_HELP("Entity scale vector") ),
			OutputPortConfig<Vec3>( "FwdDir",_HELP("Entity direction vector - Y axis") ),
			OutputPortConfig<Vec3>( "RightDir",_HELP("Entity direction vector - X axis") ),
			OutputPortConfig<Vec3>( "UpDir",_HELP("Entity direction vector - Z axis") ),
			{0}
		};
		config.sDescription = _HELP("Get Entity Position/Rotation/Scale");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate || IsPortActive(pActInfo, IN_GET) == false)
			return;
		// only if IN_GET is activated.

		IEntity *pEntity = pActInfo->pEntity;
		if (pEntity == 0)
			return;

		ActivateOutput(pActInfo, OUT_POS, pEntity->GetWorldPos());
		ActivateOutput(pActInfo, OUT_ROTATE, Vec3(RAD2DEG(Ang3(pEntity->GetWorldAngles()))));
		ActivateOutput(pActInfo, OUT_SCALE, pEntity->GetScale());

		const Matrix34& mat = pEntity->GetWorldTM();
		ActivateOutput( pActInfo, OUT_YDIR, mat.GetColumn1()); // forward -> mat.TransformVector(Vec3(0,1,0)) );
		ActivateOutput( pActInfo, OUT_XDIR, mat.GetColumn0()); // right   -> mat.TransformVector(Vec3(1,0,0)) );
		ActivateOutput( pActInfo, OUT_ZDIR, mat.GetColumn2()); // up      -> mat.TransformVector(Vec3(0,0,1)) );
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

};

//////////////////////////////////////////////////////////////////////////
// Flow node for entity material.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityMaterial : public CFlowEntityNodeBase
{
public:
	enum EInputs
	{
		IN_MATERIAL,
	};
	CFlowNode_EntityMaterial( SActivationInfo *pActInfo ) : CFlowEntityNodeBase()
	{
		m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
		// m_event = ENTITY_EVENT_MATERIAL;
	}

	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo ) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntityMaterial(pActInfo); };
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		/*
		if (ser.IsReading())
		{
			bool bChangeMat = false;
			ser.Value("changeMat", bChangeMat);
			IEntity* pEntity = GetEntity();
			if (pEntity && bChangeMat)
			{
				ChangeMat(pActInfo, pEntity);
			}
			else
				m_pMaterial = 0;
		}
		else
		{
			bool bChangeMat = m_pMaterial != 0;
			ser.Value("changeMat", bChangeMat);
		}
		*/
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>( "material",_HELP("Name of material to apply") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			{0}
		};
		config.sDescription = _HELP("Apply material to the entity");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		CFlowEntityNodeBase::ProcessEvent( event, pActInfo );

		IEntity *pEntity = GetEntity();
		if (!pEntity)
			return;

		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo,IN_MATERIAL))
				{
					ChangeMat(pActInfo, pEntity);
				}
			}
			break;
		}
	}

	void ChangeMat(SActivationInfo *pActInfo, IEntity* pEntity)
	{
		const string &mtlName = GetPortString(pActInfo,IN_MATERIAL);
		_smart_ptr<IMaterial> pMtl = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(mtlName.c_str());
		if (pMtl != NULL)
		{
			pEntity->SetMaterial( pMtl );
		}
	}

	void OnEntityEvent( IEntity *pEntity,SEntityEvent &event )
	{
		/*
		if (event.event == ENTITY_EVENT_MATERIAL && pEntity && pEntity->GetId() == m_entityId)
		{
			IMaterial* pMat = (IMaterial*)event.nParam[0];
			if (pMat == 0 || (m_pMaterial != 0 && pMat != m_pMaterial))
				m_pMaterial = 0;
		}
		*/
	}

	// _smart_ptr<IMaterial> m_pMaterial;
	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_BroadcastEntityEvent : public CFlowBaseNode
{
public:
	enum EInputs
	{
		IN_SEND,
		IN_EVENT,
		IN_NAME,
	};
	CFlowNode_BroadcastEntityEvent( SActivationInfo *pActInfo ) {}
	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>( "send",_HELP("When signal recieved on this input entity event is broadcasted") ),
			InputPortConfig<string>( "event",_HELP("Entity event to be sent") ),
			InputPortConfig<string>( "name",_HELP("Any part of the entity name to receive event") ),
			{0}
		};
		config.sDescription = _HELP("This node broadcasts specified event to all entities which names contain sub string in parameter name");
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate)
			return;

		if (IsBoolPortActive(pActInfo,IN_SEND))
		{
			const string& sEvent = GetPortString( pActInfo,IN_EVENT );
			const string& sSubName = GetPortString( pActInfo,IN_NAME );
			IEntityItPtr pEntityIterator = gEnv->pEntitySystem->GetEntityIterator();
			pEntityIterator->MoveFirst();
			IEntity *pEntity = 0;
			while (pEntity = pEntityIterator->Next())
			{
				if (strstr(pEntity->GetName(),sSubName) != 0)
				{
					IEntityScriptProxy* pScriptProxy = (IEntityScriptProxy*)pEntity->GetProxy(ENTITY_PROXY_SCRIPT);
					if (pScriptProxy)
						pScriptProxy->CallEvent( sEvent );
				}
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for entity ID.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityId : public CFlowEntityNodeBase
{
public:
	CFlowNode_EntityId( SActivationInfo *pActInfo ) : CFlowEntityNodeBase()
	{
		m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
	}
	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo ) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntityId(pActInfo); };
	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>( "Id",_HELP("Entity ID") ),
			{0}
		};
		config.sDescription = _HELP("Entity ID");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = 0;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void OnEntityEvent( IEntity *pEntity,SEntityEvent &event ) {}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		CFlowEntityNodeBase::ProcessEvent( event, pActInfo );

		IEntity *pEntity = GetEntity();
		if (!pEntity)
			return;
		if( event == eFE_SetEntityId )
		{
			ActivateOutput(pActInfo, 0, pEntity->GetId());
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// Flow node for getting parent's entity ID.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_ParentId : public CFlowBaseNode
{
public:
	CFlowNode_ParentId( SActivationInfo *pActInfo ) : CFlowBaseNode() {}
	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>( "parentId",_HELP("Entity ID of the parent entity") ),
			{0}
		};
		config.sDescription = _HELP("Parent entity ID");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = 0;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if ( event != eFE_SetEntityId )
			return;
		IEntity* pEntity = pActInfo->pEntity;
		if ( !pEntity )
			return;
		IEntity* pParentEntity = pEntity->GetParent();
		if ( !pParentEntity )
			return;
		ActivateOutput( pActInfo, 0, pParentEntity->GetId() );
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for setting an entity property
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntitySetProperty : public CFlowEntityNodeBase
{
public:
	enum EInputs
	{
		IN_ACTIVATE,
		IN_NAME,
		IN_FLOAT,
		IN_PERARCHETYPE,
	};
	CFlowNode_EntitySetProperty( SActivationInfo *pActInfo ) : CFlowEntityNodeBase()
	{
		m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
	}
	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo ) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntitySetProperty(pActInfo); };
	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>( "activate",_HELP("Trigger it to set the property.") ),
			InputPortConfig<string>( "name",_HELP("Property Name") ),
			InputPortConfig<float>( "number",_HELP("Property Float Value") ),
			InputPortConfig<bool>( "perArchetype",true,_HELP("False: property is a per instance property True: property is a per archetype property.") ),
			{0}
		};
		config.sDescription = _HELP("Change entity property value");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}

	SmartScriptTable DoResolveScriptTable(IScriptTable* pTable, const string& key, string& outKey)
	{
		ScriptAnyValue value;
		
		string token;
		int pos = 0;
		token = key.Tokenize(".", pos);
		pTable->GetValueAny(token, value);

		token = key.Tokenize(".", pos);
		if (token.empty())
			return 0;

		string nextToken = key.Tokenize(".", pos);
		while (nextToken.empty() == false)
		{
			if (value.type != ANY_TTABLE)
				return 0;
			ScriptAnyValue temp;
			value.table->GetValueAny(token, temp);
			value = temp;
			token = nextToken;
			nextToken = token.Tokenize(".", pos);
		}
		outKey = token;
		return value.table;
	}

	SmartScriptTable ResolveScriptTable(IScriptTable* pTable, const char* sKey, bool bPerArchetype, string& outKey)
	{
		string key = bPerArchetype ? "Properties." : "PropertiesInstance.";
		key += sKey;
		return DoResolveScriptTable(pTable, key, outKey);
	}

	void OnEntityEvent( IEntity *pEntity,SEntityEvent &event ) {}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		CFlowEntityNodeBase::ProcessEvent( event, pActInfo );

		IEntity *pEntity = GetEntity();
		if (!pEntity)
			return;
		switch (event)
		{
		case eFE_Activate:
			{
				bool bValuePort=IsPortActive(pActInfo,IN_FLOAT);
				bool bTriggerPort=IsPortActive(pActInfo, IN_ACTIVATE);
				if (bValuePort || bTriggerPort )
				{
					bool bPerArchetype=GetPortBool(pActInfo,IN_PERARCHETYPE);

					IScriptTable *pTable = pEntity->GetScriptTable();
					if (!pTable)
						return;

					SmartScriptTable propTable;
					const char *sKey = GetPortString(pActInfo,IN_NAME).c_str();
					string realKey;
					propTable = ResolveScriptTable(pTable, sKey, bPerArchetype, realKey);
					if (!propTable)
					{
						GameWarning("[flow] CFlowNode_EntitySetProperty: Cannot resolve property '%s' in entity '%s'",
							sKey, pEntity->GetName());
						return;
					}
					sKey = realKey.c_str();
					int scriptVarType = propTable->GetValueType( sKey );
					if (scriptVarType == svtNull)
					{
						GameWarning("[flow] CFlowNode_EntitySetProperty: Cannot resolve property '%s' in entity '%s' -> Creating",
							sKey, pEntity->GetName());
					}
					if (scriptVarType == svtNumber || scriptVarType == svtBool)
					{
						float fValue = GetPortFloat(pActInfo,IN_FLOAT);
						bool bVal=(bool)(fabs(fValue)>0.001f);

#if 0
						gEnv->pLog->Log("CFlowNode_EntitySetProperty(%s):(%s): %s %s: %f (%c) Ports:%c%c\n",
							(scriptVarType != svtBool) ? "float" : "bool",pEntity->GetName(), (!bPerArchetype) ? "Instance Properties" : "Archetype Properties",
							sKey,fValue, bVal ? 'T' : 'F', bValuePort ? 'V' : '-', bTriggerPort ? 'S' : '-' );
#endif

						if (scriptVarType != svtBool)
							propTable->SetValue( sKey,fValue );
						else
							propTable->SetValue( sKey,bVal );
							//propTable->SetValue( sKey,(bool)(fabs(fValue)>0.001f) );
						Script::CallMethod( pTable,"OnPropertyChange" );
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
	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// Flow node for setting an entity property
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityGetProperty : public CFlowEntityNodeBase
{
public:
	enum EInputs
	{
		IN_ACTIVATE,
		IN_NAME,
	};
	CFlowNode_EntityGetProperty( SActivationInfo *pActInfo ) : CFlowEntityNodeBase()
	{
		m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
	}
	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo ) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntityGetProperty(pActInfo); };
	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>( "activate",_HELP("Trigger it to get the property!") ),
			InputPortConfig<string>( "name",_HELP("Property Name") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType( "value",_HELP("Value of the property") ),
			{0}
		};
		config.sDescription = _HELP("Retrieve entity property value");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void OnEntityEvent( IEntity *pEntity,SEntityEvent &event ) {}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		CFlowEntityNodeBase::ProcessEvent( event, pActInfo );

		IEntity *pEntity = GetEntity();
		if (!pEntity)
			return;
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo,IN_NAME) || IsPortActive(pActInfo, IN_ACTIVATE))
				{
					IScriptTable *pTable = pEntity->GetScriptTable();
					if (!pTable)
						return;

					SmartScriptTable propTable;
					if (!pTable->GetValue( "Properties",propTable ))
						return;

					const char *sKey = GetPortString(pActInfo,IN_NAME).c_str();
					int scriptVarType = propTable->GetValueType( sKey );
					if (scriptVarType == svtNumber || scriptVarType == svtBool)
					{
						float fValue;
						propTable->GetValue( sKey, fValue);

						if (scriptVarType != svtBool)
							ActivateOutput( pActInfo, 0, fValue );
						else
							ActivateOutput( pActInfo, 0, (bool)(fabs(fValue)>0.001f) );
					}
					if (scriptVarType == svtString)
					{
						const char* sValue;
						propTable->GetValue( sKey, sValue);
						ActivateOutput( pActInfo, 0, string(sValue));
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

	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// Flow node for Attaching child to parent.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityAttachChild : public CFlowBaseNode
{
public:
	CFlowNode_EntityAttachChild( SActivationInfo *pActInfo ) : CFlowBaseNode() {}
	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "Attach",_HELP("Trigger to attach entity") ),
			InputPortConfig<EntityId>( "Child",_HELP("Child Entity to Attach") ),
			InputPortConfig<bool>( "KeepTransform",_HELP("Child entity will remain in the same transformation in world space") ),
			InputPortConfig<bool>( "DisablePhysics", false, _HELP("Force disable physics of child entity on attaching") ),
			{0}
		};
		config.sDescription = _HELP("Attach Child Entity");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if ( event != eFE_Activate )
			return;
		if (!pActInfo->pEntity)
			return;
		if (IsPortActive(pActInfo,0))
		{
			EntityId nChildId = GetPortEntityId(pActInfo,1);
			IEntity *pChild = gEnv->pEntitySystem->GetEntity(nChildId);
			if (pChild)
			{
				int nFlags = 0;
				if (GetPortBool(pActInfo,2))
					nFlags |= IEntity::ATTACHMENT_KEEP_TRANSFORMATION;
				if (GetPortBool(pActInfo,3))
				{
					IEntityPhysicalProxy *pPhysicsProxy = (IEntityPhysicalProxy*) pChild->GetProxy(ENTITY_PROXY_PHYSICS);
					if (pPhysicsProxy)
						pPhysicsProxy->EnablePhysics(false);
				}
				pActInfo->pEntity->AttachChild( pChild,nFlags );
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for detaching child from parent.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityDetachThis : public CFlowBaseNode
{
public:
	CFlowNode_EntityDetachThis( SActivationInfo *pActInfo ) : CFlowBaseNode() {}
	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "Detach",_HELP("Trigger to detach entity from parent") ),
			InputPortConfig<bool>( "KeepTransform",_HELP("When attaching entity will stay in same transformation in world space") ),
			InputPortConfig<bool>( "EnablePhysics", false, _HELP("Force enable physics of entity after detaching") ),
			{0}
		};
		config.sDescription = _HELP("Detach child from its parent");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if ( event != eFE_Activate )
			return;
		if (!pActInfo->pEntity)
			return;
		if (IsPortActive(pActInfo,0))
		{
			int nFlags = 0;
			if (GetPortBool(pActInfo,1))
				nFlags |= IEntity::ATTACHMENT_KEEP_TRANSFORMATION;
			if (GetPortBool(pActInfo,2))
			{
				IEntityPhysicalProxy *pPhysicsProxy = (IEntityPhysicalProxy*) pActInfo->pEntity->GetProxy(ENTITY_PROXY_PHYSICS);
				if (pPhysicsProxy)
					pPhysicsProxy->EnablePhysics(true);
			}
			pActInfo->pEntity->DetachThis(nFlags);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for Attaching child to parent.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityAttachChildToBone : public CFlowBaseNode
{
	bool m_bAttached;
	EntityId m_parent;
	EntityId m_child;
	string m_bone;
	QuatT m_childTR;

	void ExecuteAttach(IEntity *pParent, IEntity *pChild, string bone_name, const QuatT &trans, bool recalc)
		{
		if (pParent && pChild)
			{
				if (bone_name.empty())
					return;

			ICharacterInstance *pChar=pParent->GetCharacter(0);
				IAttachmentManager *pManager=pChar ? pChar->GetIAttachmentManager() : NULL;
				if (pManager)
				{
					IAttachment * pAtt=pManager->GetInterfaceByName(pChild->GetName());

					if (pAtt)
				{
					m_bAttached=true;
					m_parent=pParent->GetId();
					m_child=pChild->GetId();
					m_bone=bone_name;
					m_childTR=pAtt->GetAttRelativeDefault();

						return;
				}
					else
						pAtt=pManager->CreateAttachment(pChild->GetName(), CA_BONE, bone_name);

					QuatT childTr;												//Absolute world transformation of child
					QuatT parentTr;												//Absolute world transformation of parent
					QuatT boneTr;													//Transformation of bone relative to the entity
				QuatT attTr=trans;										//Relative transformation to be set for the attachment

				if (recalc)
				{
					attTr.q=Quat::CreateIdentity();
					attTr.t=Vec3(0, 0, 0);
					pAtt->SetAttRelativeDefault(attTr);

					childTr.t=pChild->GetWorldPos();
					childTr.q=pChild->GetWorldRotation();
					parentTr.t=pParent->GetWorldPos();
					parentTr.q=pParent->GetWorldRotation();
					boneTr=pAtt->GetAttWorldAbsolute();

					Quat invParentRot=parentTr.q.GetInverted();
					invParentRot.Normalize();
					Quat invBoneRot=boneTr.q.GetInverted();
					invBoneRot.Normalize();

					attTr.q=(invBoneRot*invParentRot)*childTr.q;
					attTr.q.Normalize();
					attTr.t=invParentRot*(childTr.t-parentTr.t);
					attTr.t=invBoneRot*(attTr.t-boneTr.t);
				}
					
					if (pAtt)
					{
						CEntityAttachment *pEAtt=new CEntityAttachment();
						pEAtt->SetEntityId(pChild->GetId());
						pAtt->AddBinding(pEAtt);
						pAtt->SetAttRelativeDefault(attTr);
						pAtt->HideAttachment(0);

					m_bAttached=true;
					m_parent=pParent->GetId();
					m_child=pChild->GetId();
					m_bone=bone_name;
					m_childTR=attTr;
				}
					}
				}
			}

public:
	CFlowNode_EntityAttachChildToBone( SActivationInfo *pActInfo ) 
		: CFlowBaseNode()
		, m_bAttached(false)
	{}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "Attach",_HELP("Trigger to attach entity") ),
			InputPortConfig<EntityId>( "Child",_HELP("Child Entity to Attach") ),
			InputPortConfig<string>( "BoneName",_HELP("The bone to link the entity to") ),
			{0}
		};
		config.sDescription = _HELP("Attach Child Entity and link it to a given bone");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if ( event != eFE_Activate )
			return;
		
		if (IsPortActive(pActInfo,0))
		{
			EntityId nChildId = GetPortEntityId(pActInfo,1);
			IEntity *pChild = gEnv->pEntitySystem->GetEntity(nChildId);

			ExecuteAttach(pActInfo->pEntity, pChild, GetPortString(pActInfo, 2), QuatT(), true);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo *nfo, TSerialize ser)
	{
		CFlowBaseNode::Serialize(nfo, ser);

		ser.BeginGroup("bone_attach");
		ser.Value("attached", m_bAttached);
		ser.Value("parent", m_parent);
		ser.Value("child", m_child);
		ser.Value("bone", m_bone);
		Quat rot=m_childTR.q;
		Vec3 trn=m_childTR.t;
		ser.Value("childRot", rot);
		ser.Value("childTrn", trn);
		m_childTR.q=rot;
		m_childTR.t=trn;
		ser.EndGroup();

		if (ser.IsReading() && m_bAttached)
		{
			ExecuteAttach(gEnv->pEntitySystem->GetEntity(m_parent), gEnv->pEntitySystem->GetEntity(m_child), m_bone, m_childTR, false);
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for getting the player [deprecated].
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityGetPlayer : public CFlowBaseNode
{
public:
	CFlowNode_EntityGetPlayer( SActivationInfo *pActInfo ) : CFlowBaseNode() {}
	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<EntityId>( "Player",_HELP("Outputs Id of the current player") ),
			{0}
		};
		config.sDescription = _HELP("Retrieve current player entity - Deprecated Use Game:LocalPlayer!");
		config.pInputPorts = 0;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_OBSOLETE);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		IActor *pActor = CCryAction::GetCryAction()->GetClientActor();
		if (pActor)
		{
			IEntity *pLocalPlayer = pActor->GetEntity();
			if (pLocalPlayer)
			{
				ActivateOutput(pActInfo, 0, pLocalPlayer->GetId() );
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for beaming an entity 
//////////////////////////////////////////////////////////////////////////
class CFlowNode_BeamEntity : public CFlowBaseNode
{
public:
	CFlowNode_BeamEntity( SActivationInfo *pActInfo ) : CFlowBaseNode() {}

	enum EInputPorts
	{
		EIP_Beam = 0,
		EIP_Pos,
		EIP_Rot,
		EIP_Scale
	};

	enum EOutputPorts
	{
		EOP_Done = 0,
	};

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "Beam",_HELP("Trigger to beam the Entity") ),
			InputPortConfig<Vec3>( "Position",_HELP("Position in World Coords") ),
			InputPortConfig<Vec3>( "Rotation",_HELP("Rotation [Degrees] in World Coords. (0,0,0) leaves Rotation untouched.") ),
			InputPortConfig<Vec3>( "Scale",_HELP("Scale. (0,0,0) leaves Scale untouched.") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void ("Done", _HELP("Triggered when done")),
			{0}
		};

		config.sDescription = _HELP("Beam an Entity to a new Position");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if ( event != eFE_Activate )
			return;
		if (!pActInfo->pEntity)
			return;
		if (IsPortActive(pActInfo, EIP_Beam))
		{
			const Vec3* vRot = pActInfo->pInputPorts[EIP_Rot].GetPtr<Vec3>();
			if (!vRot || vRot->IsZero())
			{
				Matrix34 tm = pActInfo->pEntity->GetWorldTM();
				tm.SetTranslation(GetPortVec3(pActInfo, EIP_Pos));
				pActInfo->pEntity->SetWorldTM(tm, ENTITY_XFORM_TRACKVIEW);
			}
			else
			{
				Matrix34 tm = Matrix33( Quat::CreateRotationXYZ( Ang3(DEG2RAD(*vRot)) ) );
				tm.SetTranslation(GetPortVec3(pActInfo, EIP_Pos));
				pActInfo->pEntity->SetWorldTM(tm, ENTITY_XFORM_TRACKVIEW);
			}
			const Vec3* vScale = pActInfo->pInputPorts[EIP_Scale].GetPtr<Vec3>();
			if (vScale && vScale->IsZero() == false)
				pActInfo->pEntity->SetScale( *vScale, ENTITY_XFORM_TRACKVIEW );

			// TODO: Maybe add some tweaks/checks wrt. physics/collisions
			ActivateOutput(pActInfo, EOP_Done, true);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for getting entity info.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityGetInfo : public CFlowBaseNode
{
public:
	CFlowNode_EntityGetInfo( SActivationInfo *pActInfo ) : CFlowBaseNode() {}
	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "Get",_HELP("Trigger to get info") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<EntityId> ( "Id", _HELP("Entity Id") ),
			OutputPortConfig<string>   ( "Name", _HELP("Entity Name") ),
			OutputPortConfig<string>   ( "Class", _HELP("Entity Class") ),
			OutputPortConfig<string>   ( "Archetype", _HELP("Entity Archetype") ),
			{0}
		};
		config.sDescription = _HELP("Get Entity Information");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if ( event != eFE_Activate )
			return;
		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == 0)
			return;
		if (IsPortActive(pActInfo,0))
		{
			string empty;
			ActivateOutput(pActInfo, 0, pEntity->GetId());
			string temp (pEntity->GetName());
			ActivateOutput(pActInfo, 1, temp);
			temp = pEntity->GetClass()->GetName();
			ActivateOutput(pActInfo, 2, temp);
			IEntityArchetype* pArchetype = pEntity->GetArchetype();
			if (pArchetype)
				temp = pArchetype->GetName();
			ActivateOutput(pActInfo, 3, pArchetype ? temp : empty);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

////////////////////////////////////////////////////////////////////////////////////////
// Flow node for setting entity render parameters (opacity, glow, motionblur, etc)
////////////////////////////////////////////////////////////////////////////////////////
class CFlowNodeRenderParams : public CFlowEntityNodeBase
{
public:
  CFlowNodeRenderParams( SActivationInfo *pActInfo ) : CFlowEntityNodeBase() 
  {
    m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
  }

  enum EInputPorts
  {
    EIP_Opacity = 0,
    EIP_GlowAmount,
  };

  virtual IFlowNodePtr Clone( SActivationInfo *pActInfo ) { pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; return new CFlowNode_EntityMaterial(pActInfo); };

  virtual void GetConfiguration( SFlowNodeConfig& config )
  {
    static const SInputPortConfig in_config[] = {
      InputPortConfig<float>( "Opacity", 1.0f, "Set opacity amount" ),
      //InputPortConfig<float>( "GlowAmount", 1.0f, "Set glow amount" ),
      {0}
    };
    config.sDescription = _HELP("Set render specific parameters");
    config.nFlags |= EFLN_TARGET_ENTITY;
    config.pInputPorts = in_config;
    config.pOutputPorts = 0;
    config.SetCategory(EFLN_APPROVED);
  }

  virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
  {
    CFlowEntityNodeBase::ProcessEvent( event, pActInfo );

    if (event != eFE_Activate)
    {
      return;
    }

    IEntity *pEntity = GetEntity();
   
    if (!pEntity)
    {
      return;
    }

    IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
    if ( !pRenderProxy )
    {
      return;
    }

    if (IsPortActive(pActInfo, EIP_Opacity))
    {
      pRenderProxy->SetOpacity( GetPortFloat(pActInfo, EIP_Opacity ) );
    }
  }

  void OnEntityEvent( IEntity *pEntity,SEntityEvent &event )
  {
  }


	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

#if 0 // replace by CFlowNodeMaterialShaderParam
////////////////////////////////////////////////////////////////////////////////////////
// Flow node for setting entity render parameters (opacity, glow, motionblur, etc)
////////////////////////////////////////////////////////////////////////////////////////
class CFlowNodeMaterialParam : public CFlowEntityNodeBase
{
public:
  CFlowNodeMaterialParam( SActivationInfo *pActInfo ) : CFlowEntityNodeBase() 
  {
    m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
  }

  virtual IFlowNodePtr Clone( SActivationInfo *pActInfo )
	{ 
		pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; 
		return new CFlowNodeMaterialParam(pActInfo);
	};

	enum EInputPorts
	{
		EIP_Get = 0,
		EIP_Slot,
		EIP_SubMtlId,
		EIP_ParamFloat,
		EIP_Float,
		EIP_ParamColor,
		EIP_Color,
	};

	enum EOutputPorts
	{
		EOP_Float = 0,
		EOP_Color,
	};

  virtual void GetConfiguration( SFlowNodeConfig& config )
  {
    static const SInputPortConfig in_config[] = {
			InputPortConfig_Void ("Get", _HELP("Trigger to get current value")),
			InputPortConfig<int> ("Slot", 0, _HELP("Material Slot")),
			InputPortConfig<int> ("SubMtlId", 0, _HELP("Sub Material Id")),
			InputPortConfig<string> ("ParamFloat", _HELP("Float Parameter to be set/get"), 0, _UICONFIG("enum_string:Glow=glow,Shininess=shininess,Alpha=alpha,Opacity=opacity")),
      InputPortConfig<float>( "ValueFloat", 0.0f, _HELP("Trigger to set Float value") ),
			InputPortConfig<string> ("ParamColor", _HELP("Color Parameter to be set/get"), 0, _UICONFIG("enum_string:Diffuse=diffuse,Specular=specular,Emissive=emissive")),
			InputPortConfig<Vec3>( "color_ValueColor", _HELP("Trigger to set Color value") ),
      {0}
    };

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>( "ValueFloat", _HELP("Current Float Value") ),
			OutputPortConfig<Vec3> ( "ValueColor",  _HELP("Current Color Value") ),
			{0}
		};

		config.sDescription = _HELP("Set/Get Material Parameters");
    config.nFlags |= EFLN_TARGET_ENTITY;
    config.pInputPorts = in_config;
    config.pOutputPorts = out_config;
    config.SetCategory(EFLN_APPROVED);
  }

  virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
  {
    CFlowEntityNodeBase::ProcessEvent( event, pActInfo );

    if (event != eFE_Activate)
      return;

		const bool bGet = IsPortActive(pActInfo, EIP_Get);
		const bool bSetFloat = IsPortActive(pActInfo, EIP_Float);
		const bool bSetColor = IsPortActive(pActInfo, EIP_Color);

		if (!bGet && !bSetFloat && !bSetColor)
			return;

    IEntity* pEntity = GetEntity();
    if (pEntity == 0)
      return;

    IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
    if (pRenderProxy == 0)
      return;

		const int& slot = GetPortInt(pActInfo, EIP_Slot);
		IMaterial* pMtl = pRenderProxy->GetRenderMaterial(slot);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeSetMaterialParam: Entity '%s' [%d] has no material at slot %d", pEntity->GetName(), pEntity->GetId(), slot);
			return;
		}

		const int& subMtlId = GetPortInt(pActInfo, EIP_SubMtlId);
		pMtl = pMtl->GetSafeSubMtl(subMtlId);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeSetMaterialParam: Entity '%s' [%d] has no sub-material %d at slot %d", pEntity->GetName(), pEntity->GetId(), subMtlId, slot);
			return;
		}
		const string& paramNameFloat = GetPortString(pActInfo, EIP_ParamFloat);
		const string& paramNameVec3 = GetPortString(pActInfo, EIP_ParamColor);

		float floatValue = 0.0f;
		Vec3 vec3Value = Vec3(ZERO);
		if (bSetFloat)
		{
			floatValue = GetPortFloat(pActInfo, EIP_Float);
			pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, false);
		}
		if (bSetColor)
		{
			vec3Value = GetPortVec3(pActInfo, EIP_Color);
			pMtl->SetGetMaterialParamVec3(paramNameVec3, vec3Value, false);
		}
		if (bGet)
		{
			if (pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, true))
				ActivateOutput(pActInfo, EOP_Float, floatValue);
			if (pMtl->SetGetMaterialParamVec3(paramNameVec3, vec3Value, true))
			{
				ActivateOutput(pActInfo, EOP_Color, vec3Value);
			}
		}
  }

  void OnEntityEvent( IEntity *pEntity,SEntityEvent &event )
  {
  }


	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Flow node for setting entity render parameters (opacity, glow, motionblur, etc)
////////////////////////////////////////////////////////////////////////////////////////
class CFlowNodeEntityMaterialShaderParam : public CFlowEntityNodeBase
{
public:
	CFlowNodeEntityMaterialShaderParam( SActivationInfo *pActInfo ) : CFlowEntityNodeBase() 
	{
		m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
	}

	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo )
	{ 
		pActInfo->m_pUserData = (void*)(UINT_PTR)m_entityId; 
		return new CFlowNodeEntityMaterialShaderParam(pActInfo);
	};

	enum EInputPorts
	{
		EIP_Get = 0,
		EIP_Slot,
		EIP_SubMtlId,
		EIP_ParamFloat,
		EIP_Float,
		EIP_ParamColor,
		EIP_Color,
	};

	enum EOutputPorts
	{
		EOP_Float = 0,
		EOP_Color,
	};

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void ("Get", _HELP("Trigger to get current value")),
			InputPortConfig<int> ("Slot", 0, _HELP("Material Slot")),
			InputPortConfig<int> ("SubMtlId", 0, _HELP("Sub Material Id")),
			InputPortConfig<string> ("ParamFloat", _HELP("Float Parameter to be set/get"), 0, _UICONFIG("dt=matparamslot,slot_ref=Slot,sub_ref=SubMtlId,param=float") ),
			InputPortConfig<float>( "ValueFloat", 0.0f, _HELP("Trigger to set Float value") ),
			InputPortConfig<string> ("ParamColor", _HELP("Color Parameter to be set/get"), 0, _UICONFIG("dt=matparamslot,slot_ref=Slot,sub_ref=SubMtlId,param=vec") ),
			InputPortConfig<Vec3>( "color_ValueColor", _HELP("Trigger to set Color value") ),
			{0}
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>( "ValueFloat", _HELP("Current Float Value") ),
			OutputPortConfig<Vec3> ( "ValueColor",  _HELP("Current Color Value") ),
			{0}
		};

		config.sDescription = _HELP("Set/Get Entity's Material Shader Parameters");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		CFlowEntityNodeBase::ProcessEvent( event, pActInfo );

		if (event == eFE_Initialize)
		{
			m_pMaterial = 0;
			return;
		}

		if (event != eFE_Activate)
			return;

		const bool bGet = IsPortActive(pActInfo, EIP_Get);
		const bool bSetFloat = IsPortActive(pActInfo, EIP_Float);
		const bool bSetColor = IsPortActive(pActInfo, EIP_Color);

		if (!bGet && !bSetFloat && !bSetColor)
			return;

		IEntity* pEntity = GetEntity();
		if (pEntity == 0)
			return;

		IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy == 0)
			return;

		const int slot = GetPortInt(pActInfo, EIP_Slot);
		IMaterial* pMtl = pRenderProxy->GetRenderMaterial(slot);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeEntityMaterialShaderParam: Entity '%s' [%d] has no material at slot %d", pEntity->GetName(), pEntity->GetId(), slot);
			return;
		}

		if (m_pMaterial != pMtl)
		{
			pMtl = gEnv->p3DEngine->GetMaterialManager()->CloneMultiMaterial(pMtl);
			if (pMtl == 0)
			{
				GameWarning("[flow] CFlowNodeEntityMaterialShaderParam: Entity '%s' [%d] Can't clone material at slot %d", pEntity->GetName(), pEntity->GetId(), slot);
				return;
			}
			m_pMaterial = pMtl;
			pRenderProxy->SetSlotMaterial(slot, pMtl);
		}

		const int& subMtlId = GetPortInt(pActInfo, EIP_SubMtlId);
		pMtl = pMtl->GetSafeSubMtl(subMtlId);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeEntityMaterialShaderParam: Entity '%s' [%d] has no sub-material %d at slot %d", pEntity->GetName(), pEntity->GetId(), subMtlId, slot);
			return;
		}


		const string& paramNameFloat = GetPortString(pActInfo, EIP_ParamFloat);
		const string& paramNameVec3 = GetPortString(pActInfo, EIP_ParamColor);

		const SShaderItem& shaderItem = pMtl->GetShaderItem();
		IRenderShaderResources* pRendRes = shaderItem.m_pShaderResources;
		if (pRendRes == 0)
		{
			GameWarning("[flow] CFlowNodeEntityMaterialShaderParam: Entity '%s' [%d] Material's '%s' sub-material [%d] '%s' has not shader-resources", pEntity->GetName(), pEntity->GetId(), m_pMaterial->GetName(), subMtlId, pMtl->GetName() );
			return;
		}

    DynArray<SShaderParam>& params = pRendRes->GetParameters(); // pShader->GetPublicParams();

		bool bUpdateShaderConstants = false;
		float floatValue = 0.0f;
		Vec3 vec3Value (ZERO);
		if (bSetFloat)
		{
			floatValue = GetPortFloat(pActInfo, EIP_Float);
			// GameWarning("mtl: setting mtl '%s' (0x%p) submtl '%s' (%d) (0x%p) param '%s' to %f", pMultiMtl->GetName(), pMultiMtl, pMtl->GetName(), subMtlId, pMtl, paramNameFloat.c_str(), floatValue);
			if (pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, false) == false)
			{
				UParamVal val;
				val.m_Float = floatValue;
				SShaderParam::SetParam(paramNameFloat, &params, val);
				bUpdateShaderConstants = true;
			}
		}
		if (bSetColor)
		{
			vec3Value = GetPortVec3(pActInfo, EIP_Color);
			// GameWarning("mtl: setting mtl '%s' (0x%p) submtl '%s' (%d) (0x%p) param '%s' to %f,%f,%f", pMultiMtl->GetName(), pMultiMtl, pMtl->GetName(), subMtlId, pMtl, paramNameFloat.c_str(), vec3Value[0], vec3Value[1], vec3Value[2]);
			if (pMtl->SetGetMaterialParamVec3(paramNameVec3, vec3Value, false) == false)
			{
				UParamVal val;
				val.m_Color[0] = vec3Value[0];
				val.m_Color[1] = vec3Value[1];
				val.m_Color[2] = vec3Value[2];
				val.m_Color[3] = 1.0f;
				SShaderParam::SetParam(paramNameVec3, &params, val);
				bUpdateShaderConstants = true;
			}
		}
		if (bUpdateShaderConstants)
		{
			shaderItem.m_pShaderResources->UpdateConstants(shaderItem.m_pShader);
		}
		if (bGet)
		{
			if (pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, true))
				ActivateOutput(pActInfo, EOP_Float, floatValue);
			if (pMtl->SetGetMaterialParamVec3(paramNameVec3, vec3Value, true))
				ActivateOutput(pActInfo, EOP_Color, vec3Value);

			for (int i=0; i< params.size(); ++i)
			{
				SShaderParam& param = params[i];
				if (stricmp(param.m_Name, paramNameFloat) == 0)
				{
					float val = 0.0f;
					switch (param.m_Type)
					{
					case eType_BOOL: val = param.m_Value.m_Bool; break;
					case eType_SHORT: val = param.m_Value.m_Short; break;
					case eType_INT: val = param.m_Value.m_Int; break;
					case eType_FLOAT: val = param.m_Value.m_Float; break;
					default: break;
					}
					ActivateOutput(pActInfo, EOP_Float, val);
				}
				if (stricmp(param.m_Name, paramNameVec3) == 0)
				{
					Vec3 val (ZERO);
					if (param.m_Type == eType_VECTOR)
					{
						val.Set(param.m_Value.m_Vector[0], param.m_Value.m_Vector[1], param.m_Value.m_Vector[2]);
					}
					if (param.m_Type == eType_FCOLOR)
					{
						val.Set(param.m_Value.m_Color[0], param.m_Value.m_Color[1], param.m_Value.m_Color[2]);
					}
					ActivateOutput(pActInfo, EOP_Color, val);
				}
			}
		}
	}

	void OnEntityEvent( IEntity *pEntity,SEntityEvent &event )
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

protected:
	_smart_ptr<IMaterial> m_pMaterial;
};


class CFlowNodeCharAttachmentMaterialShaderParam : public CFlowBaseNode
{
public:
	CFlowNodeCharAttachmentMaterialShaderParam( SActivationInfo *pActInfo ) : m_pMaterial(0), m_bFailedCloningMaterial(false)
	{
	}

	virtual IFlowNodePtr Clone( SActivationInfo* pActInfo )
	{ 
		return new CFlowNodeCharAttachmentMaterialShaderParam(pActInfo);
	};

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			// new chance on load
			m_bFailedCloningMaterial = false;
		}
	}

	enum EInputPorts
	{
		EIP_CharSlot = 0,
		EIP_Attachment,
		EIP_SetMaterial,
		EIP_ForcedMaterial,
		EIP_SubMtlId,
		EIP_Get,
		EIP_ParamFloat,
		EIP_Float,
		EIP_ParamColor,
		EIP_Color,
	};

	enum EOutputPorts
	{
		EOP_Float = 0,
		EOP_Color,
	};

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<int> ("CharSlot", 0, _HELP("Character Slot within Entity")),
			InputPortConfig<string>("Attachment", _HELP("Attachment"), 0, _UICONFIG("dt=attachment,ref_entity=entityId") ),
			InputPortConfig_Void ("SetMaterial", _HELP("Trigger to force setting a material [ForcedMaterial]")),
			InputPortConfig<string>("ForcedMaterial", _HELP("Material"), 0, _UICONFIG("dt=mat") ),
			InputPortConfig<int> ("SubMtlId", 0, _HELP("Sub Material Id")),
			InputPortConfig_Void ("Get", _HELP("Trigger to get current value")),
			InputPortConfig<string> ("ParamFloat", _HELP("Float Parameter to be set/get"), 0, _UICONFIG("dt=matparamcharatt,charslot_ref=CharSlot,attachment_ref=Attachment,sub_ref=SubMtlId,param=float") ),
			InputPortConfig<float>( "ValueFloat", 0.0f, _HELP("Trigger to set Float value") ),
			InputPortConfig<string> ("ParamColor", _HELP("Color Parameter to be set/get"), 0, _UICONFIG("dt=matparamcharatt,charslot_ref=CharSlot,attachment_ref=Attachment,sub_ref=SubMtlId,param=vec") ),
			InputPortConfig<Vec3>( "color_ValueColor", _HELP("Trigger to set Color value") ),
			{0}
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>( "ValueFloat", _HELP("Current Float Value") ),
			OutputPortConfig<Vec3> ( "ValueColor",  _HELP("Current Color Value") ),
			{0}
		};

		config.sDescription = _HELP("[CUTSCENE ONLY] Set ShaderParams of Character Attachments.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate)
			return;

		const bool bGet = IsPortActive(pActInfo, EIP_Get);
		const bool bSetFloat = IsPortActive(pActInfo, EIP_Float);
		const bool bSetColor = IsPortActive(pActInfo, EIP_Color);
		const bool bSetMaterial = IsPortActive(pActInfo, EIP_SetMaterial);

		if (!bGet && !bSetFloat && !bSetColor && !bSetMaterial)
			return;

		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == 0)
			return;

		const int charSlot = GetPortInt(pActInfo, EIP_CharSlot);
		ICharacterInstance* pChar = pActInfo->pEntity->GetCharacter(charSlot);
		if (pChar == 0)
			return;

		IAttachmentManager* pAttMgr = pChar->GetIAttachmentManager();
		if (pAttMgr == 0)
			return;

		const string& attachment = GetPortString(pActInfo, EIP_Attachment);
		IAttachment* pAttachment = pAttMgr->GetInterfaceByName(attachment.c_str());
		if (pAttachment == 0)
		{
			GameWarning("[flow] CFlowNodeCharAttachmentMaterialShaderParam: Entity '%s' [%d] Cannot access Attachment '%s' [CharSlot=%d]", pEntity->GetName(), pEntity->GetId(), attachment.c_str(), charSlot);
			return;
		}

		IAttachmentObject* pAttachObj = pAttachment->GetIAttachmentObject();
		if (pAttachObj == 0)
		{
			GameWarning("[flow] CFlowNodeCharAttachmentMaterialShaderParam: Entity '%s' [%d] Cannot access AttachmentObject at Attachment '%s' [CharSlot=%d]", pEntity->GetName(), pEntity->GetId(), attachment.c_str(), charSlot);
			return;
		}

		IMaterialManager* pMatMgr = gEnv->p3DEngine->GetMaterialManager();
		if (bSetMaterial)
		{
			const string& matName = GetPortString(pActInfo, EIP_ForcedMaterial);
			if (true /* m_pMaterial == 0 || matName != m_pMaterial->GetName() */) // always reload the mat atm
			{
				m_pMaterial = pMatMgr->LoadMaterial(matName.c_str());
				if (m_pMaterial)
				{
					m_pMaterial = pMatMgr->CloneMultiMaterial(m_pMaterial);
					m_bFailedCloningMaterial = false;
				}
				else
				{
					m_bFailedCloningMaterial = true;
				}
			}
		}

		if (m_pMaterial == 0 && m_bFailedCloningMaterial == false)
		{
			IMaterial* pOldMtl = pAttachObj->GetMaterial();
			if (pOldMtl)
			{
				m_pMaterial = pMatMgr->CloneMultiMaterial(pOldMtl);
				m_bFailedCloningMaterial = false;
			}
			else
			{
				m_bFailedCloningMaterial = true;
			}
		}

		IMaterial* pMtl = m_pMaterial;
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeCharAttachmentMaterialShaderParam: Entity '%s' [%d] Cannot access material at Attachment '%s' [CharSlot=%d]", pEntity->GetName(), pEntity->GetId(), attachment.c_str(), charSlot);
			return;
		}

		const int& subMtlId = GetPortInt(pActInfo, EIP_SubMtlId);
		pMtl = pMtl->GetSafeSubMtl(subMtlId);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeCharAttachmentMaterialShaderParam: Entity '%s' [%d] Material '%s' has no sub-material %d", pEntity->GetName(), pEntity->GetId(), m_pMaterial->GetName(), subMtlId);
			return;
		}

		// set our material
		pAttachObj->SetMaterial(pMtl);

		const string& paramNameFloat = GetPortString(pActInfo, EIP_ParamFloat);
		const string& paramNameVec3 = GetPortString(pActInfo, EIP_ParamColor);

		const SShaderItem& shaderItem = pMtl->GetShaderItem();
		IRenderShaderResources* pRendRes = shaderItem.m_pShaderResources;
		if (pRendRes == 0)
		{
			GameWarning("[flow] CFlowNodeCharAttachmentMaterialShaderParam: Entity '%s' [%d] Material '%s' (sub=%d) at Attachment '%s' [CharSlot=%d] has no render resources", 
				pEntity->GetName(), pEntity->GetId(), pMtl->GetName(), subMtlId, attachment.c_str(), charSlot);
			return;
		}
		DynArray<SShaderParam>& params = pRendRes->GetParameters(); // pShader->GetPublicParams();

		bool bUpdateShaderConstants = false;
		float floatValue = 0.0f;
		Vec3 vec3Value = Vec3(ZERO);
		if (bSetFloat)
		{
			floatValue = GetPortFloat(pActInfo, EIP_Float);
			if (pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, false) == false)
			{
				UParamVal val;
				val.m_Float = floatValue;
				SShaderParam::SetParam(paramNameFloat, &params, val);
				bUpdateShaderConstants = true;
			}
		}
		if (bSetColor)
		{
			vec3Value = GetPortVec3(pActInfo, EIP_Color);
			if (pMtl->SetGetMaterialParamVec3(paramNameVec3, vec3Value, false) == false)
			{
				UParamVal val;
				val.m_Color[0] = vec3Value[0];
				val.m_Color[1] = vec3Value[1];
				val.m_Color[2] = vec3Value[2];
				val.m_Color[3] = 1.0f;
				SShaderParam::SetParam(paramNameVec3, &params, val);
				bUpdateShaderConstants = true;
			}
		}
		if (bUpdateShaderConstants)
		{
			shaderItem.m_pShaderResources->UpdateConstants(shaderItem.m_pShader);
		}
		if (bGet)
		{
			if (pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, true))
				ActivateOutput(pActInfo, EOP_Float, floatValue);
			if (pMtl->SetGetMaterialParamVec3(paramNameVec3, vec3Value, true))
				ActivateOutput(pActInfo, EOP_Color, vec3Value);

			for (int i=0; i< params.size(); ++i)
			{
				SShaderParam& param = params[i];
				if (stricmp(param.m_Name, paramNameFloat) == 0)
				{
					float val = 0.0f;
					switch (param.m_Type)
					{
					case eType_BOOL: val = param.m_Value.m_Bool; break;
					case eType_SHORT: val = param.m_Value.m_Short; break;
					case eType_INT: val = param.m_Value.m_Int; break;
					case eType_FLOAT: val = param.m_Value.m_Float; break;
					default: break;
					}
					ActivateOutput(pActInfo, EOP_Float, val);
				}
				if (stricmp(param.m_Name, paramNameVec3) == 0)
				{
					Vec3 val (ZERO);
					if (param.m_Type == eType_VECTOR)
					{
						val.Set(param.m_Value.m_Vector[0], param.m_Value.m_Vector[1], param.m_Value.m_Vector[2]);
					}
					if (param.m_Type == eType_FCOLOR)
					{
						val.Set(param.m_Value.m_Color[0], param.m_Value.m_Color[1], param.m_Value.m_Color[2]);
					}
					ActivateOutput(pActInfo, EOP_Color, val);
				}
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	_smart_ptr<IMaterial> m_pMaterial;
	bool m_bFailedCloningMaterial;
};


////////////////////////////////////////////////////////////////////////////////////////
// Flow node for cloning an entity's material 
////////////////////////////////////////////////////////////////////////////////////////
class CFlowNodeEntityCloneMaterial : public CFlowBaseNode
{
public:
	CFlowNodeEntityCloneMaterial( SActivationInfo *pActInfo ) : CFlowBaseNode() 
	{
	}

	enum EInputPorts
	{
		EIP_Clone = 0,
		EIP_Reset,
		EIP_Slot,
	};

	enum EOutputPorts
	{
		EOP_Cloned = 0,
		EOP_Reset
	};

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void ("Clone", _HELP("Trigger to clone material")),
			InputPortConfig_Void ("Reset", _HELP("Trigger to reset material")),
			InputPortConfig<int> ("Slot", 0, _HELP("Material Slot")),
			{0}
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void ("Cloned", _HELP("Triggered when cloned.") ),
			OutputPortConfig_Void ("Reset", _HELP("Triggered when reset.") ),
			{0}
		};

		config.sDescription = _HELP("Clone an Entity's Material and Reset it back to original.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				const int slot = GetPortInt(pActInfo, EIP_Slot);
				UnApplyMaterial(pActInfo->pEntity, slot);
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Reset))
				{
					const int slot = GetPortInt(pActInfo, EIP_Slot);
					UnApplyMaterial(pActInfo->pEntity, slot);
					ActivateOutput(pActInfo, EOP_Reset, true);
				}
				if (IsPortActive(pActInfo, EIP_Clone))
				{
					const int slot = GetPortInt(pActInfo, EIP_Slot);
					UnApplyMaterial(pActInfo->pEntity, slot);
					CloneAndApplyMaterial(pActInfo->pEntity, slot);
					ActivateOutput(pActInfo, EOP_Cloned, true);
				}
			}
			break;
		default:
			break;
		}
	}

	void UnApplyMaterial(IEntity* pEntity, int slot)
	{
		if (pEntity == 0)
			return;
		IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy == 0)
			return;
		pRenderProxy->SetSlotMaterial(slot, 0);
	}

	void CloneAndApplyMaterial(IEntity* pEntity, int slot)
	{
		if (pEntity == 0)
			return;

		IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy == 0)
			return;

		IMaterial* pMtl = pRenderProxy->GetSlotMaterial(slot);
		if (pMtl)
			return;

		pMtl = pRenderProxy->GetRenderMaterial(slot);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeEntityCloneMaterial: Entity '%s' [%d] has no material at slot %d", pEntity->GetName(), pEntity->GetId(), slot);
			return;
		}

		pMtl = gEnv->p3DEngine->GetMaterialManager()->CloneMultiMaterial(pMtl);
		pRenderProxy->SetSlotMaterial(slot, pMtl);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

};


////////////////////////////////////////////////////////////////////////////////////////
// Flow node for setting entity render parameters (opacity, glow, motionblur, etc)
////////////////////////////////////////////////////////////////////////////////////////
class CFlowNodeMaterialShaderParam : public CFlowBaseNode
{
public:
	CFlowNodeMaterialShaderParam( SActivationInfo *pActInfo ) : CFlowBaseNode() 
	{
	}

	enum EInputPorts
	{
		EIP_Get = 0,
		EIP_Name,
		EIP_SubMtlId,
		EIP_ParamFloat,
		EIP_Float,
		EIP_ParamColor,
		EIP_Color,
	};

	enum EOutputPorts
	{
		EOP_Float = 0,
		EOP_Color,
	};

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void ("Get", _HELP("Trigger to get current value")),
			InputPortConfig<string> ("mat_Material", _HELP("Material Name")),
			InputPortConfig<int> ("SubMtlId", 0, _HELP("Sub Material Id")),
			InputPortConfig<string> ("ParamFloat", _HELP("Float Parameter to be set/get"), 0, _UICONFIG("dt=matparamname,name_ref=mat_Material,sub_ref=SubMtlId,param=float") ),
			InputPortConfig<float>( "ValueFloat", 0.0f, _HELP("Trigger to set Float value") ),
			InputPortConfig<string> ("ParamColor", _HELP("Color Parameter to be set/get"), 0, _UICONFIG("dt=matparamname,name_ref=mat_Material,sub_ref=SubMtlId,param=vec")),
			InputPortConfig<Vec3>( "color_ValueColor", _HELP("Trigger to set Color value") ),
			{0}
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>( "ValueFloat", _HELP("Current Float Value") ),
			OutputPortConfig<Vec3> ( "ValueColor",  _HELP("Current Color Value") ),
			{0}
		};

		config.sDescription = _HELP("Set/Get Material's Shader Parameters");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate)
			return;

		const bool bGet = IsPortActive(pActInfo, EIP_Get);
		const bool bSetFloat = IsPortActive(pActInfo, EIP_Float);
		const bool bSetColor = IsPortActive(pActInfo, EIP_Color);

		if (!bGet && !bSetFloat && !bSetColor)
			return;

		const string& matName = GetPortString(pActInfo, EIP_Name);
		IMaterial* pMtl = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(matName);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeMaterialParam: Material '%s' not found.", matName.c_str());
			return;
		}

		const int& subMtlId = GetPortInt(pActInfo, EIP_SubMtlId);
		pMtl = pMtl->GetSafeSubMtl(subMtlId);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeMaterialParam: Material '%s' has no sub-material %d", matName.c_str(), subMtlId);
			return;
		}

		const string& paramNameFloat = GetPortString(pActInfo, EIP_ParamFloat);
		const string& paramNameVec3 = GetPortString(pActInfo, EIP_ParamColor);

		const SShaderItem& shaderItem = pMtl->GetShaderItem();
		IRenderShaderResources* pRendRes = shaderItem.m_pShaderResources;
		if (pRendRes == 0)
		{
			GameWarning("[flow] CFlowNodeMaterialParam: Material '%s' (submtl %d) has no shader-resources.", matName.c_str(), subMtlId);
			return;
		}
    DynArray<SShaderParam>& params = pRendRes->GetParameters(); // pShader->GetPublicParams();

		bool bUpdateShaderConstants = false;
		float floatValue = 0.0f;
		Vec3 vec3Value = Vec3(ZERO);
		if (bSetFloat)
		{
			floatValue = GetPortFloat(pActInfo, EIP_Float);
			if (pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, false) == false)
			{
				UParamVal val;
				val.m_Float = floatValue;
				SShaderParam::SetParam(paramNameFloat, &params, val);
				bUpdateShaderConstants = true;
			}
		}
		if (bSetColor)
		{
			vec3Value = GetPortVec3(pActInfo, EIP_Color);
			if (pMtl->SetGetMaterialParamVec3(paramNameVec3, vec3Value, false) == false)
			{
				UParamVal val;
				val.m_Color[0] = vec3Value[0];
				val.m_Color[1] = vec3Value[1];
				val.m_Color[2] = vec3Value[2];
				val.m_Color[3] = 1.0f;
				SShaderParam::SetParam(paramNameVec3, &params, val);
				bUpdateShaderConstants = true;
			}
		}
		if (bUpdateShaderConstants)
		{
			shaderItem.m_pShaderResources->UpdateConstants(shaderItem.m_pShader);
		}

		if (bGet)
		{
			if (pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, true))
				ActivateOutput(pActInfo, EOP_Float, floatValue);
			if (pMtl->SetGetMaterialParamVec3(paramNameVec3, vec3Value, true))
				ActivateOutput(pActInfo, EOP_Color, vec3Value);

			for (int i=0; i< params.size(); ++i)
			{
				SShaderParam& param = params[i];
				if (stricmp(param.m_Name, paramNameFloat) == 0)
				{
					float val = 0.0f;
					switch (param.m_Type)
					{
					case eType_BOOL: val = param.m_Value.m_Bool; break;
					case eType_SHORT: val = param.m_Value.m_Short; break;
					case eType_INT: val = param.m_Value.m_Int; break;
					case eType_FLOAT: val = param.m_Value.m_Float; break;
					default: break;
					}
					ActivateOutput(pActInfo, EOP_Float, val);
				}
				if (stricmp(param.m_Name, paramNameVec3) == 0)
				{
					Vec3 val (ZERO);
					if (param.m_Type == eType_VECTOR)
					{
						val.Set(param.m_Value.m_Vector[0], param.m_Value.m_Vector[1], param.m_Value.m_Vector[2]);
					}
					if (param.m_Type == eType_FCOLOR)
					{
						val.Set(param.m_Value.m_Color[0], param.m_Value.m_Color[1], param.m_Value.m_Color[2]);
					}
					ActivateOutput(pActInfo, EOP_Color, val);
				}
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Flow node for setting entity material layers.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_EntityMaterialLayer : public CFlowBaseNode
{
public:
	CFlowNode_EntityMaterialLayer( SActivationInfo *pActInfo ) : CFlowBaseNode() {}

	enum INPUTS
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_Layer
	};

	enum OUTPUTS
	{
		EOP_Done = 0,
	};

	static const char* GetUIConfig()
	{
		static CryFixedStringT<128> layers;
		if (layers.empty())
		{
			// make sure that string fits into 128 bytes, otherwise it is [safely] truncated
			//layers.FormatFast("enum_int:Frozen=%d,Wet=%d,Dirt=%d,Burned=%d", MTL_LAYER_FROZEN, MTL_LAYER_WET, MTL_LAYER_DIRT, MTL_LAYER_BURNED);
      layers.FormatFast("enum_int:Frozen=%d,Wet=%d,Cloak=%d,DynamicFrozen=%d", MTL_LAYER_FROZEN, MTL_LAYER_WET, MTL_LAYER_CLOAK, MTL_LAYER_DYNAMICFROZEN);
		}
		return layers.c_str();
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "Enable",_HELP("Trigger to enable the layer") ),
			InputPortConfig_Void( "Disable",_HELP("Trigger to disable the layer") ),
			InputPortConfig<int> ("Layer", MTL_LAYER_FROZEN, _HELP("Layer to be enabled or disabled"), 0, _UICONFIG(GetUIConfig())),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void ( "Done", _HELP("Triggered when done") ),
			{0}
		};
		config.sDescription = _HELP("Enable/Disable Entity Material Layers");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if ( event != eFE_Activate )
			return;
		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == 0)
			return;
		const bool bEnable = IsPortActive(pActInfo, EIP_Enable);
		const bool bDisable = IsPortActive(pActInfo, EIP_Disable);
		if (bEnable || bDisable)
		{
			IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
			if (pRenderProxy)
			{
				uint8 layer = (uint8) GetPortInt(pActInfo, EIP_Layer);
				uint8 activeLayers = pRenderProxy->GetMaterialLayersMask();
				if (bEnable)
					activeLayers |= layer;
				else
					activeLayers &= ~layer;

				pRenderProxy->SetMaterialLayersMask( activeLayers );
			}
			ActivateOutput(pActInfo, EOP_Done, true);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


REGISTER_FLOW_NODE( "Entity:EntityPos",CFlowNode_EntityPos )
REGISTER_FLOW_NODE_SINGLETON( "Entity:GetPos",CFlowNode_EntityGetPos )
REGISTER_FLOW_NODE_SINGLETON( "Entity:BroadcastEvent",CFlowNode_BroadcastEntityEvent )
REGISTER_FLOW_NODE( "Entity:EntityId",CFlowNode_EntityId )
REGISTER_FLOW_NODE_SINGLETON( "Entity:EntityInfo",CFlowNode_EntityGetInfo )
REGISTER_FLOW_NODE_SINGLETON( "Entity:ParentId",CFlowNode_ParentId )
REGISTER_FLOW_NODE( "Entity:SetProperty",CFlowNode_EntitySetProperty )
REGISTER_FLOW_NODE( "Entity:GetProperty",CFlowNode_EntityGetProperty )
REGISTER_FLOW_NODE( "Entity:Material",CFlowNode_EntityMaterial )
REGISTER_FLOW_NODE( "Entity:MaterialLayer",CFlowNode_EntityMaterialLayer )
REGISTER_FLOW_NODE_SINGLETON( "Entity:AttachChild",CFlowNode_EntityAttachChild )
REGISTER_FLOW_NODE( "Entity:AttachChildToBone",CFlowNode_EntityAttachChildToBone )
REGISTER_FLOW_NODE_SINGLETON( "Entity:DetachThis",CFlowNode_EntityDetachThis )
REGISTER_FLOW_NODE_SINGLETON( "Entity:GetPlayer",CFlowNode_EntityGetPlayer )
REGISTER_FLOW_NODE_SINGLETON( "Entity:BeamEntity",CFlowNode_BeamEntity )
REGISTER_FLOW_NODE( "Entity:RenderParams",CFlowNodeRenderParams )
// REGISTER_FLOW_NODE( "Entity:MaterialParam",CFlowNodeMaterialParam )
REGISTER_FLOW_NODE( "Entity:MaterialParam",CFlowNodeEntityMaterialShaderParam )
REGISTER_FLOW_NODE_SINGLETON( "Entity:MaterialClone",CFlowNodeEntityCloneMaterial )

REGISTER_FLOW_NODE( "Entity:CharAttachmentMaterialParam",CFlowNodeCharAttachmentMaterialShaderParam )

// engine based
REGISTER_FLOW_NODE( "Engine:MaterialParam",CFlowNodeMaterialShaderParam )
