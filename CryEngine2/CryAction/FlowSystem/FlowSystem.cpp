#include "StdAfx.h"

#include "FlowSystem.h"

#include <CryAction.h>

#include "FlowGraph.h"
#include "Nodes/FlowBaseNode.h"
#include "Nodes/FlowLogNode.h"
#include "Nodes/FlowStartNode.h"
#include "Nodes/FlowDelayNode.h"
#include "Nodes/FlowConditionNode.h"

#include "Nodes/FlowScriptedNode.h"
#include "Nodes/FlowCompositeNode.h"
#include "Nodes/FlowTimeNode.h"
#include "Nodes/FlowEntityNode.h"

#include "Inspectors/FlowInspectorDefault.h"

// hacky
#include "PersistantDebug.h"
#include "AnimationGraph/AnimationGraphManager.h"
#include "AnimationGraph/DebugHistory.h"
#include "ITimer.h"
#include "TimeOfDayScheduler.h"

//////////////////////////////////////////////////////////////////////////
// CAutoFlowNodeFactoryBase
//////////////////////////////////////////////////////////////////////////
CAutoRegFlowNodeBase* CAutoRegFlowNodeBase::m_pFirst = 0;
CAutoRegFlowNodeBase* CAutoRegFlowNodeBase::m_pLast = 0;
//////////////////////////////////////////////////////////////////////////

template <class T>
class CAutoFlowFactory : public IFlowNodeFactory
{
public:
	CAutoFlowFactory() : m_refs(0) {}
	void AddRef() { m_refs++; }
	void Release() { if (0 == --m_refs) delete this; }
	IFlowNodePtr Create( IFlowNode::SActivationInfo * pActInfo ) { return new T(pActInfo); }
	void GetMemoryStatistics(ICrySizer * s) 
	{
		SIZER_SUBCOMPONENT_NAME(s, "CAutoFlowFactory");
		s->Add(*this);
	}

private:
	int m_refs;
};

template <class T>
class CSingletonFlowFactory : public IFlowNodeFactory
{
public:
	CSingletonFlowFactory() : m_refs(0) {}
	void AddRef() { m_refs++; }
	void Release() { if (0 == --m_refs) delete this; }
	void GetMemoryStatistics(ICrySizer * s) 
	{
		SIZER_SUBCOMPONENT_NAME(s, "CSingletonFlowFactory");
		s->Add(*this);
		if (m_pInstance)
			m_pInstance->GetMemoryStatistics(s);
	}
	IFlowNodePtr Create( IFlowNode::SActivationInfo * pActInfo ) 
	{ 
		if (!m_pInstance)
			m_pInstance = new T();
		return m_pInstance;
	}

private:
	IFlowNodePtr m_pInstance;
	int m_refs;
};

CFlowSystem::CFlowSystem() : m_bInspectingEnabled(false)
{
	TFlowNodeTypeId typeId = RegisterType( "InvalidType", 0);
	assert (typeId == InvalidFlowNodeTypeId);
	RegisterType( "Log", new CSingletonFlowFactory<CFlowLogNode>() );
	RegisterType( "Start", new CAutoFlowFactory<CFlowStartNode>() );

	RegisterAutoTypes();
	RegisterEntityTypes();
	//CryLogAlways( "[flow] scanning for scripted extensions" );
	LoadExtensions( "Libs/FlowNodes" );

	m_pDefaultInspector = new CFlowInspectorDefault(this);
	RegisterInspector(m_pDefaultInspector, 0);
	EnableInspecting(false);
}


CFlowSystem::~CFlowSystem()
{
	int i = m_graphs.size();
	while ( i-- )
		m_graphs[i]->NotifyFlowSystemDestroyed();
}

void CFlowSystem::Release()
{
	UnregisterInspector(m_pDefaultInspector, 0);
	delete this;
}

IFlowGraphPtr CFlowSystem::CreateFlowGraph()
{
	return new CFlowGraph( this );
}

