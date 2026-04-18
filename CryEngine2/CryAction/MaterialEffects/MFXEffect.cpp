#include "StdAfx.h"
#include "MFXEffect.h"

CMFXEffect::CMFXEffect()
{
}
CMFXEffect::~CMFXEffect()
{
}

void CMFXEffect::Execute(SMFXRunTimeEffectParams& params)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	std::vector<IMFXEffectPtr>::iterator iter = m_effects.begin();
	std::vector<IMFXEffectPtr>::iterator iterEnd = m_effects.end();
	while (iter != iterEnd)
	{
		IMFXEffectPtr& cur = *iter;
		if (cur)
		{
			if (cur->CanExecute(params))
				cur->Execute(params);
		}
		++iter;
	}
}

IMFXEffectPtr CMFXEffect::Clone()
{
	CMFXEffect *clone = new CMFXEffect();
	clone->m_effectParams = m_effectParams;
	std::vector<IMFXEffectPtr>::iterator iter = m_effects.begin();
	std::vector<IMFXEffectPtr>::iterator iterEnd = m_effects.end();
	while (iter != iterEnd)
	{
		IMFXEffectPtr& cur = *iter;
		if (cur)
		{
			IMFXEffectPtr childClone = cur->Clone();
			clone->AddChild(childClone);
		}
		++iter;
	}
	return clone;
}

void CMFXEffect::ReadXMLNode(XmlNodeRef& node)
{
	IMFXEffect::ReadXMLNode(node);
}

void CMFXEffect::GetResources(SMFXResourceList& rlist)
{
	std::vector<IMFXEffectPtr>::iterator it = m_effects.begin();
	while (it != m_effects.end())
	{
		IMFXEffectPtr cur = *it;
		if (cur)
		{
			cur->GetResources(rlist);
		}
		it++;
	}
}

void CMFXEffect::GetMemoryStatistics(ICrySizer * s)
{
	IMFXEffect::GetMemoryStatistics(s);
	s->Add(*this);
}