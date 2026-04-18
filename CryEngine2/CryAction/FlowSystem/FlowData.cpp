#include "StdAfx.h"
#include "FlowData.h"
#include "FlowSerialize.h"

// do some magic stuff to be backwards compatible with flowgraphs
// which don't use the REAL port name but a stripped version 

#define FG_ALLOW_STRIPPED_PORTNAMES
// #undef  FG_ALLOW_STRIPPED_PORTNAMES

#define FG_WARN_ABOUT_STRIPPED_PORTNAMES
#undef  FG_WARN_ABOUT_STRIPPED_PORTNAMES

CFlowData::CFlowData()
	: m_nInputs(0),
		m_nOutputs(0),
		m_pInputData(0), 
		m_pOutputFirstEdge(0), 
		m_hasEntity(false),
		m_failedGettingFlowgraphForwardingEntity(false),
		m_hasAlternateEntity(false),
		m_getFlowgraphForwardingEntity(0)
{
}

CFlowData::CFlowData( IFlowNodePtr pImpl, const string& name, TFlowNodeTypeId typeId )
{
	m_pImpl = pImpl;
	m_name = name;
	m_typeId = typeId;
	m_getFlowgraphForwardingEntity = 0;
	m_failedGettingFlowgraphForwardingEntity = false;
	m_hasAlternateEntity = false;

	SFlowNodeConfig config;
	DoGetConfiguration( config );
	m_hasEntity = 0 != (config.nFlags & EFLN_TARGET_ENTITY);
	if (!config.pInputPorts)
		m_nInputs = 0;
	else for (m_nInputs = 0; config.pInputPorts[m_nInputs].name; m_nInputs++)
		;
	if (!config.pOutputPorts)
		m_nOutputs = 0;
	else for (m_nOutputs = 0; config.pOutputPorts[m_nOutputs].name; m_nOutputs++)
		;
	m_pInputData = new TFlowInputData[m_nInputs];
	for (int i=0; i<m_nInputs; i++)
		m_pInputData[i] = config.pInputPorts[i].defaultData;
	m_pOutputFirstEdge = new int[m_nOutputs];
}

CFlowData::~CFlowData()
{
	if (m_getFlowgraphForwardingEntity)
		gEnv->pScriptSystem->ReleaseFunc(m_getFlowgraphForwardingEntity);
	delete[] m_pInputData;
	delete[] m_pOutputFirstEdge;
}

CFlowData::CFlowData( const CFlowData& rhs )
{
	m_nInputs = rhs.m_nInputs;
	m_nOutputs = rhs.m_nOutputs;
	m_pImpl = rhs.m_pImpl;
	m_pInputData = new TFlowInputData[m_nInputs];
	m_pOutputFirstEdge = new int[m_nOutputs];
	m_name = rhs.m_name;
	m_typeId = rhs.m_typeId;
	m_hasEntity = rhs.m_hasEntity;
	m_failedGettingFlowgraphForwardingEntity = true;
	m_getFlowgraphForwardingEntity = 0;
	m_hasAlternateEntity = false;
	for (int i=0; i<m_nInputs; i++)
		m_pInputData[i] = rhs.m_pInputData[i];
	for (int i=0; i<m_nOutputs; i++)
		m_pOutputFirstEdge[i] = rhs.m_pOutputFirstEdge[i];
}

void CFlowData::DoGetConfiguration( SFlowNodeConfig& config ) const
{
	static const int MAX_INPUT_PORTS = 64;
	static const int MAX_OUTPUT_PORTS = 64;

	m_pImpl->GetConfiguration( config );
	if (config.nFlags & EFLN_TARGET_ENTITY)
	{
		static SInputPortConfig inputs[MAX_INPUT_PORTS];

		SInputPortConfig * pInput = inputs;
		*pInput++ = InputPortConfig<EntityId>("entityId", _HELP("Changes the attached entity dynamically") );

		if (config.pInputPorts)
		{
			while (config.pInputPorts->name)
			{
				assert( pInput != inputs + MAX_INPUT_PORTS );
				*pInput++ = *config.pInputPorts++;
			}
		}

		SInputPortConfig nullInput = {0};
		*pInput++ = nullInput;

		config.pInputPorts = inputs;
	}
}

