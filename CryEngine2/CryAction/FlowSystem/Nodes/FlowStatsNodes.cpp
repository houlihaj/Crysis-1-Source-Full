/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2005.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 30:6:2005   18:43 : Created by Mrcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "FlowBaseNode.h"


class CFlowNode_MemoryStats : public CFlowBaseNode
{
public:
	enum Outputs
	{
		OUT_SYSMEM,		
		OUT_VIDEOMEM_THISFRAME,
		OUT_VIDEOMEM_RECENTLY,
		OUT_MESHMEM,
	};
	CFlowNode_MemoryStats( SActivationInfo * pActInfo )
	{
	}
	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("sysmem"),
			OutputPortConfig<int>("videomem_thisframe"),
			OutputPortConfig<int>("videomem_recently"),
			OutputPortConfig<int>("meshmem"),
			{0}
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_DEBUG);
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
				ISystem *pSystem = GetISystem();
				IRenderer *pRenderer = pSystem->GetIRenderer();

				int sysMem = pSystem->GetUsedMemory();
				size_t vidMemThisFrame( 0 );
				size_t vidMemRecently( 0 );
				pRenderer->GetVideoMemoryUsageStats( vidMemThisFrame, vidMemRecently );
				int meshMem = *(int *)pRenderer->EF_Query(EFQ_Alloc_APIMesh);				
				
				ActivateOutput(pActInfo, OUT_SYSMEM, sysMem);
				// potentially unsafe if we start using >2gb of video memory...?
				ActivateOutput(pActInfo, OUT_VIDEOMEM_THISFRAME, int(vidMemThisFrame));
				ActivateOutput(pActInfo, OUT_VIDEOMEM_RECENTLY, int(vidMemRecently));
				ActivateOutput(pActInfo, OUT_MESHMEM, meshMem);
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


class CFlowNode_FrameStats : public CFlowBaseNode
{
public:
	enum Outputs
	{
		OUT_FRAMETIME,
		OUT_FRAMERATE,
		OUT_FRAMEID,
	};
	CFlowNode_FrameStats( SActivationInfo * pActInfo )
	{
	}
	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("frametime"),
			OutputPortConfig<float>("framerate"),
			OutputPortConfig<int>("frameid"),
			{0}
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_DEBUG);
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
				ISystem *pSystem = GetISystem();
				IRenderer *pRenderer = pSystem->GetIRenderer();

				float frameTime = pSystem->GetITimer()->GetFrameTime();
				float frameRate = pSystem->GetITimer()->GetFrameRate();
				int frameId = pRenderer->GetFrameID(false);

				ActivateOutput(pActInfo, OUT_FRAMETIME, frameTime);
				ActivateOutput(pActInfo, OUT_FRAMERATE, frameId);
				ActivateOutput(pActInfo, OUT_FRAMEID, frameRate);
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE_SINGLETON( "Stats:Memory", CFlowNode_MemoryStats );
REGISTER_FLOW_NODE_SINGLETON( "Stats:Frame", CFlowNode_FrameStats );