void CFlowSystem::LoadExtensions( string path )
{
	ICryPak * pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	char filename[_MAX_PATH];

	path.TrimRight("/\\");
	string search = path + "/*.node";
	intptr_t handle = pCryPak->FindFirst( search.c_str(), &fd );
	if (handle != -1)
	{
		int res = 0;

		TPendingComposites pendingComposites;

		do 
		{
			strcpy( filename, path.c_str() );
			strcat( filename, "/" );
			strcat( filename, fd.name );

			XmlNodeRef root = GetISystem()->LoadXmlFile( filename );
			if (root)
				LoadExtensionFromXml( root, &pendingComposites );

			res = pCryPak->FindNext( handle, &fd );
		}
		while (res >= 0);

		LoadComposites( &pendingComposites );

		pCryPak->FindClose( handle );
	}
}

void CFlowSystem::LoadExtensionFromXml( XmlNodeRef nodeParent, TPendingComposites * pComposites )
{
	XmlNodeRef node;
	int nChildren=nodeParent->getChildCount();
	int iChild;

	// run for nodeParent, and any children it contains
	for( iChild=-1,node=nodeParent ; ; )
	{
		const char * tag = node->getTag();
		if (!tag || !*tag)
			return;

		// Multiple components may be contained within a <NodeList> </NodeList> pair
		if( (0 == strcmp("NodeList", tag)) || (0 == strcmp("/NodeList", tag)) )
		{
			// a list node, do nothing.
			// if root level, will advance to child below
		}
		else
		{
			const char * name = node->getAttr( "name" );
			if (!name || !*name)
				return;

			if (0 == strcmp("Script", tag))
			{
				const char * path = node->getAttr( "script" );
				if (!path || !*path)
					return;
				CFlowScriptedNodeFactoryPtr pFactory = new CFlowScriptedNodeFactory();
				if (pFactory->Init( path, name ))
					RegisterType( name, &*pFactory );
			}
			else if (0 == strcmp("SimpleScript", tag))
			{
				const char * path = node->getAttr( "script" );
				if (!path || !*path)
					return;
				CFlowSimpleScriptedNodeFactoryPtr pFactory = new CFlowSimpleScriptedNodeFactory();
				if (pFactory->Init( path, name ))
					RegisterType( name, &*pFactory );
			}
			else if (0 == strcmp("Composite", tag))
			{
				pComposites->push( node );
			}
		}

		if(++iChild>=nChildren)
			break;
		node=nodeParent->getChild(iChild);
	}
}

void CFlowSystem::LoadComposites( TPendingComposites * pComposites )
{
	size_t failCount = 0;
	while (!pComposites->empty() && failCount < pComposites->size())
	{
		CFlowCompositeNodeFactoryPtr pFactory = new CFlowCompositeNodeFactory();
		switch (pFactory->Init( pComposites->front(), this ))
		{
		case CFlowCompositeNodeFactory::eIR_Failed:
			GameWarning( "Failed to load composite due to invalid data: %s", pComposites->front()->getAttr("name") );
			pComposites->pop();
			failCount = 0;
			break;
		case CFlowCompositeNodeFactory::eIR_Ok:
			RegisterType( pComposites->front()->getAttr("name"), &*pFactory );
			pComposites->pop();
			failCount = 0;
			break;
		case CFlowCompositeNodeFactory::eIR_NotYet:
			pComposites->push( pComposites->front() );
			pComposites->pop();
			failCount ++;
			break;
		}
	}

	while (!pComposites->empty())
	{
		GameWarning( "Failed to load composite due to failed dependency: %s", pComposites->front()->getAttr("name") );
		pComposites->pop();
	}
}

