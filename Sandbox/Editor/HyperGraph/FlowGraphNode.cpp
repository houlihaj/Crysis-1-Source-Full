////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowGraphNode.h
//  Version:     v1.00
//  Created:     7/4/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
//  Modified:    22/2/2008 Visual FG Debugging by Jan Müller
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowGraphNode.h"
#include "FlowGraphVariables.h"
#include "GameEngine.h"
#include "FlowGraph.h"
#include "Objects\Entity.h"
#include "Objects\Group.h"

#define FG_ALLOW_STRIPPED_PORTNAMES
//#undef  FG_ALLOW_STRIPPED_PORTNAMES

#define FG_WARN_ABOUT_STRIPPED_PORTNAMES
#undef  FG_WARN_ABOUT_STRIPPED_PORTNAMES

#define TITLE_COLOR_APPROVED       Gdiplus::Color(20,200,20)
#define TITLE_COLOR_ADVANCED       Gdiplus::Color(40,40,220)
#define TITLE_COLOR_DEBUG          Gdiplus::Color(220,180,20)
#define TITLE_COLOR_OBSOLETE       Gdiplus::Color(255,0,0)

//////////////////////////////////////////////////////////////////////////
CFlowNode::CFlowNode()
: CHyperNode()
{
	m_flowNodeId = InvalidFlowNodeId;
	m_szDescription = ""; 
	m_szUIClassName = 0; // 0 is ok -> means use GetClassName() 
	ZeroStruct(m_entityGuid);
}

//////////////////////////////////////////////////////////////////////////
CFlowNode::~CFlowNode()
{
}

