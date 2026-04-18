#include "StdAfx.h"
#include "MGEndMusic.h"
#include "IMusicSystem.h"

CMGEndMusic::CMGEndMusic()
{
	m_fForceLimit = 10.0f;
	m_nFadeType = 1;
}

CMGEndMusic::~CMGEndMusic()
{
}

void CMGEndMusic::EnterState( SAnimationStateData& data, bool dueToRollback )
{
}

EHasEnteredState CMGEndMusic::HasEnteredState( SAnimationStateData& data )
{
	return eHES_Instant;
}

void CMGEndMusic::EnteredState( SAnimationStateData& data )
{
	if (gEnv->pMusicSystem)
	{
		m_sThemeName = gEnv->pMusicSystem->GetTheme();
		gEnv->pMusicSystem->EndTheme((EThemeFadeType)m_nFadeType, (int)m_fForceLimit, false);
	}
}

bool CMGEndMusic::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CMGEndMusic::LeaveState( SAnimationStateData& data )
{
}

void CMGEndMusic::LeftState( SAnimationStateData& data, bool wasEntered )
{
}

void CMGEndMusic::Update( SAnimationStateData& data )
{
	assert(false);
}

void CMGEndMusic::GetCompletionTimes(SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky)
{
	hard = sticky = 0.0f;
}

const IAnimationStateNodeFactory::Params * CMGEndMusic::GetParameters()
{
	static const Params params[] = 
	{
		{0}
	};
	return params;
}

IAnimationStateNodeFactory * CMGEndMusic::GetFactory()
{
	return this;
}

void CMGEndMusic::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
}

bool CMGEndMusic::Init(const XmlNodeRef& node, IAnimationGraphPtr pGraph)
{
	node->getAttr("forcelimit", m_fForceLimit);
	node->getAttr("fadetype", m_nFadeType);

	return true;
}

void CMGEndMusic::Release()
{
	delete this;
}

IAnimationStateNode * CMGEndMusic::Create()
{
	return this;
}

const char * CMGEndMusic::GetCategory()
{
	return "EndMusic";
}

const char * CMGEndMusic::GetName()
{
	return "EndMusic";
}