class CFlowSystem::CNodeTypeIterator : public IFlowNodeTypeIterator
{
public:
	CNodeTypeIterator( CFlowSystem* pImpl ) : m_nRefs(0), m_pImpl(pImpl), m_iter(pImpl->m_typeRegistryVec.begin()) 
	{
		assert (m_iter != m_pImpl->m_typeRegistryVec.end());
		++m_iter;
		m_id = InvalidFlowNodeTypeId;
		assert (m_id == 0);
	}
	void AddRef() 
	{ 
		++m_nRefs;
	}
	void Release()
	{ 
		if (0 == --m_nRefs)
			delete this; 
	}
	bool Next(SNodeType& nodeType)
	{
		if (m_iter == m_pImpl->m_typeRegistryVec.end())
			return false;
		nodeType.typeId = ++m_id;
		nodeType.typeName = m_iter->name;
		++m_iter;
		return true;
	}
private:
	int m_nRefs;
	int m_id;
	CFlowSystem* m_pImpl;
	std::vector<STypeInfo>::iterator m_iter;
};

IFlowNodeTypeIteratorPtr CFlowSystem::CreateNodeTypeIterator()
{
	return new CNodeTypeIterator(this);
}

TFlowNodeTypeId CFlowSystem::RegisterType( const char *type, IFlowNodeFactoryPtr factory )
{
	const TTypeNameToIdMap::const_iterator iter = m_typeNameToIdMap.find(type);
	if (iter == m_typeNameToIdMap.end())
	{
		size_t nTypeId = m_typeRegistryVec.size();
		if (nTypeId >= CFlowData::TYPE_MAX_COUNT)
		{
			CryError("CFlowSystem::RegisterType: Reached maximum amount of NodeTypes: Limit=%d", CFlowData::TYPE_MAX_COUNT);
			return InvalidFlowNodeTypeId;
		}
		string typeName = type;
		m_typeNameToIdMap[typeName] = (TFlowNodeTypeId) nTypeId;
		m_typeRegistryVec.push_back(STypeInfo(typeName,factory));
		return nTypeId;
	}
	else
	{
		// overriding
		TFlowNodeTypeId nTypeId = iter->second;
		GameWarning("CFlowSystem::RegisterType: Type '%s' Id=%d already registered. Overriding.", type, (int)nTypeId);
		assert (nTypeId < m_typeRegistryVec.size());
		STypeInfo& typeInfo = m_typeRegistryVec[nTypeId];
		typeInfo.factory = factory;
		return nTypeId;
	}
}