void CFlowData::Swap( CFlowData& rhs )
{
	std::swap( m_nInputs, rhs.m_nInputs );
	std::swap( m_nOutputs, rhs.m_nOutputs );
	std::swap( m_pInputData, rhs.m_pInputData );
	std::swap( m_pOutputFirstEdge, rhs.m_pOutputFirstEdge );
	std::swap( m_pImpl, rhs.m_pImpl );
	m_name.swap( rhs.m_name );
	TFlowNodeTypeId typeId = rhs.m_typeId; rhs.m_typeId = m_typeId; m_typeId = typeId;
	bool temp;
	temp = rhs.m_hasEntity;	rhs.m_hasEntity = m_hasEntity; m_hasEntity = temp;
	temp = rhs.m_failedGettingFlowgraphForwardingEntity; rhs.m_failedGettingFlowgraphForwardingEntity = m_failedGettingFlowgraphForwardingEntity; m_failedGettingFlowgraphForwardingEntity = temp;
	temp = rhs.m_hasAlternateEntity; rhs.m_hasAlternateEntity = m_hasAlternateEntity; m_hasAlternateEntity = temp;
	std::swap( m_getFlowgraphForwardingEntity, rhs.m_getFlowgraphForwardingEntity);
}

CFlowData& CFlowData::operator =( const CFlowData& rhs )
{
	CFlowData temp(rhs);
	Swap(temp);
	return *this;
}

bool CFlowData::ResolvePort( const char * name, TFlowPortId& port, bool isOutput )
{
	SFlowNodeConfig config;
	DoGetConfiguration( config );
	if (isOutput)
	{
		for (int i=0; i<m_nOutputs; i++)
		{
			if (!stricmp(name, config.pOutputPorts[i].name))
			{
				port = i;
				return true;
			}
		}
	}
	else
	{
		for (int i=0; i<m_nInputs; i++)
		{
			const char * sPortName = config.pInputPorts[i].name;

			if (!stricmp(name, sPortName)) {
				port = i;
				return true;
			}

#ifdef FG_ALLOW_STRIPPED_PORTNAMES
			// fix for t_ stuff in current graphs these MUST NOT be stripped!
			if (sPortName != 0 && sPortName[0] == 't' && sPortName[1] == '_') {
				if (!stricmp(name, sPortName)) {
					port = i;
					return true;
				}
			}
			else 
			{
				// strip special char '_' and text before it!
				// it defines special data type only...
				const char * sSpecial = strchr(sPortName, '_');
				if (sSpecial)
					sPortName = sSpecial + 1;

				if (!stricmp(name, sPortName))
				{
#ifdef FG_WARN_ABOUT_STRIPPED_PORTNAMES
					CryLogAlways("[flow] CFlowData::ResolvePort: Deprecation warning for port name: '%s' should be '%s'", 
						sPortName, config.pInputPorts[i].name);
					CryLogAlways("[flow] Solution is to resave flowgraph in editor!");
#endif
					port = i;
					return true;
				}
			}
#endif
		}
	}

	return false;
}

bool CFlowData::SerializeXML( IFlowNode::SActivationInfo * pActInfo, const XmlNodeRef& node, bool reading )
{
	SFlowNodeConfig config;
	DoGetConfiguration( config );
	if (reading)
	{
		if (XmlNodeRef inputsNode = node->findChild("Inputs"))
		{
			for (int i=m_hasEntity; i<m_nInputs; i++)
			{
				const char * value = inputsNode->getAttr(config.pInputPorts[i].name);
				if (value[0])
					SetFromString( m_pInputData[i], value);
#ifdef FG_ALLOW_STRIPPED_PORTNAMES
				else 
				{
					// strip special char '_' and text before it!
					// it defines special data type only...
					const char * sPortName = config.pInputPorts[i].name;
					const char * sSpecial = strchr(sPortName, '_');
					if (sSpecial)
						sPortName = sSpecial + 1;

					value = inputsNode->getAttr(sPortName);
					if (value[0]) {
#ifdef FG_WARN_ABOUT_STRIPPED_PORTNAMES
						CryLogAlways("[flow] CFlowData::SerializeXML: Deprecation warning for port name: '%s' should be '%s'", 
							sPortName, config.pInputPorts[i].name);
						CryLogAlways("[flow] Solution is to resave flowgraph in editor!");
#endif
						SetFromString( m_pInputData[i], value );
					}
				}
#endif
			}
		}

		if ( config.nFlags & EFLN_TARGET_ENTITY )
		{
			EntityId entityId;
			const char *sGraphEntity = node->getAttr("GraphEntity");
			if (*sGraphEntity != 0)
			{
				int nIndex = atoi(sGraphEntity);
				if (nIndex == 0)
				{
					if (SetEntityId((EntityId)EFLOWNODE_ENTITY_ID_GRAPH1))
						pActInfo->pGraph->ActivateNode( pActInfo->myID );
				}
				else
				{
					if (SetEntityId((EntityId)EFLOWNODE_ENTITY_ID_GRAPH2))
						pActInfo->pGraph->ActivateNode( pActInfo->myID );
				}
			}
			else
			{
				if (node->haveAttr("EntityGUID_64"))
				{
					EntityGUID entGuid = 0;
					node->getAttr( "EntityGUID_64",entGuid );
					entityId = gEnv->pEntitySystem->FindEntityByGuid(entGuid);
					if (entityId)
					{
						if (SetEntityId(entityId))
							pActInfo->pGraph->ActivateNode( pActInfo->myID );
					}
					else
					{
#if defined(_MSC_VER)
						GameWarning( "Flow Graph Node targets unknown entity guid: %I64x",entGuid );
#else
						GameWarning( "Flow Graph Node targets unknown entity guid: %llx", (long long)entGuid );
#endif
					}
				}
			}
		}
	}

	return true;
}

