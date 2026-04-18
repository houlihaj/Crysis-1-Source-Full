#include "StdAfx.h"
#include "AGTransitionParams.h"

#define LAYER_NAMES LAYER_NAMES_TP

static const char * LAYER_NAMES[] = {
	"TransitionParamsLayer1",
	"TransitionParamsLayer2",
	"TransitionParamsLayer3",
	"TransitionParamsLayer4",
	"TransitionParamsLayer5",
	"TransitionParamsLayer6",
	"TransitionParamsLayer7",
	"TransitionParamsLayer8",
	"TransitionParamsLayer9",
};

void CAGTransitionParams::EnterState( SAnimationStateData& data, bool dueToRollback )
{
	CAGParams::EnterState(data, dueToRollback);
}

void CAGTransitionParams::LeaveState( SAnimationStateData& data )
{
	CAGParams::LeaveState(data);
	data.overrides[m_layer].overrideStartAfter |= m_overrides.overrideStartAfter;
	if ( m_overrides.overrideStartAtKeyFrame )
		data.overrides[m_layer] = m_overrides;
}

const IAnimationStateNodeFactory::Params * CAGTransitionParams::GetParameters()
{
	static std::vector<Params> * pVec = 0;

	if (!pVec)
	{
		pVec = new std::vector<Params>();
		AddParams( "", *pVec );

		Params paramWait = {false, "bool", "WaitForAnimation", "WaitForAnimation", "0"};
		Params paramWaitKey = {false, "float", "WaitForKeyTime", "WaitForKeyTime", "0.4"};
		pVec->push_back(paramWait);
		pVec->push_back(paramWaitKey);
		EndParams( *pVec );
	}
	return &((*pVec)[0]);
}

bool CAGTransitionParams::Init( const XmlNodeRef& node, IAnimationGraphPtr pG )
{
	bool ok = CAGParams::Init(node, pG);
	node->getAttr("WaitForAnimation", m_overrides.overrideStartAfter);
	m_overrides.overrideStartAtKeyFrame = node->haveAttr("WaitForKeyTime");
	node->getAttr("WaitForKeyTime", m_overrides.startAtKeyFrame);
	return ok;
}

const char * CAGTransitionParams::GetName()
{
	return LAYER_NAMES[m_layer];
}
#undef LAYER_NAMES