IFlowNodePtr CFlowSystem::CreateNodeOfType( IFlowNode::SActivationInfo * pActInfo, TFlowNodeTypeId typeId )
{
	assert (typeId < m_typeRegistryVec.size());
	if (typeId >= m_typeRegistryVec.size())
		return 0;

	const STypeInfo& type = m_typeRegistryVec[typeId];
	IFlowNodeFactory *pFactory = type.factory;
	if (pFactory)
		return pFactory->Create( pActInfo );
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::Update()
{
	{
		FRAME_PROFILER( "CFlowSystem::ForeignSystemUpdate", gEnv->pSystem, PROFILE_GAME);
		// HACK ALERT - these things need to be updated in game mode and ai/physics mode
		CCryAction::GetCryAction()->GetPersistantDebug()->Update( gEnv->pTimer->GetFrameTime() );
		CDebugHistoryManager::RenderAll();
		CCryAction::GetCryAction()->GetAnimationGraphManager()->Update();
		CCryAction::GetCryAction()->GetTimeOfDayScheduler()->Update();
	}

	{
		FRAME_PROFILER( "CFlowSystem::Update()", gEnv->pSystem, PROFILE_GAME);
		if (m_cVars.m_enableUpdates == 0)
		{
			/* 
				IRenderer * pRend = gEnv->pRenderer;
				float white[4] = {1,1,1,1};
				pRend->Draw2dLabel( 10, 100, 2, white, false, "FlowGraphSystem Disabled");
			*/
			return;
		}

		if (m_bInspectingEnabled) {
			// call pre updates

			// 1. system inspectors
			std::for_each (m_systemInspectors.begin(), m_systemInspectors.end(), std::bind2nd (std::mem_fun(&IFlowGraphInspector::PreUpdate), (IFlowGraph*) 0));

			// 2. graph inspectors TODO: optimize not to go over all graphs ;-)
			std::vector<CFlowGraphBase*>::iterator iter = m_graphs.begin();
			while (iter != m_graphs.end()) {
				const std::vector<IFlowGraphInspectorPtr>& graphInspectors ((*iter)->GetInspectors());
				std::for_each (graphInspectors.begin(), graphInspectors.end(), std::bind2nd (std::mem_fun(&IFlowGraphInspector::PreUpdate), *iter));
				++iter;
			}
		}

		CFlowGraphBase::UpdateAll();

		if (m_bInspectingEnabled) {
			// call post updates

			// 1. system inspectors
			std::for_each (m_systemInspectors.begin(), m_systemInspectors.end(), std::bind2nd (std::mem_fun(&IFlowGraphInspector::PostUpdate), (IFlowGraph*) 0));

			// 2. graph inspectors TODO: optimize not to go over all graphs ;-)
			std::vector<CFlowGraphBase*>::iterator iter = m_graphs.begin();
			while (iter != m_graphs.end()) {
				const std::vector<IFlowGraphInspectorPtr>& graphInspectors ((*iter)->GetInspectors());
				std::for_each (graphInspectors.begin(), graphInspectors.end(), std::bind2nd (std::mem_fun(&IFlowGraphInspector::PostUpdate), *iter));
				++iter;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::Reset()
{
	for (int i = 0; i < m_graphs.size(); ++i)
	{
		m_graphs[i]->InitializeValues();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::RegisterGraph( CFlowGraphBase *pGraph )
{
	stl::push_back_unique( m_graphs,pGraph );
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::UnregisterGraph( CFlowGraphBase *pGraph )
{
	stl::find_and_erase( m_graphs,pGraph );
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::RegisterAutoTypes()
{
	//////////////////////////////////////////////////////////////////////////
	CAutoRegFlowNodeBase *pFactory = CAutoRegFlowNodeBase::m_pFirst;
	while (pFactory)
	{
		RegisterType( pFactory->m_sClassName,pFactory );
		pFactory = pFactory->m_pNext;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::RegisterEntityTypes()
{
	// Register all entity class from entities registry.
	// Each entity class is registered as a flow type entity:ClassName, ex: entity:ProximityTrigger
	IEntityClassRegistry *pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	IEntityClass *pEntityClass = 0;
  pClassRegistry->IteratorMoveFirst();
	string entityPrefixStr = "entity:";
	while (pEntityClass = pClassRegistry->IteratorNext())
	{
		string classname = entityPrefixStr + pEntityClass->GetName();
		RegisterType( classname, new CFlowEntityClass(pEntityClass) );
	}
}

//////////////////////////////////////////////////////////////////////////
const CFlowSystem::STypeInfo& CFlowSystem::GetTypeInfo(TFlowNodeTypeId typeId) const
{
	assert (typeId < m_typeRegistryVec.size());
	if (typeId < m_typeRegistryVec.size())
		return m_typeRegistryVec[typeId];
	return m_typeRegistryVec[InvalidFlowNodeTypeId];
}

//////////////////////////////////////////////////////////////////////////
const char* CFlowSystem::GetTypeName( TFlowNodeTypeId typeId )
{
	assert (typeId < m_typeRegistryVec.size());
	if (typeId < m_typeRegistryVec.size())
		return m_typeRegistryVec[typeId].name.c_str();
	return "";
}

//////////////////////////////////////////////////////////////////////////
TFlowNodeTypeId CFlowSystem::GetTypeId( const char* typeName )
{
	TFlowNodeTypeId typeId = stl::find_in_map(m_typeNameToIdMap, CONST_TEMP_STRING(typeName), InvalidFlowNodeTypeId);
	if (typeId != InvalidFlowNodeTypeId)
		return typeId;

	//////////////////////////////////////////////////////////////////////////
	// Hack for Math: to Vec3: change.
	//////////////////////////////////////////////////////////////////////////
	if (strncmp(typeName,"Math:",5) == 0)
	{
		string newtype = "Vec3:";
		newtype += (typeName+5);
		typeId = stl::find_in_map(m_typeNameToIdMap, newtype, InvalidFlowNodeTypeId);
	}
	return typeId;
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::RegisterInspector(IFlowGraphInspectorPtr pInspector, IFlowGraphPtr pGraph)
{
	if (pGraph == 0) 
	{
		stl::push_back_unique(m_systemInspectors, pInspector);
	}
	else
	{
		std::vector<CFlowGraphBase*>::iterator iter (std::find(m_graphs.begin(), m_graphs.end(), pGraph));
		if (iter != m_graphs.end())
			(*iter)->RegisterInspector(pInspector);
		else
			GameWarning("CFlowSystem::RegisterInspector: Unknown graph 0x%p", (IFlowGraph *)pGraph);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::UnregisterInspector(IFlowGraphInspectorPtr pInspector, IFlowGraphPtr pGraph)
{
	if (pGraph == 0)
	{
		stl::find_and_erase(m_systemInspectors, pInspector);
	}
	else
	{
		std::vector<CFlowGraphBase*>::iterator iter (std::find(m_graphs.begin(), m_graphs.end(), pGraph));
		if (iter != m_graphs.end())
			(*iter)->UnregisterInspector(pInspector);
		else
			GameWarning("CFlowSystem::UnregisterInspector: Unknown graph 0x%p", (IFlowGraph *)pGraph);		
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::NotifyFlow(CFlowGraphBase *pGraph, const SFlowAddress from, const SFlowAddress to)
{
	if (!m_bInspectingEnabled) return;

	if (!m_systemInspectors.empty())
	{
		std::vector<IFlowGraphInspectorPtr>::iterator iter (m_systemInspectors.begin());
		while (iter != m_systemInspectors.end())
		{
			(*iter)->NotifyFlow(pGraph, from, to);
			++iter;
		}
	}
	const std::vector<IFlowGraphInspectorPtr>& graphInspectors (pGraph->GetInspectors());
	if (!graphInspectors.empty())
	{
		std::vector<IFlowGraphInspectorPtr>::const_iterator iter (graphInspectors.begin());
		while (iter != graphInspectors.end())
		{
			(*iter)->NotifyFlow(pGraph, from, to);
			++iter;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowSystem::NotifyProcessEvent(CFlowGraphBase *pGraph, IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo *pActInfo, IFlowNode *pImpl)
{
	if (!m_bInspectingEnabled) return;
}

void CFlowSystem::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "FlowSystem");
	s->Add(*this);
	
	{
		SIZER_SUBCOMPONENT_NAME(s, "Factories");
		{
			SIZER_SUBCOMPONENT_NAME(s, "FactoriesLookup");
			s->AddContainer(m_typeNameToIdMap);
			s->AddContainer(m_typeRegistryVec);
		}
		{
			SIZER_SUBCOMPONENT_NAME(s, "FactoriesData");
			for (std::vector<STypeInfo>::iterator iter = m_typeRegistryVec.begin(); iter != m_typeRegistryVec.end(); ++iter)
			{
				s->Add(iter->name);
				if (iter->factory)
					iter->factory->GetMemoryStatistics(s);
			}
		}
	}

	{
		SIZER_SUBCOMPONENT_NAME(s, "Inspectors");
		s->AddContainer(m_systemInspectors);
		for (size_t i=0; i<m_systemInspectors.size(); i++)
			m_systemInspectors[i]->GetMemoryStatistics(s);
	}

	{
		int nNodes = 0;
		int nEdges = 0;
		SIZER_SUBCOMPONENT_NAME(s, "Graphs");
		s->AddContainer(m_graphs);
		for (size_t i=0; i<m_graphs.size(); i++)
		{
			m_graphs[i]->GetMemoryStatistics(s);
			int nodes=0,edges=0;
			m_graphs[i]->GetGraphStats(nodes, edges);
			nNodes+=nodes;
			nEdges+=edges;
		}
#if defined(USER_alexl)
		CryLogAlways("[FlowSystem] MemoryStats: NumFactories=%d NumNodes=%d NumEdges=%d", m_typeRegistryVec.size(), nNodes, nEdges);
#endif
	}
}