void CFlowData::Serialize( IFlowNode::SActivationInfo * pActInfo, TSerialize ser )
{
	SFlowNodeConfig config;
	DoGetConfiguration( config );

	int startIndex = 0;

	// need to make sure we activate the node if we change the entity id due to serialization
	// so that the node ends up knowing
	if (m_hasEntity && ser.IsReading())
	{
		startIndex = 1;
		TFlowInputData data;
		ser.Value( config.pInputPorts[0].name, data );

#if 0
		// AlexL: 5th September 2007: actually we should only activate and re-set the EntityId if it actually changes
		// otherwise nodes like Entity:EntityId will re-trigger on quickload for no reason and confusing other nodes
		// but we leave things as they were for now... see below, always re-setting the EntityId and sending an eFE_SetEntityId event
		EntityId oldId = *(m_pInputData[0].GetPtr<EntityId>());
		EntityId newId = *data.GetPtr<EntityId>();

		if (oldId != newId)
		{
			SetEntityId(newId);
			pActInfo->pGraph->ActivateNode( pActInfo->myID );
		}
#else
		// this will always re-set the EntityId of nodes on quickload (causing re-activations after QL)
		SetEntityId(*data.GetPtr<EntityId>());
		pActInfo->pGraph->ActivateNode( pActInfo->myID );
#endif

	}

	for (int i=startIndex; i<m_nInputs; i++)
	{
		ser.Value( config.pInputPorts[i].name, m_pInputData[i] );
	}

	CompleteActivationInfo(pActInfo);
	m_pImpl->Serialize(pActInfo, ser);
}

void CFlowData::FlagInputPorts()
{
	for (int i=0; i<m_nInputs; i++)
		m_pInputData[i].SetUserFlag(true);
}

bool CFlowData::SetEntityId(EntityId id)
{
	if (m_hasEntity)
	{
		m_pInputData[0].Set(id);
		m_pInputData[0].SetUserFlag(true);

		if (m_getFlowgraphForwardingEntity)
			gEnv->pScriptSystem->ReleaseFunc(m_getFlowgraphForwardingEntity);
		m_getFlowgraphForwardingEntity = 0;
		m_failedGettingFlowgraphForwardingEntity = true;
		if (id == 0)
			m_hasAlternateEntity = false;
	}
	return m_hasEntity;
}

bool CFlowData::NoForwarding( IFlowNode::SActivationInfo * pActInfo )
{
	if (m_hasAlternateEntity)
	{
		m_pImpl->ProcessEvent(IFlowNode::eFE_SetEntityId, pActInfo);
		m_hasAlternateEntity = false;
	}
	return false;
}

bool CFlowData::ForwardingActivated( IFlowNode::SActivationInfo * pActInfo, IFlowNode::EFlowEvent event )
{
//	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	CompleteActivationInfo( pActInfo );

	if (m_failedGettingFlowgraphForwardingEntity)
	{
		IEntity * pEntity = pActInfo->pEntity;
		if (pEntity)
		{
			SmartScriptTable scriptTable = pEntity->GetScriptTable();
			if (!!scriptTable)
			{
				scriptTable->GetValue("GetFlowgraphForwardingEntity", m_getFlowgraphForwardingEntity);
				m_failedGettingFlowgraphForwardingEntity = false;
			}
			else
			{
				return NoForwarding(pActInfo);
			}
		}
		else
		{
			return NoForwarding(pActInfo);
		}
	}

	if (!m_getFlowgraphForwardingEntity)
		return NoForwarding(pActInfo);

	IEntity * pEntity = pActInfo->pEntity;
	if (!pEntity)
		return NoForwarding(pActInfo);

	SmartScriptTable pTable = pEntity->GetScriptTable();
	if (!pTable)
		return NoForwarding(pActInfo);
	ScriptHandle ent;
	if (!Script::CallReturn( pTable->GetScriptSystem(), m_getFlowgraphForwardingEntity, pTable, ent ))
		return NoForwarding(pActInfo);

	pActInfo->pEntity = gEnv->pEntitySystem->GetEntity( (EntityId)ent.n );
	if (!m_hasAlternateEntity)
	{
		m_pImpl->ProcessEvent(IFlowNode::eFE_SetEntityId, pActInfo);
		m_hasAlternateEntity = true;
	}

	m_pImpl->ProcessEvent( event, pActInfo );
	return HasForwarding(pActInfo);
}
