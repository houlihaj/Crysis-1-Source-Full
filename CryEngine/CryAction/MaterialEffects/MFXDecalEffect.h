////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   IMFXEffect.h
//  Version:     v1.00
//  Created:     28/11/2006 by JohnN/AlexL
//  Compilers:   Visual Studio.NET
//  Description: Decal effect
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __MFXDECALEFFECT_H__
#define __MFXDECALEFFECT_H__

#pragma once

#include "IMFXEffect.h"

struct SMFXDecalParams 
{
	string filename;
	string material;
	float minscale;
	float maxscale;
	float lifetime;
};

class CMFXDecalEffect :
	public IMFXEffect
{
public:
	CMFXDecalEffect();
	virtual ~CMFXDecalEffect();
	virtual void ReadXMLNode(XmlNodeRef& node);
	virtual void Execute(SMFXRunTimeEffectParams& params);
	virtual void GetResources(SMFXResourceList& rlist);
	virtual IMFXEffectPtr Clone();
	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->Add(m_decalParams.filename);
		s->Add(m_decalParams.material);
		IMFXEffect::GetMemoryStatistics(s);
	}
protected:
	SMFXDecalParams m_decalParams;
};

#endif
