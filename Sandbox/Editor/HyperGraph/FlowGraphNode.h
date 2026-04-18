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

#ifndef __FlowGraphNode_h__
#define __FlowGraphNode_h__
#pragma once

#include <IEntitySystem.h>
#include <IFlowSystem.h>

#include "HyperGraphNode.h"

class CFlowNode;
class CEntity;

//////////////////////////////////////////////////////////////////////////
class CFlowNode : public CHyperNode
{
	friend class CFlowGraphManager;

public:
	CFlowNode();
	virtual ~CFlowNode();

	//////////////////////////////////////////////////////////////////////////
	// CHyperNode implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void Init();
	virtual void Done();
	virtual CHyperNode* Clone();
	virtual const char* GetDescription();
	virtual void Serialize( XmlNodeRef &node,bool bLoading, CObjectArchive* ar);
	virtual void SetName( const char *sName );
	virtual void OnInputsChanged();
	virtual void Unlinked( bool bInput );
	virtual CString GetPortName( const CHyperNodePort &port );
	virtual void PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx );
	virtual Gdiplus::Color GetCategoryColor();
	virtual void DebugPortActivation(TFlowPortId port, const char *value);
	virtual bool IsPortActivationModified(const CHyperNodePort *port = NULL);
	virtual void ClearDebugPortActivation();
	virtual CString GetDebugPortValue(const CHyperNodePort &pp);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// FlowSystem Flags. We don't want to replicate those again....
	//////////////////////////////////////////////////////////////////////////
	uint32 GetCoreFlags() const { return m_flowSystemNodeFlags & EFLN_CORE_MASK; }
	uint32 GetCategory() const { return m_flowSystemNodeFlags & EFLN_CATEGORY_MASK; }
	uint32 GetUsageFlags() const { return m_flowSystemNodeFlags & EFLN_USAGE_MASK; }
	const char* GetCategoryName() const;
	const char* GetUIClassName() const;

	void SetFromNodeId( TFlowNodeId flowNodeId );

	void SetEntity( CEntity *pEntity );
	CEntity* GetEntity() const;



	// Takes selected entity as target entity.
	void SetSelectedEntity();
	// Takes graph default entity as target entity.
	void SetDefaultEntity();
	CEntity* GetDefaultEntity() const;

	// Returns IFlowNode.
	IFlowGraph* GetIFlowGraph() const;

	// Return ID of the flow node.
	TFlowNodeId GetFlowNodeId() { return m_flowNodeId; }

	void SetInputs( bool bActivate, bool bForceResetEntities = false);
	virtual CVarBlock* GetInputsVarBlock();

	virtual CString GetTitle() const;

	virtual IUndoObject* CreateUndo();
protected:
	virtual CString GetEntityTitle() const;

protected:
	GUID m_entityGuid;
	_smart_ptr<CEntity> m_pEntity;
	uint32 m_flowSystemNodeFlags;
	TFlowNodeId m_flowNodeId;
	const char* m_szUIClassName;
	const char* m_szDescription;
	std::map<TFlowPortId, string> m_portActivationMap;
};


#endif // __FlowGraphNode_h__
