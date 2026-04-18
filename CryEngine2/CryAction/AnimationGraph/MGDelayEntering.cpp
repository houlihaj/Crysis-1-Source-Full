#include "StdAfx.h"
#include "MGDelayEntering.h"
#include "IMusicSystem.h"

CMGDelayEnteringNode::CMGDelayEnteringNode( CMGDelayEnteringFactory * pFactory ) : IAnimationStateNode(eASNF_Update)
{
	m_pFactory = pFactory;
	m_timeRemaining = pFactory->m_time;
}

CMGDelayEnteringNode::~CMGDelayEnteringNode()
{
}

void CMGDelayEnteringNode::EnterState( SAnimationStateData& data, bool dueToRollback )
{
	m_timeRemaining = m_pFactory->m_time;
}

EHasEnteredState CMGDelayEnteringNode::HasEnteredState( SAnimationStateData& data )
{
	if (m_timeRemaining > 0.0f)
		return eHES_Waiting;
	else
		return eHES_Entered;
}

void CMGDelayEnteringNode::EnteredState( SAnimationStateData& data )
{
}

bool CMGDelayEnteringNode::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CMGDelayEnteringNode::LeaveState( SAnimationStateData& data )
{
}

void CMGDelayEnteringNode::LeftState( SAnimationStateData& data, bool wasEntered )
{
	delete this;
}

void CMGDelayEnteringNode::Update( SAnimationStateData& data )
{
	m_timeRemaining -= gEnv->pTimer->GetFrameTime();
}

void CMGDelayEnteringNode::GetCompletionTimes(SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky)
{
	hard = sticky = 0.0f;
}

IAnimationStateNodeFactory * CMGDelayEnteringNode::GetFactory()
{
	return m_pFactory;
}

void CMGDelayEnteringNode::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "time remaining: %.2f / %.2f", m_timeRemaining, m_pFactory->m_time );
	y += yIncrement;
}

/*
 * Factory here
 */

CMGDelayEnteringFactory::CMGDelayEnteringFactory()
{
	m_time = 0;
}

CMGDelayEnteringFactory::~CMGDelayEnteringFactory()
{
}

bool CMGDelayEnteringFactory::Init(const XmlNodeRef& node, IAnimationGraphPtr pGraph)
{
	node->getAttr("time", m_time);
	return true;
}

void CMGDelayEnteringFactory::Release()
{
	delete this;
}

IAnimationStateNode * CMGDelayEnteringFactory::Create()
{
	return new CMGDelayEnteringNode(this);
}

const char * CMGDelayEnteringFactory::GetCategory()
{
	return "MusicDelayEntering";
}

const char * CMGDelayEnteringFactory::GetName()
{
	return "MusicDelayEntering";
}

const IAnimationStateNodeFactory::Params * CMGDelayEnteringFactory::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "float", "time",    "Time",                                      "" },
		{0}
	};
	return params;
}
