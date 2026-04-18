////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowFlashNode.h
//  Version:     v1.00
//  Created:     23/5/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowBaseNode.h"
#include <IVideoPlayer.h>

////////////////////////////////////////////////////////////////////////////////////////
// Flow node for communicating with a DTS_I_VIDEOPLAYER dyntexture stored in an entity's material
////////////////////////////////////////////////////////////////////////////////////////
class CFlowVideoPlayerNode : public CFlowBaseNode
{
public:
	CFlowVideoPlayerNode( SActivationInfo *pActInfo ) 
	{
	}

	enum EInputPorts
	{
		EIP_Slot = 0,
		EIP_SubMtlId,
		EIP_TexSlot,
		EIP_Start,
		EIP_Stop,
		EIP_Pause,
		EIP_UnPause,
	};

	enum EOutputPorts
	{
	};

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<int> ("Slot", 0, _HELP("Material Slot")),
			InputPortConfig<int> ("SubMtlId", 0, _HELP("Sub Material Id")),
			InputPortConfig<int> ("TexSlot", 0, _HELP("Texture Slot")),
			InputPortConfig_Void ("Start", _HELP("Trigger to start")),
			InputPortConfig_Void ("Stop", _HELP("Trigger to stop")),
			InputPortConfig_Void ("Pause", _HELP("Trigger to pause")),
			InputPortConfig_Void ("UnPause", _HELP("Trigger to unpause")),
			{0}
		};

		static const SOutputPortConfig out_config[] = {
			{0}
		};

		config.sDescription = _HELP("Interface to VideoPlayer in DynTexture");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	IVideoPlayer* GetVideoPlayer(SActivationInfo* pActInfo)
	{
		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == 0)
			return 0;

		IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy == 0)
			return 0;

		const int slot = GetPortInt(pActInfo, EIP_Slot);
		IMaterial* pMtl = pRenderProxy->GetRenderMaterial(slot);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowVideoPlayerNode: Entity '%s' [%d] has no material at slot %d", pEntity->GetName(), pEntity->GetId(), slot);
			return 0;
		}

		const int& subMtlId = GetPortInt(pActInfo, EIP_SubMtlId);
		pMtl = pMtl->GetSafeSubMtl(subMtlId);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowVideoPlayerNode: Entity '%s' [%d] has no sub-material %d at slot %d", pEntity->GetName(), pEntity->GetId(), subMtlId, slot);
			return 0;
		}

		const int texSlot = GetPortInt(pActInfo, EIP_TexSlot);
		const SShaderItem& shaderItem(pMtl->GetShaderItem());
		if (shaderItem.m_pShaderResources)
		{
			if (shaderItem.m_pShaderResources->GetTexture(texSlot))
			{
				SEfResTexture* pTex(shaderItem.m_pShaderResources->GetTexture(texSlot));
				if (pTex->m_Sampler.m_pDynTexSource)
				{
					IVideoPlayer* pVideoPlayer(0);
					IDynTextureSource::EDynTextureSource type(IDynTextureSource::DTS_I_VIDEOPLAYER);

					pTex->m_Sampler.m_pDynTexSource->GetDynTextureSource((void*&)pVideoPlayer, type);
					if (pVideoPlayer && type == IDynTextureSource::DTS_I_VIDEOPLAYER)
						return pVideoPlayer;
					else
						GameWarning("[flow] CFlowFlashInvokeNode: Entity '%s' [%d] has no VideoDynTexture at sub-material %d at slot %d at texslot %d", pEntity->GetName(), pEntity->GetId(), subMtlId, slot, texSlot);
				}	
				else
					GameWarning("[flow] CFlowFlashInvokeNode: Entity '%s' [%d] has no dyn-texture at sub-material %d at slot %d at texslot %d", pEntity->GetName(), pEntity->GetId(), subMtlId, slot, texSlot);
			}
			else
				GameWarning("[flow] CFlowFlashInvokeNode: Entity '%s' [%d] has no texture at sub-material %d at slot %d at texslot %d", pEntity->GetName(), pEntity->GetId(), subMtlId, slot, texSlot);
		}
		return 0;
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate)
			return;

		IVideoPlayer* pVideoPlayer = GetVideoPlayer(pActInfo);
		if (pVideoPlayer == 0)
			return;
		if (IsPortActive(pActInfo, EIP_Stop))
		{
			pVideoPlayer->EnablePerFrameUpdate(false);
			pVideoPlayer->Stop();
		}
		if (IsPortActive(pActInfo, EIP_Start))
		{
			pVideoPlayer->Start();
			pVideoPlayer->EnablePerFrameUpdate(true);
			pVideoPlayer->Pause(false);
		}
		if (IsPortActive(pActInfo, EIP_Pause))
		{
			pVideoPlayer->Pause(true);
		}
		if (IsPortActive(pActInfo, EIP_UnPause))
		{
			pVideoPlayer->Pause(false);
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


REGISTER_FLOW_NODE_SINGLETON( "Entity:VideoPlayer",CFlowVideoPlayerNode )
