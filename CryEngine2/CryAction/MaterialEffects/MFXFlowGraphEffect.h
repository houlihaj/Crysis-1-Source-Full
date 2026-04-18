////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   MFXFlowGraphEffect.h
//  Version:     v1.00
//  Created:     29/11/2006 by AlexL-Benito GR
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __MFXFLOWGRAPHEFFECT_H__
#define __MFXFLOWGRAPHEFFECT_H__

#pragma once

#include "IMFXEffect.h"

struct SMFXFlowGraphParams 
{
	static const int MAX_CUSTOM_PARAMS = 4;
	string fgName;
	float maxdistSq; // max distance (squared) for spawning this effect
	float params[MAX_CUSTOM_PARAMS];

	SMFXFlowGraphParams()
	{
		maxdistSq = 0.0f;
		memset(&params, 0, sizeof(params));
	}
};

class CMFXFlowGraphEffect :
	public IMFXEffect
{
public:
	CMFXFlowGraphEffect();
	virtual ~CMFXFlowGraphEffect();
	virtual void ReadXMLNode(XmlNodeRef& node);
	virtual void Execute(SMFXRunTimeEffectParams& params);
	virtual void GetResources(SMFXResourceList& rlist);
	virtual IMFXEffectPtr Clone();
	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->Add(m_flowGraphParams.fgName);
		IMFXEffect::GetMemoryStatistics(s);
	}
protected:
	SMFXFlowGraphParams m_flowGraphParams;
};

#endif
