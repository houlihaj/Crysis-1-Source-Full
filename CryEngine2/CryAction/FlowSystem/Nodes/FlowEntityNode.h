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

#ifndef __FlowEntityNode_h__
#define __FlowEntityNode_h__
#pragma once

#include "FlowBaseNode.h"

//////////////////////////////////////////////////////////////////////////
class CFlowEntityClass : public IFlowNodeFactory
{
public:
	CFlowEntityClass( IEntityClass *pEntityClass );
	~CFlowEntityClass();
	virtual void AddRef() { m_nRefCount++; }
	virtual void Release() { if (0 == --m_nRefCount) delete this; }
	virtual IFlowNodePtr Create( IFlowNode::SActivationInfo* );

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		SIZER_SUBCOMPONENT_NAME(s, "CFlowEntityClass");
		int nSize = sizeof(this);
		//nSize += m_classname.size();
		nSize += m_inputs.size() * sizeof(std::vector<SInputPortConfig>::value_type);
		nSize += m_outputs.size() * sizeof(std::vector<SOutputPortConfig>::value_type);
		for (size_t i=0; i<m_inputs.size(); i++)
			nSize += m_inputs[i].defaultData.GetMemorySize();

		s->AddObject(this,nSize);
	}

	void GetConfiguration( SFlowNodeConfig& );

	const char* CopyStr( const char *src );
	void FreeStr( const char *src );

	size_t GetNumOutputs() { return m_outputs.size() - 1; }

private:
	void GetInputsOutputs( IEntityClass *pEntityClass );
	friend class CFlowEntityNode;

	int m_nRefCount;
	//string m_classname;
	IEntityClass *m_pEntityClass;
	std::vector<SInputPortConfig> m_inputs;
	std::vector<SOutputPortConfig> m_outputs;
};

//////////////////////////////////////////////////////////////////////////
class CFlowEntityNodeBase : public CFlowBaseNode, public IEntityEventListener
{
public:
	CFlowEntityNodeBase()
	{
		m_entityId = 0;
		m_event = ENTITY_EVENT_LAST;
	};
	~CFlowEntityNodeBase()
	{
		UnregisterEvent( m_event );
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo * pActInfo )
	{
		if (event != eFE_SetEntityId)
			return;

		if (m_entityId)
			UnregisterEvent(m_event);
		if (pActInfo->pEntity)
			m_entityId = pActInfo->pEntity->GetId();
		else
			m_entityId = 0;
		if (m_entityId)
			RegisterEvent(m_event);
	}
	// Return entity of this node.
	IEntity* GetEntity()
	{
		return gEnv->pEntitySystem->GetEntity(m_entityId);
	}

	void RegisterEvent( EEntityEvent event )
	{
		if (event != ENTITY_EVENT_LAST)
		{
			gEnv->pEntitySystem->AddEntityEventListener( m_entityId,event,this );
			gEnv->pEntitySystem->AddEntityEventListener( m_entityId,ENTITY_EVENT_DONE,this );
		}
	}
	void UnregisterEvent( EEntityEvent event )
	{
		if ( m_entityId && event != ENTITY_EVENT_LAST )
		{
			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
			if ( pEntitySystem )
			{
				pEntitySystem->RemoveEntityEventListener( m_entityId,event,this );
				pEntitySystem->RemoveEntityEventListener( m_entityId,ENTITY_EVENT_DONE,this );
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

protected:
	EEntityEvent m_event; // This member must be set in derived class constructor.
	EntityId m_entityId;
};

//////////////////////////////////////////////////////////////////////////
// Flow node for entity.
//////////////////////////////////////////////////////////////////////////
class CFlowEntityNode : public CFlowEntityNodeBase
{
public:
	CFlowEntityNode( CFlowEntityClass *pClass,SActivationInfo *pActInfo );
	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo );
	virtual void GetConfiguration( SFlowNodeConfig& );
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo * pActInfo );
	virtual bool SerializeXML( SActivationInfo *, const XmlNodeRef&, bool );

	//////////////////////////////////////////////////////////////////////////
	// IEntityEventListener
	virtual void OnEntityEvent( IEntity *pEntity,SEntityEvent &event );
	//////////////////////////////////////////////////////////////////////////

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

protected:
	void Serialize( SActivationInfo * pActInfo, TSerialize ser );

	IFlowGraph *m_pGraph;
	TFlowNodeId m_nodeID;
	_smart_ptr<CFlowEntityClass> m_pClass;
	int m_lastInitializeFrameId;
};


#endif // __FlowEntityNode_h__