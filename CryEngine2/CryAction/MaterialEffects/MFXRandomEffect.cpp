#include "StdAfx.h"
#include "MFXRandomEffect.h"

namespace
{
	DynArray<int> gCandidatesArray;
};

CMFXRandomEffect::CMFXRandomEffect()
{
}

CMFXRandomEffect::~CMFXRandomEffect()
{
}

/* static */ size_t CMFXRandomEffect::GetMemoryUsage()
{
	return sizeof(gCandidatesArray) + gCandidatesArray.get_alloc_size();
}

IMFXEffectPtr CMFXRandomEffect::ChooseCandidate(SMFXRunTimeEffectParams& params)
{
	const size_t nEffects = m_effects.size();
	if (nEffects == 0)
		return 0;

	if (gCandidatesArray.capacity() == 0)
		gCandidatesArray.reserve(16); // reserve at least 16 slots at the beginning

	gCandidatesArray.reserve(nEffects);
	int* pCandidates = gCandidatesArray.begin();
	int  nCandidates = 0;
	int  i = 0;
	std::vector<IMFXEffectPtr>::const_iterator iter = m_effects.begin();
	std::vector<IMFXEffectPtr>::const_iterator iterEnd = m_effects.end();
	while (iter != iterEnd)
	{
		const IMFXEffectPtr& cur = *iter;
		if (cur)
		{
			if (cur->CanExecute(params))
			{
				pCandidates[nCandidates++] = i;
			}
		}
		++i;
		++iter;
	}
	IMFXEffectPtr pEffect = 0;
	if (nCandidates > 0)
	{
		const int randChoice = Random(nCandidates);
		pEffect = m_effects[pCandidates[randChoice]];
	}
	return pEffect;
}

void CMFXRandomEffect::Execute(SMFXRunTimeEffectParams &params)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	const size_t numEffects = m_effects.size();
	if (numEffects > 1)
	{
		IMFXEffectPtr pEffect = ChooseCandidate(params);
		if (pEffect)
		{
		  if (m_params.particleParams.Filled())
		  {
		    params.particleParams = m_params.particleParams;
		  }
			pEffect->Execute(params);
		}
	}
	else if (numEffects == 1)
	{
		IMFXEffectPtr& pEffect = *m_effects.begin();
		if (pEffect && pEffect->CanExecute(params))
			pEffect->Execute(params);
	}
	else
	{
		// num effects == 0
		;
	}
}

IMFXEffectPtr CMFXRandomEffect::Clone()
{
	CMFXRandomEffect *clone = new CMFXRandomEffect();
  clone->m_params = m_params;
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

void CMFXRandomEffect::ReadXMLNode(XmlNodeRef &node)
{
	IMFXEffect::ReadXMLNode(node);

  if (node->haveAttr("minscale"))
    node->getAttr("minscale", m_params.particleParams.minscale);

  if (node->haveAttr("maxscale"))
    node->getAttr("maxscale", m_params.particleParams.maxscale);

  if (node->haveAttr("maxscaledist"))
    node->getAttr("maxscaledist", m_params.particleParams.maxscaledist);
}

void CMFXRandomEffect::GetResources(SMFXResourceList &rlist)
{
	std::vector< IMFXEffectPtr > candidates;
	std::vector< IMFXEffectPtr >::iterator it = m_effects.begin();
	while (it != m_effects.end())
	{
		IMFXEffectPtr cur = *it;
		if (cur)
		{
			candidates.push_back(cur);
		}
		it++;
	}
	int numChildren = candidates.size();
	int randChoice = Random(numChildren);
	IMFXEffectPtr effect = candidates[randChoice];
	effect->GetResources(rlist);
}