//////////////////////////////////////////////////////////////////////////
IFlowGraph* CFlowNode::GetIFlowGraph() const
{
	CFlowGraph* pGraph = static_cast<CFlowGraph*> (GetGraph());
	return pGraph ? pGraph->GetIFlowGraph() : 0;
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::Init()
{
	//const char *sNodeName = GuidUtil::ToString(GetId());
	char temp[16];
	const char *sNodeName = temp;
	if (m_name.IsEmpty())
	{
		sprintf( temp,"%d",GetId() );
	}
	else
		sNodeName = m_name;
	m_flowNodeId = GetIFlowGraph()->CreateNode( m_classname,sNodeName );
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::Done()
{
	if (m_flowNodeId != InvalidFlowNodeId)
		GetIFlowGraph()->RemoveNode(m_flowNodeId);
	m_flowNodeId = InvalidFlowNodeId;
	m_flowNodeId = 0;
	m_pEntity = 0;
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetName( const char *sName )
{
	GetIFlowGraph()->SetNodeName( m_flowNodeId,sName );
	__super::SetName( sName );
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetFromNodeId( TFlowNodeId flowNodeId )
{
	m_flowNodeId = flowNodeId;
	
	IFlowNodeData *pNode = GetIFlowGraph()->GetNodeData(m_flowNodeId);
	if (pNode)
	{
		//m_entityId = pNode->GetEntityId();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::DebugPortActivation(TFlowPortId port, const char *value)
{
	m_portActivationMap[port] = string(value);

	CHyperGraph* pGraph = static_cast<CHyperGraph*> (GetGraph());
	if(pGraph)
		pGraph->SendNotifyEvent(this, EHG_NODE_CHANGE_DEBUG_PORT);
}

//////////////////////////////////////////////////////////////////////////
bool CFlowNode::IsPortActivationModified(const CHyperNodePort *port)
{
	if(port)
	{
		const char *value = stl::find_in_map(m_portActivationMap, port->nPortIndex, NULL);
		if(value && strlen(value) > 0)
			return true;
		return false;
	}
	return m_portActivationMap.size() > 0; 
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::ClearDebugPortActivation()
{
	m_portActivationMap.clear();
}

//////////////////////////////////////////////////////////////////////////
CString CFlowNode::GetDebugPortValue(const CHyperNodePort &pp)
{
	const char *value = stl::find_in_map(m_portActivationMap, pp.nPortIndex, NULL);
	if(value && strlen(value) > 0 && stricmp(value, "out") && stricmp(value, "unknown"))
	{
		string portName = GetPortName(pp);
		portName += "=";
		portName += value;
		return portName.c_str();
	}
	return GetPortName(pp);
}

//////////////////////////////////////////////////////////////////////////
CHyperNode* CFlowNode::Clone()
{
	CFlowNode *pNode = new CFlowNode;
	pNode->CopyFrom(*this);

	pNode->m_entityGuid = m_entityGuid;
	pNode->m_pEntity = m_pEntity;
	pNode->m_flowSystemNodeFlags = m_flowSystemNodeFlags;
	pNode->m_szDescription = m_szDescription;
	pNode->m_szUIClassName = m_szUIClassName;

#if 1 // AlexL: currently there is a per-variable-instance GetCustomItem interface
	// as there is a problem with undo
	// tell all input-port variables which node they belong to
	// output-port variables are not told (currently no need)
	Ports::iterator iter = pNode->m_inputs.begin();
	while (iter != pNode->m_inputs.end())
	{
		IVariable* pVar = (*iter).pVar;
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*> (pVar->GetUserData());
		if (pGetCustomItems != 0)
		{
			pGetCustomItems->SetFlowNode(pNode);
		}
		++iter;
	}
#endif

	return pNode;
}

//////////////////////////////////////////////////////////////////////////
IUndoObject* CFlowNode::CreateUndo()
{
	// we create a full copy of the flowgraph. See comment in CFlowGraph::CUndoFlowGraph
	CHyperGraph* pGraph = static_cast<CHyperGraph*> (GetGraph());
	assert (pGraph != 0);
	return pGraph->CreateUndo();
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetSelectedEntity()
{
	CBaseObject *pSelObj = GetIEditor()->GetSelectedObject();
	if (pSelObj && pSelObj->IsKindOf(RUNTIME_CLASS(CEntity)))
	{
		SetEntity( (CEntity*)pSelObj );
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetDefaultEntity()
{
	CEntity *pEntity = ((CFlowGraph*)GetGraph())->GetEntity();
	SetEntity( pEntity );
	SetFlag( EHYPER_NODE_GRAPH_ENTITY,true );
	SetFlag( EHYPER_NODE_GRAPH_ENTITY2,false );
}

//////////////////////////////////////////////////////////////////////////
CEntity* CFlowNode::GetDefaultEntity() const
{
	CEntity *pEntity = ((CFlowGraph*)GetGraph())->GetEntity();
	return pEntity;
}


//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetEntity( CEntity *pEntity )
{
	SetFlag( EHYPER_NODE_ENTITY|EHYPER_NODE_ENTITY_VALID,true );
	SetFlag( EHYPER_NODE_GRAPH_ENTITY,false );
	SetFlag( EHYPER_NODE_GRAPH_ENTITY2,false );
	m_pEntity = pEntity;
	if (pEntity)
	{
		if (pEntity != GetDefaultEntity())
			m_entityGuid = pEntity->GetId();
		else
		{
			m_entityGuid = GuidUtil::NullGuid;
			SetFlag( EHYPER_NODE_GRAPH_ENTITY,true );
		}
	}
	else
	{
		ZeroStruct(m_entityGuid);
		SetFlag( EHYPER_NODE_ENTITY_VALID,false );
	}
	GetIFlowGraph()->SetEntityId( m_flowNodeId, m_pEntity? m_pEntity->GetEntityId() : 0 );
/* removed... craig... new structure
	IFlowNodeData *pNode = GetIFlowGraph()->GetNodeData(m_flowNodeId);
	if (pNode)
	{
		pNode->SetEntityId( m_pEntity? m_pEntity->GetEntityId() : 0 );
	}
*/
}

//////////////////////////////////////////////////////////////////////////
CEntity* CFlowNode::GetEntity() const
{
	return m_pEntity;
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::Unlinked( bool bInput )
{
	if (bInput)
		SetInputs(false, true);
	Invalidate(true);
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::SetInputs( bool bActivate, bool bForceResetEntities )
{
	IFlowGraph *pGraph = GetIFlowGraph();

	// small hack to make sure that entity id on target-entity nodes doesnt
	// get reset here
	int startPort = 0;
	if (CheckFlag(EHYPER_NODE_ENTITY))
		startPort = 1;

	for (int i = startPort; i < m_inputs.size(); i++)
	{
		IVariable *pVar = m_inputs[i].pVar;
		int type = pVar->GetType();
		if (type == IVariable::UNKNOWN)
			continue;

		// all variables which are NOT editable are not set!
		// e.g. EntityIDs
		if (bForceResetEntities == false && pVar->GetFlags() & IVariable::UI_DISABLED)
			continue;

		TFlowInputData value;
		bool bSet = false;

		switch (type)
		{
		case IVariable::INT:
			{
				int v;
				pVar->Get(v);
				bSet = value.Set(v);
			}
			break;
		case IVariable::BOOL:
			{
				bool v;
				pVar->Get(v);
				bSet = value.Set(v);
			}
			break;
		case IVariable::FLOAT:
			{
				float v;
				pVar->Get(v);
				bSet = value.Set(v);
			}
			break;
		case IVariable::VECTOR:
			{
				Vec3 v;
				pVar->Get(v);
				bSet = value.Set(v);
			}
			break;
		case IVariable::STRING:
			{
				CString v;
				pVar->Get(v);
				bSet = value.Set( string((const char*)v) );
				//string *str = value.GetPtr<string>();
			}
			break;
		}
		if (bSet)
		{
			if (bActivate)
			{
				// we explicitly set the value here, because the flowgraph might not be enabled
				// in which case the ActivatePort would not set the value
				pGraph->SetInputValue( m_flowNodeId,i,value );
				pGraph->ActivatePort( SFlowAddress(m_flowNodeId,i,false),value );
			}
			else
				pGraph->SetInputValue( m_flowNodeId,i,value );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::OnInputsChanged()
{
	SetInputs( true );
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
const char* CFlowNode::GetDescription()
{
	if (m_szDescription && *m_szDescription)
		return m_szDescription;
	return "";
}

//////////////////////////////////////////////////////////////////////////
const char* CFlowNode::GetUIClassName() const
{
	if (m_szUIClassName && *m_szUIClassName)
		return m_szUIClassName;
	return GetClassName();
}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::Serialize( XmlNodeRef &node,bool bLoading, CObjectArchive* ar)
{
	__super::Serialize( node,bLoading, ar );

	if (bLoading)
	{
#ifdef  FG_ALLOW_STRIPPED_PORTNAMES
		// backwards compatibility
		// variables (port values) are now saved with its real name

		XmlNodeRef portsNode = node->findChild( "Inputs" );
		if (portsNode)
		{
			for (int i = 0; i < m_inputs.size(); i++)
			{
				IVariable *pVar = m_inputs[i].pVar;
				if (pVar->GetType() != IVariable::UNKNOWN) {
					if (portsNode->haveAttr(pVar->GetName()) == false) {
						// if we did not find a value for the variable we try to use the old name
						// with the stripped version
						const char * sVarName = pVar->GetName();
						const char * sSpecial = strchr(sVarName, '_');
						if (sSpecial)
							sVarName = sSpecial + 1;
						const char * val = portsNode->getAttr(sVarName);
						if (val[0]) {
							pVar->Set(val);
#ifdef FG_WARN_ABOUT_STRIPPED_PORTNAMES
							CryLogAlways("CFlowNode::Serialize: FG '%s': Value of Deprecated Variable %s -> %s successfully resolved.",
								GetGraph()->GetName(), sVarName, pVar->GetName());
							CryLogAlways(" --> To get rid of this warning re-save flowgraph!",sVarName);
#endif
						} 
#ifdef FG_WARN_ABOUT_STRIPPED_PORTNAMES
						else {
							CryLogAlways("CFlowNode::Serialize: FG '%s': Can't resolve value for '%s' <may not be severe>", GetGraph()->GetName(), sVarName);
						}
#endif
					}
				}
			}
		}
#endif

		SetInputs(false);

		m_nFlags &= ~(EHYPER_NODE_GRAPH_ENTITY|EHYPER_NODE_GRAPH_ENTITY2);

		m_pEntity = 0;
		if (CheckFlag(EHYPER_NODE_ENTITY))
		{
			const bool bIsAIAction = (m_pGraph != 0 && m_pGraph->GetAIAction() != 0);
			if (bIsAIAction)
			{
				SetEntity(0);
			}
			else
			{
				if (node->getAttr( "EntityGUID",m_entityGuid ))
				{
					// If empty GUId then it is a default graph entity.
					if (GuidUtil::IsEmpty(m_entityGuid))
					{
						SetFlag( EHYPER_NODE_GRAPH_ENTITY,true );
						SetEntity( GetDefaultEntity() );
					}
					else
					{
						GUID origGuid = m_entityGuid;
						if (ar)
						{
							// use ar to remap ids
							m_entityGuid = ar->ResolveID(m_entityGuid);
						}
						CBaseObject *pObj = GetIEditor()->GetObjectManager()->FindObject(m_entityGuid);
						if (!pObj)
						{
							SetEntity( NULL );
							// report error
							CErrorRecord err;
							err.module = VALIDATOR_MODULE_FLOWGRAPH;
							err.severity = CErrorRecord::ESEVERITY_WARNING;
							err.error.Format("FlowGraph %s Node %d (Class %s) targets unknown entity %s",
								m_pGraph->GetName(), GetId(), GetClassName(), GuidUtil::ToString(origGuid)	);
							err.pItem = 0;
							GetIEditor()->GetErrorReport()->ReportError( err );
						}
						else if (pObj->IsKindOf(RUNTIME_CLASS(CEntity)))
						{
							SetEntity( (CEntity*)pObj );
						}
					}
				}
				else
					SetEntity( NULL );
			}
			if (node->haveAttr("GraphEntity"))
			{
				const char *sEntity = node->getAttr("GraphEntity");
					int index = atoi(node->getAttr( "GraphEntity"));
				if (index == 0)
				{
					SetFlag(EHYPER_NODE_GRAPH_ENTITY,true);
					if (bIsAIAction == false) 
						SetEntity( GetDefaultEntity() );
				}
				else
					SetFlag(EHYPER_NODE_GRAPH_ENTITY2,true);
			}
		}
	}
	else // saving
	{
		if (CheckFlag(EHYPER_NODE_GRAPH_ENTITY) || CheckFlag(EHYPER_NODE_GRAPH_ENTITY2))
		{
			int index = 0;
			if (CheckFlag(EHYPER_NODE_GRAPH_ENTITY2))
				index = 1;
			node->setAttr( "GraphEntity",index );
		}
		else 
		{
			if (!GuidUtil::IsEmpty(m_entityGuid))
			{
				node->setAttr( "EntityGUID",m_entityGuid );
				EntityGUID entid64 = ToEntityGuid(m_entityGuid);
				if (entid64)
					node->setAttr( "EntityGUID_64",entid64 );
			}

			//if (m_pEntity)
			//node->setAttr( "EntityId",m_pEntity->GetEntityId() );
		}
	}

}

//////////////////////////////////////////////////////////////////////////
void CFlowNode::PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx )
{
	// fix up all references
	CBaseObject *pOld = GetIEditor()->GetObjectManager()->FindObject(m_entityGuid);
	CBaseObject* pNew = ctx.FindClone(pOld);
	if (pNew && pNew->IsKindOf(RUNTIME_CLASS(CEntity)))
	{
		SetEntity((CEntity*)pNew);
	}
}

//////////////////////////////////////////////////////////////////////////
Gdiplus::Color CFlowNode::GetCategoryColor()
{
	// This color coding is used to identify critical and obsolete nodes in the flowgraph editor. [Jan Müller]
	int32 category = GetCategory();
	switch (category)
	{
	case EFLN_APPROVED:
		return TITLE_COLOR_APPROVED;
		//return CHyperNode::GetCategoryColor();
	case EFLN_ADVANCED:
		return TITLE_COLOR_ADVANCED;
	case EFLN_DEBUG:
		return TITLE_COLOR_DEBUG;
	case EFLN_OBSOLETE:
		return TITLE_COLOR_OBSOLETE;
	default:
		return CHyperNode::GetCategoryColor();
	}
}

//////////////////////////////////////////////////////////////////////////
CString CFlowNode::GetEntityTitle() const
{
	if (CheckFlag(EHYPER_NODE_ENTITY))
	{
		// Check if entity port is connected.
		if (m_inputs.size() > 0 && m_inputs[0].nConnected != 0)
			return "<Input Entity>";
	}
	if (CheckFlag(EHYPER_NODE_GRAPH_ENTITY))
	{
		if (m_pGraph->GetAIAction() != 0)
			return "<User Entity>";
		else
			return "<Graph Entity>";
	}
	else if (CheckFlag(EHYPER_NODE_GRAPH_ENTITY2))
	{
		if (m_pGraph->GetAIAction() != 0)
			return "<Object Entity>";
		else
			return "<Graph Entity 2>";
	}

	CEntity *pEntity = GetEntity();
	if (pEntity)
	{
		if (pEntity->GetGroup())
		{
			CString name = "[";
			name += pEntity->GetGroup()->GetName();
			name += "] ";
			name += pEntity->GetName();
			return name;
		}
		else
			return pEntity->GetName();
	}
	return "Choose Entity";
}

//////////////////////////////////////////////////////////////////////////
CString CFlowNode::GetPortName( const CHyperNodePort &port )
{
	//IFlowGraph *pGraph = GetIFlowGraph();

	//const TFlowInputData *pInputValue = pGraph->GetInputValue( m_flowNodeId,port.nPortIndex );

	if (port.bInput)
	{
		CString text = port.pVar->GetHumanName();
		if (port.pVar->GetType() != IVariable::UNKNOWN && !port.nConnected)
		{
			text = text + "=" + VarToValue(port.pVar);

			/*
			if (pInputValue)
			{
				switch (pInputValue->GetType())
				{
				case eFDT_Int:
					break;
				case eFDT_Float:
					break;
				}
			}
			*/
		}
		return text;
	}
	return port.pVar->GetHumanName();
}

const char* CFlowNode::GetCategoryName() const
{
	const uint32 cat = GetCategory();
	switch (cat)
	{
	case EFLN_APPROVED:
		return _T("Release");
	case EFLN_ADVANCED:
		return _T("Advanced");
	case EFLN_DEBUG:
		return _T("Debug");
	//case EFLN_WIP:
	//	return _T("WIP");
	//case EFLN_LEGACY:
	//	return _T("Legacy");
	//case EFLN_NOCATEGORY:
	//	return _T("No Category");
	case EFLN_OBSOLETE:
		return _T("Obsolete");
	default:
		return _T("UNDEFINED!");
	}
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CFlowNode::GetInputsVarBlock()
{
	CVarBlock *pVarBlock = __super::GetInputsVarBlock();

#if 0 // commented out, because currently there is per-variable IGetCustomItems
	// which is set on node creation (in CFlowNode::Clone)
	for (int i = 0; i < pVarBlock->GetVarsCount(); ++i) 
	{
		IVariable *pVar = pVarBlock->GetVariable(i);
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*> (pVar->GetUserData());
		if (pGetCustomItems != 0)
			pGetCustomItems->SetFlowNode(this);
	}
#endif

	return pVarBlock;
}

//////////////////////////////////////////////////////////////////////////
CString CFlowNode::GetTitle() const
{
	CString title;
	if (m_name.IsEmpty())
	{
		title = m_classname;

#if 0 // show full group:class 
		{
			int p = title.Find(':');
			if (p >= 0)
				title = title.Mid(p+1);
		}
#endif

#if 0
		// Hack: drop AI: from AI: nodes (does not look nice)
		{
			int p = title.Find("AI:");
			if (p >= 0)
				title = title.Mid(p+3);
		}
#endif

	}
	else
		title = m_name + " (" + m_classname + ")";

	return title;
}

