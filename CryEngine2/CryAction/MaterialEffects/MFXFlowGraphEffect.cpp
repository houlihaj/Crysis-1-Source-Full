////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   MFXFlowGraphEffect.cpp
//  Version:     v1.00
//  Created:     29/11/2006 by AlexL-Benito GR
//  Compilers:   Visual Studio.NET
//  Description: IMFXEffect implementation
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "MFXFlowGraphEffect.h"
#include "MaterialEffects.h"
#include "MaterialFGManager.h"
#include "MaterialEffectsCVars.h"
#include "CryAction.h"

namespace
{
	CMaterialFGManager* GetFGManager()
	{
		CMaterialEffects* pMFX = static_cast<CMaterialEffects*> (CCryAction::GetCryAction()->GetIMaterialEffects());
		if (pMFX == 0)
			return 0;
		CMaterialFGManager* pMFXFGMgr = pMFX->GetFGManager();
		assert (pMFXFGMgr != 0);
		return pMFXFGMgr;
	}
};

CMFXFlowGraphEffect::CMFXFlowGraphEffect()
{
}

CMFXFlowGraphEffect::~CMFXFlowGraphEffect()
{
	CMaterialFGManager* pMFXFGMgr = GetFGManager();
	if (pMFXFGMgr && m_flowGraphParams.fgName.empty() == false)
		pMFXFGMgr->EndFGEffect(m_flowGraphParams.fgName);
}

//CMFXFlowGraphEffect::ReadXMLNodeXmlNodeRef &node)
void CMFXFlowGraphEffect::ReadXMLNode(XmlNodeRef &node)
{
	IMFXEffect::ReadXMLNode(node);
	m_flowGraphParams.fgName = node->getAttr("name");
	if(node->haveAttr("maxdist"))
	{
		node->getAttr("maxdist", m_flowGraphParams.maxdistSq);
		m_flowGraphParams.maxdistSq*=m_flowGraphParams.maxdistSq;
	}
	int i = 0;
	node->getAttr("param1", m_flowGraphParams.params[i++]);
	node->getAttr("param2", m_flowGraphParams.params[i++]);
	node->getAttr("param3", m_flowGraphParams.params[i++]);
	node->getAttr("param4", m_flowGraphParams.params[i++]);
}

IMFXEffectPtr CMFXFlowGraphEffect::Clone()
{
	CMFXFlowGraphEffect* clone = new CMFXFlowGraphEffect();
	clone->m_flowGraphParams = m_flowGraphParams;
	clone->m_effectParams = m_effectParams;
	return clone;
}

//CMFXFlowGraphEffect::Execute(SMFXRunTimeEffectParams &params)
//Starts the effect
void CMFXFlowGraphEffect::Execute(SMFXRunTimeEffectParams& params)
{
  FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	if (!(params.playflags & MFX_PLAY_FLOWGRAPH) || CMaterialEffectsCVars::Get().mfx_EnableFGEffects == 0)
		return;

	float distToPlayerSq = 0.f;
	IActor *pAct = gEnv->pGame->GetIGameFramework()->GetClientActor();
	if (pAct)
	{
		distToPlayerSq = (pAct->GetEntity()->GetWorldPos() - params.pos).GetLengthSquared();
	}

	//Check max distance
	if(m_flowGraphParams.maxdistSq == 0.f || distToPlayerSq <= m_flowGraphParams.maxdistSq)
	{
		CMaterialFGManager* pMFXFGMgr = GetFGManager();
		pMFXFGMgr->StartFGEffect(m_flowGraphParams, cry_sqrtf(distToPlayerSq));
		//if(pMFXFGMgr->StartFGEffect(m_flowGraphParams.fgName))
		//	CryLogAlways("Starting FG HUD effect %s (player distance %f)", m_flowGraphParams.fgName.c_str(),distToPlayer);
	}
}

void CMFXFlowGraphEffect::GetResources(SMFXResourceList& rlist)
{
	SMFXFlowGraphListNode *listNode = SMFXFlowGraphListNode::Create();
	listNode->m_flowGraphParams.name = m_flowGraphParams.fgName;

	SMFXFlowGraphListNode* next = rlist.m_flowGraphList;

	if (!next)
		rlist.m_flowGraphList = listNode;
	else
	{ 
		while (next->pNext)
			next = next->pNext;

		next->pNext = listNode;
	}  
}