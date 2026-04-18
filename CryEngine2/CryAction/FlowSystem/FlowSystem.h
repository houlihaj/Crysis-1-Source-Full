#ifndef __FLOWSYSTEM_H__
#define __FLOWSYSTEM_H__

#pragma once

#include "IFlowSystem.h"
#include <queue>
#include "FlowSystemCVars.h"

class CFlowGraphBase;

class CFlowSystem : public IFlowSystem
{
public:
	CFlowSystem();
	~CFlowSystem();
	
	// IFlowSystem
	virtual void Release();
	virtual void Update();
	virtual void Reset();
	virtual IFlowGraphPtr CreateFlowGraph();
	virtual TFlowNodeTypeId RegisterType( const char *type, IFlowNodeFactoryPtr factory );
	virtual const char* GetTypeName( TFlowNodeTypeId typeId );
	virtual TFlowNodeTypeId GetTypeId( const char* typeName );
	virtual IFlowNodeTypeIteratorPtr CreateNodeTypeIterator();
	virtual void RegisterInspector(IFlowGraphInspectorPtr pInspector, IFlowGraphPtr pGraph = 0);
	virtual void UnregisterInspector(IFlowGraphInspectorPtr pInspector, IFlowGraphPtr pGraph = 0);
	virtual void EnableInspecting(bool bEnable) { m_bInspectingEnabled = bEnable; }
	virtual bool IsInspectingEnabled() const { return m_bInspectingEnabled; }
	virtual IFlowGraphInspectorPtr GetDefaultInspector() const { return m_pDefaultInspector; }
	virtual void GetMemoryStatistics(ICrySizer * s);
	// ~IFlowSystem

	IFlowNodePtr CreateNodeOfType( IFlowNode::SActivationInfo *, TFlowNodeTypeId typeId );

	void RegisterGraph( CFlowGraphBase *pGraph );
	void UnregisterGraph( CFlowGraphBase *pGraph );

	// resembles IFlowGraphInspector currently
	void NotifyFlow(CFlowGraphBase *pGraph, const SFlowAddress from, const SFlowAddress to);
	void NotifyProcessEvent(CFlowGraphBase *pGraph, IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo *pActInfo, IFlowNode *pImpl);

	struct STypeInfo
	{
		STypeInfo(const string& typeName, IFlowNodeFactoryPtr pFactory) : name(typeName), factory(pFactory) {}
		string name;
		IFlowNodeFactoryPtr factory;
	};

	const STypeInfo& GetTypeInfo(TFlowNodeTypeId typeId) const;

private:
	typedef std::queue<XmlNodeRef> TPendingComposites;

	void LoadExtensions( string path );
	void LoadExtensionFromXml( XmlNodeRef xml, TPendingComposites * pComposites );
	void LoadComposites( TPendingComposites * pComposites );

	void RegisterAutoTypes();
	void RegisterEntityTypes();

private:
	class CNodeTypeIterator;
	typedef std::map<string, TFlowNodeTypeId> TTypeNameToIdMap;
	TTypeNameToIdMap m_typeNameToIdMap;
	std::vector<STypeInfo> m_typeRegistryVec; // 0 is invalid
	std::vector<CFlowGraphBase*> m_graphs;
	std::vector<IFlowGraphInspectorPtr> m_systemInspectors; // only inspectors which watch all graphs

	IFlowGraphInspectorPtr m_pDefaultInspector;

	CFlowSystemCVars m_cVars;

	// Inspecting enabled/disabled
	bool m_bInspectingEnabled;
};


#endif
