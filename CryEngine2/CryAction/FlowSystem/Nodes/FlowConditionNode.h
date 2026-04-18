#ifndef __FLOWCONDITIONNODE_H__
#define __FLOWCONDITIONNODE_H__

#pragma once

#include "FlowBaseNode.h"

class CFlowNode_Condition : public CFlowBaseNode
{
public:
	CFlowNode_Condition( SActivationInfo * ) {}

	void GetConfiguration( SFlowNodeConfig &config );
	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo );

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

#endif
