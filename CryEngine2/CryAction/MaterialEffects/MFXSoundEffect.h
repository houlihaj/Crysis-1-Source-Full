#include "IMFXEffect.h"
#pragma once
#include "ISystem.h"

struct SMFXSoundParams {
	string name;
	bool bIgnoreDistMult;
};

class CMFXSoundEffect :
	public IMFXEffect
{
public:
	CMFXSoundEffect();
	virtual ~CMFXSoundEffect();
	virtual void Execute(SMFXRunTimeEffectParams& params);
	virtual void GetResources(SMFXResourceList& rlist);
	virtual void ReadXMLNode(XmlNodeRef& node);
	virtual IMFXEffectPtr Clone();
	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->Add(m_soundParams.name);
		IMFXEffect::GetMemoryStatistics(s);
	}
protected:
	SMFXSoundParams m_soundParams;
};
