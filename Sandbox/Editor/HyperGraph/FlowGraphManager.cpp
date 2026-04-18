////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowGraphManager.h
//  Version:     v1.00
//  Created:     8/4/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowGraphManager.h"
#include "FlowGraph.h"
#include "FlowGraphNode.h"
#include "FlowGraphVariables.h"
#include "IconManager.h"
#include "GameEngine.h"

#include "HyperGraphDialog.h"

#include <IEntitySystem.h>
#include <IFlowSystem.h>
#include <IAIAction.h>

#include "Objects\Entity.h"

#include "UIEnumsDatabase.h"
#include "Util/Variable.h"

#define FLOWGRAPH_BITMAP_FOLDER "Editor\\Icons\\FlowGraph\\"

#define FG_INTENSIVE_DEBUG
#undef  FG_INTENSIVE_DEBUG

//////////////////////////////////////////////////////////////////////////
CFlowGraphManager::~CFlowGraphManager()
{
	while ( !m_graphs.empty() )
	{
		CFlowGraph* pFG = m_graphs.back();
		if (pFG)
		{
			pFG->OnHyperGraphManagerDestroyed();
			UnregisterGraph( pFG );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::Init()
{
	CHyperGraphManager::Init();

	// Enumerate all flow graph classes.
	ReloadClasses();
	REGISTER_CVAR(fg_iEnableFlowgraphNodeDebugging, 0, VF_DUMPTODISK, "Toggles visual flowgraph debugging." );
}

//////////////////////////////////////////////////////////////////////////
CHyperGraph* CFlowGraphManager::CreateGraph()
{
	CFlowGraph *pGraph = new CFlowGraph(this);
	return pGraph;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::RegisterGraph( CFlowGraph *pGraph )
{
	if (stl::find(m_graphs, pGraph) == true)
	{
		CryLogAlways("CFlowGraphManager::RegisterGraph: OLD Graph 0x%p", pGraph);
		assert (false);
	}
	else
	{
#ifdef FG_INTENSIVE_DEBUG
		CryLogAlways("CFlowGraphManager::RegisterGraph: NEW Graph 0x%p", pGraph);
#endif
		m_graphs.push_back(pGraph);
	}
	// assert (stl::find(m_graphs, pGraph) == false);
	SendNotifyEvent( EHG_GRAPH_ADDED );
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::UnregisterGraph( CFlowGraph *pGraph )
{
#ifdef FG_INTENSIVE_DEBUG
	CryLogAlways("CFlowGraphManager::UnregisterGraph: Graph 0x%p", pGraph);
#endif
	stl::find_and_erase(m_graphs,pGraph);
	SendNotifyEvent( EHG_GRAPH_REMOVED );
}

//////////////////////////////////////////////////////////////////////////
CHyperNode* CFlowGraphManager::CreateNode( CHyperGraph* pGraph,const char *sNodeClass,HyperNodeID nodeId, const Gdiplus::PointF& /* pos */, CBaseObject* pObj)
{
	// AlexL: ignore pos, as a SetPos would screw up Undo. And it will be set afterwards anyway

	CHyperNode* pNode = 0;

	CFlowGraph *pFlowGraph = (CFlowGraph*)pGraph;
	// Check for special node classes.
	if (pObj)
	{
		pNode = CreateSelectedEntityNode(pFlowGraph, pObj);
	}
	else if (strcmp(sNodeClass,"default_entity") == 0)
	{
		pNode = CreateEntityNode( pFlowGraph,pFlowGraph->GetEntity() );
	}
	else
	{
		pNode = __super::CreateNode( pGraph,sNodeClass,nodeId );
	}

	return pNode;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::ReloadClasses()
{
	m_prototypes.clear();
	
	IFlowSystem *pFlowSystem = GetIEditor()->GetGameEngine()->GetIFlowSystem();

	IFlowGraphPtr pFlowGraph = pFlowSystem->CreateFlowGraph();

	IFlowNodeTypeIteratorPtr pTypeIterator = pFlowSystem->CreateNodeTypeIterator();
	IFlowNodeTypeIterator::SNodeType nodeType;
	while (pTypeIterator->Next(nodeType))
	{
		CFlowNode *pProtoFlowNode = new CFlowNode;
		pProtoFlowNode->SetClass( nodeType.typeName );
		m_prototypes[nodeType.typeName] = pProtoFlowNode;
		
		TFlowNodeId nodeId = pFlowGraph->CreateNode( nodeType.typeName,nodeType.typeName );
		IFlowNodeData *pFlowNodeData = pFlowGraph->GetNodeData(nodeId);
		assert(pFlowNodeData);

		GetNodeConfig( pFlowNodeData, pProtoFlowNode );
		LoadAssociatedBitmap( pProtoFlowNode );
	}
}

enum UIEnumType
{
	eUI_None,
	eUI_Int,
	eUI_Float,
	eUI_String,
	eUI_GlobalEnum,
	eUI_GlobalEnumRef
};

float GetValue(const CString& s, const char* token)
{
	float fValue = 0.0f;
	int pos = s.Find(token);
	if (pos >= 0)
	{
		// fill in Enum Pairs
		pos = s.Find('=', pos+1);
		if (pos >= 0)
		{
			sscanf(s.GetString() + pos + 1, "%f", &fValue);
		}
	}
	return fValue;
}

UIEnumType ParseUIConfig(const char* sUIConfig, std::map<CString, CString>& outEnumPairs, Vec3& uiValueRanges)
{
	UIEnumType enumType = eUI_None;
	CString uiConfig (sUIConfig);

	// ranges
	uiValueRanges.x = GetValue(uiConfig, "v_min");
	uiValueRanges.y = GetValue(uiConfig, "v_max");

	int pos = uiConfig.Find(':');
	if (pos <= 0)
		return enumType;

	CString enumTypeS = uiConfig.Mid(0,pos);
	if (enumTypeS == "enum_string")
		enumType = eUI_String;
	else if (enumTypeS == "enum_int")
		enumType = eUI_Int;
	else if (enumTypeS == "enum_float")
		enumType = eUI_Float;
	else if (enumTypeS == "enum_global")
	{
		// don't do any tokenzing. "enum_global:ENUM_NAME"
		enumType = eUI_GlobalEnum;
		CString enumName = uiConfig.Mid(pos+1);
		if (enumName.IsEmpty() == false)
			outEnumPairs[enumName] = enumName;
		return enumType;
	}
	else if (enumTypeS == "enum_global_ref")
	{
		// don't do any tokenzing. "enum_global:ENUM_NAME_FORMAT_STRING:REF_PORT"
		enumType = eUI_GlobalEnumRef;
		int pos1 = uiConfig.Find(':', pos+1);
		if (pos1 < 0)
		{
			Warning( _T("FlowGraphManager: Wrong enum_global_ref format while parsing UIConfig '%s'"), sUIConfig);
			return eUI_None;
		}

		CString enumFormat = uiConfig.Mid(pos+1, pos1 - pos - 1);
		CString enumPort = uiConfig.Mid(pos1+1);
		if (enumFormat.IsEmpty() || enumPort.IsEmpty())
		{
			Warning( _T("FlowGraphManager: Wrong enum_global_ref format while parsing UIConfig '%s'"), sUIConfig);
			return eUI_None;
		}
		outEnumPairs[enumFormat] = enumPort;
		return enumType;

	}

	if (enumType != eUI_None)
	{
		// fill in Enum Pairs
		CString values = uiConfig.Mid(pos+1);
		pos = 0;
		CString resToken = TokenizeString( values, " ,", pos);
		while (!resToken.IsEmpty())
		{
			CString str = resToken;
			CString value = str;
			int pos_e = str.Find('=');
			if (pos_e >= 0)
			{
				value = str.Mid(pos_e+1);
				str = str.Mid(0,pos_e);
			}
			outEnumPairs[str]=value;
			resToken = TokenizeString( values, " ,", pos);
		};
	}

	return enumType;
}

template<typename T>
void FillEnumVar (CVariableEnum<T>* pVar, const std::map<CString, CString>& nameValueMap)
{
	var_type::type_convertor convertor;
	T val;
	for (std::map<CString, CString>::const_iterator iter = nameValueMap.begin(); iter != nameValueMap.end(); ++iter)
	{
		convertor (iter->second,val);
		//int val = atoi (iter->second.GetString());
		pVar->AddEnumItem(iter->first, val);
	}
}


//////////////////////////////////////////////////////////////////////////
IVariable* CFlowGraphManager::MakeInVar(const SInputPortConfig* pPortConfig)
{
	int type = pPortConfig->defaultData.GetType();
	if (!pPortConfig->defaultData.IsLocked())
		type = eFDT_Any;

	IVariable* pVar = 0;
	// create variable
	
	CFlowNodeGetCustomItemsBase* pGetCustomItems = 0;

	const char* name = pPortConfig->name;
	bool isEnumDataType = false;

	Vec3 uiValueRanges (ZERO);

	// UI Parsing
	if (pPortConfig->sUIConfig != 0)
	{
		isEnumDataType = true;
		std::map<CString, CString> enumPairs;
		UIEnumType type = ParseUIConfig(pPortConfig->sUIConfig, enumPairs, uiValueRanges);
		switch (type)
		{
		case eUI_Int:
			{
				CVariableFlowNodeEnum<int>* pEnumVar = new CVariableFlowNodeEnum<int>;
				FillEnumVar<int> (pEnumVar, enumPairs);
				pVar = pEnumVar;
			}
			break;
		case eUI_Float:
			{
				CVariableFlowNodeEnum<float>* pEnumVar = new CVariableFlowNodeEnum<float>;
				FillEnumVar<float> (pEnumVar, enumPairs);
				pVar = pEnumVar;
			}
			break;
		case eUI_String:
			{
				CVariableFlowNodeEnum<CString>* pEnumVar = new CVariableFlowNodeEnum<CString>;
				FillEnumVar<CString> (pEnumVar, enumPairs);
				pVar = pEnumVar;
			}
			break;
		case eUI_GlobalEnum:
			{
				CVariableFlowNodeEnum<CString> *pEnumVar = new CVariableFlowNodeEnum<CString>;
				pEnumVar->SetFlags(IVariable::UI_USE_GLOBAL_ENUMS); // FIXME: is not really needed. may be dropped
				CString globalEnumName;
				if (enumPairs.empty())
					globalEnumName=name;
				else
					globalEnumName=enumPairs.begin()->first;

				CVarGlobalEnumList* pEnumList = new CVarGlobalEnumList(globalEnumName);
				pEnumVar->SetEnumList(pEnumList);
				pVar = pEnumVar;
			}
			break;
		case eUI_GlobalEnumRef:
			{
				assert (enumPairs.size() == 1);
				CVariableFlowNodeDynamicEnum *pEnumVar = new CVariableFlowNodeDynamicEnum (enumPairs.begin()->first, enumPairs.begin()->second);
				pEnumVar->SetFlags(IVariable::UI_USE_GLOBAL_ENUMS); // FIXME: is not really needed. may be dropped
				pVar = pEnumVar;
				// take care of custom item base
				pGetCustomItems = new CFlowNodeGetCustomItemsBase;
				pGetCustomItems->SetUIConfig(pPortConfig->sUIConfig);
			}
			break;
		default:
			isEnumDataType = false;
			break;
		}
	}

	// Make a simple var, it was no enum type
	if (pVar == 0)
	{
		pVar = MakeSimpleVarFromFlowType(type);
	}

	// Set ranges if applicable
	if (uiValueRanges.x != 0.0f || uiValueRanges.y != 0.0f)
		pVar->SetLimits(uiValueRanges.x, uiValueRanges.y, false, false);

	// Take care of predefined datatypes
	if (!isEnumDataType)
	{
		const FlowGraphVariables::MapEntry* mapEntry = 0;
		if (pPortConfig->sUIConfig != 0)
		{
			// parse UI config, if a certain datatype is specified
			// ui_dt=datatype
			CString uiConfig (pPortConfig->sUIConfig);
			const char* prefix = "dt=";
			const int pos = uiConfig.Find(prefix);
			if (pos >= 0)
			{
				CString dt = uiConfig.Mid(pos+3); // 3==strlen(prefix)
				dt = dt.SpanExcluding(" ,");
				dt+="_";
				mapEntry = FlowGraphVariables::FindEntry(dt);
			}
		}
		if (mapEntry == 0)
		{
			mapEntry = FlowGraphVariables::FindEntry(pPortConfig->name);
			if (mapEntry != 0)
				name += strlen( mapEntry->sPrefix );
		}

		if (mapEntry != 0)
		{
			pVar->SetDataType( mapEntry->eDataType );
			if (mapEntry->eDataType == IVariable::DT_USERITEMCB)
			{
				assert (pGetCustomItems == 0);
				assert (mapEntry->pGetCustomItemsCreator != 0);
				pGetCustomItems = mapEntry->pGetCustomItemsCreator();
				pGetCustomItems->SetUIConfig(pPortConfig->sUIConfig);
			}
		}
	}

	// Set Name of Variable
	pVar->SetName( pPortConfig->name ); // ALEXL 08/11/05: from now on we always use the REAL port name

	// Set HumanName of Variable
	if (pPortConfig->humanName)
		pVar->SetHumanName(pPortConfig->humanName);
	else
		pVar->SetHumanName(name);  // if there is no human name we set the 'name' (stripped prefix if it was a predefined data type!)

	// Set variable description
	if (pPortConfig->description)
		pVar->SetDescription(pPortConfig->description);

	if (pGetCustomItems)
		pGetCustomItems->AddRef();

	pVar->SetUserData(pGetCustomItems);

	return pVar;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::GetNodeConfig( IFlowNodeData *pSrcNode,CFlowNode *pFlowNode )
{
	if (!pSrcNode)
		return;
	pFlowNode->ClearPorts();
	SFlowNodeConfig config;
	pSrcNode->GetConfiguration( config );

	if (config.nFlags & EFLN_TARGET_ENTITY)
	{
		pFlowNode->SetFlag( EHYPER_NODE_ENTITY,true );
	}
	if (config.nFlags & EFLN_HIDE_UI)
	{
		pFlowNode->SetFlag( EHYPER_NODE_HIDE_UI,true );
	}

	pFlowNode->m_flowSystemNodeFlags = config.nFlags;
	if (pFlowNode->GetCategory() != EFLN_APPROVED)
	{
		pFlowNode->SetFlag( EHYPER_NODE_CUSTOM_COLOR1, true );
	}

	pFlowNode->m_szDescription = config.sDescription; // can be 0
	pFlowNode->m_szUIClassName = config.sUIClassName; // can be 0

	if (config.pInputPorts)
	{
		const SInputPortConfig *pPortConfig = config.pInputPorts;
		while (pPortConfig->name)
		{
			CHyperNodePort port;
			port.bInput = true;

			int type = pPortConfig->defaultData.GetType();
			if (!pPortConfig->defaultData.IsLocked())
				type = eFDT_Any;

			port.pVar = MakeInVar(pPortConfig);

			if (port.pVar)
			{
				switch (type)
				{
				case eFDT_Bool:
					port.pVar->Set( *pPortConfig->defaultData.GetPtr<bool>() );
					break;
				case eFDT_Int:
					port.pVar->Set( *pPortConfig->defaultData.GetPtr<int>() );
					break;
				case eFDT_Float:
					port.pVar->Set( *pPortConfig->defaultData.GetPtr<float>() );
					break;
				case eFDT_EntityId:
					port.pVar->Set( (int)*pPortConfig->defaultData.GetPtr<EntityId>() );
					port.pVar->SetFlags(port.pVar->GetFlags() | IVariable::UI_DISABLED);
					break;
				case eFDT_Vec3:
					port.pVar->Set( *pPortConfig->defaultData.GetPtr<Vec3>() );
					break;
				case eFDT_String:
					port.pVar->Set( (pPortConfig->defaultData.GetPtr<string>())->c_str() );
					break;
				}

				pFlowNode->AddPort(port);
			}
			pPortConfig++;
		}
	}
	if (config.pOutputPorts)
	{
		const SOutputPortConfig *pPortConfig = config.pOutputPorts;
		while (pPortConfig->name)
		{
			CHyperNodePort port;
			port.bInput = false;
			port.pVar = MakeSimpleVarFromFlowType( pPortConfig->type );
			if (port.pVar)
			{
				port.pVar->SetName( pPortConfig->name );
				if (pPortConfig->description)
					port.pVar->SetDescription(pPortConfig->description);
				if (pPortConfig->humanName)
					port.pVar->SetHumanName(pPortConfig->humanName);
				pFlowNode->AddPort(port);
			}
			pPortConfig++;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IVariable* CFlowGraphManager::MakeSimpleVarFromFlowType( int type )
{
	switch (type)
	{
	case eFDT_Void:
		return new CVariableFlowNodeVoid;
		break;
	case eFDT_Bool:
		return new CVariableFlowNode<bool>;
		break;
	case eFDT_Int:
		return new CVariableFlowNode<int>;
		break;
	case eFDT_Float:
		return new CVariableFlowNode<float>;
		break;
	case eFDT_EntityId:
		return new CVariableFlowNode<int>;
		break;
	case eFDT_Vec3:
		return new CVariableFlowNode<Vec3>;
		break;
	case eFDT_String:
		return new CVariableFlowNode<CString>;
		break;
	default:
		// Any type.
		return new CVariableFlowNodeVoid;
		break;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::LoadAssociatedBitmap( CFlowNode *pFlowNode )
{
	CString classname = pFlowNode->GetClassName();
	classname.Replace( ':','\\' );
	CString bmpFilename = Path::Make( FLOWGRAPH_BITMAP_FOLDER,classname,"bmp" );
	pFlowNode->SetBitmap( GetIEditor()->GetIconManager()->GetIconBitmap( bmpFilename ) );
}

//////////////////////////////////////////////////////////////////////////
CFlowGraph* CFlowGraphManager::CreateGraphForEntity( CEntity *pEntity,const char *sGroupName )
{
	CFlowGraph *pFlowGraph = new CFlowGraph(this,sGroupName);
	pFlowGraph->SetEntity( pEntity );
#ifdef FG_INTENSIVE_DEBUG
	CryLogAlways("CFlowGraphManager::CreateGraphForEntity: graph=0x%p entity=0x%p guid=%s", pFlowGraph, pEntity, GuidUtil::ToString(pEntity->GetId()));
#endif
	return pFlowGraph;
}

//////////////////////////////////////////////////////////////////////////
CFlowGraph* CFlowGraphManager::FindGraphForAction( IAIAction* pAction )
{
	for ( int i = 0; i < m_graphs.size(); ++i )
		if ( m_graphs[i]->GetAIAction() == pAction )
			return m_graphs[i];
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
CFlowGraph* CFlowGraphManager::CreateGraphForAction( IAIAction* pAction )
{
	CFlowGraph* pFlowGraph = new CFlowGraph( this );
	pFlowGraph->GetIFlowGraph()->SetSuspended(true);
	pFlowGraph->SetAIAction( pAction );
	return pFlowGraph;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::FreeGraphsForActions()
{
	int i = m_graphs.size();
	while ( i-- )
	{
		if ( m_graphs[i]->GetAIAction() )
		{
			// delete will call UnregisterGraph and remove itself from the vector
		//	assert( m_graphs[i]->NumRefs() == 1 );
			m_graphs[i]->SetAIAction( NULL );
			m_graphs[i]->SetName( "" );
			m_graphs[i]->SetName( "Default" );
			m_graphs[i]->Release();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CFlowNode* CFlowGraphManager::CreateSelectedEntityNode( CFlowGraph* pFlowGraph, CBaseObject *pSelObj)
{
	if (pSelObj && pSelObj->IsKindOf(RUNTIME_CLASS(CEntity)))
	{
		return CreateEntityNode( pFlowGraph,(CEntity*)pSelObj );
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CFlowNode* CFlowGraphManager::CreateEntityNode( CFlowGraph* pFlowGraph,CEntity *pEntity )
{
	if (!pEntity || !pEntity->GetIEntity())
		return 0;

	string classname = string("entity:") + pEntity->GetIEntity()->GetClass()->GetName();
	CFlowNode *pNode = (CFlowNode*)__super::CreateNode( pFlowGraph,classname.c_str(),pFlowGraph->AllocateId() );
	if (pNode)
	{
		// check if we add it to an AIAction
		if (pFlowGraph->GetAIAction() == 0)
			pNode->SetEntity(pEntity);
		else
			pNode->SetEntity(0); // AIAction -> reset entity
	}
	
	return pNode;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::OpenFlowGraphView( IFlowGraph *pIFlowGraph )
{
	assert(pIFlowGraph);
	CFlowGraph *pFlowGraph = new CFlowGraph(this,pIFlowGraph);
	OpenView( pFlowGraph );
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::UnregisterAndResetView( CFlowGraph *pFlowGraph )
{
	if (pFlowGraph)
	{
		// CryLogAlways("CFlowGraphManager::UnregisterAndResetView: Graph to 0x%p", pFlowGraph);
		UnregisterGraph(pFlowGraph);
		CWnd *pWnd = GetIEditor()->FindView( "Flow Graph" );
		if (pWnd != 0 && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
		{
			((CHyperGraphDialog*)pWnd)->SetGraph( 0 );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::GetAvailableGroups(std::set<CString>& outGroups, bool bActionGraphs)
{
	std::vector<CFlowGraph*>::iterator iter = m_graphs.begin();
	while (iter != m_graphs.end())
	{
		CFlowGraph* pGraph = *iter;
		if (pGraph != 0)
		{
			if (bActionGraphs == false)
			{
				CEntity* pEntity = pGraph->GetEntity();
				if (pEntity && pEntity->CheckFlags(OBJFLAG_PREFAB) == false)
				{
					CString& groupName = pGraph->GetGroupName();
					if (groupName.IsEmpty() == false)
						outGroups.insert(groupName);
				}
			}
			else
			{
				IAIAction* pAction = pGraph->GetAIAction();
				if (pAction)
				{
					CString& groupName = pGraph->GetGroupName();
					if (groupName.IsEmpty() == false)
						outGroups.insert(groupName);
				}
			}
		}
		++iter;
	}
}
