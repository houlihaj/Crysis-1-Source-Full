#include "StdAfx.h"
#include "MGDelayLeaving.h"
#include "IMusicSystem.h"
#include "AnimationGraphState.h"

CMGDelayLeavingNode::CMGDelayLeavingNode( CMGDelayLeavingFactory * pFactory ) : IAnimationStateNode(eASNF_Update)
{
	m_pFactory = pFactory;
	m_timeRemaining = pFactory->m_time;
	m_entered = false;
	m_speedup_threshold = pFactory->m_speedup_threshold;
}

CMGDelayLeavingNode::~CMGDelayLeavingNode()
{
}

void CMGDelayLeavingNode::EnterState( SAnimationStateData& data, bool dueToRollback )
{
}

EHasEnteredState CMGDelayLeavingNode::HasEnteredState( SAnimationStateData& data )
{
	return eHES_Instant;
}

void CMGDelayLeavingNode::EnteredState( SAnimationStateData& data )
{
	m_entered = true;
	m_timeRemaining = m_pFactory->m_time;
	m_speedup_threshold = m_pFactory->m_speedup_threshold;
}

bool CMGDelayLeavingNode::CanLeaveState( SAnimationStateData& data )
{
	IAnimationGraph::InputID ID = data.pState->GetInputId("Allow_Change");

	if ((data.queryChanged == true) && data.pState->GetInputAsFloat(ID) > m_speedup_threshold)
	{
		if (gEnv->pGame)
			if (gEnv->pGame->GetIGameFramework())
				if (gEnv->pGame->GetIGameFramework()->GetMusicLogic())
					gEnv->pGame->GetIGameFramework()->GetMusicLogic()->SetEvent(eMUSICLOGICEVENT_CHANGE_ALLOWCHANGE, -m_speedup_threshold);
		return true;
	}
	

	return !m_entered || m_timeRemaining <= 0;
}

void CMGDelayLeavingNode::LeaveState( SAnimationStateData& data )
{
}

void CMGDelayLeavingNode::LeftState( SAnimationStateData& data, bool wasEntered )
{
	delete this;
}

void CMGDelayLeavingNode::Update( SAnimationStateData& data )
{
	m_timeRemaining -= gEnv->pTimer->GetFrameTime();
}

void CMGDelayLeavingNode::GetCompletionTimes(SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky)
{
	hard = sticky = 0.0f;
}

IAnimationStateNodeFactory * CMGDelayLeavingNode::GetFactory()
{
	return m_pFactory;
}

void CMGDelayLeavingNode::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "time remaining: %.2f / %.2f", m_timeRemaining, m_pFactory->m_time );
	y += yIncrement;
	pRenderer->Draw2dLabel( x, y, 1, white, false, "speedup threshold: %.2f / %.2f", m_speedup_threshold, m_pFactory->m_speedup_threshold );
	y += yIncrement;
}

/*
 * Factory here
 */

CMGDelayLeavingFactory::CMGDelayLeavingFactory()
{
	m_time = 0;
	m_speedup_threshold = 0;
}

CMGDelayLeavingFactory::~CMGDelayLeavingFactory()
{
}

bool CMGDelayLeavingFactory::Init(const XmlNodeRef& node, IAnimationGraphPtr pGraph)
{
	node->getAttr("time", m_time);
	node->getAttr("speedupthreshold", m_speedup_threshold);

	return true;
}

void CMGDelayLeavingFactory::Release()
{
	delete this;
}

IAnimationStateNode * CMGDelayLeavingFactory::Create()
{
	return new CMGDelayLeavingNode(this);
}

const char * CMGDelayLeavingFactory::GetCategory()
{
	return "MusicDelayLeaving";
}

const char * CMGDelayLeavingFactory::GetName()
{
	return "MusicDelayLeaving";
}

const IAnimationStateNodeFactory::Params * CMGDelayLeavingFactory::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "float", "time",    "Time",                                      "" },
		{0}
	};
	return params;
}
